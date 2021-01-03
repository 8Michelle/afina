#include "Connection.h"

#include <iostream>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    _logger->debug("Connection started on {} socket", _socket);
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    _running = true;
    _offset = 0;
}


// See Connection.h
void Connection::OnError() {
    _logger->error("Error on socket {}", _socket);
    _running = false;
}


// See Connection.h
void Connection::OnClose() {
    _running = false;
    _logger->debug("Connection closed on socket {}", _socket);
}


// See Connection.h
void Connection::DoRead() {
    if (_queue.size() > _max_queue_size){
        _event.events &= ~EPOLLIN;
    }

    try {
        int readed_bytes = -1;
        while ((readed_bytes = read(_socket, _buffer + _pos, sizeof(_buffer) - _pos)) > 0) {
            _logger->debug("Got {} bytes from socket", readed_bytes);
            _pos += readed_bytes;

            while (_pos > 0) {
                _logger->debug("Process {} bytes", _pos);

                if (!command_to_execute) {
                    size_t parsed = 0;

                    if (parser.Parse(_buffer, _pos, parsed)) {
                        _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(_buffer, _buffer + parsed, _pos - parsed);
                        _pos -= parsed;
                    }
                }

                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", _pos, arg_remains);
                    size_t to_read = std::min(arg_remains, size_t(_pos));
                    argument_for_command.append(_buffer, to_read);
                    std::memmove(_buffer, _buffer + to_read, _pos - to_read);
                    arg_remains -= to_read;
                    _pos -= to_read;
                }

                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    result += "\r\n";
                    bool was_empty = _queue.empty();
                    _queue.push_back(result);

                    if (was_empty) {
                        _event.events |= EPOLLOUT;
                    }
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (_pos > 0)
        }

        if (readed_bytes == 0) {
            _logger->debug("Connection closed");
            _running = false;
        } else {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        if (errno != EAGAIN) {
            _logger->error("Failed to read connection on descriptor {}: {}", _socket, ex.what());
            _running = false;
        }
    }
}


void Connection::DoWrite() {
    _logger->debug("Writing on socket {}", _socket);
    static constexpr size_t max_queue = 64;
    iovec write_vector[max_queue];
    size_t write_vector_ind = 0;

    try {
        auto it = _queue.begin();
        write_vector[write_vector_ind].iov_base = &((*it)[0]) + _offset;
        write_vector[write_vector_ind].iov_len = it->size() - _offset;
        it++;
        write_vector_ind++;

        for (; it != _queue.end(); it++) {
            write_vector[write_vector_ind].iov_base = &((*it)[0]);
            write_vector[write_vector_ind].iov_len = it->size();
            if (++write_vector_ind > max_queue) {
                break;
            }
        }

        int writed = 0;
        if ((writed = writev(_socket, write_vector, write_vector_ind)) >= 0) {
            size_t i = 0;

            while (i < write_vector_ind && writed >= write_vector[i].iov_len) {
                _queue.pop_front();
                writed -= write_vector[i].iov_len;
                i++;
            }
            _offset = writed;

        } else {
            throw std::runtime_error("Failed to send response");
        }

        if (_queue.empty()) {
            _event.events &= ~EPOLLOUT;
        }

        if (_queue.size() <= _max_queue_size){
            _event.events |= EPOLLIN;
        }

    } catch (std::runtime_error &ex) {
        if (errno != EAGAIN) {
            _logger->error("Failed to write connection on descriptor {}: {}", _socket, ex.what());
            _running = false;
        }
    }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina

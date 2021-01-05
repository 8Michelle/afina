#include "Connection.h"

#include <iostream>

namespace Afina {
namespace Network {
namespace MTnonblock {

// See Connection.h
void Connection::Start() {
    std::lock_guard<std::mutex> lock(_mutex);
    _running = true;
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    _logger->debug("Connection started on {} socket", _socket);
}

// See Connection.h
void Connection::OnError() {
    std::lock_guard<std::mutex> lock(_mutex);
    _running = false;
    _logger->error("Connection error on socket {}", _socket);
}

// See Connection.h
void Connection::OnClose() {
    std::lock_guard<std::mutex> lock(_mutex);
    _running = false;
    _logger->debug("Connection closed on socket {}", _socket);
}

// See Connection.h
void Connection::DoRead() {
    std::lock_guard<std::mutex> lock(_mutex);

    try {
        int readed_bytes = -1;
        while ((readed_bytes = read(_socket, _buffer + _pos, sizeof(_buffer) - _pos)) > 0) {
            _logger->debug("Got {} bytes from socket", readed_bytes);
            _pos += readed_bytes;

            while (_pos > 0) {
                _logger->debug("Process {} bytes", _pos);
                if (!command_to_execute) {
                    std::size_t parsed = 0;
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
                    std::size_t to_read = std::min(arg_remains, std::size_t(_pos));
                    argument_for_command.append(_buffer, to_read);

                    std::memmove(_buffer, _buffer + to_read, _pos - to_read);
                    arg_remains -= to_read;
                    _pos -= to_read;
                }

                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    command_to_execute->Execute(*_pStorage, argument_for_command, result);

                    if (_queue.size() > _max_queue){
                        _event.events &= ~EPOLLIN;
                    }

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

// See Connection.h
void Connection::DoWrite() {
    std::lock_guard<std::mutex> lock(_mutex);
    _logger->debug("Writing on socket {}", _socket);
    iovec write_vector[_max_queue];
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
            if (++write_vector_ind > _max_queue) {
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

        if (_queue.size() <= _max_queue) {
            _event.events |= EPOLLIN;
        }

    } catch (std::runtime_error &ex) {
        if (errno != EAGAIN) {
            _logger->error("Failed to write connection on descriptor {}: {}", _socket, ex.what());
            _running = false;
        }
    }
}

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

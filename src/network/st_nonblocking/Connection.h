#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H


#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <memory>
#include "protocol/Parser.h"
#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <deque>
#include <spdlog/logger.h>
#include <sys/epoll.h>
#include <sys/uio.h>
#include <iostream>


namespace Afina {
namespace Network {
namespace STnonblock {


class Connection {
public:
    Connection(int s, std::shared_ptr<spdlog::logger> &logger_,
               std::shared_ptr<Afina::Storage> &pStorage_) :
               _socket(s),
               _logger{logger_},
               pStorage{pStorage_},
               _max_queue_size(64),
               arg_remains(0) {

        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
        _pos = 0;
    }

    inline bool isAlive() const { return _running; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;
    bool _running;
    Protocol::Parser parser;
    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> pStorage;
    size_t arg_remains;
    std::unique_ptr<Execute::Command> command_to_execute;
    char _buffer[4096];
    std::deque<std::string> _queue;
    std::string argument_for_command;
    int _pos;
    size_t _offset;
    size_t _max_queue_size;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
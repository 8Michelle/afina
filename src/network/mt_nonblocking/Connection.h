#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <cstring>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <memory>
#include "protocol/Parser.h"
#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <deque>
#include <spdlog/logger.h>
#include <sys/uio.h>
#include <iostream>


namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<spdlog::logger> &logger,
               std::shared_ptr<Afina::Storage> &pStorage) :
        _socket(s),
        _logger(logger),
        _pStorage(pStorage),
        _pos(0),
        _offset(0),
        _max_queue(64),
        arg_remains(0) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }

    inline bool isAlive() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _running;
    }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;
    friend class Worker;

    int _socket;
    struct epoll_event _event;

    bool _running;

    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> _pStorage;
    std::unique_ptr<Execute::Command> command_to_execute;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::size_t arg_remains;

    char _buffer[4096];
    std::deque<std::string> _queue;
    size_t _pos;
    size_t _offset;
    size_t _max_queue;

    std::mutex _mutex;
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

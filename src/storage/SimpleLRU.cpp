#include "SimpleLRU.h"

namespace Afina {
    namespace Backend {

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Put(const std::string &key,
                            const std::string &value) {
            size_t node_size = key.size() + value.size();

            if (node_size <= _max_size) {
                auto block = _lru_index.find(key);
                if (block != _lru_index.end()) {
                    _set_node(block->second.get(), value);
                } else {
                    _add_node(key, value);
                }
                return true;
            }
            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::PutIfAbsent(const std::string &key,
                                    const std::string &value) {
            size_t node_size = key.size() + value.size();

            if (node_size <= _max_size && _lru_index.find(key) == _lru_index.end()) {
                _add_node(key, value);
                return true;
            }
            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Set(const std::string &key,
                            const std::string &value) {
            size_t node_size = key.size() + value.size();

            if (node_size <= _max_size) {
                auto block = _lru_index.find(key);
                if (block != _lru_index.end()) {
                    _set_node(block->second.get(), value);
                    return true;
                }
            }
            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Delete(const std::string &key) {
            auto block =_lru_index.find(key);

            if (block != _lru_index.end()) {
                lru_node& node = block->second.get();
                _cur_size -= key.size() + node.value.size();
                _lru_index.erase(key);

                _move_up(node);
                _lru_tail = _lru_tail->prev;
                _lru_tail->next = nullptr;

                return true;
            }
            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Get(const std::string &key,
                            std::string &value) {
            auto block = _lru_index.find(key);

            if (block != _lru_index.end()) {
                _move_up(block->second.get());
                value = _lru_tail->value;

                return true;
            }
            return false;
        }

        void SimpleLRU::_set_node(lru_node& node,
                                  const std::string &value) {
            size_t size_diff = value.size() - node.value.size();

            _move_up(node);
            if (size_diff > _max_size - _cur_size) {
                _free_mem(size_diff);
            }
            node.value = value;
            _cur_size += size_diff;
        }

        void SimpleLRU::_add_node(const std::string &key,
                                  const std::string &value) {
            size_t node_size = key.size() + value.size();

            if (node_size > _max_size - _cur_size) {
                _free_mem(node_size);
            }
            auto node = new lru_node({key, value});
            _lru_index.insert(std::make_pair(std::ref(node->key), std::ref(*node)));
            if (_lru_head) {
                _lru_tail->next = std::unique_ptr<lru_node>(node);
                node->prev = _lru_tail;
                _lru_tail = node;
            } else {
                _lru_head = std::unique_ptr<lru_node>(node);
                _lru_tail = node;
            }
            _cur_size += node_size;
        }

        void SimpleLRU::_move_up(lru_node& node) {
            if (_lru_tail->key != node.key) {
                if (_lru_head->key != node.key) {

                    _lru_tail->next = std::move(node.prev->next);
                    node.next->prev = node.prev;
                    node.prev->next = std::move(node.next);
                    node.prev = _lru_tail;
                    _lru_tail = &node;

                } else {

                    _lru_tail->next = std::move(_lru_head);
                    _lru_head = std::move(node.next);
                    node.prev = _lru_tail;
                    _lru_tail = &node;
                    _lru_head->prev = nullptr;
                }
            }
        }

        void SimpleLRU::_free_mem(size_t size) {
            while (size > _max_size - _cur_size) {
                _cur_size -= _lru_head->key.size() + _lru_head->value.size();
                _lru_index.erase(_lru_head->key);

                if (_lru_head->key == _lru_tail->key) {
                    _lru_head = nullptr;
                    _lru_tail = nullptr;
                } else {
                    _lru_head = std::move(_lru_head->next);
                    _lru_head->prev = nullptr;
                }
            }
        }

    } // namespace Backend
} // namespace Afina

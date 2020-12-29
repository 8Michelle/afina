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

        std::swap(node.prev, node.next->prev);
        std::swap(node.next, node.next->prev->next);

        node.next.reset();

        return true;
    }
    return false;
}


// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key,
                    std::string &value) {
    auto block = _lru_index.find(key);

    if (block != _lru_index.end()) {
        lru_node& node = block->second.get();
        value = node.value;
        std::swap(node.prev, node.next->prev);
        std::swap(node.next, node.next->prev->next);
        std::swap(node.prev, _lru_head->next->prev);
        std::swap(node.next, _lru_head->next);
        return true;
    }
    return false;
}


void SimpleLRU::_set_node(lru_node& node,
                          const std::string &value) {
    size_t size_diff = value.size();

    std::swap(node.prev, node.next->prev);
    std::swap(node.next, node.next->prev->next);
    std::swap(node.prev, _lru_head->next->prev);
    std::swap(node.next, _lru_head->next);
    _cur_size -= node.value.size();

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
    _lru_index.emplace(std::reference_wrapper<const std::string>(node->key), std::reference_wrapper<lru_node>(*node));

    node->prev = node;
    node->next = std::unique_ptr<lru_node>(node);
    std::swap(node->prev, _lru_head->next->prev);
    std::swap(node->next, _lru_head->next);
    _cur_size += node_size;
}


void SimpleLRU::_free_mem(size_t size) {
    while (size > _max_size - _cur_size) {
        auto node = _lru_head->prev;
        _cur_size -= node->value.size() + node->value.size();
        _lru_index.erase(node->key);
        std::swap(node->prev, node->next->prev);
        std::swap(node->next, node->next->prev->next);
        node->next.reset();
    }
}

} // namespace Backend
} // namespace Afina

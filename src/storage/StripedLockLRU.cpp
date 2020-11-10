#include "StripedLockLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool StripedLockLRU::Put(const std::string &key, const std::string &value) {
    return shards_[get_shard_num(key)]->Put(key, value);
}

// See MapBasedGlobalLockImpl.h
bool StripedLockLRU::PutIfAbsent(const std::string &key,
                            const std::string &value) {
    return shards_[get_shard_num(key)]->PutIfAbsent(key, value);
}

// See MapBasedGlobalLockImpl.h
bool StripedLockLRU::Set(const std::string &key, const std::string &value) {
    return shards_[get_shard_num(key)]->Set(key, value);
}

// See MapBasedGlobalLockImpl.h
bool StripedLockLRU::Delete(const std::string &key) {
    return shards_[get_shard_num(key)]->Delete(key);
}

// See MapBasedGlobalLockImpl.h
bool StripedLockLRU::Get(const std::string &key, std::string &value) {
    return shards_[get_shard_num(key)]->Get(key, value);
}

size_t StripedLockLRU::get_shard_num(const std::string &key) {
    return std::hash<std::string>{}(key) % shards_.size(); // проверить
}


} // namespace Backend
} // namespace Afina

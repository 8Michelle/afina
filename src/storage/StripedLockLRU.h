#ifndef AFINA_STORAGE_STRIPED_LOCK_LRU_H
#define AFINA_STORAGE_STRIPED_LOCK_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <afina/Storage.h>
#include "ThreadSafeSimpleLRU.h"

namespace Afina {
namespace Backend {

/**
* # Map based implementation
* That IS thread safe and striped implementation!!
*/
class StripedLockLRU : public Afina::Storage {
public:
    StripedLockLRU(size_t max_size = 1024, size_t n_shards = 4) : _max_size(max_size) {
        if (_max_size / n_shards < 8) {
            throw std::runtime_error("Shards are too small.");
        } else if (_max_size / n_shards > 1024 * 1024) {
            throw std::runtime_error("Shards are too large.");
        }

        for (size_t i = 0; i < n_shards; ++i) {
            shards_.emplace_back(new ThreadSafeSimplLRU(_max_size / n_shards));
        }
    }

    ~StripedLockLRU() {}

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) override;

private:
    // Shards vector
    std::vector<std::unique_ptr<ThreadSafeSimplLRU>> shards_;

    // Maximum number of bytes could be stored in this cache.
    // i.e all (keys+values) must be less the _max_size
    std::size_t _max_size;

    // Function gets the shard number by the node key
    size_t get_shard_num(const std::string& key);
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_STRIPED_LOCK_LRU_H

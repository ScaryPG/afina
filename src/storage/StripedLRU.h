#ifndef AFINA_STORAGE_STRIPED_LRU_H
#define AFINA_STORAGE_STRIPED_LRU_H

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

class StripedLRU : public SimpleLRU {
private:
    StripedLRU(size_t memory_limit, size_t stripe_count) : _stripe_count(stripe_count) {
        for (size_t i = 0; i < stripe_count; ++i) {
            _shards.emplace_back(memory_limit);
        }
    }

public:
    static std::unique_ptr<StripedLRU> BuildStripedLRU(size_t memory_limit, size_t stripe_count);

    ~StripedLRU() {}

    // see SimpleLRU.h
    bool Put(const std::string &key, const std::string &value) override {
        return _shards[std::hash<std::string>{}(key) % _stripe_count].Put(key, value);
    }

    // see SimpleLRU.h
    bool PutIfAbsent(const std::string &key, const std::string &value) override {
        return _shards[std::hash<std::string>{}(key) % _stripe_count].PutIfAbsent(key, value);
    }

    // see SimpleLRU.h
    bool Set(const std::string &key, const std::string &value) override {
        return _shards[std::hash<std::string>{}(key) % _stripe_count].Set(key, value);
    }

    // see SimpleLRU.h
    bool Delete(const std::string &key) override {
        return _shards[std::hash<std::string>{}(key) % _stripe_count].Delete(key);
    }

    // see SimpleLRU.h
    bool Get(const std::string &key, std::string &value) override {
        return _shards[std::hash<std::string>{}(key) % _stripe_count].Get(key, value);
    }

private:
    size_t _stripe_count = 8;
    std::vector<SimpleLRU> _shards;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_STRIPED_LRU_H

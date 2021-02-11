#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    size_t size = key.size() + value.size();
    if (size > _max_size) {
        return false;
    }
    auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (it != _lru_index.end()) {
        return UpdateNode(it->second, value);
    }
    return AddNode(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    size_t size = key.size() + value.size();
    if (size > _max_size) {
        return false;
    }
    auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (it != _lru_index.end()) {
        return false;
    }
    return AddNode(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (it == _lru_index.end()) {
        return false;
    }
    while (_cur_size + value.size() - it->second.get().value.size() > _max_size) {
        if (!DeleteNode()) {
            return false;
        }
    }
    _cur_size += value.size() - it->second.get().value.size();
    it->second.get().value = value;
    MoveNode(it->second.get());
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (it == _lru_index.end()) {
        return false;
    }
    MoveNode(it->second.get());
    if (_lru_head.get() == _lru_tail) {
        _lru_head.reset();
    } else {
        _lru_head = std::move(_lru_head->next);
        _lru_head->prev = nullptr;
    }
    _lru_index.erase(std::reference_wrapper<const std::string>(key));
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto it = _lru_index.find(std::reference_wrapper<const std::string>(key));
    if (it == _lru_index.end()) {
        return false;
    }
    value = it->second.get().value;
    MoveNode(it->second.get());
    return true;
}

bool SimpleLRU::AddNode(const std::string& key, const std::string& value) {
    size_t size = key.size() + value.size();
    while (_cur_size + size > _max_size) {
        if (!DeleteNode()) {
            return false;
        }
    }
    _cur_size += size;
    auto node = new lru_node({ key, value, nullptr, nullptr });
    if (_lru_head == nullptr) {
        _lru_head = std::unique_ptr<lru_node>(node);
        _lru_tail = node;
    } else {
        _lru_head->prev = node;
        node->next = std::move(_lru_head);
        _lru_head = std::unique_ptr<lru_node>(node);
    }
    _lru_index.emplace(std::reference_wrapper<const std::string>(_lru_head->key),
                       std::reference_wrapper<lru_node>(*_lru_head));
    return true;
}

bool SimpleLRU::DeleteNode() {
    if (_lru_tail != nullptr) {
        size_t size = _lru_tail->key.size() + _lru_tail->value.size();
        _cur_size -= size;
        _lru_index.erase(std::reference_wrapper<const std::string>(_lru_tail->key));
        if (_lru_tail != _lru_head.get()) {
            _lru_tail = _lru_tail->prev;
            _lru_tail->next.reset();
        } else {
            _lru_head.reset();
        }
        return true;
    }
    return false;
}

void SimpleLRU::MoveNode(SimpleLRU::lru_node& node) {
    if (_lru_head.get() == &node) {
        return;
    }
    if (&node == _lru_tail) {
        _lru_tail = _lru_tail->prev;
    }
    auto ptr = std::move(node.prev->next);
    if (node.next != nullptr) {
        node.next->prev = node.prev;
    }
    node.prev->next = std::move(node.next);
    node.next = std::move(_lru_head);
    node.next->prev = &node;
    _lru_head = std::move(ptr);
    _lru_head->prev = nullptr;
}

bool SimpleLRU::UpdateNode(SimpleLRU::lru_node& node, const std::string& value) {
    MoveNode(node);
    while (_cur_size + value.size() - node.value.size() > _max_size) {
        if (!DeleteNode()) {
            return false;
        }
    }
    _cur_size += value.size() - node.value.size();
    node.value = value;
    return true;
}

} // namespace Backend
} // namespace Afina

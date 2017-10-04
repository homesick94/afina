#include "MapBasedGlobalLockImpl.h"

#include <mutex>

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
    std::unique_lock<std::mutex> guard(_lock);

    if (_backend.size () + 1 == _max_size)
      {
        auto last_element = lru_cache.back ();
        _backend.erase (last_element);
        lru_cache.pop_back ();
      }

    auto found_it = _backend.find (key);
    if (found_it == _backend.end ())
      {
        _backend.insert ({key, value});

        // must always be
        auto iterator = _backend.find (key);
        if (iterator == _backend.end ())
          return false;

        lru_cache.push_front (iterator);
      }
    else
      _backend[key] = value;

    return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
    std::unique_lock<std::mutex> guard(_lock);

    auto found_it = _backend.find (key);
    if (found_it == _backend.end ())
      {
        guard.unlock ();
        Put (key, value);
      }

    return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
    std::unique_lock<std::mutex> guard(_lock);

    auto found_it = _backend.find (key);
    if (found_it == _backend.end ())
      return false;

    _backend[key] = value;
    return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) {
    std::unique_lock<std::mutex> guard(_lock);

    auto found_it = _backend.find (key);
    if (found_it == _backend.end ())
      return false;

    lru_cache.remove (found_it);
    _backend.erase (found_it);

    return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {
    std::unique_lock<std::mutex> guard(*const_cast<std::mutex *>(&_lock));

    auto found_it = _backend.find (key);
    if (found_it == _backend.end ())
      return false;

    value = found_it->second;
    return true;
}

} // namespace Backend
} // namespace Afina

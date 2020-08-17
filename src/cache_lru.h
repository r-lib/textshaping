#pragma once

#include <unordered_map>
#include <list>
#include <functional>

template<typename key_t, typename value_t>
class LRU_Cache {

public:
  LRU_Cache() :
  _max_size(32) {

  }
  LRU_Cache(size_t max_size) :
  _max_size(max_size) {

  }

  ~LRU_Cache() {
    clear();
  }

  // Add a key-value pair, potentially passing a removed key and value back
  // through the removed_key and removed_value argument. Returns true if a value
  // was removed and false otherwise
  inline bool add(key_t& key, value_t value) {
    cache_map_it_t it = _cache_map.find(key);
    _cache_list.push_front(key_value_t(key, value));
    if (it != _cache_map.end()) {
      _cache_list.erase(it->second);
      _cache_map.erase(it);
    }
    _cache_map[key] = _cache_list.begin();

    if (_cache_map.size() > _max_size) {
      cache_list_it_t last = _cache_list.end();
      last--;
      _cache_map.erase(last->first);
      _cache_list.pop_back();
      return true;
    }
    return false;
  }

  // Retrieve a value based on a key, returning true if a value was found. Will
  // move the key-value pair to the top of the list
  inline bool get(key_t& key, value_t& value) {
    cache_map_it_t it = _cache_map.find(key);
    if (it == _cache_map.end()) {
      return false;
    }

    value = it->second->second;
    _cache_list.splice(_cache_list.begin(), _cache_list, it->second);

    return true;
  }

  // Check for the existence of a key-value pair
  inline bool exist(key_t& key) {
    return _cache_map.find(key) != _cache_map.end();
  }

  // Remove a key-value pair
  inline void remove(key_t& key) {
    cache_map_it_t it = _cache_map.find(key);
    if (it == _cache_map.end()) {
      return;
    }
    _cache_list.erase(it->second);
    _cache_map.erase(it);
  }

  // Clear the cache
  inline void clear() {
    _cache_list.clear();
    _cache_map.clear();
  }

private:
  size_t _max_size;

protected:
  typedef typename std::pair<key_t, value_t> key_value_t;
  typedef typename std::list<key_value_t> list_t;
  typedef typename list_t::iterator cache_list_it_t;
  typedef typename std::unordered_map<key_t, cache_list_it_t> map_t;
  typedef typename std::unordered_map<key_t, cache_list_it_t>::iterator cache_map_it_t;

  list_t _cache_list;
  map_t _cache_map;
};

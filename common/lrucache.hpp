/*
 * File:   lrucache.hpp
 * Author: Alexander Ponomarev
 *
 * Created on June 20, 2013, 5:09 PM
 */
/*
 * modifed to:
 *	- avoid exceptions for embeddd use
 *	- add a sentinel item to be returned on cache miss
 *  - add an optional evict callback
 *  - add items() iterator
 *  - add remove() method
 * haberlerm@gmail.com 2/2024
 */
#ifndef _LRUCACHE_HPP_INCLUDED_
#define	_LRUCACHE_HPP_INCLUDED_

#include <unordered_map>
#include <list>
#include <cstddef>

namespace cache {

template<typename key_t, typename value_t>
class lru_cache {

  public:
    typedef typename std::pair<key_t, value_t> key_value_pair_t;
    typedef typename std::list<key_value_pair_t>::iterator list_iterator_t;
    typedef  value_t value_type ;
    typedef void (*evict_cb_t)(key_t, value_t );

    lru_cache(size_t max_size, value_t sentinel) :
        _max_size(max_size), _sentinel(sentinel), _evict(NULL) {
    }

    lru_cache(size_t max_size, value_t sentinel, evict_cb_t ev) :
        _max_size(max_size), _sentinel(sentinel), _evict(ev) {
    }

    void put(const key_t& key, const value_t& value) {
        auto it = _cache_items_map.find(key);
        _cache_items_list.push_front(key_value_pair_t(key, value));
        if (it != _cache_items_map.end()) {
            _cache_items_list.erase(it->second);
            _cache_items_map.erase(it);
        }
        _cache_items_map[key] = _cache_items_list.begin();

        if (_cache_items_map.size() > _max_size) {
            auto last = _cache_items_list.end();
            last--;
            if (_evict != NULL) _evict(last->first, last->second);
            _cache_items_map.erase(last->first);
            _cache_items_list.pop_back();
        }
    }

    const value_t& get(const key_t& key) {
        auto it = _cache_items_map.find(key);
        if (it == _cache_items_map.end()) {
            // log_e("There is no such key in cache");
            return _sentinel;
        } else {
            _cache_items_list.splice(_cache_items_list.begin(), _cache_items_list, it->second);
            return it->second->second;
        }
    }

    void remove(const key_t& key) {
        auto it = _cache_items_map.find(key);
        if (it != _cache_items_map.end()) {
            if (_evict != NULL) _evict(it->first, it->second);
            _cache_items_list.erase(it->second);
            _cache_items_map.erase(it);
        }
    }

    bool exists(const key_t& key) const {
        return _cache_items_map.find(key) != _cache_items_map.end();
    }

    size_t size() const {
        return _cache_items_map.size();
    }

    const std::list<key_value_pair_t>& items() const {
        return _cache_items_list;
    }

  private:
    std::list<key_value_pair_t> _cache_items_list;
    std::unordered_map<key_t, list_iterator_t> _cache_items_map;
    size_t _max_size;
    value_type _sentinel;
    evict_cb_t _evict;
};

} // namespace cache

#endif	/* _LRUCACHE_HPP_INCLUDED_ */

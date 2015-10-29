#ifndef OBJECT_CACHE_TPP
#define OBJECT_CACHE_TPP

#include <iostream>
#include <cassert>

/*
 * Public methods
 */
template<typename K, typename V, template<typename ...> class MAP>
ObjectCache<K,V,MAP>::~ObjectCache() {
    if(print_stats_) {
        std::cout << "Cache Statistics at destruction:" << std::endl;
        std::cout << "  Hits         : " << num_hits_ << std::endl;
        std::cout << "  Misses       : " << num_misses_ << std::endl;
        std::cout << "  Hits + Misses: " << num_misses_ + num_hits_ << std::endl;
        std::cout << "  Hit Rate     : " << ((float) num_hits_) / (num_misses_ + num_hits_) << std::endl;
        std::cout << "  Evictions    : " << num_evictions_ << std::endl;
        std::cout << "  Eviction Rate: " << ((float) num_evictions_) / (num_misses_ + num_hits_) << std::endl;
        std::cout << "  Target Capacity (# items): " << capacity_ << std::endl;
        std::cout << "  Actual Size (# items)    : " << object_lookup_.size() << std::endl;
    }
}

//Add an value to the cache for the specified key, evicting the LRU if neccessary
template<typename K, typename V, template<typename ...> class MAP>
typename ObjectCache<K,V,MAP>::value_t& ObjectCache<K,V,MAP>::insert(const key_t& key, const value_t& new_value) {
    if(!allow_key_overwrites_ && object_lookup_.count(key)) {
        throw std::runtime_error("Attempted to overwrite key in cache!");
    }


    if(capacity_ > 0 && object_lookup_.size() == capacity_) {
        //Delete an item from the cache 
        evict();
    }

    //Verify that there is space
    assert(capacity_ == 0 || object_lookup_.size() < capacity_);

    if(allow_key_overwrites_) {
        //We need to move the key to the front if it is already in the access order list
        auto iter = object_lookup_.find(key);
        if(iter != object_lookup_.end()) {
            //Object already exists
            //Set is as MRU
            set_most_recently_used(iter->second.second); //The iterator in the access list

            assert(*(key_access_order_.begin()) == key);

            //Add the updated element
            object_lookup_[key] = {new_value, key_access_order_.begin()};
        } else {
            //First time seen, normal insert
            key_access_order_.push_front(key);

            //Add to the cache and return a refernce to it from the cache
            object_lookup_[key] = {new_value, key_access_order_.begin()};
        }

    } else {
        //Keys are uniquely added, so we can just push it onto the beginning
        //of the list as the most recently accessed item
        key_access_order_.push_front(key);

        //Add to the cache and return a refernce to it from the cache
        object_lookup_[key] = {new_value, key_access_order_.begin()};
    }

    //The map and key list should always bee the same size
    // Note that the std::list used for key_access_order_ should keep
    // track of the list size, so the size() query is O(1) when run in C++11
    // mode, BUT gcc is non-conformant in this respect, making size() O(N)
    // so only use this for debugging
    //assert(key_access_order_.size() == object_lookup_.size());

    return value(key);
}


//Check whether the object is caching the given key
template<typename K, typename V, template<typename ...> class MAP>
bool ObjectCache<K,V,MAP>::contains(const key_t& key) {
    bool val =  object_lookup_.count(key);

    //Record stats
    if(val) {
        num_hits_++;
    } else {
        num_misses_++;
    }

    return val;
}

//Returns the value associated with the given key
// Raises an exception if the key is not found
template<typename K, typename V, template<typename ...> class MAP>
typename ObjectCache<K,V,MAP>::value_t& ObjectCache<K,V,MAP>::value(const key_t& key) {
    auto iter = object_lookup_.find(key);
    if(iter == object_lookup_.end()) {
        throw std::runtime_error("Could not find key in object cache!");
    }
    //Second is the 'value' of the map
    //First is the actual stored value, not the LRU list iterator
    return iter->second.first;
}


/*
 * Private methods
 */


//Make the key associated with the iterator the Most Recently Used.
// (i.e. move it to the head of the list)
template<typename K, typename V, template<typename ...> class MAP>
void ObjectCache<K,V,MAP>::set_most_recently_used(typename key_order_t::iterator it) {
    key_access_order_.push_front(*it); //Re-insert at the front
    key_access_order_.erase(it); //Remove from old position
}


//Evicts an element based on an LRU policy
template<typename K, typename V, template<typename ...> class MAP>
void ObjectCache<K,V,MAP>::evict() {
    //Use an LRU policy. The tail of key_access_order_ is the
    //least recently used
    assert(object_lookup_.size() > 0);

    //Remove the key from the cache
    object_lookup_.erase(key_access_order_.back());

    //Remove the key from the access order list - it will be the last entry
    key_access_order_.erase(--(key_access_order_.end()));

    num_evictions_++;
}

//Adjusts the cache size to match capacity
template<typename K, typename V, template<typename ...> class MAP>
void ObjectCache<K,V,MAP>::resize() {
    while(object_lookup_.size() > capacity_) {
        evict();
    }
}
#endif

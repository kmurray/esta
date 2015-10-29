#ifndef OBJECT_CACHE_HPP
#define OBJECT_CACHE_HPP

#include <list>
#include <map>
#include <tuple>


template <
    typename K, //The type of the key indexing the cache
    typename V, //The type of the value being stored in the cache
    template<typename ...> class MAP  //The map type
    >
class ObjectCache {
    public:
        //The key type used to index the cache
        typedef K key_t;

        //The value type of the thing we are caching
        typedef V value_t;

        //Stores the most recently accesed elements
        typedef std::list<key_t> key_order_t;

        //Could use something like a splay tree here, so commonly accessed
        //objects are fastest - or a Hash/unordered_map
        typedef MAP<
            key_t, 
            std::pair<
                value_t, //The actual value
                typename key_order_t::iterator //The iterator that marks this values position in the order of accesses
            > 
        > obj_cache_t;

        //Constructor
        //  'capacity' of zero means no size limit
        //  'allow_key_overwrites' determines whether an exception is 
        //    thrown if keys are overwritten
        //  'print_stats' prints hit/miss/capacity statistics on destruction
        ObjectCache(size_t capacity = 0, bool allow_key_overwrites = false, bool print_stats = true) 
            : capacity_(capacity)
            , allow_key_overwrites_(allow_key_overwrites)
            , print_stats_(print_stats)
            , num_hits_(0)
            , num_misses_(0)
            , num_evictions_(0) {}

        //Destructor
        ~ObjectCache();

        //Inserts the given key and value into the case, evicting an item
        //if it is already at capacity
        value_t& insert(const key_t& key, const value_t& value);

        //Query to see if a value is in the cache
        bool contains(const key_t& key);

        //Access a value in the cache.  Throws an exception if the key does not
        //exist in the cache
        value_t& value(const key_t& key);

        size_t capacity() { return capacity_; }
        void set_capacity(size_t val) { capacity_ = val; resize(); }

    private:

        //Moves the key specified by this iterator to the front of the key access list
        // i.e. to be Most Recently Used (MRU)
        void set_most_recently_used(typename key_order_t::iterator it);

        //Evicts and element from the cache
        void evict();

        void resize();

        //The actual cache mapping key_t objects to value_t
        obj_cache_t object_lookup_;

        //What order the keys have been accessed in
        // MRU at the head, LRU at the tail
        key_order_t key_access_order_;

        //How many items the cache can hold
        size_t capacity_;

        //Whether we error if a key is over-written
        bool allow_key_overwrites_;

        //Should we print statistics on destruction?
        bool print_stats_;

        //Cache statistics
        size_t num_hits_; //Number of times a value was found in the cache

        size_t num_misses_; //Number of time a value was NOT found in the cache

        size_t num_evictions_;

};

//For template definitions
#include "object_cache.tpp"

template<typename K, typename V>
using ObjectCacheMap = ObjectCache<K, V, std::map>;

template<typename K, typename V>
using ObjectCacheUnorderedMap = ObjectCache<K, V, std::unordered_map>;

#endif

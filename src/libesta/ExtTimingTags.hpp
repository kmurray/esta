#pragma once
#include <algorithm>
#include <memory>
#include "ExtTimingTag.hpp"

class ExtTimingTags {
    public:
        typedef ExtTimingTag Tag;
        typedef std::vector<Tag::ptr>::iterator iterator;
        typedef std::vector<Tag::ptr>::const_iterator const_iterator;

        /*
         * Getters
         */
        ///\returns The number of timing tags in this set
        size_t num_tags() const { return tags_.size(); };

        ///Finds a TimingTag in the current set that has clock domain id matching domain_id
        ///\param base_tag The tag to match meta-data against
        ///\returns An iterator to the tag if found, or end() if not found
        iterator find_matching_tag(Tag::cptr base_tag);
        const_iterator find_matching_tag(Tag::cptr base_Tag) const;

        ///\returns An iterator to the first tag in the current set
        iterator begin() { return tags_.begin(); }
        const_iterator begin() const { return tags_.begin(); }

        ///\returns An iterator 'one-past-the-end' of the current set
        iterator end() { return tags_.end(); }
        const_iterator end() const { return tags_.end(); }

        /*
         * Modifiers
         */
        ///Adds a TimingTag to the current set provided it has a valid clock domain
        ///\param src_tag The source tag who is inserted. Note that the src_tag is copied when inserted (the original is unchanged)
        iterator add_tag(Tag::ptr src_tag);

        /*
         * Setup operations
         */
        ///Updates the arrival time of this set of tags to be the maximum.
        ///\param new_time The new arrival time to compare against
        ///\param base_tag The associated metat-data for new_time
        ///\remark Finds (or creates) the tag with the same clock domain as base_tag and update the arrival time if new_time is larger
        void max_arr(Tag::cptr base_tag);
        void max_arr(iterator merge_tag_iter, Tag::cptr base_tag);

        ///Updates the required time of this set of tags to be the minimum.
        ///\param new_time The new arrival time to compare against
        ///\param base_tag The associated metat-data for new_time
        ///\remark Finds (or creates) the tag with the same clock domain as base_tag and update the required time if new_time is smaller
        //void min_req(const Time& new_time, const Tag& base_tag);

        ///Clears the tags in the current set
        void clear();

    protected:


    private:
        std::vector<Tag::ptr> tags_;
};

/*
 * ExtTimingTags implementation
 */

//Modifiers
inline ExtTimingTags::iterator ExtTimingTags::add_tag(Tag::ptr tag) {
    //Don't add invalid clock domains
    //Some sources like constant generators may yeild illegal clock domains
    //if(tag->clock_domain() == INVALID_CLOCK_DOMAIN) {
        //return end();;
    //}

    tags_.push_back(tag);

    return tags_.end() - 1;
}

inline void ExtTimingTags::max_arr(Tag::cptr tag) {
    auto iter = find_matching_tag(tag);

    if(iter == end()) {
        //First time we've seen this tag
        add_tag(Tag::make_ptr(*tag));
    } else {
        max_arr(iter, tag);    
    }
}

inline void ExtTimingTags::max_arr(iterator merge_tag_iter, Tag::cptr tag) {
    assert(merge_tag_iter != end());

    Tag::ptr matched_tag = *merge_tag_iter;
    
    matched_tag->max_arr(tag->arr_time(), tag);

    //'tag' has been merged, with 'merge_tag_iter', so we need to update 
    //'merge_tag_iter's switching scenarios (i.e. input tags that generate
    //the tag)
    for(auto& scenario : tag->input_tags()) {
        matched_tag->add_input_tags(scenario);
    }
}


inline void ExtTimingTags::clear() {
    //TODO: handle memory leaks...
    tags_.clear();
}

inline ExtTimingTags::iterator ExtTimingTags::find_matching_tag(Tag::cptr base_tag) {
    auto pred = [&](Tag::cptr tag) {
        return tag->matches(base_tag);
    };
    return std::find_if(begin(), end(), pred);
}

inline ExtTimingTags::const_iterator ExtTimingTags::find_matching_tag(Tag::cptr base_tag) const {
    auto pred = [&](Tag::cptr tag) {
        return tag->matches(base_tag);
    };
    return std::find_if(begin(), end(), pred);
}

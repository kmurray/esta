#pragma once
#include <algorithm>
#include "ExtTimingTag.hpp"

class ExtTimingTags {
    public:
        typedef ExtTimingTag Tag;
        typedef std::vector<Tag*>::iterator iterator;
        typedef std::vector<Tag*>::const_iterator const_iterator;

        /*
         * Getters
         */
        ///\returns The number of timing tags in this set
        size_t num_tags() const { return tags_.size(); };

        ///Finds a TimingTag in the current set that has clock domain id matching domain_id
        ///\param base_tag The tag to match meta-data against
        ///\returns An iterator to the tag if found, or end() if not found
        iterator find_matching_tag(const Tag* base_tag, double delay_bin_size);
        const_iterator find_matching_tag(const Tag* base_Tag, double delay_bin_size) const;

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
        iterator add_tag(Tag* src_tag);

        /*
         * Setup operations
         */
        ///Updates the arrival time of this set of tags to be the maximum.
        ///\param new_time The new arrival time to compare against
        ///\param base_tag The associated metat-data for new_time
        ///\remark Finds (or creates) the tag with the same clock domain as base_tag and update the arrival time if new_time is larger
        void max_arr(const Tag* base_tag, double delay_bin_size);

        ///Updates the required time of this set of tags to be the minimum.
        ///\param new_time The new arrival time to compare against
        ///\param base_tag The associated metat-data for new_time
        ///\remark Finds (or creates) the tag with the same clock domain as base_tag and update the required time if new_time is smaller
        //void min_req(const Time& new_time, const Tag& base_tag);

        ///Clears the tags in the current set
        void clear();

    protected:


    private:
        std::vector<Tag*> tags_;
};

/*
 * ExtTimingTags implementation
 */

//Modifiers
inline ExtTimingTags::iterator ExtTimingTags::add_tag(Tag* tag) {
    //Don't add invalid clock domains
    //Some sources like constant generators may yeild illegal clock domains
    //if(tag->clock_domain() == INVALID_CLOCK_DOMAIN) {
        //return end();;
    //}

    tags_.push_back(tag);

    return tags_.end() - 1;
}

inline void ExtTimingTags::max_arr(const Tag* tag, double delay_bin_size) {
    auto iter = find_matching_tag(tag, delay_bin_size);

    if(iter == end()) {
        //First time we've seen this tag
        add_tag(new Tag(*tag));
    } else {
        assert(iter != end());

        Tag* matched_tag = *iter;
        
        matched_tag->max_arr(tag->arr_time(), tag);


        //'tag' has been merged, with 'iter', so we need to update 
        //'iter's switching scenarios (i.e. input tags that generate
        //the iter tag
        for(auto scenario : tag->input_tags()) {
            matched_tag->add_input_tags(scenario);
        }
    }
}


inline void ExtTimingTags::clear() {
    //TODO: handle memory leaks...
    tags_.clear();
}

inline ExtTimingTags::iterator ExtTimingTags::find_matching_tag(const Tag* base_tag, double delay_bin_size) {
    auto pred = [&](const Tag* tag) {
        return tag->matches(base_tag, delay_bin_size);
    };
    return std::find_if(begin(), end(), pred);
}

inline ExtTimingTags::const_iterator ExtTimingTags::find_matching_tag(const Tag* base_tag, double delay_bin_size) const {
    auto pred = [&](const Tag* tag) {
        return tag->matches(base_tag, delay_bin_size);
    };
    return std::find_if(begin(), end(), pred);
}

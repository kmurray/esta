#include <algorithm>
#include "assert.hpp"
/*
 * TimingTags implementation
 */

//Modifiers
inline void TimingTags::add_tag(const TimingTag& tag) {
    ASSERT_MSG(tag.next() == nullptr, "Attempted to add new timing tag which is already part of a Linked List");

    //Don't add invalid clock domains
    //Some sources like constant generators may yeild illegal clock domains
    if(tag.clock_domain() == INVALID_CLOCK_DOMAIN) {
        return;
    }

    if(num_tags_ < (int) head_tags_.max_size()) {
        //Store it as a head tag
        head_tags_[num_tags_] = tag;
        if(num_tags_ != 0) {
            //Link from previous if it exists
            head_tags_[num_tags_-1].set_next(&head_tags_[num_tags_]);
        }
    } else {
        //Store it in a linked list from head tags

        TimingTag* new_tag = new TimingTag(tag);

        //Insert one-after the last head in O(1) time
        //Note that we don't maintain the tags in any order since we expect a relatively small number of tags
        //per node
        TimingTag* next_tag = head_tags_[head_tags_.max_size()-1].next(); //Save next link (may be nullptr)
        head_tags_[head_tags_.max_size()-1].set_next(new_tag); //Tag is now in the list
        new_tag->set_next(next_tag); //Attach tail of the list
    }

    //Tag has been added
    num_tags_++;
}

inline void TimingTags::max_arr(const Time& new_time, const TimingTag& base_tag) {
    TimingTagIterator iter = find_tag_by_clock_domain(base_tag.clock_domain());
    if(iter == end()) {
        //First time we've seen this domain
        TimingTag tag = TimingTag(new_time, Time(NAN), base_tag);
        add_tag(tag);
    } else {
        iter->max_arr(new_time, base_tag);
    }
}

inline void TimingTags::min_req(const Time& new_time, const TimingTag& base_tag) {
    TimingTagIterator iter = find_tag_by_clock_domain(base_tag.clock_domain());
    if(iter == end()) {
        //First time we've seen this domain
        TimingTag tag = TimingTag(Time(NAN), new_time, base_tag);
        add_tag(tag);
    } else {
        iter->min_req(new_time, base_tag);
    }
}

inline void TimingTags::min_arr(const Time& new_time, const TimingTag& base_tag) {
    TimingTagIterator iter = find_tag_by_clock_domain(base_tag.clock_domain());
    if(iter == end()) {
        //First time we've seen this domain
        TimingTag tag = TimingTag(new_time, Time(NAN), base_tag);
        add_tag(tag);
    } else {
        iter->min_arr(new_time, base_tag);
    }
}

inline void TimingTags::max_req(const Time& new_time, const TimingTag& base_tag) {
    TimingTagIterator iter = find_tag_by_clock_domain(base_tag.clock_domain());
    if(iter == end()) {
        //First time we've seen this domain
        TimingTag tag = TimingTag(new_time, Time(NAN), base_tag);
        add_tag(tag);
    } else {
        iter->max_req(new_time, base_tag);
    }
}

inline void TimingTags::clear() {
    //TODO: handle memory leaks...
    num_tags_ = 0;
}

inline TimingTagIterator TimingTags::find_tag_by_clock_domain(DomainId domain_id) {
    auto pred = [domain_id](const TimingTag& tag) {
        return tag.clock_domain() == domain_id;
    };
    return std::find_if(begin(), end(), pred);
}

inline TimingTagConstIterator TimingTags::find_tag_by_clock_domain(DomainId domain_id) const {
    auto pred = [domain_id](const TimingTag& tag) {
        return tag.clock_domain() == domain_id;
    };
    return std::find_if(begin(), end(), pred);
}

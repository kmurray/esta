#pragma once
#include "ExtTimingTag.hpp"

class ExtTimingTags {
    public:
        typedef ExtTimingTag Tag;
        typedef TimingTagIter<Tag> iterator;
        typedef TimingTagIter<Tag const> const_iterator;
        /*
         * Getters
         */
        ///\returns The number of timing tags in this set
        size_t num_tags() const { return num_tags_; };

        ///Finds a TimingTag in the current set that has clock domain id matching domain_id
        ///\param base_tag The tag to match meta-data against
        ///\returns An iterator to the tag if found, or end() if not found
        iterator find_matching_tag(const Tag& base_tag);
        const_iterator find_matching_tag(const Tag& base_Tag) const;

        ///\returns An iterator to the first tag in the current set
        iterator begin() { return (num_tags_ > 0) ? iterator(&head_tags_[0]) : end(); };
        const_iterator begin() const { return (num_tags_ > 0) ? const_iterator(&head_tags_[0]) : end(); };

        ///\returns An iterator 'one-past-the-end' of the current set
        iterator end() { return iterator(nullptr); };
        const_iterator end() const { return const_iterator(nullptr); };

        /*
         * Modifiers
         */
        ///Adds a TimingTag to the current set provided it has a valid clock domain
        ///\param src_tag The source tag who is inserted. Note that the src_tag is copied when inserted (the original is unchanged)
        iterator add_tag(const Tag& src_tag);

        /*
         * Setup operations
         */
        ///Updates the arrival time of this set of tags to be the maximum.
        ///\param new_time The new arrival time to compare against
        ///\param base_tag The associated metat-data for new_time
        ///\remark Finds (or creates) the tag with the same clock domain as base_tag and update the arrival time if new_time is larger
        void max_arr(const Tag& base_tag);

        ///Updates the required time of this set of tags to be the minimum.
        ///\param new_time The new arrival time to compare against
        ///\param base_tag The associated metat-data for new_time
        ///\remark Finds (or creates) the tag with the same clock domain as base_tag and update the required time if new_time is smaller
        //void min_req(const Time& new_time, const Tag& base_tag);

        /*
         * Hold operations
         */
        ///Updates the arrival time of this set of tags to be the minimum.
        ///\param new_time The new arrival time to compare against
        ///\param base_tag The associated metat-data for new_time
        ///\remark Finds (or creates) the tag with the same clock domain as base_tag and update the arrival time if new_time is smaller
        //void min_arr(const Time& new_time, const Tag& base_tag);

        ///Updates the required time of this set of tags to be the maximum.
        ///\param new_time The new arrival time to compare against
        ///\param base_tag The associated metat-data for new_time
        ///\remark Finds (or creates) the tag with the same clock domain as base_tag and update the required time if new_time is larger
        //void max_req(const Time& new_time, const Tag& base_tag);

        ///Clears the tags in the current set
        void clear();


    private:
        int num_tags_;

        //The first NUM_FLAT_TAGS tags are stored directly as members
        //of this object. Any additional tags are stored in a dynamically
        //allocated linked list.
        //Note that despite being an array, each element of head_tags_ is
        //hooked into the linked list
        std::array<Tag, NUM_FLAT_TAGS> head_tags_;
};

#include <algorithm>
#include "assert.hpp"
/*
 * ExtTimingTags implementation
 */

//Modifiers
inline ExtTimingTags::iterator ExtTimingTags::add_tag(const Tag& tag) {
    ASSERT_MSG(tag.next() == nullptr, "Attempted to add new timing tag which is already part of a Linked List");

    //Don't add invalid clock domains
    //Some sources like constant generators may yeild illegal clock domains
    if(tag.clock_domain() == INVALID_CLOCK_DOMAIN) {
        return end();;
    }

    Tag* new_tag = nullptr;

    if(num_tags_ < (int) head_tags_.max_size()) {
        //Store it as a head tag
        head_tags_[num_tags_] = tag;

        new_tag = &head_tags_[num_tags_];

        if(num_tags_ != 0) {
            //Link from previous if it exists
            head_tags_[num_tags_-1].set_next(&head_tags_[num_tags_]);
        }
    } else {
        //Store it in a linked list from head tags

        new_tag = new ExtTimingTags::Tag(tag);

        //Insert one-after the last head in O(1) time
        //Note that we don't maintain the tags in any order since we expect a relatively small number of tags
        //per node
        auto* next_tag = head_tags_[head_tags_.max_size()-1].next(); //Save next link (may be nullptr)
        head_tags_[head_tags_.max_size()-1].set_next(new_tag); //Tag is now in the list
        new_tag->set_next(next_tag); //Attach tail of the list
    }
    assert(new_tag != nullptr);

    //Tag has been added
    num_tags_++;

    return iterator(new_tag);
}

inline void ExtTimingTags::max_arr(const Tag& tag) {
    auto iter = find_matching_tag(tag);
    if(iter == end()) {
        //First time we've seen this tag
        iter = add_tag(tag);
        return;
    } else {
        iter->max_arr(tag.arr_time(), tag);
    }

    assert(iter != end());

    //HACK copy the current input transitions, append any from the incoming tag
    //and then set the updated transitions and the new (current) transitions
    std::vector<std::vector<TransitionType>> input_transitions = iter->input_transitions();

    for(auto scenario : tag.input_transitions()) {
        input_transitions.push_back(scenario);
    }

    iter->set_input_transitions(input_transitions);
}

/*
 *inline void ExtTimingTags::min_req(const Time& new_time, const Tag& base_tag) {
 *    auto iter = find_matching_tag(base_tag);
 *    if(iter == end()) {
 *        //First time we've seen this domain
 *        auto tag = ExtTimingTags::Tag(Time(NAN), new_time, base_tag);
 *        add_tag(tag);
 *    } else {
 *        iter->min_req(new_time, base_tag);
 *    }
 *}
 *
 *inline void ExtTimingTags::min_arr(const Time& new_time, const Tag& base_tag) {
 *    auto iter = find_matching_tag(base_tag);
 *    if(iter == end()) {
 *        //First time we've seen this domain
 *        auto tag = ExtTimingTags::Tag(new_time, Time(NAN), base_tag);
 *        add_tag(tag);
 *    } else {
 *        iter->min_arr(new_time, base_tag);
 *    }
 *}
 *
 *inline void ExtTimingTags::max_req(const Time& new_time, const Tag& base_tag) {
 *    auto iter = find_matching_tag(base_tag);
 *    if(iter == end()) {
 *        //First time we've seen this domain
 *        auto tag = ExtTimingTags::Tag(new_time, Time(NAN), base_tag);
 *        add_tag(tag);
 *    } else {
 *        iter->max_req(new_time, base_tag);
 *    }
 *}
 */

inline void ExtTimingTags::clear() {
    //TODO: handle memory leaks...
    num_tags_ = 0;
}

inline ExtTimingTags::iterator ExtTimingTags::find_matching_tag(const Tag& base_tag) {
    auto pred = [base_tag](const Tag& tag) {
        return tag.matches(base_tag);
    };
    return std::find_if(begin(), end(), pred);
}

inline ExtTimingTags::const_iterator ExtTimingTags::find_matching_tag(const Tag& base_tag) const {
    auto pred = [base_tag](const ExtTimingTags::Tag& tag) {
        return tag.matches(base_tag);
    };
    return std::find_if(begin(), end(), pred);
}

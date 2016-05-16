#pragma once
/*
 *
 */
#include <cassert>
#include <unordered_map>
#include "bdd.hpp"
#include "Time.hpp"
#include "TransitionType.hpp"
#include "timing_graph_fwd.hpp"

/*
 * These defines control how a tag is said to 'match' another
 * tag.
 *
 * For example, defining TAG_MATCH_TRANSITION will require that
 * the transition type be the same in two matching tags.
 *
 * For more detail see ExtTimingTag::matches()
 */
#define TAG_MATCH_TRANSITION
#define TAG_MATCH_DELAY
//#define TAG_MATCH_SWITCH_FUNC

//Print out extra tag debug information
#define DEBUG_TAG_PRINT

class ExtTimingTag {
    public:
        /*
         * Constructors
         */
        ExtTimingTag();

        ///\param arr_time_val The tagged arrival time
        ///\param req_time_val The tagged required time
        ///\param domain The clock domain the arrival/required times were launched from
        ///\param node The original launch node's id (i.e. primary input that originally launched this tag)
        ///\param trans The transition type
        ///\param f The switching function (evaluates true when the specified transition occurs)
        ExtTimingTag(const Time& arr_time_val, const Time& req_time_val, DomainId domain, NodeId node, TransitionType trans);

        ///\param arr_time_val The tagged arrival time
        ///\param req_time_val The tagged required time
        ///\param base_tag The tag from which to copy auxilary meta-data (e.g. domain, launch node)
        ExtTimingTag(const Time& arr_time_val, const Time& req_time_val, const ExtTimingTag& base_tag);

        /*
         * Getters
         */
        ///\returns This tag's arrival time
        const Time& arr_time() const { return arr_time_; }

        ///\returns This tag's required time
        const Time& req_time() const { return req_time_; }

        ///\returns This tag's launching clock domain
        DomainId clock_domain() const { return clock_domain_; }

        ///\returns This tag's launching node's id
        NodeId launch_node() const { return launch_node_; }

        ///\returns This tag's associated transition type
        TransitionType trans_type() const { return trans_type_; }

        const std::vector<std::vector<const ExtTimingTag*>>& input_tags() const { return input_tags_; }

        /*
         * Setters
         */
        ///\param new_arr_time The new value set as the tag's arrival time
        void set_arr_time(const Time& new_arr_time) { arr_time_ = new_arr_time; };

        ///\param new_req_time The new value set as the tag's required time
        void set_req_time(const Time& new_req_time) { req_time_ = new_req_time; };

        ///\param new_req_time The new value set as the tag's clock domain
        void set_clock_domain(const DomainId new_clock_domain) { clock_domain_ = new_clock_domain; };

        ///\param new_launch_node The new value set as the tag's launching node
        void set_launch_node(const NodeId new_launch_node) { launch_node_ = new_launch_node; };

        ///\param new_trans The new value to set as the tag's transition type
        void set_trans_type(const TransitionType& new_trans_type) { trans_type_ = new_trans_type; }

        void add_input_tags(const std::vector<const ExtTimingTag*>& t) { input_tags_.push_back(t); }

        /*
         * Modification operations
         *  For the following the passed in time is maxed/minned with the
         *  respective arr/req time.  If the value of this tag is updated
         *  the meta-data (domain, launch node etc) are copied from the
         *  base tag
         */
        ///Updates the tag's arrival time if new_arr_time is larger than the current arrival time.
        ///If the arrival time is updated, meta-data is also updated from base_tag
        ///\param new_arr_time The arrival time to compare against
        ///\param base_tag The tag from which meta-data is copied
        void max_arr(const Time new_arr, const ExtTimingTag* base_tag);

        ///Updates the tag's arrival time if new_arr_time is smaller than the current arrival time.
        ///If the arrival time is updated, meta-data is also updated from base_tag
        ///\param new_arr_time The arrival time to compare against
        ///\param base_tag The tag from which meta-data is copied
        //void min_arr(const Time& new_arr_time, const ExtTimingTag& base_tag);

        ///Updates the tag's required time if new_req_time is smaller than the current required time.
        ///If the required time is updated, meta-data is also updated from base_tag
        ///\param new_arr_time The arrival time to compare against
        ///\param base_tag The tag from which meta-data is copied
        //void min_req(const Time& new_req_time, const ExtTimingTag& base_tag);

        ///Updates the tag's required time if new_req_time is larger than the current required time.
        ///If the required time is updated, meta-data is also updated from base_tag
        ///\param new_arr_time The arrival time to compare against
        ///\param base_tag The tag from which meta-data is copied
        //void max_req(const Time& new_req_time, const ExtTimingTag& base_tag);

        /*
         * Comparison
         */
        ///\param other The tag to compare against
        ///\returns true if the meta-data of the current and other tag match
        bool matches(const ExtTimingTag* other, double delay_bin_size) const;

    private:
        void update_arr(const Time new_arr, const ExtTimingTag* base_tag);
        //void update_req(const Time& new_req_time, const ExtTimingTag& base_tag);

        /*
         * Data
         */
        Time arr_time_; //Arrival time
        Time req_time_; //Required time
        DomainId clock_domain_; //Clock domain for arr/req times
        NodeId launch_node_; //Node which launched this arrival time
        TransitionType trans_type_; //The transition type associated with this tag
        std::vector<std::vector<const ExtTimingTag*>> input_tags_;
};

std::ostream& operator<<(std::ostream& os, const ExtTimingTag& tag);

inline ExtTimingTag::ExtTimingTag()
    : arr_time_(NAN)
    , req_time_(NAN)
    , clock_domain_(INVALID_CLOCK_DOMAIN)
    , launch_node_(-1)
    , trans_type_(TransitionType::UNKOWN)
    {}

inline ExtTimingTag::ExtTimingTag(const Time& arr_time_val, const Time& req_time_val, DomainId domain, NodeId node, TransitionType trans)
    : arr_time_(arr_time_val)
    , req_time_(req_time_val)
    , clock_domain_(domain)
    , launch_node_(node)
    , trans_type_(trans)
    {}

inline ExtTimingTag::ExtTimingTag(const Time& arr_time_val, const Time& req_time_val, const ExtTimingTag& base_tag)
    : arr_time_(arr_time_val)
    , req_time_(req_time_val)
    , clock_domain_(base_tag.clock_domain())
    , launch_node_(base_tag.launch_node())
    , trans_type_(base_tag.trans_type())
    {}

inline void ExtTimingTag::update_arr(const Time new_arr, const ExtTimingTag* base_tag) {
    if(base_tag->clock_domain() != INVALID_CLOCK_DOMAIN) {
        assert(clock_domain() == base_tag->clock_domain()); //Domain must be the same
        set_arr_time(new_arr);
        set_launch_node(base_tag->launch_node());
    }
}

inline void ExtTimingTag::max_arr(const Time new_arr, const ExtTimingTag* base_tag) {
    //Need to min with existing value
    if(!arr_time().valid() || new_arr.value() > arr_time().value()) {
        //New value is smaller, or no previous valid value existed
        //Update min
        update_arr(new_arr, base_tag);
    }
}

inline bool ExtTimingTag::matches(const ExtTimingTag* other, double delay_bin_size) const {
    //If a tag 'matches' it is typically collapsed into the matching tag.

    bool match = (clock_domain() == other->clock_domain());

#ifdef TAG_MATCH_TRANSITION
    match &= (trans_type() == other->trans_type());
#endif

#ifdef TAG_MATCH_DELAY
    //match &= (arr_time() == other->arr_time());

    //We say that a delay 'matches' if it falls within the same delay bin
    double this_bin = arr_time().value();
    double other_bin = other->arr_time().value();
    
    if(delay_bin_size != 0.) {
        //Map to the appropriate bin, we treat a bin size of zero as no binning
        this_bin = std::floor(this_bin / delay_bin_size);
        other_bin = std::floor(other_bin / delay_bin_size);
    }

    match &= (this_bin == other_bin);
#endif

#ifdef TAG_MATCH_SWITCH_FUNC
    match &= (switch_func() == other->switch_func());
#endif

    return match;
}

inline std::ostream& operator<<(std::ostream& os, const ExtTimingTag& tag) {
    os << &tag << " ";
    //os << "Domain: " << tag.clock_domain() << " ";
    //os << "Launch Node: " << tag.launch_node() << " ";
    os << "OutTrans: " << tag.trans_type() << " ";
    os << "Arr: " << tag.arr_time().value() << " ";
    //os << "Req: " << tag.req_time().value() << " ";

#ifdef DEBUG_TAG_PRINT
    if(tag.input_tags().size() > 0) {
        os << "\n";
        os << "\t\t";
        for(auto scenario : tag.input_tags()) {
            for(auto in_tag : scenario) {
                os << "  " << in_tag;
                os << " Trans: " << in_tag->trans_type();  
                os << " Arr:   " << in_tag->arr_time().value();  
            }
            os << "\n";
            os << "\t\t";
        }
    }
    //os << "\t\t";
#endif

    return os;
}

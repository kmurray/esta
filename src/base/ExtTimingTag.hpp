#pragma once
/*
 *
 */

#include "cuddObj.hh"
extern Cudd g_cudd;

enum class TransitionType {
    RISE,
    FALL,
    HIGH,
    LOW
};

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
        ExtTimingTag(const Time& arr_time_val, const Time& req_time_val, DomainId domain, NodeId node, TransitionType trans, const BDD& f);

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

        ///\returns This tag's associated logic function
        const BDD& func() const { return func_; }

        ///\returns The next ExtTimingTag in the current set of ExtTimingTags (i.e. the next tag at a specific nonde in the TimingGraph)
        ExtTimingTag* next() const { return next_; }

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

        ///\param new_func The new value to set as the tag's logic function
        void set_func(const BDD& new_func) { func_ = new_func; }

        ///\param new_next The new timing tag to insert in the current set of ExtTimingTags
        void set_next(ExtTimingTag* new_next) { next_ = new_next; }

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
        void max_arr(const Time& new_arr_time, const ExtTimingTag& base_tag);

        ///Updates the tag's arrival time if new_arr_time is smaller than the current arrival time.
        ///If the arrival time is updated, meta-data is also updated from base_tag
        ///\param new_arr_time The arrival time to compare against
        ///\param base_tag The tag from which meta-data is copied
        void min_arr(const Time& new_arr_time, const ExtTimingTag& base_tag);

        ///Updates the tag's required time if new_req_time is smaller than the current required time.
        ///If the required time is updated, meta-data is also updated from base_tag
        ///\param new_arr_time The arrival time to compare against
        ///\param base_tag The tag from which meta-data is copied
        void min_req(const Time& new_req_time, const ExtTimingTag& base_tag);

        ///Updates the tag's required time if new_req_time is larger than the current required time.
        ///If the required time is updated, meta-data is also updated from base_tag
        ///\param new_arr_time The arrival time to compare against
        ///\param base_tag The tag from which meta-data is copied
        void max_req(const Time& new_req_time, const ExtTimingTag& base_tag);

        /*
         * Comparison
         */
        ///\param other The tag to compare against
        ///\returns true if the meta-data of the current and other tag match
        bool matches(const ExtTimingTag& other) const;

    private:
        void update_arr(const Time& new_arr_time, const ExtTimingTag& base_tag);
        void update_req(const Time& new_req_time, const ExtTimingTag& base_tag);

        /*
         * Data
         */
        ExtTimingTag* next_; //Next element in linked list of tags at a particular timing graph node
        Time arr_time_; //Arrival time
        Time req_time_; //Required time
        DomainId clock_domain_; //Clock domain for arr/req times
        NodeId launch_node_; //Node which launched this arrival time
        TransitionType trans_type_;
        BDD func_; //The logic function

};

inline ExtTimingTag::ExtTimingTag()
    : next_(nullptr)
    , arr_time_(NAN)
    , req_time_(NAN)
    , clock_domain_(INVALID_CLOCK_DOMAIN)
    , launch_node_(-1)
    , trans_type_(TransitionType::RISE)
    , func_(g_cudd.bddOne())
    {}

inline ExtTimingTag::ExtTimingTag(const Time& arr_time_val, const Time& req_time_val, DomainId domain, NodeId node, TransitionType trans, const BDD& f)
    : next_(nullptr)
    , arr_time_(arr_time_val)
    , req_time_(req_time_val)
    , clock_domain_(domain)
    , launch_node_(node)
    , trans_type_(trans)
    , func_(f)
    {}

inline ExtTimingTag::ExtTimingTag(const Time& arr_time_val, const Time& req_time_val, const ExtTimingTag& base_tag)
    : next_(nullptr)
    , arr_time_(arr_time_val)
    , req_time_(req_time_val)
    , clock_domain_(base_tag.clock_domain())
    , launch_node_(base_tag.launch_node())
    , trans_type_(base_tag.trans_type())
    , func_(base_tag.func())
    {}


inline void ExtTimingTag::update_arr(const Time& new_arr_time, const ExtTimingTag& base_tag) {
    //NOTE: leave next alone, since we want to keep the linked list intact
    ASSERT(clock_domain() == base_tag.clock_domain()); //Domain must be the same
    set_arr_time(new_arr_time);
    set_launch_node(base_tag.launch_node());
    set_trans_type(base_tag.trans_type());
    set_func(base_tag.func());
}

inline void ExtTimingTag::update_req(const Time& new_req_time, const ExtTimingTag& base_tag) {
    //NOTE: We only update the req time, since everything else is determined by the arrival
    //TODO: remove base tag argument?
    ASSERT(clock_domain() == base_tag.clock_domain()); //Domain must be the same
    set_req_time(new_req_time);
}

inline void ExtTimingTag::max_arr(const Time& new_arr_time, const ExtTimingTag& base_tag) {
    //Need to min with existing value
    if(!arr_time().valid() || new_arr_time.value() > arr_time().value()) {
        //New value is smaller, or no previous valid value existed
        //Update min
        update_arr(new_arr_time, base_tag);
    }
}

inline void ExtTimingTag::min_req(const Time& new_req_time, const ExtTimingTag& base_tag) {
    //Need to min with existing value
    if(!req_time().valid() || new_req_time.value() < req_time().value()) {
        //New value is smaller, or no previous valid value existed
        //Update min
        update_req(new_req_time, base_tag);
    }
}

inline void ExtTimingTag::min_arr(const Time& new_arr_time, const ExtTimingTag& base_tag) {
    //Need to min with existing value
    if(!arr_time().valid() || new_arr_time.value() < arr_time().value()) {
        //New value is smaller, or no previous valid value existed
        //Update min
        update_arr(new_arr_time, base_tag);
    }
}

inline void ExtTimingTag::max_req(const Time& new_req_time, const ExtTimingTag& base_tag) {
    //Need to min with existing value
    if(!req_time().valid() || new_req_time.value() > req_time().value()) {
        //New value is smaller, or no previous valid value existed
        //Update min
        update_req(new_req_time, base_tag);
    }
}
inline bool ExtTimingTag::matches(const ExtTimingTag& other) const {
    return (clock_domain() == other.clock_domain()) && (trans_type() == other.trans_type());    
}

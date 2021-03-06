template<class BaseAnalysisMode, class Tags>
void SetupAnalysisMode<BaseAnalysisMode,Tags>::initialize_traversal(const TimingGraph& tg) {
    //Chain to base class
    BaseAnalysisMode::initialize_traversal(tg);

    //Initialize
    setup_data_tags_ = std::vector<Tags>(tg.num_nodes());
    setup_clock_tags_ = std::vector<Tags>(tg.num_nodes());
}

template<class BaseAnalysisMode, class Tags>
template<class DelayCalc>
void SetupAnalysisMode<BaseAnalysisMode,Tags>::pre_traverse_node(const TimingGraph& tg, const TimingConstraints& tc, const DelayCalc& dc, const NodeId node_id) {
    //Chain to base class
    BaseAnalysisMode::pre_traverse_node(tg, tc, dc, node_id);

    //Primary Input
    ASSERT_MSG(tg.num_node_in_edges(node_id) == 0, "Primary input has input edges: timing graph not levelized.");

    TN_Type node_type = tg.node_type(node_id);

    //Note that we assume that edge counting has set the effective period constraint assuming a
    //launch edge at time zero.  This means we don't need to do anything special for clocks
    //with rising edges after time zero.
    if(node_type == TN_Type::CONSTANT_GEN_SOURCE) {
        //Pass, we don't propagate any tags from constant generators,
        //since they do not effect they dynamic timing behaviour of the
        //system

    } else if(node_type == TN_Type::CLOCK_SOURCE) {
        ASSERT_MSG(setup_clock_tags_[node_id].num_tags() == 0, "Clock source already has clock tags");

        //Initialize a clock tag with zero arrival, invalid required time
        Tag clock_tag = Tag(Time(0.), Time(NAN), tg.node_clock_domain(node_id), node_id);

        //Add the tag
        setup_clock_tags_[node_id].add_tag(clock_tag);

    } else {
        ASSERT(node_type == TN_Type::INPAD_SOURCE);

        //A standard primary input

        //We assume input delays are on the arc from INPAD_SOURCE to INPAD_OPIN,
        //so we do not need to account for it directly in the arrival time of INPAD_SOURCES

        //Initialize a data tag with zero arrival, invalid required time
        Tag input_tag = Tag(Time(0.), Time(NAN), tg.node_clock_domain(node_id), node_id);

        //Figure out if we are an input which defines a clock
        if(tg.node_is_clock_source(node_id)) {
            ASSERT_MSG(setup_clock_tags_[node_id].num_tags() == 0, "Primary input already has clock tags");

            setup_clock_tags_[node_id].add_tag(input_tag);
        } else {
            ASSERT_MSG(setup_clock_tags_[node_id].num_tags() == 0, "Primary input already has data tags");

            setup_data_tags_[node_id].add_tag(input_tag);
        }
    }
}

template<class BaseAnalysisMode, class Tags>
template<class DelayCalcType>
void SetupAnalysisMode<BaseAnalysisMode,Tags>::forward_traverse_edge(const TimingGraph& tg, const TimingConstraints& tc, const DelayCalcType& dc, const NodeId node_id, const EdgeId edge_id) {
    //Chain to base class
    BaseAnalysisMode::forward_traverse_edge(tg, tc, dc, node_id, edge_id);

    //We must use the tags by reference so we don't accidentally wipe-out any
    //existing tags
    Tags& node_data_tags = setup_data_tags_[node_id];
    Tags& node_clock_tags = setup_clock_tags_[node_id];

    //Pulling values from upstream source node
    NodeId src_node_id = tg.edge_src_node(edge_id);

    const Time& edge_delay = dc.max_edge_delay(tg, edge_id);

    /*
     * Clock tags
     */
    if(tg.node_type(src_node_id) != TN_Type::FF_SOURCE) {
        //We do not propagate clock tags from an FF_SOURCE.
        //The clock arrival will have already been converted to a
        //data tag when the previous level was traversed.

        const Tags& src_clk_tags = setup_clock_tags_[src_node_id];
        for(const Tag& src_clk_tag : src_clk_tags) {
            //Standard propagation through the clock network
            node_clock_tags.max_arr(src_clk_tag.arr_time() + edge_delay, src_clk_tag);

            if(tg.node_type(node_id) == TN_Type::FF_SOURCE) {
                //We are traversing a clock to data launch edge.
                //
                //We convert the clock arrival time to a data
                //arrival time at this node (since the clock
                //arrival launches the data).

                //Make a copy of the tag
                Tag launch_tag = src_clk_tag;

                //Update the launch node, since the data is
                //launching from this node
                launch_tag.set_launch_node(node_id);
                ASSERT(launch_tag.next() == nullptr);

                //Mark propagated launch time as a DATA tag
                node_data_tags.max_arr(launch_tag.arr_time() + edge_delay, launch_tag);
            }
        }
    }

    /*
     * Data tags
     */
    const Tags& src_data_tags = setup_data_tags_[src_node_id];

    for(const Tag& src_data_tag : src_data_tags) {
        //Standard data-path propagation
        node_data_tags.max_arr(src_data_tag.arr_time() + edge_delay, src_data_tag);
    }
}

template<class BaseAnalysisMode, class Tags>
template<class DelayCalc>
void SetupAnalysisMode<BaseAnalysisMode,Tags>::forward_traverse_finalize_node(const TimingGraph& tg, const TimingConstraints& tc, const DelayCalc& dc, const NodeId node_id, const TagReducer& /*unused*/, size_t max_permutations) {
    //Chain to base class
    BaseAnalysisMode::forward_traverse_finalize_node(tg, tc, dc, node_id);

    //Take tags by reference so they are updated in-place
    Tags& node_data_tags = setup_data_tags_[node_id];
    Tags& node_clock_tags = setup_clock_tags_[node_id];

    /*
     * Calculate required times
     */
    if(tg.node_type(node_id) == TN_Type::OUTPAD_SINK) {
        //Determine the required time for outputs.
        //
        //We assume any output delay is on the OUTPAT_IPIN to OUTPAD_SINK edge,
        //so we only need set the constraint on the OUTPAD_SINK
        DomainId node_domain = tg.node_clock_domain(node_id);
        for(const Tag& data_tag : node_data_tags) {
            //Should we be analyzing paths between these two domains?
            if(tc.should_analyze(data_tag.clock_domain(), node_domain)) {
                //These clock domains should be analyzed

                float clock_constraint = tc.setup_clock_constraint(data_tag.clock_domain(), node_domain);

                //Set the required time on the sink.
                if(clock_constraint < 0.) {
                    auto max_delay = 0.f;
                    
                    for(auto& tag : node_data_tags) {
                        max_delay = std::max(max_delay, tag.arr_time().value());
                    }

                    node_data_tags.min_req(Time(max_delay), data_tag);
                } else {
                    node_data_tags.min_req(Time(clock_constraint), data_tag);
                }
            }
        }
    } else if (tg.node_type(node_id) == TN_Type::FF_SINK) {
        //Determine the required time at this FF
        //
        //We need to generate a required time for each clock domain for which there is a data
        //arrival time at this node, while considering all possible clocks that could drive
        //this node (i.e. take the most restrictive constraint accross all clock tags at this
        //node)

        for(Tag& node_data_tag : node_data_tags) {
            for(const Tag& node_clock_tag : node_clock_tags) {
                //Should we be analyzing paths between these two domains?
                if(tc.should_analyze(node_data_tag.clock_domain(), node_clock_tag.clock_domain())) {

                    //We only set a required time if the source domain actually reaches this sink
                    //domain.  This is indicated by having a valid arrival time.
                    if(node_data_tag.arr_time().valid()) {
                        float clock_constraint = tc.setup_clock_constraint(node_data_tag.clock_domain(),
                                                                           node_clock_tag.clock_domain());

                        //Update the required time. This will keep the most restrictive constraint.
                        node_data_tag.min_req(node_clock_tag.arr_time() + Time(clock_constraint), node_data_tag);
                    }
                }
            }
        }
    }
}

template<class BaseAnalysisMode, class Tags>
template<class DelayCalcType>
void SetupAnalysisMode<BaseAnalysisMode,Tags>::backward_traverse_edge(const TimingGraph& tg, const TimingConstraints& tc, const DelayCalcType& dc, const NodeId node_id, const EdgeId edge_id) {
    //Chain to base class
    BaseAnalysisMode::backward_traverse_edge(tg, tc, dc, node_id, edge_id);

    //We must use the tags by reference so we don't accidentally wipe-out any
    //existing tags
    Tags& node_data_tags = setup_data_tags_[node_id];

    //Pulling values from downstream sink node
    int sink_node_id = tg.edge_sink_node(edge_id);

    const Time& edge_delay = dc.max_edge_delay(tg, edge_id);

    const Tags& sink_data_tags = setup_data_tags_[sink_node_id];

    for(const Tag& sink_tag : sink_data_tags) {
        //We only propogate the required time if we have a valid arrival time
        auto matched_tag_iter = node_data_tags.find_tag_by_clock_domain(sink_tag.clock_domain());
        if(matched_tag_iter != node_data_tags.end() && matched_tag_iter->arr_time().valid()) {
            //Valid arrival, update required
            matched_tag_iter->min_req(sink_tag.req_time() - edge_delay, sink_tag);
        }
    }
}

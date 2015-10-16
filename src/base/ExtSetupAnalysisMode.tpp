template<class BaseAnalysisMode, class Tags>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::initialize_traversal(const TimingGraph& tg) {
    //Chain to base class
    BaseAnalysisMode::initialize_traversal(tg);

    //Initialize
    setup_data_tags_ = std::vector<Tags>(tg.num_nodes());
    setup_clock_tags_ = std::vector<Tags>(tg.num_nodes());

    //We have a unique logic variable for each Primary Input
    //
    //We assume the variables used in the timing graph logic function represent the 'current' logic values
    //We will need a similar set of variables representing the 'next' logic values in order to calculate
    //switching functions.
    //
    //To generate these 'next' logic functions we just replace the variables in the current function.
    //To do so requries we know the mapping from 'curr' to 'next' variables. To generate this mapping
    //we walk the primary inputs.
    curr_bdd_vars_.clear();
    next_bdd_vars_.clear();
    for(NodeId node_id : tg.primary_inputs()) {
        BDD node_func = tg.node_func(node_id);
        //Should be just a single variable
        assert(node_func.nodeCount() == 2); //Terminal + variable = 2

        //Record the current variable
        curr_bdd_vars_.push_back(node_func);

        //We need to generate and record a new 'next' variable
        next_bdd_vars_.push_back(g_cudd.bddVar());
    }
}

template<class BaseAnalysisMode, class Tags>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::pre_traverse_node(const TimingGraph& tg, const TimingConstraints& tc, const NodeId node_id) {
    //Chain to base class
    BaseAnalysisMode::pre_traverse_node(tg, tc, node_id);

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
        assert(0);
/*
 *
 *        //Initialize a clock tag with zero arrival, invalid required time
 *        Tag clock_tag = Tag(Time(0.), Time(NAN), tg.node_clock_domain(node_id), node_id);
 *
 *        //Add the tag
 *        setup_clock_tags_[node_id].add_tag(clock_tag);
 */

    } else {
        ASSERT(node_type == TN_Type::INPAD_SOURCE);

        //A standard primary input

        //We assume input delays are on the arc from INPAD_SOURCE to INPAD_OPIN,
        //so we do not need to account for it directly in the arrival time of INPAD_SOURCES

        //Initialize a data tag with zero arrival, invalid required time
        for(auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {

            BDD switch_func = generate_switch_func(tg.node_func(node_id), trans);

            Tag input_tag = Tag(Time(0.), Time(NAN), tg.node_clock_domain(node_id), node_id, trans, switch_func);

            //Figure out if we are an input which defines a clock
            if(tg.node_is_clock_source(node_id)) {
                /*ASSERT_MSG(setup_clock_tags_[node_id].num_tags() == 0, "Primary input already has clock tags");*/
                assert(0);

                setup_clock_tags_[node_id].add_tag(input_tag);
            } else {
                /*ASSERT_MSG(setup_clock_tags_[node_id].num_tags() == 0, "Primary input already has data tags");*/

                setup_data_tags_[node_id].add_tag(input_tag);
            }
        }
    }
}

#if 0
template<class BaseAnalysisMode, class Tags>
template<class DelayCalcType>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::forward_traverse_edge(const TimingGraph& tg, const DelayCalcType& dc, const NodeId node_id, const EdgeId edge_id) {
    //Chain to base class
    BaseAnalysisMode::forward_traverse_edge(tg, dc, node_id, edge_id);

    //We must use the tags by reference so we don't accidentally wipe-out any
    //existing tags
    Tags& node_data_tags = setup_data_tags_[node_id];
    Tags& node_clock_tags = setup_clock_tags_[node_id];

    //Pulling values from upstream source node
    NodeId src_node_id = tg.edge_src_node(edge_id);

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
            TransitionType trans = src_clk_tag.trans_type();
            const Time& edge_delay = dc.max_edge_delay(tg, edge_id, trans);

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

        TransitionType trans = src_data_tag.trans_type();

        const Time& edge_delay = dc.max_edge_delay(tg, edge_id, trans);

        node_data_tags.max_arr(src_data_tag.arr_time() + edge_delay, src_data_tag);
    }
}
#endif

template<class BaseAnalysisMode, class Tags>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::forward_traverse_finalize_node(const TimingGraph& tg, const TimingConstraints& tc, const NodeId node_id) {
    //Chain to base class
    BaseAnalysisMode::forward_traverse_finalize_node(tg, tc, node_id);

    //Grab the tags from all inputs
    std::vector<Tags> input_tag_sets;
    for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
        EdgeId edge_id = tg.node_in_edge(node_id, edge_idx);

        NodeId src_node = tg.edge_src_node(edge_id);

        const Tags& input_data_tags = setup_data_tags_[src_node];
        input_tag_sets.push_back(input_data_tags);
    }

    //Generate all tag transition permutations
    std::vector<std::vector<Tag>> input_tag_perms = gen_input_tag_permutations(input_tag_sets);

    std::cout << "Evaluating Node: " << node_id << "\n";

    int iscenario = 0;
    for(const auto& input_tags_scenario : input_tag_perms) {

        //Print case
        std::cout << "\tScenario #" << iscenario << "\n";
        for(size_t i = 0; i < input_tags_scenario.size(); i++) {
            auto& tag = input_tags_scenario[i];
            std::cout << "\t\tinput #"<< i << ": " << tag.trans_type() << ", ";
        }
        std::cout << "\n";

        //Calculate the output transition type
        const BDD& node_func = tg.node_func(node_id);
        TransitionType output_transition = evaluate_transition(input_tags_scenario, node_func);
        std::cout << "\t\toutput " << output_transition << "\n";

        //Determine the switching function
        /*BDD switch_func = generate_switch_func(input_tags_scenario, node_func);*/

/*
 *        //Collect up the input switching functions
 *        std::vector<BDD> input_switch_funcs;
 *
 *        output_switch_func = node_func.VectorCompose(input_switch_funcs);
 */
        
        iscenario++;
    }
}

template<class BaseAnalysisMode, class Tags>
BDD ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::generate_switch_func(const BDD& node_func, TransitionType trans) {
    BDD f_curr = node_func;
    BDD f_next = node_func.SwapVariables(curr_bdd_vars_, next_bdd_vars_);

    BDD switch_func;
    switch(trans) {
        case TransitionType::RISE:
            switch_func = (!f_curr) & f_next; 
            break;
        case TransitionType::FALL:
            switch_func = f_curr & (!f_next); 
            break;
        case TransitionType::HIGH:
            switch_func = f_curr & f_next; 
            break;
        case TransitionType::LOW:
            switch_func = (!f_curr) & (!f_next); 
            break;
        /*
         *case TransitionType::STEADY:
         *    switch_func = !(f_curr ^ f_next);
         *    break;
         *case TransitionType::SWITCH:
         *    switch_func = f_curr ^ f_next;
         *    break;
         */
        default:
            assert(0);
    }
    return switch_func;
}

template<class BaseAnalysisMode, class Tags>
std::vector<std::vector<typename Tags::Tag>> ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::gen_input_tag_permutations(const std::vector<Tags>& input_tags) {
    std::vector<std::vector<Tag>> permuations;

    gen_input_tag_permutations_helper(input_tags, 0, std::vector<Tag>(), permuations);

    return permuations;
}

template<class BaseAnalysisMode, class Tags>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::gen_input_tag_permutations_helper(const std::vector<Tags>& input_tags, size_t var_idx, const std::vector<Tag>& partial_perm, std::vector<std::vector<Tag>>& permutations) {
    
    for(const Tag& tag : input_tags[var_idx]) {
        //Make a copy since we will be adding our values
        auto new_perm = partial_perm;
        new_perm.push_back(tag);

        if(var_idx < input_tags.size() - 1) {
            //Recursive case, other variables remain.
            //Fill in permuations of remaining variables
            gen_input_tag_permutations_helper(input_tags, var_idx+1, new_perm, permutations);
        } else {
            //Base case -- last variarble
            assert(var_idx == input_tags.size() - 1);

            //Add the final permutation
            assert(new_perm.size() == input_tags.size());
            permutations.push_back(new_perm);
        }
    }
}

template<class BaseAnalysisMode, class Tags>
TransitionType ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::evaluate_transition(const std::vector<Tag>& input_tags_scenario, const BDD& node_func) {
    //CUDD assumes we provide the inputs defined up to the maximum variable index in the bdd
    //so we need to allocate a vector atleast as large as the highest index.
    //To do this we query the support indicies and take the max + 1 of these as the size.
    //The vectors and then initialized (all values false) to this size.
    //Note that cudd will only read the indicies associated with the variables in the bdd, so the values of variablse not in the bdd don't matter
    auto support_indicies = node_func.SupportIndices();
    size_t max_var_index = *std::max_element(support_indicies.begin(), support_indicies.end());
    std::vector<int> initial_inputs(max_var_index+1, 0);
    std::vector<int> final_inputs(max_var_index+1, 0);
    
    for(size_t i = 0; i < input_tags_scenario.size(); i++) {
        const Tag& tag = input_tags_scenario[i];
        size_t var_idx = support_indicies[i];
        switch(tag.trans_type()) {
            case TransitionType::RISE:
                initial_inputs[var_idx] = 0;
                final_inputs[var_idx] = 1;
                break;
            case TransitionType::FALL:
                initial_inputs[var_idx] = 1;
                final_inputs[var_idx] = 0;
                break;
            case TransitionType::HIGH:
                initial_inputs[var_idx] = 1;
                final_inputs[var_idx] = 1;
                break;
            case TransitionType::LOW:
                initial_inputs[var_idx] = 0;
                final_inputs[var_idx] = 0;
                break;

            default:
                assert(0);
        }
    }

    std::cout << node_func << "\n";

#if 0
    std::cout << "Init input: ";
    for(auto val : initial_inputs) {
        std::cout << val;
    }
    std::cout << "\n";
    std::cout << "Init output: ";
    for(auto val : final_inputs) {
        std::cout << val;
    }
    std::cout << "\n";
#endif

    BDD init_output = node_func.Eval(initial_inputs.data());
    BDD final_output = node_func.Eval(final_inputs.data());

#if 0
    std::cout << "output: " << init_output << " -> " << final_output << "\n";
#endif

    if(init_output.IsOne()) {
        if(final_output.IsOne()) {
            return TransitionType::HIGH;
        } else {
            assert(final_output.IsZero());
            return TransitionType::FALL;
        }
    } else {
        assert(init_output.IsZero());
        if(final_output.IsOne()) {
            return TransitionType::RISE;
        } else {
            assert(final_output.IsZero());
            return TransitionType::LOW;
        }
    }

    assert(0); //Shouldn't get here
}


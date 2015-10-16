template<class BaseAnalysisMode, class Tags>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::initialize_traversal(const TimingGraph& tg) {
    //Chain to base class
    BaseAnalysisMode::initialize_traversal(tg);

    //Initialize
    setup_data_tags_ = std::vector<Tags>(tg.num_nodes());
    setup_clock_tags_ = std::vector<Tags>(tg.num_nodes());

    //We have a unique logic variable for each Primary Input
    //
    //To represent transitions we have both a 'curr' and 'next' variable
    pi_curr_bdd_vars_.clear();
    pi_next_bdd_vars_.clear();
    for(NodeId node_id : tg.primary_inputs()) {
        //Generate the current variable
        pi_curr_bdd_vars_[node_id] = g_cudd.bddVar();

        //We need to generate and record a new 'next' variable
        pi_next_bdd_vars_[node_id] = g_cudd.bddVar();
    }
}

template<class BaseAnalysisMode, class Tags>
template<class DelayCalc>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::pre_traverse_node(const TimingGraph& tg, const TimingConstraints& tc, const DelayCalc& dc, const NodeId node_id) {
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

            BDD switch_func = generate_pi_switch_func(node_id, trans);

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

template<class BaseAnalysisMode, class Tags>
template<class DelayCalcType>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::forward_traverse_finalize_node(const TimingGraph& tg, const TimingConstraints& tc, const DelayCalcType& dc, const NodeId node_id) {
    //Chain to base class
    BaseAnalysisMode::forward_traverse_finalize_node(tg, tc, dc, node_id);

    //Grab the tags from all inputs
    std::vector<Tags> src_tag_sets;
    for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
        EdgeId edge_id = tg.node_in_edge(node_id, edge_idx);

        NodeId src_node = tg.edge_src_node(edge_id);

        const Tags& src_data_tags = setup_data_tags_[src_node];
        src_tag_sets.push_back(src_data_tags);
    }

    //The output tag set
    Tags& sink_tags = setup_data_tags_[node_id];

    //Generate all tag transition permutations
    //TODO: use a generater rather than pre-compute
    std::vector<std::vector<Tag>> src_tag_perms = gen_tag_permutations(src_tag_sets);

    const BDD& node_func = tg.node_func(node_id);
    std::cout << "Evaluating Node: " << node_id << " (" << node_func << ")\n";

    int iscenario = 0;
    for(const auto& src_tags : src_tag_perms) {

        //Print case
        std::cout << "\tScenario #" << iscenario << "\n";
        std::cout << "\t\tinput: {";
        for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
            const auto& tag = src_tags[edge_idx];
            std::cout << tag.trans_type();
            if(edge_idx < tg.num_node_in_edges(node_id) - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "}\n";

        //Calculate the output transition type
        TransitionType output_transition = evaluate_transition(src_tags, node_func);
        std::cout << "\t\toutput: " << output_transition << "\n";

        //We get the associated output transition when all the transitions in each tag
        //of this input set occur -- that is when all the input switch functions evaluate
        //true
        assert(src_tags.size() > 0);
        Tag scenario_tag;
        scenario_tag.set_trans_type(output_transition);

        BDD scenario_switch_func = g_cudd.bddOne();
        for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
            EdgeId edge_id = tg.node_in_edge(node_id, edge_idx);
            const Tag& src_tag = src_tags[edge_idx];

            Time edge_delay = dc.max_edge_delay(tg, edge_id, output_transition);

            Time new_arr = src_tag.arr_time() + edge_delay;

            if(edge_idx == 0) {
                scenario_tag.set_clock_domain(src_tag.clock_domain());
            }
            scenario_tag.max_arr(new_arr, src_tag);

            scenario_switch_func &= src_tag.switch_func();
            assert(scenario_tag.trans_type() == output_transition);
        }
        scenario_tag.set_switch_func(scenario_switch_func);
        std::cout << "\t\tScenario Func: " << scenario_switch_func << " #SAT: " << scenario_switch_func.CountMinterm(2*tg.primary_inputs().size()) << "\n";
        
        //Now we need to merge the scenario into the output tags
        sink_tags.max_arr(scenario_tag); 

        auto pred = [output_transition](const Tag& tag) {
            return tag.trans_type() == output_transition;
        };
        auto iter = std::find_if(sink_tags.begin(), sink_tags.end(), pred);
        assert(iter != sink_tags.end());
        std::cout << "\t\tSink " << iter->trans_type() << " Func: " << iter->switch_func() << " #SAT: " << iter->switch_func().CountMinterm(2*tg.primary_inputs().size()) << "\n";

        iscenario++;
    }
}

template<class BaseAnalysisMode, class Tags>
BDD ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::generate_pi_switch_func(NodeId node_id, TransitionType trans) {
    auto curr_iter = pi_curr_bdd_vars_.find(node_id);
    assert(curr_iter != pi_curr_bdd_vars_.end());

    auto next_iter = pi_next_bdd_vars_.find(node_id);
    assert(next_iter != pi_next_bdd_vars_.end());

    BDD f_curr = curr_iter->second;
    BDD f_next = next_iter->second;

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
std::vector<std::vector<typename Tags::Tag>> ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::gen_tag_permutations(const std::vector<Tags>& tags) {
    std::vector<std::vector<Tag>> tag_permuations;

    gen_tag_permutations_recurr(tags, 0, std::vector<Tag>(), tag_permuations);

    return tag_permuations;
}

template<class BaseAnalysisMode, class Tags>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::gen_tag_permutations_recurr(const std::vector<Tags>& tags, size_t var_idx, const std::vector<Tag>& partial_perm, std::vector<std::vector<Tag>>& tag_permutations) {
    
    for(const Tag& tag : tags[var_idx]) {
        //Make a copy since we will be adding our values
        auto new_perm = partial_perm;
        new_perm.push_back(tag);

        if(var_idx < tags.size() - 1) {
            //Recursive case, other variables remain.
            //Fill in permuations of remaining variables
            gen_tag_permutations_recurr(tags, var_idx+1, new_perm, tag_permutations);
        } else {
            //Base case -- last variarble
            assert(var_idx == tags.size() - 1);

            //Add the final permutation
            assert(new_perm.size() == tags.size());
            tag_permutations.push_back(new_perm);
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

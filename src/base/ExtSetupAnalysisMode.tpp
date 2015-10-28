#include <chrono>
#include "util.hpp"

/*#define TAG_DEBUG*/

extern EtaStats g_eta_stats;

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
    for(NodeId node_id = 0; node_id < tg.num_nodes(); node_id++) {
        auto node_type = tg.node_type(node_id);
        if(node_type == TN_Type::INPAD_SOURCE || node_type == TN_Type::FF_SOURCE) {
            //Generate the current variable
            pi_curr_bdd_vars_[node_id] = g_cudd.bddVar();
            g_cudd.pushVariableName("n" + std::to_string(node_id));

            //We need to generate and record a new 'next' variable
            pi_next_bdd_vars_[node_id] = g_cudd.bddVar();
            g_cudd.pushVariableName("n" + std::to_string(node_id) + "'");
        }
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

        //Initialize a clock tag with zero arrival, invalid required time
        Tag clock_tag = Tag(Time(0.), Time(NAN), tg.node_clock_domain(node_id), node_id, TransitionType::CLOCK, g_cudd.bddOne());

        //Add the tag
        setup_clock_tags_[node_id].add_tag(clock_tag);

    } else {
        ASSERT(node_type == TN_Type::INPAD_SOURCE);

        //A standard primary input

        //We assume input delays are on the arc from INPAD_SOURCE to INPAD_OPIN,
        //so we do not need to account for it directly in the arrival time of INPAD_SOURCES

        if(tg.node_is_clock_source(node_id)) {
            //Figure out if we are an input which defines a clock
            for(auto trans : {TransitionType::CLOCK}) {
                Tag input_tag = Tag(Time(0.), Time(NAN), tg.node_clock_domain(node_id), node_id, trans, g_cudd.bddOne());
                setup_clock_tags_[node_id].add_tag(input_tag);
            }
        } else {
            //Initialize a data tag with zero arrival, invalid required time
            for(auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {

                BDD switch_func = generate_pi_switch_func(node_id, trans);

                Tag input_tag = Tag(Time(0.), Time(NAN), tg.node_clock_domain(node_id), node_id, trans, switch_func);
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
    std::vector<Tags> src_data_tag_sets;
    for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
        EdgeId edge_id = tg.node_in_edge(node_id, edge_idx);

        NodeId src_node = tg.edge_src_node(edge_id);

        //Handle data tags
        const Tags& src_data_tags = setup_data_tags_[src_node];
        if(src_data_tags.num_tags() != 0) {
            src_data_tag_sets.push_back(src_data_tags);
        }

        //Handle clock tags
        const Tags& src_clock_tags = setup_clock_tags_[src_node];
        if(tg.node_type(node_id) == TN_Type::FF_SOURCE) {
            assert(tg.node_type(src_node) == TN_Type::FF_CLOCK);

            /*
             * Convert the clock tag into a data tag at this node
             */
            //Edge delay for the clock
            const Time& edge_delay = dc.max_edge_delay(tg, edge_id, TransitionType::CLOCK);

            Tags& sink_data_tags = setup_data_tags_[node_id];
            for(const Tag& clk_tag : src_clock_tags) {
                //Determine the new data tag based on the arriving clock tag
                Time new_arr = clk_tag.arr_time() + edge_delay;
                for(auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
                    auto switch_func = generate_pi_switch_func(node_id, trans);
                    Tag launch_data_tag = Tag(new_arr, Time(NAN), clk_tag.clock_domain(), node_id, trans, switch_func);
                    sink_data_tags.max_arr(launch_data_tag);
                }
            }
        } else if(tg.node_type(node_id) == TN_Type::FF_SINK) {
            //TODO: annotate required time
        } else {
            //Standard clock tag propogation
            const Time& edge_delay = dc.max_edge_delay(tg, edge_id, TransitionType::CLOCK);
            Tags& sink_clock_tags = setup_clock_tags_[node_id];

            for(const Tag& clk_tag : src_clock_tags) {
                //Determine the new data tag based on the arriving clock tag
                Time new_arr = clk_tag.arr_time() + edge_delay;
                Tag new_clk_tag = Tag(new_arr, Time(NAN), clk_tag);
                sink_clock_tags.max_arr(new_clk_tag);
            }
        }
    }
    if(src_data_tag_sets.size() > 0) {

        //The output tag set
        Tags& sink_tags = setup_data_tags_[node_id];

        //Generate all tag transition permutations
        //TODO: use a generator rather than pre-compute
        std::vector<std::vector<Tag>> src_tag_perms = gen_tag_permutations(src_data_tag_sets);

        const BDD& node_func = tg.node_func(node_id);

#ifdef TAG_DEBUG
        std::cout << "Evaluating Node: " << node_id << " " << tg.node_type(node_id) << " (" << node_func << ")\n";
#endif

        for(const auto& src_tags : src_tag_perms) {

#ifdef TAG_DEBUG
            std::cout << "\tCase\n";
            std::cout << "\t\tinput: {";
            for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
                const auto& tag = src_tags[edge_idx];
                std::cout << tag.trans_type();
                if(edge_idx < tg.num_node_in_edges(node_id) - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "}\n";
#endif

            //Calculate the output transition type
            TransitionType output_transition = evaluate_transition(src_tags, node_func);

            //We get the associated output transition when all the transitions in each tag
            //of this input set occur -- that is when all the input switch functions evaluate
            //true
            assert(src_tags.size() > 0);
            Tag scenario_tag;
            scenario_tag.set_trans_type(output_transition);

            BDD scenario_switch_func = g_cudd.bddOne();
            assert((int) src_tags.size() <= tg.num_node_in_edges(node_id)); //May be less than if we are ignoring non-data edges like those from FF_CLOCK to FF_SINK
            /*for(int edge_idx = 0; edge_idx < (int) src_tags.size(); edge_idx++) {*/
            for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
                EdgeId edge_id = tg.node_in_edge(node_id, edge_idx);
                NodeId src_node_id = tg.edge_src_node(edge_id);
                if(tg.node_type(src_node_id) == TN_Type::FF_CLOCK) {
                    continue; //We skip edges from FF_CLOCK since they never carry data arrivals
                }

                const Tag& src_tag = src_tags[edge_idx];

                Time edge_delay = dc.max_edge_delay(tg, edge_id, output_transition);

                Time new_arr = src_tag.arr_time() + edge_delay;

                if(edge_idx == 0) {
                    scenario_tag.set_clock_domain(src_tag.clock_domain());
                }
                scenario_tag.max_arr(new_arr, src_tag);

                //This is the most expensive operation, since at lower levels of the timing graph
                //this becomes a large operation
                scenario_switch_func &= src_tag.switch_func();

                assert(scenario_tag.trans_type() == output_transition);
            }
            if(approx_xfuncs_) {
                std::chrono::time_point<std::chrono::steady_clock> start_approx = std::chrono::steady_clock::now();

                int xfunc_node_cnt = scenario_switch_func.nodeCount();
                BDD approx_switch_func;
                if(xfunc_node_cnt > APPROX_SWITCH_NODE_THRESHOLD) {
                    g_eta_stats.approx_attempts += 1;

                    /*std::cout << "Node " << node_id << " Nbdd " << xfunc_node_cnt << "\n";*/
                    int target_node_cnt = approx_xfunc_node_threshold_*xfunc_node_cnt;
                    int xfunc_support_size = scenario_switch_func.SupportSize();


                    approx_switch_func = scenario_switch_func.SupersetCompress(xfunc_support_size, target_node_cnt);
                    /*
                     *approx_switch_func = scenario_switch_func.SupersetShortPaths(xfunc_support_size, target_node_cnt);
                     */
                    /*approx_switch_func = scenario_switch_func.SupersetHeavyBranch(xfunc_support_size, target_node_cnt);*/
                    /*approx_switch_func = scenario_switch_func.OverApprox(xfunc_support_size, target_node_cnt, 0);*/
                    /*approx_switch_func = scenario_switch_func.OverApprox(xfunc_support_size, target_node_cnt, 1);*/
                    /*approx_switch_func = scenario_switch_func.RemapOverApprox(xfunc_support_size, target_node_cnt);*/


                    std::chrono::time_point<std::chrono::steady_clock> start_approx_eval = std::chrono::steady_clock::now();
                    double orig_nsat = scenario_switch_func.CountMinterm(2*tg.logical_inputs().size());
                    double approx_nsat = approx_switch_func.CountMinterm(2*tg.logical_inputs().size());
                    double approx_nsat_ratio = approx_nsat / orig_nsat;
                    double approx_node_ratio = (float) approx_switch_func.nodeCount() / xfunc_node_cnt;
                    g_eta_stats.approx_eval_time += std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::steady_clock::now() - start_approx_eval).count();

                    if(approx_nsat_ratio < approx_xfunc_accept_sharpsat_ratio_ && approx_node_ratio < approx_xfunc_accept_node_ratio_) {
                        g_eta_stats.approx_accepted += 1;
                        std::cout << "Approx Node Ratio: " << (float) approx_node_ratio;
                        std::cout << " Approx #SAT Ratio: " << (float) approx_nsat_ratio;
                        std::cout << " Accepted";
                        std::cout << "\n";
                    } else {
                        approx_switch_func = scenario_switch_func;
                        /*std::cout << "Approx Node Ratio: " << (float) approx_node_ratio;*/
                        /*std::cout << " Approx #SAT Ratio: " << (float) approx_nsat_ratio;*/
                        /*std::cout << " Rejected";*/
                        /*std::cout << "\n";*/
                    }

                } else {
                    //No approximation
                    approx_switch_func = scenario_switch_func;
                }

                g_eta_stats.approx_time += std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::steady_clock::now() - start_approx).count();

                scenario_tag.set_switch_func(approx_switch_func);
            } else {
                scenario_tag.set_switch_func(scenario_switch_func);
            }
            
            //Now we need to merge the scenario into the output tags
            sink_tags.max_arr(scenario_tag); 

#ifdef TAG_DEBUG
            std::cout << "\t\toutput: " << output_transition << "\n";
            /*std::cout << "\t\tScenario Func: " << scenario_switch_func << " #SAT: " << scenario_switch_func.CountMinterm(2*tg.primary_inputs().size()) << "\n";*/
            auto pred = [output_transition](const Tag& tag) {
                return tag.trans_type() == output_transition;
            };
            auto iter = std::find_if(sink_tags.begin(), sink_tags.end(), pred);
            assert(iter != sink_tags.end());
            std::cout << "\t\tSink " << iter->trans_type() << "\n";
            /*<< " Func: " << iter->switch_func() << "\n";*/
            /*std::cout << " #SAT: " << iter->switch_func().CountMinterm(2*tg.primary_inputs().size()) << "\n";*/
#endif
        }
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
    //Note that cudd will only read the indicies associated with the variables in the bdd, 
    //so the values of variablse not in the bdd don't matter
    /*std::cout << "Eval Transition Func: " << node_func << "\n";*/

    if(node_func.IsOne()) {
        return TransitionType::LOW;
    } else if (node_func.IsZero()) {
        return TransitionType::HIGH;
    } else {
        auto support_indicies = node_func.SupportIndices();
        assert(support_indicies.size() > 0);
        size_t max_var_index = *std::max_element(support_indicies.begin(), support_indicies.end());
        std::vector<int> initial_inputs(max_var_index+1, 0);
        std::vector<int> final_inputs(max_var_index+1, 0);

        for(size_t i = 0; i < support_indicies.size(); i++) {
            //It is entirely possible that the support of a function may be
            //less than the number of inputs (e.g. a useless input to a
            //logic function).
            //As a result we only fill in the variables which are explicitly
            //listed in the support
            size_t var_idx = support_indicies[i];

            //We do expect the node inputs to be a superset of
            //the support
            assert(var_idx < input_tags_scenario.size());
            const Tag& tag = input_tags_scenario[var_idx];
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
    }

    assert(0); //Shouldn't get here
}


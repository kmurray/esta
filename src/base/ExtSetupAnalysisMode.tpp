#include <chrono>
#include <sstream>
#include "util.hpp"

#define TAG_DEBUG

extern EtaStats g_eta_stats;

template<class BaseAnalysisMode, class Tags>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::reset_xfunc_cache() { 
    bdd_cache_.print_stats(); 
    bdd_cache_.clear(); 
    bdd_cache_.reset_stats(); 

    //Reset the default re-order size
    //Re-ordering really big BDDs is slow (re-order time appears to be quadratic in size)
    //So cap the size for re-ordering to something reasonable when re-starting
    auto next_reorder = std::min(g_cudd.ReadNextReordering(), 100004u);
    g_cudd.SetNextReordering(next_reorder);
}

template<class BaseAnalysisMode, class Tags>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::initialize_traversal(const TimingGraph& tg) {
    //Chain to base class
    BaseAnalysisMode::initialize_traversal(tg);

    //Initialize
    setup_data_tags_ = std::vector<Tags>(tg.num_nodes());
    setup_clock_tags_ = std::vector<Tags>(tg.num_nodes());
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
        Tag* clock_tag = new Tag(Time(0.), Time(NAN), tg.node_clock_domain(node_id), node_id, TransitionType::CLOCK);

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
                Tag* input_tag = new Tag(Time(0.), Time(NAN), tg.node_clock_domain(node_id), node_id, trans);
                setup_clock_tags_[node_id].add_tag(input_tag);
            }
        } else {
            //Initialize a data tag with zero arrival, invalid required time
            for(auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {

                Tag* input_tag = new Tag(Time(0.), Time(NAN), tg.node_clock_domain(node_id), node_id, trans);
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
            const Time& edge_delay = dc.max_edge_delay(tg, edge_id, TransitionType::CLOCK, TransitionType::CLOCK);

            Tags& sink_data_tags = setup_data_tags_[node_id];
            for(const Tag* clk_tag : src_clock_tags) {
                //Determine the new data tag based on the arriving clock tag
                Time new_arr = clk_tag->arr_time() + edge_delay;
                for(auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
                    Tag launch_data_tag = Tag(new_arr, Time(NAN), clk_tag->clock_domain(), node_id, trans);
                    sink_data_tags.max_arr(&launch_data_tag);
                }
            }
        } else if(tg.node_type(node_id) == TN_Type::FF_SINK) {
            //TODO: annotate required time
        } else {
            //Standard clock tag propogation
            const Time& edge_delay = dc.max_edge_delay(tg, edge_id, TransitionType::CLOCK, TransitionType::CLOCK);
            Tags& sink_clock_tags = setup_clock_tags_[node_id];

            for(const Tag* clk_tag : src_clock_tags) {
                //Determine the new data tag based on the arriving clock tag
                Time new_arr = clk_tag->arr_time() + edge_delay;
                Tag new_clk_tag = Tag(new_arr, Time(NAN), *clk_tag);
                sink_clock_tags.max_arr(&new_clk_tag);
            }
        }
    }
    if(src_data_tag_sets.size() > 0) {

        //The output tag set
        Tags& sink_tags = setup_data_tags_[node_id];

        //Generate all tag transition permutations
        //TODO: use a generator rather than pre-compute
        std::vector<std::vector<const Tag*>> src_tag_perms = gen_tag_permutations(src_data_tag_sets);

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
                std::cout << tag->trans_type();
                std::cout << ": " << tag->arr_time().value();
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
            scenario_tag.set_clock_domain(0); //Currently only single-clock supported

            assert((int) src_tags.size() <= tg.num_node_in_edges(node_id)); //May be less than if we are ignoring non-data edges like those from FF_CLOCK to FF_SINK

            std::vector<const ExtTimingTag*> input_tags;

            const auto filtered_tags = identify_filtered_tags(src_tags, node_func);

            //Take the worst-case arrival and delay
            for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
                EdgeId edge_id = tg.node_in_edge(node_id, edge_idx);
                NodeId src_node_id = tg.edge_src_node(edge_id);
                if(tg.node_type(src_node_id) == TN_Type::FF_CLOCK) {
                    continue; //We skip edges from FF_CLOCK since they never carry data arrivals
                }

                const Tag* src_tag = src_tags[edge_idx];


                input_tags.push_back(src_tag);

                Time edge_delay;
                if(filtered_tags.count(src_tag)) {
#ifdef TAG_DEBUG
                    std::cout << "\t\tFiltered: input " << edge_idx << std::endl;
#endif
                    edge_delay = Time(0.); //This tag doesn't effect output timing 
                } else {
                    edge_delay = dc.max_edge_delay(tg, edge_id, src_tag->trans_type(), output_transition);
                }

                Time new_arr = src_tag->arr_time() + edge_delay;

                scenario_tag.max_arr(new_arr, src_tag);

                assert(scenario_tag.trans_type() == output_transition);
            }
            scenario_tag.add_input_tags(input_tags);
            
            //Now we need to merge the scenario into the output tags
            sink_tags.max_arr(&scenario_tag); 

#ifdef TAG_DEBUG
            std::cout << "\t\toutput: " << output_transition << "\n";
            /*std::cout << "\t\tScenario Func: " << scenario_switch_func << " #SAT: " << scenario_switch_func.CountMinterm(2*tg.primary_inputs().size()) << "\n";*/
            auto pred = [output_transition](const Tag* tag) {
                return tag->trans_type() == output_transition;
            };
            auto iter = std::find_if(sink_tags.begin(), sink_tags.end(), pred);
            assert(iter != sink_tags.end());
            /*std::cout << "\t\tSink " << iter->trans_type();*/
            std::cout << "\t\t" << **iter;
            /*std::cout << " Func: " << iter->switch_func();*/
            std::cout << "\n";
            /*std::cout << " #SAT: " << iter->switch_func().CountMinterm(2*tg.primary_inputs().size()) << "\n";*/
#endif
        }
    }
}

template<class BaseAnalysisMode, class Tags>
std::unordered_set<const typename Tags::Tag*> ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::identify_filtered_tags(const std::vector<const Tag*>& input_tags, BDD node_func) {

    //Make a copy of the input tags and sort them by ascending arrival time
    std::vector<size_t> sorted_input_tag_idxs;;
    for(size_t i = 0; i < input_tags.size(); ++i) {
        sorted_input_tag_idxs.push_back(i);
    }
    assert(sorted_input_tag_idxs.size() == input_tags.size());


    auto sort_order = [&](size_t i, size_t j) {
        return input_tags[i]->arr_time() < input_tags[j]->arr_time();
    };
    std::sort(sorted_input_tag_idxs.begin(), sorted_input_tag_idxs.end(), sort_order);

    //Used to track those tags which are known to have arrived at the current
    //node.  We store the known transitions as they occur
    std::vector<TransitionType> input_transitions(input_tags.size(), TransitionType::UNKOWN);

    //Those tags which are filtered (i.e. have no effect on output timing)
    std::unordered_set<const Tag*> filtered_tags;
    
    //Walk through the tags in arrival order, determining if the current
    //input will be filtered based on the known stable inputs
    for(auto i : sorted_input_tag_idxs) {
    
        if(input_is_filtered(i, input_transitions, node_func)) {
            filtered_tags.insert(input_tags[i]);
        }

        //Keep track of the tags which have arrived (and could filter later arrivals)
        input_transitions[i] = input_tags[i]->trans_type();
    }

    return filtered_tags;
}

template<class BaseAnalysisMode, class Tags>
bool ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::input_is_filtered(size_t input_idx, const std::vector<TransitionType>& input_transitions, BDD f) {
    //To figure out if the arriving_tag is filtered we need to look at the
    //logic function (node_func) and the transitions of any previously arrived
    //tags (arrived_tags).
    //
    //A tag is filtered if it is dominated by some other (arrived) input to
    //the logic function.
    //
    //We can identify a dominated input by looking at the logic function's
    //positive and negative Shannon co-factors for that variable. 
    //If the co-factors are equivalent, then the input has no impact on the
    //logic function output and is said to be dominated by other inputs (i.e.
    //filtered from the output).
    //
    //We can check for equivalent co-factors using BDDs. Since BDDs are constructed 
    //using Shannon co-factors we have already calculated the co-factors, and can
    //check if they are equivalent by XORing the co-factors together.
    //
    //In otherwords:
    //   Let f1 and f0 be the positive and negative the cofactors of f for a variable i
    //
    //      Then i is dominated if f0 == f1

    assert(input_transitions[input_idx] == TransitionType::UNKOWN); //Don't yet know this variable

    //Evaluate the node function for all the known input parameters
    // by restricting each input variable based on its final value
    for(size_t i = 0; i < input_transitions.size(); ++i) {
        if(input_transitions[i] == TransitionType::UNKOWN) continue; //Input unspecified

        //We store the node functions using variables 0..num_inputs-1
        //So get the resulting variable
        BDD var = g_cudd.bddVar(i);

        //FALL/LOW transitions result in logically false values in the next cycle, so we need to invert
        //the variable
        if(input_transitions[i] == TransitionType::LOW || input_transitions[i] == TransitionType::FALL) {
            //Invert
            var = !var;
        }

        //Refine f with this variable restriction
        f = f.Restrict(var);
    }

    //Get the co-factors of the restricted function
    BDD input_var = g_cudd.bddVar(input_idx);
    BDD f0 = f.Restrict(!input_var);
    BDD f1 = f.Restrict(input_var);

    //Check if the input variable has any possible 
    //impact on the logic functions final value
    return f0 == f1;
}

template<class BaseAnalysisMode, class Tags>
std::vector<std::vector<const typename Tags::Tag*>> ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::gen_tag_permutations(const std::vector<Tags>& tags) {
    std::vector<std::vector<const Tag*>> tag_permuations;

    gen_tag_permutations_recurr(tags, 0, std::vector<const Tag*>(), tag_permuations);

    return tag_permuations;
}

template<class BaseAnalysisMode, class Tags>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::gen_tag_permutations_recurr(const std::vector<Tags>& tags, size_t var_idx, const std::vector<const Tag*>& partial_perm, std::vector<std::vector<const Tag*>>& tag_permutations) {
    
    for(const Tag* tag : tags[var_idx]) {
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
TransitionType ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::evaluate_transition(const std::vector<const Tag*>& input_tags_scenario, const BDD& node_func) {
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
            const Tag* tag = input_tags_scenario[var_idx];
            switch(tag->trans_type()) {
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

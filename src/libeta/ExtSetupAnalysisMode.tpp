#include <chrono>
#include <sstream>
#include "transition_eval.hpp"
#include "util.hpp"
#include "TagPermutationGenerator.hpp"
#include "transition_eval.hpp"

//Print out detailed information about tags during analysis
/*#define TAG_DEBUG*/

//Verify that the transition calculated incrementally during analysis matches
//the transition calculated from scratch
/*#define VERIFY_TRANSITION*/

const double PERMUTATION_WARNING_THRESHOLD = 10e6;

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

        //Determine if this is a static high, or static low constant generator
        BDD node_func = tg.node_func(node_id);
        TransitionType trans;
        if(node_func.IsOne()) {
            trans = TransitionType::HIGH;
        } else {
            assert(node_func.IsZero());
            trans = TransitionType::LOW;
        }
        assert(trans == TransitionType::HIGH || trans == TransitionType::LOW);

        Tag* constant_tag = new Tag(Time(0.), Time(NAN), tg.node_clock_domain(node_id), node_id, trans);
        setup_data_tags_[node_id].add_tag(constant_tag);

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
                Time arr_time = Time(0.);
                /*if(trans == TransitionType::HIGH || trans == TransitionType::LOW) {*/
                    /*arr_time = Time(-std::numeric_limits<Time::scalar_type>::infinity());*/
                /*}*/

                Tag* input_tag = new Tag(arr_time, Time(NAN), tg.node_clock_domain(node_id), node_id, trans);
                setup_data_tags_[node_id].add_tag(input_tag);
            }
        }
    }
}

template<class BaseAnalysisMode, class Tags>
template<class DelayCalcType>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::forward_traverse_finalize_node(const TimingGraph& tg, const TimingConstraints& tc, const DelayCalcType& dc, const NodeId node_id, double delay_bin_size, size_t max_output_tags) {
    //Chain to base class
    BaseAnalysisMode::forward_traverse_finalize_node(tg, tc, dc, node_id);

    //Walk through all the inputs handling clock tags and collecting data tags
    std::vector<Tags> src_data_tag_sets;
    for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
        EdgeId edge_id = tg.node_in_edge(node_id, edge_idx);

        NodeId src_node = tg.edge_src_node(edge_id);

        //Collect data tags
        const Tags& src_data_tags = setup_data_tags_[src_node];
        if(src_data_tags.num_tags() != 0) {
            //Save one set of tags for each input
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
                    sink_data_tags.max_arr(&launch_data_tag, delay_bin_size=0.); //Don't bin clock tags since there are few of them
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
                sink_clock_tags.max_arr(&new_clk_tag, delay_bin_size);
            }
        }
    }

    //Evaluate the input tags at this node
    if(src_data_tag_sets.size() > 0) {

        //The output tag set for this node
        Tags& sink_tags = setup_data_tags_[node_id];


        const BDD& node_func = tg.node_func(node_id);

#ifdef TAG_DEBUG
        std::cout << "Evaluating Node: " << node_id << " " << tg.node_type(node_id) << " (" << node_func << ")\n";

        //Print out the input tags to this node
        /*for(size_t i = 0; i < src_data_tag_sets.size(); ++i) {*/
            /*std::cout << "\tInput " << i << ": ";*/
                /*for(const auto tag : src_data_tag_sets[i]) {*/
                    /*std::cout << tag->trans_type() << "@" << tag->arr_time() << " "; */
                /*}*/
            /*std::cout << "\n";*/
        /*}*/
#endif

        size_t i_case = 0;

        //Generate all tag transition permutations
        TagPermutationGenerator tag_permutation_generator(src_data_tag_sets);
        if(tag_permutation_generator.num_permutations() > PERMUTATION_WARNING_THRESHOLD) {
            std::cout << "Warning: Node " << node_id << " will evaluate " << tag_permutation_generator.num_permutations() / 1e6 << "M tag permutations" << std::endl;
            //Print out the input tags to this node
            for(size_t i = 0; i < src_data_tag_sets.size(); ++i) {
                std::cout << "\tInput " << i << ": ";
                    for(const auto tag : src_data_tag_sets[i]) {
                        std::cout << tag->trans_type() << "@" << tag->arr_time() << " "; 
                    }
                std::cout << std::endl;
            }
        }
        while(!tag_permutation_generator.done()) {
            std::vector<const Tag*> src_tags = tag_permutation_generator.next();

#ifdef TAG_DEBUG
            std::cout << "\tCase " << i_case << "\n";
            std::cout << "\t\tinputs: ";
            for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
                const auto& tag = src_tags[edge_idx];
                std::cout << tag->trans_type();
                std::cout << "@" << tag->arr_time().value();
                std::cout << " ";
            }
            std::cout << "\n";
#endif
            //Sanity checks on incomming tags
            assert(src_tags.size() > 0);
            assert((int) src_tags.size() <= tg.num_node_in_edges(node_id)); //May be less than if we are ignoring non-data edges like those from FF_CLOCK to FF_SINK

            //Initialize the tag representing the behaviour for the current set of input transitions
            Tag scenario_tag;
            scenario_tag.set_clock_domain(0); //Currently only single-clock supported
            scenario_tag.set_arr_time(Time(0.)); //Set a default arrival to avoid nan

            //Keep a collection of the input tags (note that this may be different from the src tags
            //due to skipping clock tags and filtering inputs
            std::vector<const ExtTimingTag*> input_tags;

            //Collect up the edge indicies and associated tags
            std::vector<std::tuple<int,EdgeId, const Tag*>> edge_idx_id_tag_tuples;
            for(int edge_idx = 0; edge_idx < tg.num_node_in_edges(node_id); edge_idx++) {
                EdgeId edge_id = tg.node_in_edge(node_id, edge_idx);

                NodeId src_node_id = tg.edge_src_node(edge_id);
                if(tg.node_type(src_node_id) == TN_Type::FF_CLOCK) {
                    continue; //We skip edges from FF_CLOCK since they never carry data arrivals
                }

                const Tag* src_tag = src_tags[edge_idx];

                input_tags.push_back(src_tag); //We still need to track this input for #SAT calculation purposes

                edge_idx_id_tag_tuples.emplace_back(edge_idx, edge_id, src_tag);
            }

            //Sort the edges/tags by the associated input tag arrival times (asscending)
            auto order = [&src_tags] (std::tuple<int, EdgeId, const Tag*>& lhs_edge_id_tag_pair,
                                      std::tuple<int, EdgeId, const Tag*>& rhs_edge_id_tag_pair) {
                return std::get<2>(lhs_edge_id_tag_pair)->arr_time() < std::get<2>(rhs_edge_id_tag_pair)->arr_time();
            };
            std::sort(edge_idx_id_tag_tuples.begin(), edge_idx_id_tag_tuples.end(), order);

            //Evaluate all the inputs and determine if are filtered
            // Note that the previous sorting means this occurs in order of increase arrival time (so causality is preserved)
            BDD f = node_func;
            bool only_static_inputs_applied = true;
            std::vector<std::tuple<EdgeId, const Tag*>> unfiltered_inputs;
            for(auto& edge_idx_id_tag_tuple : edge_idx_id_tag_tuples) {
                //Check if the function has already been determined.
                //If it has we don't need to look at any more inputs
                if(f.IsOne() || f.IsZero()) {
                    break;
                }


                int edge_idx; //Used to the the correct BDD var to restrict
                EdgeId edge_id; //Used to get the edge delay
                const Tag* src_tag; //To retreive the transition and arrival time
                std::tie(edge_idx, edge_id, src_tag) = edge_idx_id_tag_tuple;


                //We now apply this inputs transition to restrict the logic function
                BDD f_new = apply_restriction(edge_idx, src_tag->trans_type(), f);

                //If the variable had no effect on the logic output we do not need to consider its
                //delay impact
                if(f_new == f) {
#ifdef TAG_DEBUG
                    std::cout << "\t\tFiltered: input " << edge_idx << std::endl;
#endif
                    continue; //No effect on output delay
                }

                assert(f_new != f);

                //The logic function changed when the current input was applied.
                //
                //We update the 'current' logic function at this node with the newly restricted one
                f = f_new;

                //And note this input so it will be used in delay calculation
                unfiltered_inputs.emplace_back(edge_id, src_tag);

                //Also Record whether any non-filtered inputs were dynamic transitions (i.e. Rise/Fall)
                //this impacts what the output transition is
                if(src_tag->trans_type() == TransitionType::RISE || src_tag->trans_type() == TransitionType::FALL) {
                    only_static_inputs_applied = false;
                }
            }

            //At this stage the logic function must have been fully determined
            assert(f.IsOne() || f.IsZero());

            //We now infer from the restricted logic function what the output transition from this node is
            //
            //If only static (i.e. High/Low) inputs were applied we generate a static High/Low output
            //otherwise we produced a dynamic transition (i.e. Rise/Fall)
            TransitionType output_transition = TransitionType::UNKOWN;
            if(f.IsOne()) {
                if(only_static_inputs_applied) {
                    output_transition = TransitionType::HIGH; 
                } else {
                    output_transition = TransitionType::RISE; 
                }

#ifdef VERIFY_TRANSITION
                //Sanity check that we get and equivalent transition if we evaluate the node function up front
                auto ref_output_transition = evaluate_output_transition(src_tags, node_func);
                assert(ref_output_transition == TransitionType::RISE || ref_output_transition == TransitionType::HIGH);
#endif
            } else {
                assert(f.IsZero());
                
                if(only_static_inputs_applied) {
                    output_transition = TransitionType::LOW; 
                } else {
                    output_transition = TransitionType::FALL; 
                }

#ifdef VERIFY_TRANSITION
                //Sanity check that we get and equivalent transition if we evaluate the node function up front
                auto ref_output_transition = evaluate_output_transition(src_tags, node_func);
                assert(ref_output_transition == TransitionType::FALL || ref_output_transition == TransitionType::LOW);
#endif
            }
            scenario_tag.set_trans_type(output_transition); //Note the actual transition

            //Now that we know what inputs are/are-not filtered compute the arrival time at this node
            // This is done by taking the worst-case arrival + edge_delay from all unfiltered inputs
            for(auto& unfiltered_input : unfiltered_inputs) {
                EdgeId edge_id;
                const Tag* src_tag;
                std::tie(edge_id, src_tag) = unfiltered_input;

                //And update the arrival time to reflect this change
                Time edge_delay = dc.max_edge_delay(tg, edge_id, src_tag->trans_type(), output_transition);

                Time new_arr = src_tag->arr_time() + edge_delay;
                assert(!isnan(new_arr.value()));

                scenario_tag.max_arr(new_arr, src_tag);
            }

            scenario_tag.add_input_tags(input_tags); //Save the input tags used to produce this tag
            
            //Now we need to merge the scenario into the set of output tags
            sink_tags.max_arr(&scenario_tag, delay_bin_size); 

#ifdef TAG_DEBUG
            std::cout << "\t\toutput: " << output_transition << "@" << scenario_tag.arr_time();
            if(only_static_inputs_applied) std::cout << " (staticly determined)";
            std::cout << "\n";
#endif
            assert(!isnan(scenario_tag.arr_time().value()));
            i_case++;
        }

        const double delay_bin_size_scale_fac = 1.2;

        //Reduce tags further if too many were created
        reduce_tags(node_id, sink_tags, max_output_tags, delay_bin_size, delay_bin_size_scale_fac);

#ifdef TAG_DEBUG
        //The output tags from this node
        std::cout << "\tOutput Tags:\n";
        int i_out_tag = 0;
        for(auto sink_tag : sink_tags) {
            std::cout << "\t\t" << sink_tag->trans_type() << "@" << sink_tag->arr_time().value() << " cases: " << sink_tag->input_tags().size() << std::endl;
            i_out_tag++;
        }
#endif
    }
}

template<class BaseAnalysisMode, class Tags>
BDD ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::apply_restriction(int var_idx, TransitionType input_trans, BDD f) {
    //We store the node functions using variables 0..num_inputs-1
    //Get the resulting BDD variable
    BDD var = g_cudd.bddVar(var_idx);

    //FALL/LOW transitions result in logically false values, so we need to invert
    //the raw variable (which is non-inverted)
    if(input_trans == TransitionType::LOW || input_trans == TransitionType::FALL) {
        //Invert
        var = !var;
    }

    //Refine f with this variable restriction
    return f.Restrict(var);
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
Time ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::map_to_delay_bin(Time delay, const double delay_bin_size) {
    if(delay_bin_size == 0.) {
        //No binning
        return delay;
    } else {
        //Bin the input delay by shifting it up to the edge of its current bin
        double delay_val = delay.value();

        double remainder = std::fmod(delay_val, delay_bin_size);

        //We take care to only increase the delay if the remainder is non-zero
        //
        //This avoids increasing the delay of values which are already at the edge of
        //a bin (and introducing unneccessarily pessimism)
        double binned_delay_val = delay_val;
        if(remainder != 0.) {
            binned_delay_val = delay_val + (delay_bin_size - remainder);
        }

        /*std::cout << "Delay: " << delay_val << " Binned delay: " << binned_delay_val << std::endl;*/

        assert(std::fmod(binned_delay_val, delay_bin_size) < 1e-15);

        return Time(binned_delay_val);
    }
}

template<class BaseAnalysisMode, class Tags>
void ExtSetupAnalysisMode<BaseAnalysisMode,Tags>::reduce_tags(NodeId node_id ,Tags& sink_tags, const size_t max_num_output_tags, double delay_bin_size, double delay_bin_size_scale_fac) {

    
    if(sink_tags.num_tags() > max_num_output_tags && max_num_output_tags > 0) {
        Tags reduced_tags;

        size_t iter_cnt = 0;
        do {
            reduced_tags = Tags();

            delay_bin_size *= delay_bin_size_scale_fac;

            for(ExtTimingTag* tag : sink_tags) {
                reduced_tags.max_arr(tag, delay_bin_size);
            }

            ++iter_cnt;
        } while(reduced_tags.num_tags() > max_num_output_tags);

        std::cout << "Reduced tags at node " << node_id << " " << sink_tags.num_tags() << " -> " << reduced_tags.num_tags() << "(in " << iter_cnt << " iterations)" << std::endl;
        
        //Relace the original tags with the newly reduced tags
        sink_tags = reduced_tags;
    }

}


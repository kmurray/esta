#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <cstdio>
#include <cmath>
#include <cstring>

#include "OptionParser.h"

#include "bdd.hpp"
#include "cuddInt.h"
//#include "util.h"
#include "cudd_hooks.hpp"

#include "util.hpp"

#include "blif_parse.hpp"

#include "BlifTimingGraphBuilder.hpp"
#include "sta_util.hpp"
#include "SerialTimingAnalyzer.hpp"
#include "PreCalcDelayCalculator.hpp"
#include "PreCalcTransDelayCalc.hpp"

#include "ExtTimingTags.hpp"
#include "ExtSetupAnalysisMode.hpp"
#include "sta_util.hpp"

#include "SharpSatEvaluator.hpp"
#include "SharpSatBddEvaluator.hpp"

#include "load_delay_model.hpp"
//#include "SharpSatDecompBddEvaluator.hpp"

#include "cell_characterize.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::to_string;

using AnalysisType = ExtSetupAnalysisMode<BaseAnalysisMode,ExtTimingTags>;
using DelayCalcType = PreCalcTransDelayCalculator;
using AnalyzerType = SerialTimingAnalyzer<AnalysisType,DelayCalcType>;
using SharpSatType = SharpSatBddEvaluator<AnalyzerType>;
//using SharpSatType = SharpSatDecompBddEvaluator<AnalyzerType>;

template class std::vector<const ExtTimingTag*>; //Debuging visiblitity

//XXX: global variable
//TODO: Clean up and pass appropriately....
BlifData* g_blif_data = nullptr;
ActionTimer g_action_timer;
EtaStats g_eta_stats;

optparse::Values parse_args(int argc, char** argv);
//void print_node_tags(const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, double nvars, double nassigns, float progress, bool print_sat_cnt, bool print_switch);
void print_node_tags(const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, int nvars, real_t nassigns, float progress, bool print_sat_cnt);
void print_node_histogram(const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, float progress);
void dump_exhaustive_csv(std::ostream& os, const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, int nvars);
std::vector<std::vector<int>> get_cubes(BDD f, int trans_var_start_idx);
std::vector<std::vector<int>> get_minterms(BDD f, int nvars);
std::vector<std::vector<int>> cube_to_minterms(std::vector<int> cube);
std::vector<std::vector<TransitionType>> get_transitions(BDD f, int nvars);

PreCalcTransDelayCalculator get_pre_calc_trans_delay_calculator(std::map<EdgeId,std::map<std::tuple<TransitionType,TransitionType>,Time>>& set_edge_delays, const TimingGraph& tg);

optparse::Values parse_args(int argc, char** argv) {
    auto parser = optparse::OptionParser()
        .description("Performs Extended Timing Analysis on a blif netlist.")
        ;

    parser.add_option("-b", "--blif")
          .dest("blif_file")
          .metavar("BLIF_FILE")
          .help("The blif file to load and analyze.")
          ;

    parser.add_option("-d", "--delay_model")
          .dest("delay_model_file")
          .metavar("DELAY_MODEL_FILE")
          .help("The delay model to be loaded (in JSON format).")
          ;

    parser.add_option("--csv_base")
          .dest("csv_base")
          .metavar("CSV_OUTPUT_FILE_BASE")
          .help("The base name for output csv files")
          ;

    parser.add_option("--csv_node")
          .dest("csv_node")
          .help("Node to write to CSV file")
          ;

    parser.add_option("--print_graph")
          .action("store_true")
          .set_default("false")
          .help("Print the timing graph. Default %default")
          ;


    parser.add_option("--characterize")
          .action("store_true")
          .set_default("false")
          .help("Characterize all models in the blif file and exit. Default %default")
          ;

    parser.add_option("--write_graph_dot")
          .action("store_true")
          .set_default("false")
          .help("Print the timing graph. Default %default")
          ;

    parser.add_option("--reorder_method")
          .dest("bdd_reorder_method")
          .metavar("CUDD_REORDER_METHOD")
          .set_default("CUDD_REORDER_SIFT")
          .help("The method to use for dynamic BDD variable re-ordering. Default: %default")
          ;
    parser.add_option("--sift_nswaps")
          .set_default("2000000")
          .help("Max number of variable swaps per reordering. Default %default")
          ;
    parser.add_option("--sift_nvars")
          .set_default("1000")
          .help("Max number of variables to be sifted per reordering. Default %default")
          ;
    parser.add_option("--sift_max_growth")
          .set_default("1.2")
          .help("Max growth in (intermediate) number of BDD nodes during sifting. Default %default")
          ;
    parser.add_option("--cudd_cache_ratio")
          .set_default("1.0")
          .help("Factor adjusting cache size relative to CUDD default. Default %default")
          ;
    parser.add_option("--xfunc_cache_nelem")
          .set_default("0")
          .help("Number of BDDs to cache while building switch functions. "
                "Note a value of 0 causes all xfuncs to be memoized (unbounded cache). "
                "A larger value prevents switch functions from being re-calculated, but "
                "also increases (perhaps exponentially) the size of the composite BDD CUDD must manage. "
                "This can causing a large amount of time to be spent re-ordering the BDD. "
                "Default %default")
          ;

    parser.add_option("--approx_threshold")
          .set_default("-1")
          .help("The number of BDD nodes beyond which BDDs are approximated. "
                "Negative thresholds ensure no approximation occurs. "
                "Default %default")
          ;
    parser.add_option("--approx_ratio")
          .set_default("0.5")
          .help("The desired reduction in number BDD nodes when BDDs are approximated. Default %default")
          ;
    parser.add_option("--approx_quality")
          .set_default("1.5")
          .help("The worst degredation in #SAT quality (0.5 would accept up to a 50% under approximation, "
                "1.0 only an exact approximation, and 1.5 a 50% over approximation). Default %default")
          ;

    std::vector<std::string> print_tags_choices = {"po", "pi", "all", "none"};
    parser.add_option("-p", "--print_tags")
          .dest("print_tags")
          .choices(print_tags_choices.begin(), print_tags_choices.end())
          .metavar("VALUE")
          .set_default("po")
          .help("What node tags to print. Must be one of {'po', 'pi', 'all', 'none'} (primary inputs, primary outputs, all nodes). Default: %default")
          ;

    parser.add_option("--print_tag_switch")
          .dest("print_tag_switch")
          .action("store_true")
          .set_default("false")
          .help("Print swithc function of each tag. Default %default")
          ;

    parser.add_option("--bdd_stats")
          .dest("show_bdd_stats")
          .action("store_true")
          .set_default("false")
          .help("Print out BDD package statistics. Default: %default")
          ;

    auto options = parser.parse_args(argc, argv);

    if(!options.is_set("blif_file")) {
        cout << "Missing required argument for blif file\n";
        cout << "\n";
        parser.print_help();
        std::exit(1);
    }

    if(!options.is_set("delay_model_file")) {
        cout << "Missing required argument for delay model file\n";
        cout << "\n";
        parser.print_help();
        std::exit(1);
    }

    return options;
}

int main(int argc, char** argv) {
    g_action_timer.push_timer("ETA Application");
    cout << "\n";

    auto options = parse_args(argc, argv);

    //Initialize CUDD
    g_cudd.AutodynEnable(options.get_as<Cudd_ReorderingType>("bdd_reorder_method"));
    //g_cudd.EnableReorderingReporting();
    //Cudd_EnableOrderingMonitoring(g_cudd.getManager());
    g_cudd.AddHook(PreReorderHook, CUDD_PRE_REORDERING_HOOK);
    g_cudd.AddHook(PostReorderHook, CUDD_POST_REORDERING_HOOK);
    //g_cudd.AddHook(PreGarbageCollectHook, CUDD_PRE_GC_HOOK);
    //g_cudd.AddHook(PostGarbageCollectHook, CUDD_POST_GC_HOOK);
    g_cudd.SetSiftMaxSwap(options.get_as<int>("sift_nswaps"));
    g_cudd.SetSiftMaxVar(options.get_as<int>("sift_nvars"));
    g_cudd.SetMaxGrowth(options.get_as<double>("sift_max_growth"));
    g_cudd.SetMaxCacheHard(options.get_as<double>("cudd_cache_ratio") * g_cudd.ReadMaxCacheHard());

    g_action_timer.push_timer("Load Delay Model");

    auto delay_model = load_delay_model(options.get_as<string>("delay_model_file"));

    g_action_timer.pop_timer("Load Delay Model");
    

    //Load the file
    g_action_timer.push_timer("Loading Blif");
    cout << "\tParsing file: " << options.get_as<string>("blif_file") << endl;

    //Create the parser
    BlifParser parser;
    try {
        g_blif_data = parser.parse(options.get_as<string>("blif_file"));
    } catch (BlifParseLocationError& e) {
        cerr << argv[1] << ":" << e.line_num << " " << e.what() << " (near text '" << e.near_text << "')" << endl; 
        return 1;
    } catch (BlifParseError& e) {
        cerr << argv[1] << ":" << e.what() << endl; 
        return 1;
    }

    assert(g_blif_data != nullptr);

    g_action_timer.pop_timer("Loading Blif");
    cout << "\n";

    //Create the builder
    BlifTimingGraphBuilder tg_builder(g_blif_data, delay_model);

    TimingGraph timing_graph;

    g_action_timer.push_timer("Building Timing Graph");

    tg_builder.build(timing_graph);

    auto set_edge_delays = tg_builder.specified_edge_delays(); 

    //const auto& lo_dep_stats = tg_builder.get_logical_output_dependancy_stats();

    g_action_timer.pop_timer("Building Timing Graph");
    cout << "\n";

    if(options.get_as<bool>("print_graph")) {
        cout << "\n";
        cout << "TimingGraph: " << "\n";
        print_timing_graph(timing_graph);

        cout << "\n";
        cout << "TimingGraph logic functions: " << "\n";
        for(NodeId id = 0; id < timing_graph.num_nodes(); id++) {
            cout << "Node " << id << ": " << timing_graph.node_func(id) << "\n";
        }

        cout << "\n";
        cout << "TimingGraph Levelization: " << "\n";
        print_levelization(timing_graph);
    }

    if(options.get_as<bool>("characterize")) {
        
        for(NodeId node_id = 0; node_id < timing_graph.num_nodes(); ++node_id) {
            if(timing_graph.node_type(node_id) == TN_Type::PRIMITIVE_OPIN) {
                BDD f = timing_graph.node_func(node_id);
                
                auto vals = identify_active_transition_arcs(f);
                for(size_t i = 0; i < vals.size(); ++i) {
                    for(auto trans_tuple : vals[i]) {
                        TransitionType input_trans = std::get<0>(trans_tuple);
                        TransitionType output_trans = std::get<1>(trans_tuple);

                        cout << "node: " << node_id << " input " << i << " " << input_trans << " -> " << output_trans << endl;
                    }
                }
            }
        }

        return 0;
    }


    if(options.get_as<bool>("write_graph_dot")) {
        std::ofstream outfile("timing_graph.dot");
        write_timing_graph_dot(outfile, timing_graph);
    }

    cout << "Timing Graph Nodes: " << timing_graph.num_nodes() << "\n";
    cout << "Timing Graph Num Logical Inputs: " << timing_graph.logical_inputs().size() << "\n";
    cout << "Timing Graph Num Levels: " << timing_graph.num_levels() << "\n";
    print_level_histogram(timing_graph, 10);
    cout << "\n";

    g_action_timer.push_timer("Building Delay Calculator");

    auto delay_calc = get_pre_calc_trans_delay_calculator(set_edge_delays, timing_graph);

    g_action_timer.pop_timer("Building Delay Calculator");

    //Initialize PIs with zero input delay
    TimingConstraints timing_constraints;
    for(NodeId id : timing_graph.primary_inputs()) {
        timing_constraints.add_input_constraint(id, 0.);
    }

    //The actual analyzer
    auto analyzer = std::make_shared<AnalyzerType>(timing_graph, timing_constraints, delay_calc);

    g_action_timer.push_timer("Analysis");

    analyzer->set_xfunc_cache_size(options.get_as<size_t>("xfunc_cache_nelem"));
    analyzer->calculate_timing();

    g_action_timer.pop_timer("Analysis");


    g_action_timer.push_timer("Output Results");


    bool print_sat_cnt = true;
    int nvars = 2*timing_graph.logical_inputs().size();
    real_t nassigns = pow(2,(real_t) nvars);
    cout << "Num Logical Inputs: " << timing_graph.logical_inputs().size() << " Num BDD Vars: " << nvars << " Num Possible Assignments: " << nassigns << endl;
    cout << endl;

    auto sharp_sat_eval = std::make_shared<SharpSatType>(timing_graph, analyzer, nvars, options.get_as<int>("approx_threshold"), options.get_as<float>("approx_ratio"), options.get_as<float>("approx_quality"));


#if 0
    g_action_timer.push_timer("Raw tags");

    for(LevelId level_id = 0; level_id < timing_graph.num_levels(); level_id++) {
        for(auto node_id : timing_graph.level(level_id)) {
            print_node_tags(timing_graph, analyzer, sharp_sat_eval, node_id, nvars, nassigns, 0 , print_sat_cnt);
        }
    }

    g_action_timer.pop_timer("Raw tags");
#endif

#if 1

    g_action_timer.push_timer("Raw tag histograms");

    for(LevelId level_id = 0; level_id < timing_graph.num_levels(); level_id++) {
        for(auto node_id : timing_graph.level(level_id)) {
            print_node_histogram(timing_graph, analyzer, sharp_sat_eval, node_id, 0);
        }
    }

    g_action_timer.pop_timer("Raw tag histograms");

#endif
#if 0
    size_t nodes_processed = 0;
    auto po_nodes = timing_graph.primary_outputs();
    for(auto node_id : po_nodes) {
        std::string action_name = "Node " + to_string(node_id) + " eval";
        g_action_timer.push_timer(action_name);
        float progress = (float) nodes_processed / po_nodes.size();
        print_node_tags(timing_graph, analyzer, sharp_sat_eval, node_id, nvars, nassigns, progress, print_sat_cnt);

        //Clear the cache - this should shrink the size of CUDDs bdds
        sharp_sat_eval->reset();

        nodes_processed++;

        g_action_timer.pop_timer(action_name);
    }
#endif


    if(options.is_set("csv_base")) {
        std::string csv_filename = options.get_as<std::string>("csv_base") + ".csv";


        if(options.is_set("csv_node")) {
            NodeId csv_dump_node_id = options.get_as<NodeId>("csv_node");

            g_action_timer.push_timer("Exhaustive CSV");

            std::ofstream csv_os(csv_filename);

            dump_exhaustive_csv(csv_os, timing_graph, analyzer, sharp_sat_eval, csv_dump_node_id, nvars);

            g_action_timer.pop_timer("Exhaustive CSV");

        }
    }

    


/*
 *    bool print_switch = options.get_as<bool>("print_tag_switch");
 *
 *    if(options.get_as<string>("print_tags") == "pi") {
 *        print_tags(timing_graph, analyzer, print_switch,
 *                    [] (TimingGraph& tg, NodeId node_id) {
 *                        return tg.num_node_in_edges(node_id) == 0; 
 *                    }
 *                );
 *    } else if(options.get_as<string>("print_tags") == "po") {
 *        print_tags(timing_graph, analyzer, print_switch,
 *                    [](TimingGraph& tg, NodeId node_id) {
 *                        return tg.num_node_out_edges(node_id) == 0; 
 *                    }
 *                );
 *    } else if(options.get_as<string>("print_tags") == "all") {
 *        print_tags(timing_graph, analyzer, print_switch,
 *                    [](TimingGraph& tg, NodeId node_id) {
 *                        return true; 
 *                    }
 *                );
 *    } else if(options.get_as<string>("print_tags") == "none") {
 *        //pass
 *    } else {
 *        assert(0);
 *    }
 */

    /*
     *std::cout << "\n";
     *std::cout << "Worst Arrival:\n";
     *ExtTimingTags worst_tags;
     *auto nvars = timing_graph.logical_inputs().size();
     *auto nassigns = pow(2,2*nvars); //4 types of transitions
     *for(auto node_id : timing_graph.primary_outputs()) {
     *    for(const auto& tag : analyzer->setup_data_tags(node_id)) {
     *        auto new_tag = ExtTimingTag(tag);
     *        new_tag.set_next(nullptr);
     *        worst_tags.max_arr(new_tag);
     *    }
     *}
     *for(const auto& tag : worst_tags) {
     *    double sat_cnt = tag.switch_func().CountMinterm(2*nvars);
     *    double switch_prob = sat_cnt / nassigns;
     *    cout << "\t" << tag;
     *    cout << ", #SAT: " << sat_cnt << " (" << switch_prob << ")\n";
     *}
     */

    cout << "\n";
    cout << "BDD Stats after analysis:\n";
    cout << "\tnvars: " << g_cudd.ReadSize() << "\n";
    cout << "\tnnodes: " << g_cudd.ReadNodeCount() << "\n";
    cout << "\tpeak_nnodes: " << g_cudd.ReadPeakNodeCount() << "\n";
    cout << "\tpeak_live_nnodes: " << Cudd_ReadPeakLiveNodeCount(g_cudd.getManager()) << "\n";
    cout << "\treorderings: " << g_cudd.ReadReorderings() << "\n";
    float reorder_time_sec = (float) g_cudd.ReadReorderingTime() / 1000;
    cout << "\treorder time (s): " << reorder_time_sec << " (" << reorder_time_sec / g_action_timer.elapsed("ETA Application") << " total)\n";
    cout << "\n";

    cout << "Switch Func Approx Stats:\n";
    cout << "\tApprox Attempts  : " << g_eta_stats.approx_attempts << "\n";
    cout << "\tApprox Accepted  : " << g_eta_stats.approx_accepted << " (" << (float) g_eta_stats.approx_accepted / g_eta_stats.approx_attempts << ")\n";
    cout << "\tApprox Time      : " << g_eta_stats.approx_time << " (" << g_eta_stats.approx_time / g_action_timer.elapsed("ETA Application") << " total)\n";
    cout << "\tApprox Eval Time : " << g_eta_stats.approx_eval_time << " (" << g_eta_stats.approx_eval_time / g_action_timer.elapsed("ETA Application") << " total)\n";
    cout << "\n";


    if(options.get_as<bool>("show_bdd_stats")) {
        cout << endl;
        g_cudd.info();
    }



    g_action_timer.pop_timer("Output Results");

    cout << endl;

    /*
     *for(int i = 0; i < g_cudd.ReadSize(); i++) {
     *    std::cout << g_cudd.getVariableName(i) << "\n";
     *}
     */

    delete g_blif_data;
    return 0;
}


void print_node_tags(const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, int nvars, real_t nassigns, float progress, bool print_sat_cnt) {

    cout << "Node: " << node_id << " " << tg.node_type(node_id) << " (" << progress*100 << "%)\n";
    cout << "   Clk Tags:\n";
    auto& clk_tags = analyzer->setup_clock_tags(node_id);
    if(clk_tags.num_tags() > 0) {
        for(auto& tag : clk_tags) {
            cout << "\t" << tag;
            cout << "\n";
        }
    }
    cout << "   Data Tags:\n";
    auto& data_tags = analyzer->setup_data_tags(node_id);
    if(data_tags.num_tags() > 0) {
        if(print_sat_cnt) {
            //Calculate sat counts
            real_t total_sat_cnt = 0;

            for(auto tag : data_tags) {
                auto sat_cnt_supp = sharp_sat_eval->count_sat(tag, node_id);

                //Adjust for any variables not included in the support
                auto sat_cnt = sat_cnt_supp.count;

                auto switch_prob = sat_cnt / real_t(nassigns);
                cout << "\t" << *tag << ", #SAT: " << sat_cnt << " (" << switch_prob << ")\n";

                total_sat_cnt += sat_cnt;
            }
            cout << "\tTotal #SAT: " << total_sat_cnt << "\n";
            cout << "\n";
            assert(total_sat_cnt == nassigns);
        }
    }
}

void print_node_histogram(const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, float progress) {

    cout << "Node: " << node_id << " " << tg.node_type(node_id) << " (" << progress*100 << "%)\n";

    auto& raw_data_tags = analyzer->setup_data_tags(node_id);

    //Sort the tags so they come out in order
    std::vector<ExtTimingTag*> sorted_data_tags(raw_data_tags.begin(), raw_data_tags.end());
    auto tag_sorter = [](const ExtTimingTag* lhs, const ExtTimingTag* rhs) {
        return lhs->arr_time().value() < rhs->arr_time().value();
    };
    std::sort(sorted_data_tags.begin(), sorted_data_tags.end(), tag_sorter);

    std::map<double,double> delay_prob_histo;
    for(auto tag : sorted_data_tags) {

        auto delay = tag->arr_time().value();
        auto switch_prob = sharp_sat_eval->count_sat_fraction(tag, node_id);

        delay_prob_histo[delay] += switch_prob;
    }

    cout << "\tDelay Prob\n";
    cout << "\t----- ----\n";
    for(auto kv : delay_prob_histo) {

        auto delay = kv.first;
        auto switch_prob = kv.second;

        std::cout << "\t" << std::setw(5) << delay << " " << switch_prob << "\n";
    }
}

void dump_exhaustive_csv(std::ostream& os, const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, int nvars) {
    auto& data_tags = analyzer->setup_data_tags(node_id);

    using TupleVal = std::tuple<std::vector<TransitionType>,TransitionType,double>;
    std::vector<TupleVal> exhaustive_values;

    for(auto tag : data_tags) {
        auto sat_cnt = sharp_sat_eval->count_sat(tag, node_id).count;

        auto bdd = sharp_sat_eval->build_bdd_xfunc(tag, node_id);

        auto input_transitions = get_transitions(bdd, nvars);

        assert(input_transitions.size() == sat_cnt);

        for(auto input_case : input_transitions) {
            assert(input_case.size() == (size_t) nvars / 2);
            auto tuple = std::make_tuple(input_case, tag->trans_type(), tag->arr_time().value());
            exhaustive_values.push_back(tuple);
        }
    }

    //Covered all exhaustive cases
    assert(exhaustive_values.size() == pow(2, nvars));

    //Radix-style sort on input transitions
    for(int i = (nvars / 2) - 1; i >= 0; --i) {
        auto sort_order = [&](const TupleVal& lhs, const TupleVal& rhs) {
            return std::get<0>(lhs)[i] < std::get<0>(rhs)[i]; 
        };
        std::stable_sort(exhaustive_values.begin(), exhaustive_values.end(), sort_order);
    }

    
    //CSV Header
    for(int i = g_cudd.ReadSize() - nvars; i < g_cudd.ReadSize(); i += 2) {
        os << g_cudd.getVariableName(i) << ",";
    }
    os << "n" << node_id << ",";
    os << "delay" << ",";
    os << "exact_prob" << ",";
    os << "measured_prob" << ",";
    os << "\n";

    //CSV Values
    for(auto tuple : exhaustive_values) {
        //Input transitions
        for(auto trans : std::get<0>(tuple)) {
            os << trans << ",";
        }
        //Output transition
        os << std::get<1>(tuple) << ",";
        //Delay
        os << std::get<2>(tuple) << ",";
        
        //Exact/measured probability
        double prob = 1. / pow(2, nvars);
        os.precision(std::numeric_limits<double>::digits10);
        os << prob << ",";
        os << prob;
        os << "\n";
    }
}

std::vector<std::vector<TransitionType>> get_transitions(BDD f, int nvars) {
    std::vector<std::vector<TransitionType>> transitions;
    auto minterms = get_minterms(f, nvars);

    for(auto minterm : minterms) {
        std::vector<TransitionType> input_transitions;

        //Expect pairs of variables
        assert(minterm.size() % 2 == 0);
        assert(minterm.size() == (size_t) nvars);

        for(size_t i = 0; i < minterm.size() - 1; i += 2) {
            auto prev = minterm[i];
            auto next = minterm[i+1];

            TransitionType trans;
            if(prev == 0 && next == 1) trans = TransitionType::RISE;
            else if(prev == 1 && next == 0) trans = TransitionType::FALL;
            else if(prev == 1 && next == 1) trans = TransitionType::HIGH;
            else if(prev == 0 && next == 0) trans = TransitionType::LOW;
            else assert(false);

            input_transitions.push_back(trans);
        }


        transitions.push_back(input_transitions);
    }

    return transitions;
}

std::vector<std::vector<int>> get_minterms(BDD f, int nvars) {
    //Determine the starting index of the transition vars
    //(rather than those used to store logic functions)

    //Get the cubes (which include don't cares)
    //for the function f
    auto cubes = get_cubes(f, nvars);

    //We want minterms, so we need to expand the cubes into minterms
    //(i.e. without don't cares)
    std::vector<std::vector<int>> minterms;
    for(auto cube : cubes) {

        int num_dc = 0;
        for(auto val : cube) {
            if(val == 2) num_dc++;
        }

        auto expanded_minterms = cube_to_minterms(cube); 

        auto expected_num_minterms = pow(2, num_dc);

        assert(expanded_minterms.size() == expected_num_minterms);
        
        std::copy(expanded_minterms.begin(), expanded_minterms.end(), std::back_inserter(minterms));
    }

    return minterms;
}

std::vector<std::vector<int>> cube_to_minterms(std::vector<int> cube) {
    std::vector<std::vector<int>> minterms;

    auto iter = std::find(cube.begin(), cube.end(), 2);
    if(iter == cube.end()) {
        minterms.push_back(cube);
        return minterms;
    } else {
        //Call with DC set to 0
        *iter = 0;
        auto minterms_0 = cube_to_minterms(cube); 
        auto back_insert_iter = std::copy(minterms_0.begin(), minterms_0.end(), std::back_inserter(minterms));

        //Call with DC set to 1
        *iter = 1;
        auto minterms_1 = cube_to_minterms(cube);
        std::copy(minterms_1.begin(), minterms_1.end(), back_insert_iter);
    }

    return minterms;
}

std::vector<std::vector<int>> get_cubes(BDD f, int nvars) {
    std::vector<std::vector<int>> cubes;

    int* cube_data;
    CUDD_VALUE_TYPE val;
    DdGen* gen;
    Cudd_ForeachCube(f.manager(), f.getNode(), gen, cube_data, val) {
        assert(val == 1); //Always holds for BDDs

        std::vector<int> cube;
        std::copy(cube_data + g_cudd.ReadSize() - nvars, cube_data + g_cudd.ReadSize(), std::back_inserter(cube));
        cubes.push_back(cube);
    }

    return cubes;
}

PreCalcTransDelayCalculator get_pre_calc_trans_delay_calculator(std::map<EdgeId,std::map<std::tuple<TransitionType,TransitionType>,Time>>& set_edge_delays, const TimingGraph& tg) {
    PreCalcTransDelayCalculator::EdgeDelayModel edge_delay_model(tg.num_edges());

    std::map<std::tuple<TransitionType,TransitionType>,Time> edge_zero_delays;
    for(auto in_trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
        for(auto out_trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
            edge_zero_delays[std::make_tuple(in_trans,out_trans)] = Time(0.);
        }
    }

    for(EdgeId i = 0; i < tg.num_edges(); ++i) {
        auto iter = set_edge_delays.find(i);
        if(iter != set_edge_delays.end()) {
            std::cout << "Setting non-zero delay on edge " << i << std::endl;
            edge_delay_model[i] = iter->second;
        } else {
            edge_delay_model[i] = edge_zero_delays;
        }
    }

    return PreCalcTransDelayCalculator(edge_delay_model);
}

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
#include "SharpSatDecompBddEvaluator.hpp"

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

//XXX: global variable
//TODO: Clean up and pass appropriately....
BlifData* g_blif_data = nullptr;
ActionTimer g_action_timer;
EtaStats g_eta_stats;

optparse::Values parse_args(int argc, char** argv);
//void print_node_tags(const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, double nvars, double nassigns, float progress, bool print_sat_cnt, bool print_switch);
void print_node_tags(const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, int nvars, real_t nassigns, float progress, bool print_sat_cnt);

optparse::Values parse_args(int argc, char** argv) {
    auto parser = optparse::OptionParser()
        .description("Performs Extended Timing Analysis on a blif netlist.")
        ;

    parser.add_option("-b", "--blif")
          .dest("blif_file")
          .metavar("BLIF_FILE")
          .help("The blif file to load and analyze.")
          ;

    parser.add_option("--print_graph")
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
          .help("Number of BDDs to cache while building switch functions. Note a value of 0 causes all xfuncs to be memoized (unbounded cache). A larger value prevents switch functions from being re-calculated, but also increases (perhaps exponentially) the size of the composite BDD CUDD must manage. This can causing a large amount of time to be spent re-ordering the BDD.  Default %default")
          ;

    parser.add_option("--approx_threshold")
          .set_default("-1")
          .help("The number of BDD nodes beyond which BDDs are approximated. Negative thresholds ensure no approximation occurs. Default %default")
          ;
    parser.add_option("--approx_ratio")
          .set_default("0.5")
          .help("The desired reduction in number BDD nodes when BDDs are approximated. Default %default")
          ;
    parser.add_option("--approx_quality")
          .set_default("1.5")
          .help("The worst degredation in #SAT quality (0.5 would accept up to a 50% under approximation, 1.0 only an exact approximation, and 1.5 a 50% over approximation). Default %default")
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


    //Load the file
    g_action_timer.push_timer("Loading Blif");
    cout << "\tParsing file: " << options.get_as<string>("blif_file") << endl;

    //Create the parser
    BlifParser parser;
    try {
        g_blif_data = parser.parse(options.get_as<string>("blif_file"));
    } catch (BlifParseError& e) {
        cerr << argv[1] << ":" << e.line_num << " " << e.what() << " (near text '" << e.near_text << "')" << endl; 
        return 1;
    }

    assert(g_blif_data != nullptr);

    g_action_timer.pop_timer("Loading Blif");
    cout << "\n";

    //Create the builder
    BlifTimingGraphBuilder tg_builder(g_blif_data);

    TimingGraph timing_graph;

    g_action_timer.push_timer("Building Timing Graph");

    tg_builder.build(timing_graph);
    const auto& lo_dep_stats = tg_builder.get_logical_output_dependancy_stats();

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

    cout << "Timing Graph Nodes: " << timing_graph.num_nodes() << "\n";
    cout << "Timing Graph Num Logical Inputs: " << timing_graph.logical_inputs().size() << "\n";
    cout << "Timing Graph Num Levels: " << timing_graph.num_levels() << "\n";
    print_level_histogram(timing_graph, 10);
    cout << "\n";
    

    //Initialize PIs with zero input delay
    TimingConstraints timing_constraints;
    for(NodeId id : timing_graph.primary_inputs()) {
        timing_constraints.add_input_constraint(id, 0.);
    }



    //The actual delay calculator
    std::map<TransitionType,std::vector<float>> delays;
    //Initialize all edge delays to zero
    for(auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW, TransitionType::CLOCK}) {
        delays[trans] = std::vector<float>(timing_graph.num_edges(), 0.0);
    }

    //Override primitive edges to have unit delay when switching
    for(EdgeId edge_id = 0; edge_id < timing_graph.num_edges(); edge_id++) {
        NodeId src = timing_graph.edge_src_node(edge_id);
        NodeId sink = timing_graph.edge_sink_node(edge_id);
        TN_Type src_type = timing_graph.node_type(src);
        TN_Type sink_type = timing_graph.node_type(sink);
        if(src_type == TN_Type::PRIMITIVE_IPIN && sink_type == TN_Type::PRIMITIVE_OPIN) {
            for(auto trans : {TransitionType::RISE, TransitionType::FALL}) {
                delays[trans][edge_id] = 1.0;
            }
        }
    }

    auto delay_calc = DelayCalcType(delays);

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

    //Try to keep outputs with common dependancies together
    //hopefully will maximize cache re-use
    std::vector<NodeId> sorted_lo_nodes;
    for(const auto& kv : lo_dep_stats) {
        const std::vector<NodeId>& nodes = kv.second;
        sorted_lo_nodes.insert(sorted_lo_nodes.end(), nodes.begin(), nodes.end());
    }
    std::reverse(sorted_lo_nodes.begin(), sorted_lo_nodes.end());

    auto sharp_sat_eval = std::make_shared<SharpSatType>(timing_graph, analyzer, nvars, options.get_as<int>("approx_threshold"), options.get_as<float>("approx_ratio"), options.get_as<float>("approx_quality"));
    size_t nodes_processed = 0;
    for(auto node_id : sorted_lo_nodes) {
        std::string action_name = "Node " + to_string(node_id) + " eval";
        g_action_timer.push_timer(action_name);
        float progress = (float) nodes_processed / sorted_lo_nodes.size();
        print_node_tags(timing_graph, analyzer, sharp_sat_eval, node_id, nvars, nassigns, progress, print_sat_cnt);

        //Clear the cache - this should shrink the size of CUDDs bdds
        sharp_sat_eval->reset();

        nodes_processed++;

        g_action_timer.pop_timer(action_name);
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

            std::vector<ExtTimingTag> tags;
            for(auto tag : data_tags) {
                tags.push_back(tag);
            }

            auto order = [](const ExtTimingTag& lhs, const ExtTimingTag& rhs) {
                return lhs.clock_domain() < rhs.clock_domain() && lhs.trans_type() < rhs.trans_type() && lhs.arr_time().value() < rhs.arr_time().value();
            };

            std::sort(tags.begin(), tags.end(), order);

            for(auto& tag : tags) {
                auto sat_cnt_supp = sharp_sat_eval->count_sat(tag, node_id);

                //Adjust for any variables not included in the support
                auto sat_cnt = sat_cnt_supp.count;

                auto switch_prob = sat_cnt / real_t(nassigns);
                cout << "\t" << tag << ", #SAT: " << sat_cnt << " (" << switch_prob << ")\n";

                total_sat_cnt += sat_cnt;
            }
            cout << "\n";
        }
    }
}


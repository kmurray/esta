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

#include "sdfparse.hpp"

#include "cell_characterize.hpp"

#include "TagReducer.hpp"

//Define to print out STA node arrival and required times
//#define STA_DUMP_ARR_REQ

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::to_string;

using DelayCalcType = PreCalcTransDelayCalculator;

//STA
using StaAnalysisType = SetupAnalysisMode<BaseAnalysisMode>;
using StaAnalyzerType = SerialTimingAnalyzer<StaAnalysisType,DelayCalcType>;

//ESTA
using EstaAnalysisType = ExtSetupAnalysisMode<BaseAnalysisMode,ExtTimingTags>;
using EstaAnalyzerType = SerialTimingAnalyzer<EstaAnalysisType,DelayCalcType>;
using SharpSatType = SharpSatBddEvaluator<EstaAnalyzerType>;

template class std::vector<ExtTimingTag::cptr>; //Debuging visiblitity

//XXX: global variable
//TODO: Clean up and pass appropriately....
BlifData* g_blif_data = nullptr;
ActionTimer g_action_timer;
EtaStats g_eta_stats;

optparse::Values parse_args(int argc, char** argv);
void print_node_tags(const TimingGraph& tg, std::shared_ptr<EstaAnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, size_t nvars, float progress);
void print_node_histogram(const TimingGraph& tg, std::shared_ptr<EstaAnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, std::shared_ptr<TimingGraphNameResolver> name_resolver, NodeId node_id, float progress);
void print_max_node_histogram(const TimingGraph& tg, std::shared_ptr<EstaAnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, const TagReducer& tag_reducer);
void dump_exhaustive_csv(std::ostream& os, const TimingGraph& tg, std::shared_ptr<EstaAnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, std::shared_ptr<TimingGraphNameResolver> name_resolver, NodeId node_id, size_t nvars);
void dump_max_exhaustive_csv(std::ostream& os, const TimingGraph& tg, std::shared_ptr<EstaAnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, std::shared_ptr<TimingGraphNameResolver> name_resolver, size_t nvars, const TagReducer& tag_reducer);
std::string print_tag_debug(ExtTimingTag::cptr tag, BDD f, size_t nvars);
std::vector<std::tuple<ExtTimingTag::cptr,std::shared_ptr<BDD>>> circuit_max_delays(const TimingGraph& tg, std::shared_ptr<EstaAnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, const TagReducer& tag_reducer, bool calculate_smallest_max_bdd=true);
std::vector<std::vector<int>> get_cubes(BDD f, size_t nvars);
std::vector<std::vector<int>> get_minterms(BDD f, size_t nvars);
std::vector<std::vector<int>> cube_to_minterms(std::vector<int> cube);
std::vector<std::vector<TransitionType>> get_transitions(BDD f, size_t nvars);

PreCalcTransDelayCalculator get_pre_calc_trans_delay_calculator(std::map<EdgeId,std::map<TransitionType,Time>>& set_edge_delays, const TimingGraph& tg);

std::vector<std::string> split(const std::string& str, char delim);

void write_timing_graph_and_delays_dot(std::ostream& os, const TimingGraph& tg, const PreCalcTransDelayCalculator& delay_calc);

optparse::Values parse_args(int argc, char** argv) {
    auto parser = optparse::OptionParser()
        .description("Performs Extended Timing Analysis on a blif netlist.")
        ;

    parser.add_option("-b", "--blif")
          .dest("blif_file")
          .metavar("BLIF_FILE")
          .help("The blif file to load and analyze.")
          ;

    parser.add_option("-s", "--sdf")
          .dest("sdf_file")
          .metavar("SDF_FILE")
          .help("The SDF file to be loaded.")
          ;

    parser.add_option("-d", "--delay_bin_size_coarse")
          .dest("delay_bin_size_coarse")
          .metavar("DELAY_BIN_SIZE")
          .set_default(0.)
          .help("The delay bin size to apply during analysis (below slack threshold).")
          ;

    parser.add_option("-m", "--max_permutations")
          .dest("max_permutations")
          .metavar("MAX_PERMUTATIONS")
          .set_default(0.)
          .help("The maximum number of permutations to be evaluated at a node in the timing graph during analysis. Zero implies no limit.")
          ;

    parser.add_option("--slack_threshold")
          .dest("slack_threshold_frac")
          .set_default(0.)
          .metavar("THRESHOLD")
          .help("The fraction of worst-case delay paths to analyze in detail (e.g. 0.05 means analyze only the 95th perctile paths in detail.")
          ;

    parser.add_option("-f", "--delay_bin_size_fine")
          .dest("delay_bin_size_fine")
          .metavar("DELAY_BIN_SIZE")
          .set_default(0.)
          .help("The delay bin size to apply during analysis (above slack threshold).")
          ;

    parser.add_option("--print_graph")
          .action("store_true")
          .set_default("false")
          .help("Print the timing graph. Default %default")
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

    parser.add_option("--max_histogram")
          .set_default(true)
          .help("Output the maximum delay histogram to console and esta.max_hist.csv")
          ;

    parser.add_option("--max_exhaustive")
          .set_default(false)
          .action("store_true")
          .help("Output the maximum delay exhaustively to esta.max_trans.csv")
          ;

    //Sets of possible node choices
    std::vector<std::string> node_choices = {"po", "pi", "all", "none"};

    parser.add_option("--print_histograms")
          .dest("print_histograms")
          .choices(node_choices.begin(), node_choices.end())
          .metavar("VALUE")
          .set_default("none")
          .help("What node delay histograms to print. Must be one of {'po', 'pi', 'all', 'none'} (primary inputs, primary outputs, all nodes). Default: %default")
          ;

    parser.add_option("--print_tags")
          .dest("print_tags")
          .choices(node_choices.begin(), node_choices.end())
          .metavar("VALUE")
          .set_default("none")
          .help("What node tags to print. Must be one of {'po', 'pi', 'all', 'none'} (primary inputs, primary outputs, all nodes). Default: %default")
          ;

    parser.add_option("--dump_exhaustive_csv")
          .help("Forces the tool to exhaustively dump all the transition scenarios to CSV file(s) for "
                "the specified nodes. Must be 'po', 'all', a comma sepearted list of node names.")
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

    std::cout << "sizeof(Time) = " << sizeof(Time) << "\n";
    std::cout << "sizeof(TimingTag) = " << sizeof(TimingTag) << "\n";
    std::cout << "sizeof(TimingTags) = " << sizeof(TimingTags) << "\n";
    std::cout << "sizeof(ExtTimingTag) = " << sizeof(ExtTimingTag) << "\n";
    std::cout << "sizeof(ExtTimingTags) = " << sizeof(ExtTimingTags) << "\n";
    std::cout << "sizeof(TransitionType) = " << sizeof(TransitionType) << "\n";
    std::cout << "sizeof(NodeId) = " << sizeof(NodeId) << "\n";
    std::cout << "sizeof(DomainId) = " << sizeof(DomainId) << "\n";
    std::cout << "sizeof(std::vector<std::vector<ExtTimingTag::cptr>>) = " << sizeof(std::vector<std::vector<ExtTimingTag::cptr>>) << "\n";
    std::cout << "sizeof(std::vector<int>) = " << sizeof(std::vector<int>) << "\n";

    auto options = parse_args(argc, argv);

    //Initialize CUDD
    g_cudd.AutodynEnable(options.get_as<Cudd_ReorderingType>("bdd_reorder_method"));
    //g_cudd.EnableReorderingReporting();
    //Cudd_EnableOrderingMonitoring(g_cudd.getManager());
    g_cudd.AddHook(PreReorderHook, CUDD_PRE_REORDERING_HOOK);
    g_cudd.AddHook(PostReorderHook, CUDD_POST_REORDERING_HOOK);
    //g_cudd.AddHook(PreGarbageCollectHook, CUDD_PRE_GC_HOOK);
    //g_cudd.AddHook(PostGarbageCollectHook, CUDD_POST_GC_HOOK);
    //g_cudd.SetSiftMaxSwap(options.get_as<int>("sift_nswaps"));
    //g_cudd.SetSiftMaxVar(options.get_as<int>("sift_nvars"));
    //g_cudd.SetMaxGrowth(options.get_as<double>("sift_max_growth"));
    //g_cudd.SetMaxCacheHard(options.get_as<double>("cudd_cache_ratio") * g_cudd.ReadMaxCacheHard());

    g_action_timer.push_timer("Load SDF");

    sdfparse::DelayFile sdf_data;
    if(options.is_set("sdf_file")) {
        sdfparse::Loader sdf_loader;

        if(!sdf_loader.load(options.get_as<string>("sdf_file"))) {
            return 1;
        }

        sdf_data = sdf_loader.get_delayfile();
    }

    g_action_timer.pop_timer("Load SDF");

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

    g_action_timer.push_timer("Building Timing Graph");

    //Create the builder
    BlifTimingGraphBuilder tg_builder(g_blif_data, sdf_data);

    TimingGraph timing_graph;

    tg_builder.build(timing_graph);

    auto set_edge_delays = tg_builder.specified_edge_delays(); 

    std::shared_ptr<TimingGraphNameResolver> name_resolver = tg_builder.get_name_resolver();

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
    cout << "Timing Graph Edges: " << timing_graph.num_edges() << "\n";
    cout << "Timing Graph Num Logical Inputs: " << timing_graph.logical_inputs().size() << "\n";
    cout << "Timing Graph Num Levels: " << timing_graph.num_levels() << "\n";
    print_level_histogram(timing_graph, 10);
    cout << "\n";

    g_action_timer.push_timer("Building Delay Calculator");

    auto delay_calc = get_pre_calc_trans_delay_calculator(set_edge_delays, timing_graph);

    g_action_timer.pop_timer("Building Delay Calculator");

    if(options.get_as<bool>("write_graph_dot")) {
        std::ofstream outfile("timing_graph.dot");
        write_timing_graph_and_delays_dot(outfile, timing_graph, delay_calc);
    }

    //Initialize PIs with zero input delay
    TimingConstraints timing_constraints;
    for(NodeId id : timing_graph.primary_inputs()) {
        timing_constraints.add_input_constraint(id, 0.);
    }
    for(NodeId id : timing_graph.primary_outputs()) {
        timing_constraints.add_output_constraint(id, 0.);
    }
    timing_constraints.add_setup_clock_constraint(0, 0, -1.);

    g_action_timer.push_timer("STA Analysis");

    auto noop_reducer = NoOpTagReducer();

    //The actual analyzer
    auto sta_analyzer = std::make_shared<StaAnalyzerType>(timing_graph, timing_constraints, delay_calc, noop_reducer, options.get_as<double>("max_permutations"));

    sta_analyzer->calculate_timing();

    g_action_timer.pop_timer("STA Analysis");

    g_action_timer.push_timer("STA Output");

    double sta_cpd = 0.;

    for(LevelId level_id = 0; level_id < timing_graph.num_levels(); ++level_id) {
        for(NodeId node_id : timing_graph.level(level_id)) {
#ifdef STA_DUMP_ARR_REQ
            std::cout << "\tNode: " << node_id << "\n"; 
#endif
            for(auto tag : sta_analyzer->setup_data_tags(node_id)) {
                double arr = tag.arr_time().value();
                sta_cpd = std::max(sta_cpd, arr);

#ifdef STA_DUMP_ARR_REQ
                double req = tag.req_time().value();
                double slack = req - arr;
                std::cout << "\t\tArr: " << arr << " Req: " << req << " Slack: " << slack << "\n"; 
#endif
            }
        }
    }
    std::cout << "STA CPD: " << sta_cpd << "\n";

    g_action_timer.pop_timer("STA Output");

    g_action_timer.push_timer("ESTA Analysis");

    double slack_threshold_frac = options.get_as<double>("slack_threshold_frac");
    double slack_threshold = slack_threshold_frac*sta_cpd;
    double coarse_delay_bin_size = options.get_as<double>("delay_bin_size_coarse");
    double fine_delay_bin_size = options.get_as<double>("delay_bin_size_fine");
    double max_permutations = options.get_as<double>("max_permutations");
    std::cout << "Slack Threshold : " << slack_threshold << " ps (" << slack_threshold_frac << ")" << "\n";
    std::cout << "Delay Bin Size Coarse (below threshold) : " << coarse_delay_bin_size << "\n";
    std::cout << "Delay Bin Size Fine (above threshold): " << fine_delay_bin_size << "\n";
    std::cout << "Max Permutations: " << max_permutations << "\n";
    auto tag_reducer = StaSlackTagReducer<StaAnalyzerType>(sta_analyzer, slack_threshold, coarse_delay_bin_size, fine_delay_bin_size);

    //The actual analyzer
    auto esta_analyzer = std::make_shared<EstaAnalyzerType>(timing_graph, timing_constraints, delay_calc, tag_reducer, max_permutations);


    esta_analyzer->set_xfunc_cache_size(options.get_as<size_t>("xfunc_cache_nelem"));
    esta_analyzer->calculate_timing();

    g_action_timer.pop_timer("ESTA Analysis");


    g_action_timer.push_timer("Output Results");


    size_t nvars = 0;
    for(NodeId node_id = 0; node_id <timing_graph.num_nodes(); node_id++) {
        if(timing_graph.node_type(node_id) == TN_Type::INPAD_SOURCE || timing_graph.node_type(node_id) == TN_Type::FF_SOURCE) {
            nvars += 2; //2 vars per input to encode 4 states
        }
    }
    float nassigns = pow(2., nvars);
    cout << "Num Logical Inputs: " << timing_graph.logical_inputs().size() << " Num BDD Vars: " << nvars << " Num Possible Assignments: " << nassigns << endl;
    cout << endl;

    auto sharp_sat_eval = std::make_shared<SharpSatType>(timing_graph, esta_analyzer, nvars);

    if(options.get_as<string>("print_tags") != "none") {
        g_action_timer.push_timer("Output tags");

        if(options.get_as<string>("print_tags") == "pi") {
            for(auto node_id : timing_graph.primary_inputs()) {
                print_node_tags(timing_graph, esta_analyzer, sharp_sat_eval, node_id, nvars, 0);
            }
        } else if(options.get_as<string>("print_tags") == "po") {
            for(auto node_id : timing_graph.primary_outputs()) {
                print_node_tags(timing_graph, esta_analyzer, sharp_sat_eval, node_id, nvars, 0);
            }
        } else if(options.get_as<string>("print_tags") == "all") {
            for(LevelId level_id = 0; level_id < timing_graph.num_levels(); level_id++) {
                for(auto node_id : timing_graph.level(level_id)) {
                    print_node_tags(timing_graph, esta_analyzer, sharp_sat_eval, node_id, nvars, 0);
                }
            }
        } else {
            assert(0);
        }
        g_action_timer.pop_timer("Output tags"); 
    }

    bool do_max_hist = options.get_as<bool>("max_histogram");

    if(do_max_hist) {
        print_max_node_histogram(timing_graph, esta_analyzer, sharp_sat_eval, tag_reducer);
    }

    if(options.get_as<string>("print_histograms") != "none") {
        g_action_timer.push_timer("Output tag histograms");

        float node_count = 0;
        if(options.get_as<string>("print_histograms") == "pi") {
            for(auto node_id : timing_graph.primary_inputs()) {
                print_node_histogram(timing_graph, esta_analyzer, sharp_sat_eval, name_resolver, node_id, node_count / timing_graph.primary_inputs().size());
                node_count += 1;
            }
        } else if(options.get_as<string>("print_histograms") == "po") {
            for(auto node_id : timing_graph.primary_outputs()) {
                print_node_histogram(timing_graph, esta_analyzer, sharp_sat_eval, name_resolver, node_id, node_count / timing_graph.primary_outputs().size());
                node_count += 1;
            }
        } else if(options.get_as<string>("print_histograms") == "all") {
            for(LevelId level_id = 0; level_id < timing_graph.num_levels(); level_id++) {
                for(auto node_id : timing_graph.level(level_id)) {
                    print_node_histogram(timing_graph, esta_analyzer, sharp_sat_eval, name_resolver, node_id, node_count / timing_graph.num_nodes());
                    node_count += 1;
                }
            }
        } else {
            assert(0);
        }
        g_action_timer.pop_timer("Output tag histograms"); 
    }

    bool do_max_exhaustive = options.get_as<bool>("max_exhaustive");

    if(do_max_exhaustive) {
        g_action_timer.push_timer("Exhaustive Max CSV");
        std::string csv_filename = "esta.max_trans.csv";
        std::ofstream csv_os(csv_filename);

        std::cout << "Writing " << csv_filename << " for circuit max delay\n";

        dump_max_exhaustive_csv(csv_os, timing_graph, esta_analyzer, sharp_sat_eval, name_resolver, nvars, tag_reducer);
        g_action_timer.pop_timer("Exhaustive Max CSV");
    }

    if(options.is_set("dump_exhaustive_csv")) {
        g_action_timer.push_timer("Exhaustive CSV");


        std::string node_spec = options.get_as<string>("dump_exhaustive_csv");
        std::vector<NodeId> nodes_to_dump;
        if(node_spec == "po") {
            for(NodeId id : timing_graph.primary_outputs()) {
                nodes_to_dump.push_back(id);
            }
        } else if(node_spec == "all") {
            for(NodeId id = 0; id < timing_graph.num_nodes(); ++id) {
                nodes_to_dump.push_back(id);
            }
        } else {
            auto names = split(node_spec, ',');

            //Naieve
            for(NodeId id = 0; id < timing_graph.num_nodes(); ++id) {
                for(auto name : names) {
                    if(name == name_resolver->get_node_name(id)) {
                        nodes_to_dump.push_back(id);
                    }
                }
            }
        }

        for(NodeId node_id : nodes_to_dump) {
            std::string node_name = name_resolver->get_node_name(node_id);


            std::string csv_filename = "esta.trans." + node_name + ".n" + std::to_string(node_id) + ".csv";
            std::ofstream csv_os(csv_filename);

            std::cout << "Writing " << csv_filename << " for node " << node_id << "\n";

            dump_exhaustive_csv(csv_os, timing_graph, esta_analyzer, sharp_sat_eval, name_resolver, node_id, nvars);
        }

        g_action_timer.pop_timer("Exhaustive CSV");
    }

    






    g_action_timer.pop_timer("Output Results");

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

    if(options.get_as<bool>("show_bdd_stats")) {
        cout << endl;
        g_cudd.info();
    }
    cout << endl;

    /*
     *for(int i = 0; i < g_cudd.ReadSize(); i++) {
     *    std::cout << g_cudd.getVariableName(i) << "\n";
     *}
     */

    delete g_blif_data;
    return 0;
}


void print_node_tags(const TimingGraph& tg, std::shared_ptr<EstaAnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, size_t nvars, float progress) {

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
        float total_sat_frac = 0.;
        for(auto tag : data_tags) {

            auto switch_prob = sharp_sat_eval->count_sat_fraction(tag);
            cout << "\t" << *tag << ", #SAT frac: " << switch_prob << "\n";

            total_sat_frac += switch_prob;
        }

        assert(total_sat_frac == 1.);
    }
}

void print_node_histogram(const TimingGraph& tg, std::shared_ptr<EstaAnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, std::shared_ptr<TimingGraphNameResolver> name_resolver, NodeId node_id, float progress) {
    g_action_timer.push_timer("Node " + std::to_string(node_id) + " histogram"); 

    std::string node_name;
    if(tg.node_type(node_id) == TN_Type::OUTPAD_SINK) {
        auto edge_id = tg.node_in_edge(node_id, 0);
        auto ipin_node_id = tg.edge_src_node(edge_id);
        node_name = name_resolver->get_node_name(ipin_node_id);
    } else {
        node_name = name_resolver->get_node_name(node_id);
    }


    cout << "Node: " << node_id << " (" << node_name << ") " << tg.node_type(node_id) << " (" << progress*100 << "%)\n";

    auto& raw_data_tags = analyzer->setup_data_tags(node_id);

    //Sort the tags so they come out in order
    std::vector<ExtTimingTag::cptr> sorted_data_tags(raw_data_tags.begin(), raw_data_tags.end());
    auto tag_sorter = [](ExtTimingTag::cptr lhs, ExtTimingTag::cptr rhs) {
        return lhs->arr_time().value() < rhs->arr_time().value();
    };
    std::sort(sorted_data_tags.begin(), sorted_data_tags.end(), tag_sorter);

    std::map<double,double> delay_prob_histo;
    for(auto tag : sorted_data_tags) {

        auto delay = tag->arr_time().value();
        auto switch_prob = sharp_sat_eval->count_sat_fraction(tag);

        delay_prob_histo[delay] += switch_prob;
    }

    double total_prob = 0.;

    //Print to stdou
    cout << "\tDelay Prob\n";
    cout << "\t----- ----\n";
    for(auto kv : delay_prob_histo) {

        auto delay = kv.first;
        auto switch_prob = kv.second;

        std::cout << "\t" << std::setw(5) << delay << " " << switch_prob << "\n";

        total_prob += switch_prob;
    }

    //Sanity check, total probability should be equal to 1.0 (accounting for FP round-off)
    double epsilon = 1e-9;
    assert(total_prob >= 1. - epsilon && total_prob <= 1. + epsilon);

    //Print to a csv
    std::string filename = "esta.hist." + node_name + ".n" + std::to_string(node_id) + ".csv";
    std::ofstream os(filename);

    //Header
    os << "delay,probability\n"; 
    //Rows
    for(auto kv : delay_prob_histo) {

        auto delay = kv.first;
        auto switch_prob = kv.second;

        os << delay << "," << switch_prob << "\n"; 

        total_prob += switch_prob;
    }

    sharp_sat_eval->reset();

    g_action_timer.pop_timer("Node " + std::to_string(node_id) + " histogram"); 
}

void print_max_node_histogram(const TimingGraph& tg, std::shared_ptr<EstaAnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, const TagReducer& tag_reducer) {
    g_action_timer.push_timer("Max histogram"); 

    auto max_delays = circuit_max_delays(tg, analyzer, sharp_sat_eval, tag_reducer, false);

    bool inferred_probability = false;
    std::map<double,double> delay_prob_histo;
    for(auto tag_bdd_tuple : max_delays) {
        assert(!inferred_probability); //Should only happen on the last iteration
        auto tag = std::get<0>(tag_bdd_tuple);
        auto bdd = std::get<1>(tag_bdd_tuple);
        
        if(bdd) {
            delay_prob_histo[tag->arr_time().value()] += CountMintermFraction(bdd->getNode());
        } else {
            //Only the last tag should have a 'null' bdd ptr implying we can infer
            //the probability

            //Sum up all the pre-calculated probabilities
            double other_prob = 0;
            for(auto kv : delay_prob_histo) {
                other_prob += kv.second;
            }

            delay_prob_histo[tag->arr_time().value()] += 1. - other_prob;

            inferred_probability = true; //We can only infer one probability
        }
    }

    //To ensure correct histogram drawing, we insert a zero delay probability if none
    //already exists
    if(delay_prob_histo.find(0.) == delay_prob_histo.end()) {
        delay_prob_histo[0.] = 0.;
    }

    double total_prob = 0.;

    //Print to stdou
    cout << "\tDelay Prob\n";
    cout << "\t----- ----\n";
    for(auto kv : delay_prob_histo) {

        auto delay = kv.first;
        auto switch_prob = kv.second;

        std::cout << "\t" << std::setw(5) << delay << " " << switch_prob << "\n";

        total_prob += switch_prob;
    }

    //Sanity check, total probability should be equal to 1.0 (accounting for FP round-off)
    double epsilon = 1e-9;
    assert(total_prob >= 1. - epsilon && total_prob <= 1. + epsilon);

    //Print to a csv
    std::string filename = "esta.max_hist.csv";
    std::ofstream os(filename);

    //Header
    os << "delay:MAX,probability\n"; 

    //Rows
    for(auto kv : delay_prob_histo) {

        auto delay = kv.first;
        auto switch_prob = kv.second;

        os << delay << "," << switch_prob << "\n"; 

        total_prob += switch_prob;
    }

    sharp_sat_eval->reset();

    g_action_timer.pop_timer("Max histogram"); 
}

void dump_exhaustive_csv(std::ostream& os, const TimingGraph& tg, std::shared_ptr<EstaAnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, std::shared_ptr<TimingGraphNameResolver> name_resolver, NodeId node_id, size_t nvars) {
    auto& data_tags = analyzer->setup_data_tags(node_id);

    using TupleVal = std::tuple<std::vector<TransitionType>,TransitionType,double>;
    std::vector<TupleVal> exhaustive_values;

    for(auto tag : data_tags) {
        auto bdd = sharp_sat_eval->build_bdd_xfunc(tag, node_id);

        auto input_transitions = get_transitions(bdd, nvars);

        for(auto input_case : input_transitions) {
            assert(input_case.size() == nvars / 2);
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
        auto var_name = g_cudd.getVariableName(i);
        NodeId pi_node_id;

        //Extract the node id
        std::stringstream(std::string(var_name.begin() + 1, var_name.end())) >> pi_node_id;

        //We use pi_node_id+1 since we name the input pin, rather than the source

        os << name_resolver->get_node_name(pi_node_id+1) << ":" << var_name << ",";
    }
    os << name_resolver->get_node_name(node_id) << ":n" << node_id << ",";
    os << "delay" << ",";
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
        
        os << "\n";
    }
}

std::string print_tag_debug(ExtTimingTag::cptr tag, BDD f, size_t nvars) {
    double sat_frac = CountMintermFraction(f.getNode());

    std::stringstream ss;
    ss << tag << " OutTrans: " << tag->trans_type() << " Arr: " << tag->arr_time().value();
    ss << " #SAT: " << sat_frac * pow(2, nvars) << " (" << sat_frac << ")";
    return ss.str();
}

//Returns a vector of tags and their associated BDD's representing the maximum delay of the circuit.
//  If calculate_smallest_max_bdd is false, then the smallest-delay tag will not have it's BDD 
//  calculated (a null shared_ptr is returned instead), and it is assumed that the caller will infer the
//  probability based on the probability of the other tags.
std::vector<std::tuple<ExtTimingTag::cptr,std::shared_ptr<BDD>>> circuit_max_delays(const TimingGraph& tg, 
        std::shared_ptr<EstaAnalyzerType> analyzer, 
        std::shared_ptr<SharpSatType> sharp_sat_eval, 
        const TagReducer& tag_reducer,
        bool calculate_smallest_max_bdd) {
    //We intially calculate the the maximum tags by iterating over all the Primary output tags,
    //then we sort the resulting max tags by delay and calculate the BDD for each tag.
    //We are careful to avoid double-counting transition cases accross multiple primary outputs by tracking
    //which minterms have already been covered
    //
    //We make several run-time optimizations:
    //
    //  1) We don't need to update covered_minterms on the last while-loop iteration, since it won't be
    //     used again -- since covered_minterms is a complex function the update could be expensive
    //
    //  2) Since the probability over all tags must be one, we can infer the probability of one tag
    //     as 1. - sum(all_other_tag_probs).  This is controlled by the calculate_smallest_max_bdd parameter,
    //     which when false causes a null bdd to be passed back for the last (lowest max delay) tag.
    //     The caller can then inferr the probability from the other tags.  This avoids calculating one
    //     tag's BDD.  If we have done a very coarse binning then the lowest delay tag may cover a large part
    //     of the output space and may save run-time by avoiding the calculation of such a BDD.

    ExtTimingTags max_tags;

    //Calculate the max tags
    for(NodeId po_node_id : tg.primary_outputs()) {
        const ExtTimingTags& node_tags = analyzer->setup_data_tags(po_node_id);

        //std::cout << "Max Input Tags (Node " << po_node_id << "):" << std::endl;
        for(const auto tag : node_tags) {
            //std::cout << "\t" << *tag << std::endl;
            auto new_tag = ExtTimingTag::make_ptr(*tag);
            new_tag->set_trans_type(TransitionType::MAX);
            max_tags.max_arr(new_tag);
        }
    }

    //Reduce the tags to simplify BDD calculation
    max_tags = tag_reducer.merge_max_tags(max_tags, tg.num_nodes());

    //When we calculate the max tags above, we may end up with the same set of input transitions
    //appearing multiple times in different tags (i.e. different delays to the primary outputs).
    //
    //Since we want to ensure we report *only* the maximum delay for a particular set of input transitions
    //we walk through the tags from highest to lowest delay and record which minterms (sets of input transitions)
    //have already been covered.  These covered minterms are then used to exclude any repeated minterms/cases 
    //with delay lower than the maximum.

    //Sort into descending order
    std::sort(max_tags.begin(), max_tags.end(),
                [](ExtTimingTag::cptr lhs, ExtTimingTag::cptr rhs) {
                    return lhs->arr_time().value() > rhs->arr_time().value();
                }
             );

    std::vector<std::tuple<ExtTimingTag::cptr,std::shared_ptr<BDD>>> max_delays;

    BDD covered_terms = g_cudd.bddZero();

    auto tag_end = (calculate_smallest_max_bdd) ? max_tags.end() : max_tags.end() - 1;
    auto tag_iter = max_tags.begin();
    while(tag_iter != tag_end) {
        auto bdd = sharp_sat_eval->build_bdd_xfunc(*tag_iter);

        //Remove any terms already covered
        bdd = bdd.And(!covered_terms);

        //Save the result
        max_delays.emplace_back(*tag_iter, std::make_shared<BDD>(bdd));

        //Update already covered terms
        // Note not needed after the last iteration,
        // so don't update since it could be very expensive
        if(tag_iter != tag_end - 1) {
            covered_terms |= bdd;
        }

        ++tag_iter;
    }
    if(tag_iter != max_tags.end()) {
        assert(!calculate_smallest_max_bdd);

        //We are not directly calculating the final tag
        //
        //We mark the last tag as null to inform the caller that they should
        //infer the probability as 1. - sum(other_tag_probabilities)
        max_delays.emplace_back(*tag_iter, std::shared_ptr<BDD>(nullptr));

        ++tag_iter;
    }

    assert(tag_iter == max_tags.end());

    return max_delays;
}

void dump_max_exhaustive_csv(std::ostream& os, const TimingGraph& tg, std::shared_ptr<EstaAnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, std::shared_ptr<TimingGraphNameResolver> name_resolver, size_t nvars, const TagReducer& tag_reducer) {
    ExtTimingTags max_tags;


    //Evaluate and them
    using TupleVal = std::tuple<std::vector<TransitionType>,TransitionType,double>;
    std::vector<TupleVal> exhaustive_values;

    auto max_delays = circuit_max_delays(tg, analyzer, sharp_sat_eval, tag_reducer, false);
    for(auto tag_bdd_tuple : max_delays) {
        auto tag = std::get<0>(tag_bdd_tuple);
        auto bdd = *(std::get<1>(tag_bdd_tuple));

        auto input_transitions = get_transitions(bdd, nvars);

        for(auto input_case : input_transitions) {
            assert(input_case.size() == nvars / 2);
            auto tuple = std::make_tuple(input_case, TransitionType::MAX, tag->arr_time().value());
            exhaustive_values.push_back(tuple);
        }
    }

    assert(exhaustive_values.size() == pow(2, nvars));

    //CSV Header
    for(int i = g_cudd.ReadSize() - nvars; i < g_cudd.ReadSize(); i += 2) {
        auto var_name = g_cudd.getVariableName(i);
        NodeId pi_node_id;

        //Extract the node id
        std::stringstream(std::string(var_name.begin() + 1, var_name.end())) >> pi_node_id;

        //We use pi_node_id+1 since we name the input pin, rather than the source

        os << name_resolver->get_node_name(pi_node_id+1) << ":" << var_name << ",";
    }
    os << "MAX" << ",";
    os << "delay" << ",";
    os << "\n";

    //CSV Values
    for(auto tuple : exhaustive_values) {
        //Input transitions
        for(auto trans : std::get<0>(tuple)) {
            os << trans << ",";
        }
        //Output transition
        //os << std::get<1>(tuple) << ",";
        os << "-" << ",";
        //Delay
        os << std::get<2>(tuple) << ",";
        
        os << "\n";
    }
}

std::vector<std::vector<TransitionType>> get_transitions(BDD f, size_t nvars) {
    std::vector<std::vector<TransitionType>> transitions;
    auto minterms = get_minterms(f, nvars);

    for(auto minterm : minterms) {
        std::vector<TransitionType> input_transitions;

        //Expect pairs of variables
        assert(minterm.size() % 2 == 0);
        assert(minterm.size() == nvars);

        for(size_t i = 0; i < minterm.size() - 1; i += 2) {
            //Walk through pairs of variables
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

std::vector<std::vector<int>> get_minterms(BDD f, size_t nvars) {
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

    auto iter = std::find(cube.begin(), cube.end(), 2); //CUDD returns DC's as integer value 2
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

std::vector<std::vector<int>> get_cubes(BDD f, size_t nvars) {
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

PreCalcTransDelayCalculator get_pre_calc_trans_delay_calculator(std::map<EdgeId,std::map<TransitionType,Time>>& set_edge_delays, const TimingGraph& tg) {
    PreCalcTransDelayCalculator::EdgeDelayModel edge_delay_model(tg.num_edges());

    std::map<TransitionType,Time> edge_zero_delays;
    for(auto out_trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
        edge_zero_delays[out_trans] = Time(0.);
    }

    for(EdgeId i = 0; i < tg.num_edges(); ++i) {
        auto iter = set_edge_delays.find(i);
        if(iter != set_edge_delays.end()) {
            //std::cout << "Setting non-zero delay on edge " << i << std::endl;
            edge_delay_model[i] = iter->second;
        } else {
            edge_delay_model[i] = edge_zero_delays;
        }
    }

    return PreCalcTransDelayCalculator(edge_delay_model);
}

std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> elements;
    std::stringstream ss(str);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elements.push_back(item);
    }
    return elements;
}

void write_timing_graph_and_delays_dot(std::ostream& os, const TimingGraph& tg, const PreCalcTransDelayCalculator& delay_calc) {
    //Write out a dot file of the timing graph
    os << "digraph G {" <<std::endl;
    //os << "\tnode[shape=record]" << std::endl;

    for(int inode = 0; inode < tg.num_nodes(); inode++) {
        os << "\tnode" << inode;
        os << "[label=\"";
        os << "n" << inode;
        os << "\\n" << tg.node_type(inode);
        os << "\"";

        os << " fixedsize=false margin=0";

        switch(tg.node_type(inode)) {
            case TN_Type::INPAD_SOURCE: //Fallthrough
            case TN_Type::FF_SOURCE:
                os << " " << "shape=invhouse";
                break;
            case TN_Type::FF_SINK: //Fallthrough
            case TN_Type::OUTPAD_SINK:
                os << " " << "shape=house";
                break;
            case TN_Type::PRIMITIVE_OPIN:
                os << " " << "shape=invtrapezium";
                break;
            default:
                //Pass
                break;
        }

        os << "]";
        os <<std::endl;
    }

    //Force drawing to be levelized
    for(int ilevel = 0; ilevel < tg.num_levels(); ilevel++) {
        os << "\t{rank = same;";

        for(NodeId node_id : tg.level(ilevel)) {
            os << " node" << node_id <<";";
        }
        os << "}" <<std::endl;
    }

    for(int ilevel = 0; ilevel < tg.num_levels(); ilevel++) {
        for(NodeId node_id : tg.level(ilevel)) {
            for(int edge_idx = 0; edge_idx < tg.num_node_out_edges(node_id); edge_idx++) {
                EdgeId edge_id = tg.node_out_edge(node_id, edge_idx);

                NodeId sink_node_id = tg.edge_sink_node(edge_id);

                os << "\tnode" << node_id << " -> node" << sink_node_id;
                os << "[label=\"";
                for(auto in_trans : {TransitionType::HIGH}) {
                    for(auto out_trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
                        Time edge_delay = delay_calc.max_edge_delay(tg, edge_id, in_trans, out_trans);
                        if(edge_delay.value() != 0.) {
                            os << out_trans << "@" << edge_delay << " ";
                        }
                    }
                }
                os << "\"]";
                os << ";" <<std::endl;
            }
        }
    }

    os << "}" <<std::endl;
}

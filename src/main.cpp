#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <cstdio>
#include <cmath>

#include "OptionParser.h"

#include "bdd.hpp"

#include "blif_parse.hpp"

#include "BlifTimingGraphBuilder.hpp"
#include "sta_util.hpp"
#include "SerialTimingAnalyzer.hpp"
#include "PreCalcDelayCalculator.hpp"
#include "PreCalcTransDelayCalc.hpp"

#include "ExtTimingTags.hpp"
#include "ExtSetupAnalysisMode.hpp"
#include "sta_util.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

//XXX: global variable
//TODO: Clean up and pass appropriately....
BlifData* g_blif_data = nullptr;

optparse::Values parse_args(int argc, char** argv);

optparse::Values parse_args(int argc, char** argv) {
    auto parser = optparse::OptionParser()
        .description("Performs Extended Timing Analysis on a blif netlist.")
        ;

    parser.add_option("-b", "--blif")
          .dest("blif_file")
          .metavar("BLIF_FILE")
          .help("The blif file to load and analyze.")
          ;

    parser.add_option("--reorder_method")
          .dest("bdd_reorder_method")
          .metavar("CUDD_REORDER_METHOD")
          .set_default("CUDD_REORDER_SIFT")
          .help("The method to use for dynamic BDD variable re-ordering. Default: %default")
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
    auto options = parse_args(argc, argv);

    /*
     *if(argc != 2) {
     *    std::cout << "Usage: " << argv[0] << " filename1.blif" << endl;
     *    return 1;
     *}
     */

    //Initialize the auto-reorder in CUDD
    g_cudd.AutodynEnable(options.get_as<Cudd_ReorderingType>("bdd_reorder_method"));

    cout << "Parsing file: " << options.get_as<string>("blif_file") << endl;

    //Create the parser
    BlifParser parser;

    //Load the file
    try {
        g_blif_data = parser.parse(options.get_as<string>("blif_file"));
    } catch (BlifParseError& e) {
        cerr << argv[1] << ":" << e.line_num << " " << e.what() << " (near text '" << e.near_text << "')" << endl; 
        return 1;
    }

    assert(g_blif_data != nullptr);

    cout << "\tOK" << endl;

    //Create the builder
    BlifTimingGraphBuilder tg_builder(g_blif_data);

    TimingGraph timing_graph;

    cout << "Building Timing Graph..." << "\n";
    tg_builder.build(timing_graph);

    cout << "Levelizing Timing Graph..." << "\n";
    timing_graph.levelize();

    /*
     *cout << "\n";
     *cout << "TimingGraph: " << "\n";
     *print_timing_graph(timing_graph);
     */

    cout << "\n";
    /*
     *cout << "TimingGraph logic functions: " << "\n";
     *for(NodeId id = 0; id < timing_graph.num_nodes(); id++) {
     *    cout << "Node " << id << ": " << timing_graph.node_func(id) << "\n";
     *}
     */

    cout << "\n";
    /*
     *cout << "TimingGraph Levelization: " << "\n";
     *print_levelization(timing_graph);
     */

    cout << "Level Histogram:\n";
    print_level_histogram(timing_graph, 10);
    cout << "\n";
    

    //Initialize PIs with zero input delay
    TimingConstraints timing_constraints;
    for(NodeId id : timing_graph.primary_inputs()) {
        timing_constraints.add_input_constraint(id, 0.);
    }

    using AnalysisType = ExtSetupAnalysisMode<BaseAnalysisMode,ExtTimingTags>;

    using DelayCalcType = PreCalcTransDelayCalculator;

    //The actual delay calculator
    std::map<TransitionType,std::vector<float>> delays;
    //Initialize all edge delays to zero
    for(auto trans : {TransitionType::RISE, TransitionType::FALL, TransitionType::HIGH, TransitionType::LOW}) {
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

    /*
     *delays[TransitionType::RISE] = std::vector<float>(timing_graph.num_edges(), 1.0);
     *delays[TransitionType::FALL] = std::vector<float>(timing_graph.num_edges(), 1.0);
     *delays[TransitionType::HIGH] = std::vector<float>(timing_graph.num_edges(), 0.0);
     *delays[TransitionType::LOW] = std::vector<float>(timing_graph.num_edges(), 0.0);
     */
    /*
     *delays[TransitionType::SWITCH] = std::vector<float>(timing_graph.num_edges(), 1.);
     *delays[TransitionType::STEADY] = std::vector<float>(timing_graph.num_edges(), 0.1);
     */

    auto delay_calc = DelayCalcType(delays);

    using AnalyzerType = SerialTimingAnalyzer<AnalysisType,DelayCalcType>;

    //The actual analyzer
    auto analyzer = std::make_shared<AnalyzerType>(timing_graph, timing_constraints, delay_calc);

    cout << "Analyzing...\n";
    analyzer->calculate_timing();

    //for(NodeId i = 0; i < timing_graph.num_nodes(); i++) {
    for(NodeId i : timing_graph.primary_outputs()) {
        cout << "Node: " << i << "\n";
        for(auto& tag : analyzer->setup_data_tags(i)) {
             //cout << "\t ArrTime: " << tag.arr_time().value() << "\n";
            double sat_cnt = tag.switch_func().CountMinterm(2*timing_graph.primary_inputs().size());
            cout << "\t" << tag << ", #SAT: " << sat_cnt << " (" << sat_cnt / pow(2, 2*timing_graph.primary_inputs().size()) << ")\n";
        }
    }

    delete g_blif_data;

    return 0;
}

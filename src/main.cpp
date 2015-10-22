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

using AnalysisType = ExtSetupAnalysisMode<BaseAnalysisMode,ExtTimingTags>;
using DelayCalcType = PreCalcTransDelayCalculator;
using AnalyzerType = SerialTimingAnalyzer<AnalysisType,DelayCalcType>;

//XXX: global variable
//TODO: Clean up and pass appropriately....
BlifData* g_blif_data = nullptr;

optparse::Values parse_args(int argc, char** argv);
void print_tags(TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, bool print_tag_switch, std::function<bool(TimingGraph&,NodeId)> node_pred);

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

    std::vector<std::string> print_tags_choices = {"po", "pi", "all"};
    parser.add_option("-p", "--print_tags")
          .dest("print_tags")
          .choices(print_tags_choices.begin(), print_tags_choices.end())
          .metavar("VALUE")
          .set_default("po")
          .help("What node tags to print. Must be one of {'po', 'pi', 'all'} (primary inputs, primary outputs, all nodes). Default: %default")
          ;

    parser.add_option("--print_tag_switch")
          .dest("print_tag_switch_fucn")
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

    /*
     *cout << "\n";
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

    cout << "Analyzing...\n";
    analyzer->calculate_timing();

    if(options.get_as<string>("print_tags") == "pi") {
        print_tags(timing_graph, analyzer, options.get_as<bool>("print_tag_switch_func"),
                    [] (TimingGraph& tg, NodeId node_id) {
                        return tg.num_node_in_edges(node_id) == 0; 
                    }
                );
    } else if(options.get_as<string>("print_tags") == "po") {
        print_tags(timing_graph, analyzer, options.get_as<bool>("print_tag_switch_func"),
                    [](TimingGraph& tg, NodeId node_id) {
                        return tg.num_node_out_edges(node_id) == 0; 
                    }
                );
    } else if(options.get_as<string>("print_tags") == "all") {
        print_tags(timing_graph, analyzer, options.get_as<bool>("print_tag_switch_func"),
                    [](TimingGraph& tg, NodeId node_id) {
                        return true; 
                    }
                );
    } else {
        assert(0);
    }

    delete g_blif_data;

    if(options.get_as<bool>("show_bdd_stats")) {
        cout << endl;
        g_cudd.info();
    }

    cout << endl;

    return 0;
}

void print_tags(TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, bool print_tag_switch, std::function<bool(TimingGraph&,NodeId)> node_pred) {
    auto nvars = tg.num_logical_inputs();
    auto nassigns = pow(2,2*nvars); //4 types of transitions
    const double epsilon = 1e-10;

    cout << "Num Vars: " << nvars << " Num Possible Assignments: " << nassigns << endl;

    for(int i = 0; i < tg.num_levels(); i++) {
        for(NodeId node_id : tg.level(i)) {
        //for(NodeId node_id : timing_graph.primary_outputs()) {
            if(node_pred(tg, node_id)) { //Primary output
                cout << "Node: " << node_id << " (" << tg.node_type(node_id) << ")\n";
                cout << "   Clk Tags:\n";
                auto& clk_tags = analyzer->setup_clock_tags(node_id);
                if(clk_tags.num_tags() > 0) {
                    for(auto& tag : clk_tags) {
                        cout << "\t" << tag;
                        if(print_tag_switch) {
                            cout << " " << tag.switch_func(); 
                        }
                        cout << "\n";
                    }
                }
                cout << "   Data Tags:\n";
                auto& data_tags = analyzer->setup_data_tags(node_id);
                if(data_tags.num_tags() > 0) {
                    double data_switch_prob_sum = 0;
                    for(auto& tag : data_tags) {
                         //cout << "\t ArrTime: " << tag.arr_time().value() << "\n";
                        double sat_cnt = tag.switch_func().CountMinterm(2*nvars);
                        double switch_prob = sat_cnt / nassigns;
                        data_switch_prob_sum += switch_prob;
                        cout << "\t" << tag;
                        cout << ", #SAT: " << sat_cnt << " (" << switch_prob << ")";
                        if(print_tag_switch) {
                            cout << ", xfunc: " << tag.switch_func();
                        }
                        cout << "\n";
                    }

                    assert(data_switch_prob_sum > 1.0 - epsilon);
                    assert(data_switch_prob_sum < 1.0 + epsilon);
                }
            }
        }
    }
}

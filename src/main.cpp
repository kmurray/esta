#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <cstdio>

#include "cudd.h"
#include "cuddObj.hh"

#include "blif_parse.hpp"

#include "BlifTimingGraphBuilder.hpp"
#include "sta_util.hpp"
#include "SerialTimingAnalyzer.hpp"
#include "PreCalcDelayCalculator.hpp"

#include "ExtTimingTags.hpp"
#include "ExtSetupAnalysisMode.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

//XXX: global variable
//TODO: Clean up and pass appropriately....
BlifData* g_blif_data = nullptr;
Cudd g_cudd;

int main(int argc, char** argv) {
    if(argc != 2) {
        std::cout << "Usage: " << argv[0] << " filename1.blif" << endl;
        return 1;
    }

    cout << "Parsing file: " << argv[1] << endl;

    //Create the parser
    BlifParser parser;

    //Load the file
    try {
        g_blif_data = parser.parse(argv[1]);
    } catch (BlifParseError& e) {
        cerr << argv[1] << ":" << e.line_num << " " << e.what() << " (near text '" << e.near_text << "')" << endl; 
        return 1;
    }

    if(g_blif_data != nullptr) {
        cout << "\tOK" << endl;

        //Create the builder
        BlifTimingGraphBuilder tg_builder(g_blif_data);

        TimingGraph timing_graph;

        cout << "Building Timing Graph..." << "\n";
        tg_builder.build(timing_graph);

        cout << "Levelizing Timing Graph..." << "\n";
        timing_graph.levelize();

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

        //Initialize PIs with zero input delay
        TimingConstraints timing_constraints;
        for(NodeId id : timing_graph.primary_inputs()) {
            timing_constraints.add_input_constraint(id, 0.);
        }

        using AnalysisType = ExtSetupAnalysisMode<BaseAnalysisMode,ExtTimingTags>;

        using DelayCalcType = PreCalcDelayCalculator;

        //The actual delay calculator
        auto delay_calc = PreCalcDelayCalculator(std::vector<float>(timing_graph.num_edges(), 1.));

        using AnalyzerType = SerialTimingAnalyzer<AnalysisType,DelayCalcType>;

        //The actual analyzer
        auto analyzer = std::make_shared<AnalyzerType>(timing_graph, timing_constraints, delay_calc);

        cout << "Analyzing...\n";
        analyzer->calculate_timing();

        for(NodeId i = 0; i < timing_graph.num_nodes(); i++) {
            cout << "Node: " << i << "\n";
            for(auto tag : analyzer->setup_data_tags(i)) {
                 //cout << "\t ArrTime: " << tag.arr_time().value() << "\n";
                 cout << "\t" << tag << "\n";
            }
        }

        delete g_blif_data;
    }

    return 0;
}


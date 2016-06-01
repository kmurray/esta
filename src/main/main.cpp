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
void print_node_tags(const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, size_t nvars, real_t nassigns, float progress, bool print_sat_cnt);
void print_node_histogram(const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, std::shared_ptr<TimingGraphNameResolver> name_resolver, NodeId node_id, float progress);
void dump_exhaustive_csv(std::ostream& os, const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, std::shared_ptr<TimingGraphNameResolver> name_resolver, NodeId node_id, size_t nvars);
std::vector<std::vector<int>> get_cubes(BDD f, size_t nvars);
std::vector<std::vector<int>> get_minterms(BDD f, size_t nvars);
std::vector<std::vector<int>> cube_to_minterms(std::vector<int> cube);
std::vector<std::vector<TransitionType>> get_transitions(BDD f, size_t nvars);

PreCalcTransDelayCalculator get_pre_calc_trans_delay_calculator(std::map<EdgeId,std::map<TransitionType,Time>>& set_edge_delays, const TimingGraph& tg);

std::vector<std::string> split(const std::string& str, char delim);

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

    parser.add_option("-d", "--delay_bin_size")
          .dest("delay_bin_size")
          .metavar("DELAY_BIN_SIZE")
          .help("The delay bin size to apply during analysis.")
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

    //Sets of possible node choices
    std::vector<std::string> node_choices = {"po", "pi", "all", "none"};

    parser.add_option("--print_histograms")
          .dest("print_histograms")
          .choices(node_choices.begin(), node_choices.end())
          .metavar("VALUE")
          .set_default("po")
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

    parser.add_option("--csv_base")
          .dest("csv_base")
          .set_default("esta")
          .metavar("CSV_OUTPUT_FILE_BASE")
          .help("The base name for output csv files. Default: %default")
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

    if(options.get_as<bool>("write_graph_dot")) {
        std::ofstream outfile("timing_graph.dot");
        write_timing_graph_dot(outfile, timing_graph);
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

    //Initialize PIs with zero input delay
    TimingConstraints timing_constraints;
    for(NodeId id : timing_graph.primary_inputs()) {
        timing_constraints.add_input_constraint(id, 0.);
    }

    //The actual analyzer
    auto analyzer = std::make_shared<AnalyzerType>(timing_graph, timing_constraints, delay_calc, options.get_as<double>("delay_bin_size"));

    g_action_timer.push_timer("Analysis");

    analyzer->set_xfunc_cache_size(options.get_as<size_t>("xfunc_cache_nelem"));
    analyzer->calculate_timing();

    g_action_timer.pop_timer("Analysis");


    g_action_timer.push_timer("Output Results");


    size_t nvars = 0;
    for(NodeId node_id = 0; node_id <timing_graph.num_nodes(); node_id++) {
        if(timing_graph.node_type(node_id) == TN_Type::INPAD_SOURCE || timing_graph.node_type(node_id) == TN_Type::FF_SOURCE) {
            nvars += 2; //2 vars per input to encode 4 states
        }
    }
    real_t nassigns = pow(2,(real_t) nvars);
    cout << "Num Logical Inputs: " << timing_graph.logical_inputs().size() << " Num BDD Vars: " << nvars << " Num Possible Assignments: " << nassigns << endl;
    cout << endl;

    auto sharp_sat_eval = std::make_shared<SharpSatType>(timing_graph, analyzer, nvars);

    if(options.get_as<string>("print_tags") != "none") {
        g_action_timer.push_timer("Output tags");

        if(options.get_as<string>("print_tags") == "pi") {
            for(auto node_id : timing_graph.primary_inputs()) {
                print_node_tags(timing_graph, analyzer, sharp_sat_eval, node_id, nvars, nassigns, 0, true);
            }
        } else if(options.get_as<string>("print_tags") == "po") {
            for(auto node_id : timing_graph.primary_outputs()) {
                print_node_tags(timing_graph, analyzer, sharp_sat_eval, node_id, nvars, nassigns, 0, true);
            }
        } else if(options.get_as<string>("print_tags") == "all") {
            for(LevelId level_id = 0; level_id < timing_graph.num_levels(); level_id++) {
                for(auto node_id : timing_graph.level(level_id)) {
                    print_node_tags(timing_graph, analyzer, sharp_sat_eval, node_id, nvars, nassigns, 0, true);
                }
            }
        } else {
            assert(0);
        }
        g_action_timer.pop_timer("Output tags"); 
    }

    //sharp_sat_eval = std::make_shared<SharpSatType>(timing_graph, analyzer, nvars);

    if(options.get_as<string>("print_histogram") != "none") {
        g_action_timer.push_timer("Output tag histograms");

        float node_count = 0;
        if(options.get_as<string>("print_histograms") == "pi") {
            for(auto node_id : timing_graph.primary_inputs()) {
                print_node_histogram(timing_graph, analyzer, sharp_sat_eval, name_resolver, node_id, node_count / timing_graph.primary_inputs().size());
                node_count += 1;
            }
        } else if(options.get_as<string>("print_histograms") == "po") {
            for(auto node_id : timing_graph.primary_outputs()) {
                print_node_histogram(timing_graph, analyzer, sharp_sat_eval, name_resolver, node_id, node_count / timing_graph.primary_outputs().size());
                node_count += 1;
            }
        } else if(options.get_as<string>("print_histograms") == "all") {
            for(LevelId level_id = 0; level_id < timing_graph.num_levels(); level_id++) {
                for(auto node_id : timing_graph.level(level_id)) {
                    print_node_histogram(timing_graph, analyzer, sharp_sat_eval, name_resolver, node_id, node_count / timing_graph.num_nodes());
                    node_count += 1;
                }
            }
        } else {
            assert(0);
        }
        g_action_timer.pop_timer("Output tag histograms"); 
    }

    if(options.is_set("dump_exhaustive_csv")) {
        g_action_timer.push_timer("Exhaustive CSV");

        std::vector<NodeId> nodes_to_dump;

        std::string node_spec = options.get_as<string>("dump_exhaustive_csv");
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


            std::string csv_filename = options.get_as<std::string>("csv_base") + "." + node_name + ".n" + std::to_string(node_id) + ".csv";
            std::ofstream csv_os(csv_filename);

            std::cout << "Writing " << csv_filename << " for node " << node_id << "\n";

            dump_exhaustive_csv(csv_os, timing_graph, analyzer, sharp_sat_eval, name_resolver, node_id, nvars);
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


void print_node_tags(const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, NodeId node_id, size_t nvars, real_t nassigns, float progress, bool print_sat_cnt) {

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

void print_node_histogram(const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, std::shared_ptr<TimingGraphNameResolver> name_resolver, NodeId node_id, float progress) {
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
    std::string filename = "esta.histogram." + node_name + ".n" + std::to_string(node_id) + ".csv";
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


}

void dump_exhaustive_csv(std::ostream& os, const TimingGraph& tg, std::shared_ptr<AnalyzerType> analyzer, std::shared_ptr<SharpSatType> sharp_sat_eval, std::shared_ptr<TimingGraphNameResolver> name_resolver, NodeId node_id, size_t nvars) {
    auto& data_tags = analyzer->setup_data_tags(node_id);

    using TupleVal = std::tuple<std::vector<TransitionType>,TransitionType,double>;
    std::vector<TupleVal> exhaustive_values;

    for(auto tag : data_tags) {
        auto sat_cnt = sharp_sat_eval->count_sat(tag, node_id).count;

        auto bdd = sharp_sat_eval->build_bdd_xfunc(tag, node_id);

        auto input_transitions = get_transitions(bdd, nvars);

        assert(input_transitions.size() == sat_cnt);

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

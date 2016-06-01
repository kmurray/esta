#include <memory>

#include "TransitionType.hpp"
#include "TimingGraphBuilder.hpp"
#include "Time.hpp"
#include "sdfparse.hpp"

#include "TimingGraphBlifNameResolver.hpp"

class BlifTimingGraphBuilder : public TimingGraphBuilder {
    public:
        BlifTimingGraphBuilder(BlifData* data, const sdfparse::DelayFile& sdf_data);
 

        void build(TimingGraph& tg);
        const std::unordered_map<const BlifPort*, NodeId>& get_port_to_node_lookup() { return port_to_node_lookup_; }
        const std::map<std::pair<size_t,size_t>,std::vector<NodeId>>& get_logical_output_dependancy_stats() { return logical_output_dependancy_stats_; }

        std::map<EdgeId,std::map<TransitionType,Time>> specified_edge_delays() { return edge_delays_; }


        std::shared_ptr<TimingGraphNameResolver> get_name_resolver();

    private:
        virtual void create_input(TimingGraph& tg, const BlifPort* input_port);
        virtual void create_output(TimingGraph& tg, const BlifPort* output_port);
        virtual void create_names(TimingGraph& tg, const BlifNames* names);
        virtual void create_latch(TimingGraph& tg, const BlifLatch* latch);
        virtual void create_subckt(TimingGraph& tg, const BlifSubckt* subckt);
        virtual void create_net_edges(TimingGraph& tg);

        virtual void identify_clock_drivers();
        virtual BDD create_func_from_names(const BlifNames* names, const std::vector<BDD>& input_vars);


        virtual void verify(const TimingGraph& tg);
        virtual void check_logical_input_dependancies(const TimingGraph& tg);
        virtual void check_logical_output_dependancies(const TimingGraph& tg);

        const BlifPort* find_subckt_port_from_model_port(const BlifSubckt* subckt, const BlifPort* model_input_port);

        std::string sdf_name(std::string name);
        void set_names_edge_delays_from_sdf(const TimingGraph& tg, const BlifNames* names, const NodeId output_node_id, BDD opin_node_func);
        void set_net_edge_delay_from_sdf(const TimingGraph& tg, const BlifPort* driver_port, const BlifPort* sink_port, const size_t sink_pin_idx, const NodeId output_node_id);
        void set_latch_edge_delays_from_sdf(const TimingGraph& tg, const BlifLatch* latch, EdgeId d_to_sink_edge_id, EdgeId src_to_q_edge_id);

        sdfparse::Cell find_sdf_interconnect(std::string driver_port_name, std::string sink_port_name);
        sdfparse::Cell find_sdf_cell_inst(std::string inst_name);
    private:
        const BlifData* blif_data_;
        const sdfparse::DelayFile sdf_data_;
        std::shared_ptr<TimingGraphBlifNameResolver> name_resolver_;

        std::map<EdgeId,std::map<TransitionType, Time>> edge_delays_;

        std::unordered_map<const BlifPort*,NodeId> port_to_node_lookup_;
        std::unordered_map<const BlifPort*,DomainId> clock_driver_to_domain_;
        std::map<std::pair<size_t,size_t>,std::vector<NodeId>> logical_output_dependancy_stats_;

        std::map<std::string,sdfparse::Cell> sdf_cells_by_inst_name_;
        std::map<std::tuple<std::string,std::string>,sdfparse::Cell> sdf_interconnect_cells_;
};

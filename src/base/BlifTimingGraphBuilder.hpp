#include "TimingGraphBuilder.hpp"
#include "load_delay_model.hpp"
#include "Time.hpp"

class BlifTimingGraphBuilder : public TimingGraphBuilder {
    public:
        BlifTimingGraphBuilder(BlifData* data, const DelayModel& delay_model)
            : blif_data_(data) 
            , delay_model_(delay_model)
            {}
 

        void build(TimingGraph& tg);
        const std::unordered_map<const BlifPort*, NodeId>& get_port_to_node_lookup() { return port_to_node_lookup_; }
        const std::map<std::pair<size_t,size_t>,std::vector<NodeId>>& get_logical_output_dependancy_stats() { return logical_output_dependancy_stats_; }

        std::map<EdgeId,std::map<std::tuple<TransitionType,TransitionType>,Time>> specified_edge_delays() { return edge_delays_; }

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

        std::map<std::tuple<TransitionType,TransitionType>,Time> find_edge_delays(BlifModel* model, BlifPort* input_port, BlifPort* output_port);

        BlifPort* get_real_port(BlifPort* port);

    private:
        const BlifData* blif_data_;
        const DelayModel delay_model_;

        std::map<EdgeId,std::map<std::tuple<TransitionType,TransitionType>, Time>> edge_delays_;

        std::unordered_map<const BlifPort*,NodeId> port_to_node_lookup_;
        std::unordered_map<NodeId,const BlifPort*> node_to_port_lookup_;
        std::unordered_map<const BlifPort*,DomainId> clock_driver_to_domain_;
        std::map<std::pair<size_t,size_t>,std::vector<NodeId>> logical_output_dependancy_stats_;
};

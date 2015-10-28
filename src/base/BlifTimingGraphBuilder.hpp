#include "TimingGraphBuilder.hpp"

class BlifTimingGraphBuilder : public TimingGraphBuilder {
    public:
        BlifTimingGraphBuilder(BlifData* data);

        void build(TimingGraph& tg);
        std::unordered_map<const BlifPort*, NodeId> get_port_to_node_lookup() { return port_to_node_lookup_; }

    protected:
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

    protected:
        const BlifData* blif_data_;
        std::unordered_map<const BlifPort*,NodeId> port_to_node_lookup_;
        std::unordered_map<NodeId,const BlifPort*> node_to_port_lookup_;
        std::unordered_map<const BlifPort*,DomainId> clock_driver_to_domain_;
};

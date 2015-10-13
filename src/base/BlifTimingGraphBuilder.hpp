#include "TimingGraphBuilder.hpp"

class BlifTimingGraphBuilder : public TimingGraphBuilder {
    public:
        BlifTimingGraphBuilder(BlifData* data);

        void build(TimingGraph& tg);
        std::unordered_map<const BlifPort*, NodeId> get_port_to_node_lookup() { return port_to_node_lookup; }

    protected:
        void create_input(TimingGraph& tg, const BlifPort* input_port);
        void create_output(TimingGraph& tg, const BlifPort* output_port);
        void create_names(TimingGraph& tg, const BlifNames* names);
        void create_latch(TimingGraph& tg, const BlifLatch* latch);
        void create_subckt(TimingGraph& tg, const BlifSubckt* subckt);
        void create_net_edges(TimingGraph& tg);

    protected:
        const BlifData* blif_data_;
        std::unordered_map<const BlifPort*,NodeId> port_to_node_lookup;
};

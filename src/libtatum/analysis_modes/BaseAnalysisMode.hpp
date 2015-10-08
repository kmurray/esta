#pragma once

#include "timing_graph_fwd.hpp"
#include "timing_constraints_fwd.hpp"
#include "memory_pool.hpp"

/**
 * Interface required by any class aiming to satisfy the analysis mode
 * concept for use as a mix-in class with a class in the TimingAnalyzer
 * hierarchy.
 *
 * This is a null implementation, allowing any derived classes to override
 * only those operations it needs to implement.
 *
 * \warning All sub-classes should take a template parameter which defaults to
 *          this class.  This allows composition of multiple traversal types.
 *
 * \warning All sub-classes which override one of these interface functions
 *          should explicitly chain (call) to the base class, to allow for
 *          composition.
 */
class BaseAnalysisMode {
    protected:
        ///Performs any initial setup for the traversal.
        ///\param tg The timing graph to be analyzed
        ///\remark TimingAnalyzers should call this before any traversals occur, and whenever the timing analyzer is reset.
        void initialize_traversal(const TimingGraph& tg) {}

        ///Operations performed on a node during a pre-traversal
        ///\param tag_pool The memory pool used to allocate TimingTag objects
        ///\param tg The timing graph to be analyzed
        ///\param tc The timing constraints to apply
        ///\param node_id The node to operate on
        template<class TagPoolType>
        void pre_traverse_node(TagPoolType& tag_pool, const TimingGraph& tg, const TimingConstraints& tc, const NodeId node_id) {}

        ///Operations performed whenever an edge is traversed in the forward direction
        ///\param tag_pool The memory pool used to allocate TimingTag objects
        ///\param tg The timing graph to be analyzed
        ///\param dc The delay calculator to use
        ///\param node_id The node to operate on
        ///\param edge_id The edge to operate on
        template<class TagPoolType, class DelayCalc>
        void forward_traverse_edge(TagPoolType& tag_pool, const TimingGraph& tg, const DelayCalc& dc, const NodeId node_id, const EdgeId edge_id) {}

        ///Operations performed once a nodes incoming edges (on a forward traversal) have been traversed
        ///\param tag_pool The memory pool used to allocate TimingTag objects
        ///\param tg The timing graph to be analyzed
        ///\param tc The timing constraints to apply
        ///\param node_id The node to operate on
        template<class TagPoolType>
        void forward_traverse_finalize_node(TagPoolType& tag_pool, const TimingGraph& tg, const TimingConstraints& tc, const NodeId node_id) {}

        ///Operations performed whenever an edge is traversed in the backward direction
        ///\param tg The timing graph to be analyzed
        ///\param dc The delay calculator to use
        ///\param node_id The node to operate on
        ///\param edge_id The edge to operate on
        template<class DelayCalc>
        void backward_traverse_edge(const TimingGraph& tg, const DelayCalc& dc, const NodeId node_id, const EdgeId edge_id) {}

        ///Operations performed once a nodes outgoing edges (on a backward traversal) have been traversed
        ///\param tag_pool The memory pool used to allocate TimingTag objects
        ///\param tg The timing graph to be analyzed
        ///\param tc The timing constraints to apply
        ///\param node_id The node to operate on
        template<class TagPoolType>
        void backward_traverse_finalize_node(TagPoolType& tag_pool, const TimingGraph& tg, const TimingConstraints& tc, const NodeId node_id) {}
};


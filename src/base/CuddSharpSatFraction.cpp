#include <limits>
#include <unordered_map>
#include <cassert>

#include "cudd.h"

#include "CuddSharpSatFraction.h"

typedef std::unordered_map<DdNode*,double> NodeTable;

double CountMintermFractionRecurr(DdNode* node, NodeTable& table);

double CountMintermFraction(DdNode* node) {
    NodeTable table;
    return CountMintermFractionRecurr(node, table);
}

double CountMintermFractionRecurr(DdNode* node, NodeTable& table) {
    double frac = std::numeric_limits<double>::quiet_NaN();

    auto iter = table.find(node);

    if(iter != table.end()) {
        //Use sub-problem from table
        frac = iter->second;

    } else {
        if(Cudd_IsConstant(node)) {
            assert(Cudd_V(node) == 1);

            //Base case (leaf node)
            if(Cudd_IsComplement(node)) {
                //At the false node
                frac = 0.;
            } else {
                assert(!Cudd_IsComplement(node));
                //At the true node
                frac = 1.;
            }
        } else {
            //Recursive case (internal node)
            assert(!Cudd_IsConstant(node));

            DdNode* then_node = (Cudd_IsComplement(node)) ? Cudd_Not(Cudd_T(node)) : Cudd_T(node);
            double frac_then = CountMintermFractionRecurr(then_node, table);

            DdNode* else_node = (Cudd_IsComplement(node)) ? Cudd_Not(Cudd_E(node)) : Cudd_E(node);
            double frac_else = CountMintermFractionRecurr(else_node, table);

            // Using identity:
            //   |f| = (|f0| + |f1|) / 2
            //
            // where |f| is the number of minterms
            // and f0 and f1 are the co-factors of f
            frac = (frac_then + frac_else) / 2.;

        }

        //Store sub-problem answer in table
        auto result = table.insert(std::make_pair(node, frac)); 
        assert(result.second); //Was inserted
    }
    return frac;
}


#include <iostream>
#include "bdd.hpp"

Cudd g_cudd;

using std::cout;

double sharpSat(const BDD& bdd, int nvars) {
    //double result = Cudd_CountPathsToNonZero(bdd.getNode());
    double result = Cudd_CountMinterm(bdd.manager(), bdd.getNode(), nvars);
    //checkReturnValue(result != (double) CUDD_OUT_OF_MEM);
    return result;
}

/*
 *void print_cubes(const BDD& f) {
 *    DdManager* manager = f.manager();
 *    DdNode* node = f.getNode();
 *    Ddgen* gen;
 *    int** cube;
 *    Cudd_ForeachCube(manager, node, gen, cube, 1) {
 *        
 *    }
 *}
 */

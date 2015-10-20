#include <cassert>
#include <iostream>
#include "bdd.hpp"

Cudd g_cudd;

using std::cout;

std::istream& operator>>(std::istream& is, Cudd_ReorderingType& type) {
    std::string s;
    is >> s;
    if     (s == "CUDD_REORDER_NONE")               type = CUDD_REORDER_NONE;
    else if(s == "CUDD_REORDER_SAME")               type = CUDD_REORDER_SAME;
    else if(s == "CUDD_REORDER_RANDOM")             type = CUDD_REORDER_RANDOM;
    else if(s == "CUDD_REORDER_RANDOM_PIVOT")       type = CUDD_REORDER_RANDOM_PIVOT;
    else if(s == "CUDD_REORDER_SIFT")               type = CUDD_REORDER_SIFT;
    else if(s == "CUDD_REORDER_SIFT_CONVERGE")      type = CUDD_REORDER_SIFT_CONVERGE;
    else if(s == "CUDD_REORDER_SYMM_SIFT")          type = CUDD_REORDER_SYMM_SIFT;
    else if(s == "CUDD_REORDER_SYMM_SIFT_CONV")     type = CUDD_REORDER_SYMM_SIFT_CONV;
    else if(s == "CUDD_REORDER_GROUP_SIFT")         type = CUDD_REORDER_GROUP_SIFT;
    else if(s == "CUDD_REORDER_GROUP_SIFT_CONV")    type = CUDD_REORDER_GROUP_SIFT_CONV;
    else if(s == "CUDD_REORDER_WINDOW2")            type = CUDD_REORDER_WINDOW2;
    else if(s == "CUDD_REORDER_WINDOW3")            type = CUDD_REORDER_WINDOW3;
    else if(s == "CUDD_REORDER_WINDOW4")            type = CUDD_REORDER_WINDOW4;
    else if(s == "CUDD_REORDER_WINDOW2_CONV")       type = CUDD_REORDER_WINDOW2_CONV;
    else if(s == "CUDD_REORDER_WINDOW3_CONV")       type = CUDD_REORDER_WINDOW3_CONV;
    else if(s == "CUDD_REORDER_WINDOW4")            type = CUDD_REORDER_WINDOW4_CONV;
    else if(s == "CUDD_REORDER_ANNEALING")          type = CUDD_REORDER_ANNEALING;
    else if(s == "CUDD_REORDER_GENETIC")            type = CUDD_REORDER_GENETIC;
    else if(s == "CUDD_REORDER_EXACT")              type = CUDD_REORDER_EXACT;
    else assert(0);
    return is;
}

std::ostream& operator<<(std::ostream& os, const Cudd_ReorderingType& type) {
    if     (type == CUDD_REORDER_NONE)              os << "CUDD_REORDER_NONE";
    else if(type == CUDD_REORDER_SAME)              os << "CUDD_REORDER_SAME";
    else if(type == CUDD_REORDER_RANDOM)            os << "CUDD_REORDER_RANDOM";
    else if(type == CUDD_REORDER_RANDOM_PIVOT)      os << "CUDD_REORDER_RANDOM_PIVOT";
    else if(type == CUDD_REORDER_SIFT)              os << "CUDD_REORDER_SIFT";
    else if(type == CUDD_REORDER_SIFT_CONVERGE)     os << "CUDD_REORDER_SIFT_CONVERGE";
    else if(type == CUDD_REORDER_SYMM_SIFT)         os << "CUDD_REORDER_SYMM_SIFT";
    else if(type == CUDD_REORDER_SYMM_SIFT_CONV)    os << "CUDD_REORDER_SYMM_SIFT_CONV";
    else if(type == CUDD_REORDER_GROUP_SIFT)        os << "CUDD_REORDER_GROUP_SIFT";
    else if(type == CUDD_REORDER_GROUP_SIFT_CONV)   os << "CUDD_REORDER_GROUP_SIFT_CONV";
    else if(type == CUDD_REORDER_WINDOW2)           os << "CUDD_REORDER_WINDOW2";
    else if(type == CUDD_REORDER_WINDOW3)           os << "CUDD_REORDER_WINDOW3";
    else if(type == CUDD_REORDER_WINDOW4)           os << "CUDD_REORDER_WINDOW4";
    else if(type == CUDD_REORDER_WINDOW2_CONV)      os << "CUDD_REORDER_WINDOW2_CONV";
    else if(type == CUDD_REORDER_WINDOW3_CONV)      os << "CUDD_REORDER_WINDOW3_CONV";
    else if(type == CUDD_REORDER_WINDOW4)           os << "CUDD_REORDER_WINDOW4_CONV";
    else if(type == CUDD_REORDER_ANNEALING)         os << "CUDD_REORDER_ANNEALING";
    else if(type == CUDD_REORDER_GENETIC)           os << "CUDD_REORDER_GENETIC";
    else if(type == CUDD_REORDER_EXACT)             os << "CUDD_REORDER_EXACT";
    else assert(0);
    return os;
}

double sharpSat(const BDD& bdd, const int nvars) {
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

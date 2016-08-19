#include <cassert>
#include <iostream>
#include <cmath>
#include "bdd.hpp"
#include "cuddInt.h"

Cudd g_cudd;

using std::cout;

//Forward declaration
class CubeNum;

BDD gen_cube(Cudd& cudd, size_t nvars, size_t cube_size, const CubeNum& cube_num);

int bdd_unique_node_count_recurr(DdNode* node, int depth);

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
    else if(s == "CUDD_REORDER_WINDOW2")            type = CUDD_REORDER_WINDOW2;
    else if(s == "CUDD_REORDER_WINDOW3")            type = CUDD_REORDER_WINDOW3;
    else if(s == "CUDD_REORDER_WINDOW4")            type = CUDD_REORDER_WINDOW4;
    else if(s == "CUDD_REORDER_WINDOW2_CONV")       type = CUDD_REORDER_WINDOW2_CONV;
    else if(s == "CUDD_REORDER_WINDOW3_CONV")       type = CUDD_REORDER_WINDOW3_CONV;
    else if(s == "CUDD_REORDER_WINDOW4_CONV")       type = CUDD_REORDER_WINDOW4_CONV;
    else if(s == "CUDD_REORDER_GROUP_SIFT")         type = CUDD_REORDER_GROUP_SIFT;
    else if(s == "CUDD_REORDER_GROUP_SIFT_CONV")    type = CUDD_REORDER_GROUP_SIFT_CONV;
    else if(s == "CUDD_REORDER_ANNEALING")          type = CUDD_REORDER_ANNEALING;
    else if(s == "CUDD_REORDER_GENETIC")            type = CUDD_REORDER_GENETIC;
    else if(s == "CUDD_REORDER_LINEAR")             type = CUDD_REORDER_LINEAR;
    else if(s == "CUDD_REORDER_LINEAR_CONVERGE")    type = CUDD_REORDER_LINEAR_CONVERGE;
    else if(s == "CUDD_REORDER_LAZY_SIFT")          type = CUDD_REORDER_LAZY_SIFT;
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
    else if(type == CUDD_REORDER_WINDOW2)           os << "CUDD_REORDER_WINDOW2";
    else if(type == CUDD_REORDER_WINDOW3)           os << "CUDD_REORDER_WINDOW3";
    else if(type == CUDD_REORDER_WINDOW4)           os << "CUDD_REORDER_WINDOW4";
    else if(type == CUDD_REORDER_WINDOW2_CONV)      os << "CUDD_REORDER_WINDOW2_CONV";
    else if(type == CUDD_REORDER_WINDOW3_CONV)      os << "CUDD_REORDER_WINDOW3_CONV";
    else if(type == CUDD_REORDER_WINDOW4)           os << "CUDD_REORDER_WINDOW4_CONV";
    else if(type == CUDD_REORDER_GROUP_SIFT)        os << "CUDD_REORDER_GROUP_SIFT";
    else if(type == CUDD_REORDER_GROUP_SIFT_CONV)   os << "CUDD_REORDER_GROUP_SIFT_CONV";
    else if(type == CUDD_REORDER_ANNEALING)         os << "CUDD_REORDER_ANNEALING";
    else if(type == CUDD_REORDER_GENETIC)           os << "CUDD_REORDER_GENETIC";
    else if(type == CUDD_REORDER_LINEAR)            os << "CUDD_REORDER_LINEAR";
    else if(type == CUDD_REORDER_LINEAR_CONVERGE)   os << "CUDD_REORDER_LINEAR_CONVERGE";
    else if(type == CUDD_REORDER_LAZY_SIFT)         os << "CUDD_REORDER_LAZY_SIFT";
    else if(type == CUDD_REORDER_EXACT)             os << "CUDD_REORDER_EXACT";
    else assert(0);
    return os;
}

double sharpSat(const BDD& bdd, const int nvars) {
    double result = Cudd_CountMinterm(bdd.manager(), bdd.getNode(), nvars);
    return result;
}

class CubeNum {
    public:
        CubeNum(size_t nvars, size_t shift): var_values(nvars+1, false) {
            //Note we store nvars+1 bits in var_values to allow
            //for creation of a 'one-passed-the-end' value with shift == nvars
            assert(shift < var_values.size());
            var_values[shift] = true; 
        }

        CubeNum& operator++() {
            bool carry = true;
            for(size_t i = 0; i < var_values.size(); i++) {
                if(!var_values[i] && carry) {
                    var_values[i] = true;
                    carry = false;
                } else if (var_values[i] && carry) {
                    carry = true;
                }
                if(!carry) {
                    break;
                }
            }
            return *this;
        }

        bool operator[](size_t idx) const {
            //-1 since we store an extra value for one-passed-the-end
            assert(idx < var_values.size()-1);
            return var_values[idx];
        }
        bool operator<(const CubeNum& rhs) const {
            assert(var_values.size() == rhs.var_values.size());
            //Check from MSB to LSB
            for(int i = (int) var_values.size()-1; i > 0; i--) {
                if(var_values[i] && !rhs.var_values[i]) return false;
                else if(!var_values[i] && rhs.var_values[i]) return true;
                else assert(var_values[i] == rhs.var_values[i]);
            }
            return false; //Were equal
        }

        friend std::ostream& operator<<(std::ostream& os, const CubeNum& cube_num) {
            //Note -2 since we keep an extra bit (one more than needed)
            for(int i = (int) cube_num.var_values.size() - 2; i > 0; i--) {
                if(cube_num.var_values[i]) os << "1";
                else              os << "0";
            }
            return os;
        }

    private:
        std::vector<bool> var_values;
};

BDD gen_sharpSAT_equiv_func(Cudd& cudd, double nsat_orig, size_t nvars) {
    /*
     * This function calculates a (hopefully simple)
     * function which has exactly nsat_orig satisfying assignments
     * over nvars variables.
     *
     * To do this we build the function out of the largest cubes (i.e.
     * those with the fewest variables) possible.
     *
     * In particular, we choose the satisfying assignments to be clustered
     * as shown below.  Following this simple structure it is straight
     * forward to generate the function using non-overlapping cubes (i.e. 
     * prime implicants).
     *
     * To illustrate this process consider the case a 4 variable function
     * where we require 13 satisfying assignments.  One such function is
     * illustrated in the following K-map:
     *
     *                       x1 x2
     * 
     *          \   00     01     11     10 
     *           -----------------------------
     *           |      |      |      |      |
     *        00 |  1   |  1   |  1   |  1   |
     *           |      |      |      |      |
     *           -----------------------------
     *           |      |      |      |      |
     *        01 |  1   |  1   |  1   |  1   |
     *           |      |      |      |      |
     * x3 x4     -----------------------------
     *           |      |      |      |      |
     *        11 |  1   |  1   |  1   |      |
     *           |      |      |      |      |
     *           -----------------------------
     *           |      |      |      |      |
     *        10 |  1   |  1   |      |      |
     *           |      |      |      |      |
     *           -----------------------------
     *
     * This can be clearly covered by the function:
     *
     *      f = !x1 + x1 & !x3 + x1 & x2 & x3 & x4
     *
     * Note that to generate a (hopefully) simple function, we explicitly 
     * choose to cluster the onset together, so that it can be covered by 
     * a small number of large cubes.
     *
     * The algorithm considers cubes from largest size (smallest
     *
     *
     * As a final optimization, note that in the above example we required
     * more that half (13) of the possbile (16) assignments to be satisfied.
     * In this case it may be more efficient to actually calculate the off
     * set:
     *      f' = x1 & x3 & !x4 + x1 & !x2 & x3 & x4
     * which can then be inverted:
     *      f = !f'
     * to yeild an equivalent function.
     *
     *
     * TODO: Calculate overlapping (non-prime) implicants as this would 
     * further simplify the logic, e.g.:
     *     f = !x1 + !x3 + x2 & x4
     *
     *     or
     *
     *     f' = x1 & !x2 + x3 & !x4
     */

    //Special cases
    if(nsat_orig == 0.) {
        return g_cudd.bddZero();
    } else if(nsat_orig == exp2(nvars)) {
        return g_cudd.bddOne();
    }

    bool invert = false;
    auto nsat = nsat_orig;
    if(nsat > exp2(nvars - 1)) {
        //More than half the assignments are
        //required to be satisified.
        //Instead of calculating the approximating
        //function directly, we will calculate for (2**nvars-nsat)
        //and invert the resulting function
        invert = true;
        nsat = exp2(nvars) - nsat;
    }

    BDD f = g_cudd.bddZero();
    double approx_nsat = 0;
    bool done = false;
    //Note that here cube_size refers to the number of variables in the cube
    //so a cube_size of 1 corresponds to a (large) cube which covers half the possible
    //assignments.
    //We initialize cube_size to the largest cube < nsat
    assert(nsat > 0.); //to avoid log2 going out of range
    size_t cube_size_init = floor(nvars - log2(nsat));
    for(size_t cube_size = cube_size_init; cube_size <= nvars; cube_size++) {
        double cube_nsat = exp2(nvars - cube_size);
        if(cube_nsat > (nsat - approx_nsat)) {
            //We can only add cubes that have fewer satisfying assignments
            //than required (otherwise we would have too many satisfying assignments)
            //
            //Note: the cube_size_init is a good guess, but doesn't always get it correct
            //which is why we adjust here if it was wrong
            continue;
        }
        assert(cube_nsat <= (nsat - approx_nsat));
        //By starting cube_num at 2**(cube_size-1) we ensure that we generate a cube not covered
        //by any previously added cubes.
        for(auto cube_num = CubeNum(nvars, cube_size-1); cube_num < CubeNum(nvars, cube_size); ++cube_num) {
            BDD cube = gen_cube(cudd, nvars, cube_size, cube_num);
            f |= cube;
            approx_nsat += cube_nsat;
            if(approx_nsat == nsat) { //TODO: account for floating point roundoff
                done = true;
                break;
            }
            if(cube_nsat > (nsat - approx_nsat)) {
                //Adding another cube of this type would put us over
                //nsat, so move to the next (smaller) cube size
                break;
            }
        }
        if(done) break;
    }

    assert(f.CountMinterm(nvars) == nsat);

    if(invert) {
        //Invert the function if required
        f = !f;
        assert(f.CountMinterm(nvars) == nsat_orig);
    }
    return f;
}

std::pair<BDD,size_t> gen_norm_sharpSAT_equiv_func(Cudd& cudd, double nsat_frac, double epsilon) {
    /*
     * This function generates a boolean function with nsat_frac
     * fraction of assignments which evaluate to true.
     *
     * epsilon controls the accuracy of the approximation
     * (smaller epsilon, higher number of vars in approx)
     *
     *
     */

    //Determine the number of variables
    size_t nvars = 0;
    double nassigns;
    double nsat_frac_approx;
    do {
        nvars++;
        nassigns = exp2(nvars);
        nsat_frac_approx = floor(nsat_frac * nassigns) / nassigns;
    } while(nsat_frac_approx < nsat_frac - epsilon || nsat_frac_approx > nsat_frac + epsilon);

    assert(nsat_frac_approx >= nsat_frac - epsilon);
    assert(nsat_frac_approx <= nsat_frac + epsilon);

    BDD f_approx = gen_sharpSAT_equiv_func(cudd, round(nsat_frac*exp2(nvars)), nvars);

    return {f_approx, nvars};
}

BDD gen_cube(Cudd& cudd, size_t /*nvars*/, size_t cube_size, const CubeNum& cube_num) {
    BDD cube = cudd.bddOne();
    //We associate each but (put to cube_size-1) of cube_num
    //corresponds to a variable literal (i.e. the variable for 1, inverted for 0)
    for(size_t i = 0; i < cube_size; i++) {
        if(cube_num[i]) {
            cube &= cudd.bddVar(i); 
        } else {
            cube &= !cudd.bddVar(i); 
        }
    }
    return cube;
}

int bdd_unique_node_count(const BDD& bdd) {
    DdNode* root = bdd.getNode();

    return bdd_unique_node_count_recurr(root, 0);
}

int bdd_unique_node_count_recurr(DdNode* node, int depth) {
    int uniq_cnt = 0;

    assert(depth < 1000);

    if(cuddIsConstant(node)) {
        return 0;
    }

    if(node->ref == 0) {
        uniq_cnt += 1;
    } else {
        assert(node->ref > 1);
        return 0;
    }

    //Children
    uniq_cnt += bdd_unique_node_count_recurr(cuddT(node), depth+1);
    uniq_cnt += bdd_unique_node_count_recurr(cuddE(node), depth+1);

    return uniq_cnt;
}

#include <cmath>

#include "gtest/gtest.h"

#include "bdd.hpp"

TEST(sharpSatApprox, unsat) {
    double nsat = 0;
    size_t nvars = 20;

    BDD f_approx = gen_sharpSAT_equiv_func(g_cudd, nsat, nvars);

    double nsat_approx = f_approx.CountMinterm(nvars);

    EXPECT_DOUBLE_EQ(nsat, nsat_approx);
}

TEST(sharpSatApprox, single) {
    double nsat = 1;
    size_t nvars = 20;

    BDD f_approx = gen_sharpSAT_equiv_func(g_cudd, nsat, nvars);

    double nsat_approx = f_approx.CountMinterm(nvars);

    EXPECT_DOUBLE_EQ(nsat, nsat_approx);
}

TEST(sharpSatApprox, simple) {
    BDD f = g_cudd.bddZero();

    f &= g_cudd.bddVar();
    f |= g_cudd.bddVar();

    size_t nvars = 2;

    double nsat = f.CountMinterm(nvars);

    BDD f_approx = gen_sharpSAT_equiv_func(g_cudd, nsat, nvars);

    double nsat_approx = f_approx.CountMinterm(nvars);

    EXPECT_DOUBLE_EQ(nsat, nsat_approx);
}

TEST(sharpSatApprox, complex) {
    BDD f = g_cudd.bddZero();

    f &= g_cudd.bddVar();
    f |= g_cudd.bddVar();
    f ^= g_cudd.bddVar();
    f &= g_cudd.bddVar();
    f &= g_cudd.bddVar();
    f |= g_cudd.bddVar();
    f ^= g_cudd.bddVar();

    size_t nvars = 7;

    double nsat = f.CountMinterm(nvars);

    BDD f_approx = gen_sharpSAT_equiv_func(g_cudd, nsat, nvars);

    double nsat_approx = f_approx.CountMinterm(nvars);

    EXPECT_DOUBLE_EQ(nsat, nsat_approx);
}

TEST(sharpSatApprox, complex_invert) {
    size_t nvars = 7;

    //Make more than half of the possible assignments satisfying
    //This should trigger the inverted calculation mode
    double nsat = 1.5*pow(2, nvars-1);

    BDD f_approx = gen_sharpSAT_equiv_func(g_cudd, nsat, nvars);

    double nsat_approx = f_approx.CountMinterm(nvars);

    EXPECT_DOUBLE_EQ(nsat, nsat_approx);
}

TEST(sharpSatApprox, all_sat) {
    size_t nvars = 500;

    //Make all assignments satisfying (i.e. f_approx is true)
    double nsat = pow(2, nvars);

    BDD f_approx = gen_sharpSAT_equiv_func(g_cudd, nsat, nvars);

    double nsat_approx = f_approx.CountMinterm(nvars);

    EXPECT_DOUBLE_EQ(nsat, nsat_approx);
}

#define TIGHT_EPSILON 1e-10
#define MODERATE_EPSILON 0.001
#define LOOSE_EPSILON 0.1
TEST(norm_sharpSatApprox, none_sat) {
    double nsat_frac = 0.0;

    auto pair = gen_norm_sharpSAT_equiv_func(g_cudd, nsat_frac, TIGHT_EPSILON);
    BDD f_approx = pair.first;
    size_t nvars = pair.second;

    double nsat_frac_approx = f_approx.CountMinterm(nvars) / exp2(nvars);

    EXPECT_NEAR(nsat_frac, nsat_frac_approx, TIGHT_EPSILON);
}

TEST(norm_sharpSatApprox, all_sat) {
    double nsat_frac = 1.0;

    auto pair = gen_norm_sharpSAT_equiv_func(g_cudd, nsat_frac, TIGHT_EPSILON);
    BDD f_approx = pair.first;
    size_t nvars = pair.second;

    double nsat_frac_approx = f_approx.CountMinterm(nvars) / exp2(nvars);

    EXPECT_NEAR(nsat_frac, nsat_frac_approx, TIGHT_EPSILON);
}

TEST(norm_sharpSatApprox, small_sat) {
    double nsat_frac = 0.001;

    auto pair = gen_norm_sharpSAT_equiv_func(g_cudd, nsat_frac, TIGHT_EPSILON);
    BDD f_approx = pair.first;
    size_t nvars = pair.second;

    double nsat_frac_approx = f_approx.CountMinterm(nvars) / exp2(nvars);

    EXPECT_NEAR(nsat_frac, nsat_frac_approx, TIGHT_EPSILON);
}

TEST(norm_sharpSatApprox, few_sat) {
    double nsat_frac = 0.05;

    auto pair = gen_norm_sharpSAT_equiv_func(g_cudd, nsat_frac, TIGHT_EPSILON);
    BDD f_approx = pair.first;
    size_t nvars = pair.second;

    double nsat_frac_approx = f_approx.CountMinterm(nvars) / exp2(nvars);

    EXPECT_NEAR(nsat_frac, nsat_frac_approx, TIGHT_EPSILON);
}

TEST(norm_sharpSatApprox, many_sat) {
    double nsat_frac = 0.43;

    auto pair = gen_norm_sharpSAT_equiv_func(g_cudd, nsat_frac, TIGHT_EPSILON);
    BDD f_approx = pair.first;
    size_t nvars = pair.second;

    double nsat_frac_approx = f_approx.CountMinterm(nvars) / exp2(nvars);

    EXPECT_NEAR(nsat_frac, nsat_frac_approx, TIGHT_EPSILON);
}

TEST(norm_sharpSatApprox, most_sat) {
    double nsat_frac = 0.78;

    auto pair = gen_norm_sharpSAT_equiv_func(g_cudd, nsat_frac, TIGHT_EPSILON);
    BDD f_approx = pair.first;
    size_t nvars = pair.second;

    double nsat_frac_approx = f_approx.CountMinterm(nvars) / exp2(nvars);

    EXPECT_NEAR(nsat_frac, nsat_frac_approx, TIGHT_EPSILON);
}

TEST(norm_sharpSatApprox, epsilon) {
    double nsat_frac = 0.0333333333333;

    for(auto epsilon : {TIGHT_EPSILON, MODERATE_EPSILON, LOOSE_EPSILON}) {

        auto pair = gen_norm_sharpSAT_equiv_func(g_cudd, nsat_frac, TIGHT_EPSILON);
        BDD f_approx = pair.first;
        size_t nvars = pair.second;

        double nsat_frac_approx = f_approx.CountMinterm(nvars) / exp2(nvars);

        EXPECT_NEAR(nsat_frac, nsat_frac_approx, epsilon);
    }
}

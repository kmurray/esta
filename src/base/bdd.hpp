#pragma once

#include "cuddObj.hh"

extern Cudd g_cudd;

namespace std {
    template <>
    struct hash<BDD> {
        size_t operator()(const BDD& x) const {
            return hash<void*>()(x.manager()) ^ hash<void*>()(x.getNode());
        }
    };
}

double sharpSat(const BDD& bdd, const int nvars);

std::istream& operator>>(std::istream& is, Cudd_ReorderingType& type);
std::ostream& operator<<(std::ostream& os, const Cudd_ReorderingType& type);

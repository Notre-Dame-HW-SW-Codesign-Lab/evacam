#ifndef MCAM_PAIR_RESPONSE_H_
#define MCAM_PAIR_RESPONSE_H_

#include <vector>

#include "EvaCAMMcamAnalysis.h"

// Complete dual-cell resistance in ohms, indexed [stored symbol][query symbol].
// In particular, neither symmetry nor a minimum diagonal conductance is assumed.
void ValidateMcamPairResistance(const std::vector<std::vector<double>> &resistance);

// Empty query means bounds over every stored AND query vector. Otherwise the
// query is held fixed. Only reachable squared Euclidean distances are returned.
std::vector<EvaCAMDistanceVoltageBounds> McamPairConductanceBounds(
        const std::vector<std::vector<double>> &resistance,
        int dimensions, const std::vector<int> &query = {});

#endif

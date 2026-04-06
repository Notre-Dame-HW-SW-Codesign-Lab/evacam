#include "VariationSampler.h"

#include <cmath>
#include <stdexcept>

VariationSampler::VariationSampler(unsigned int seed)
    : rng_(seed) {}

double VariationSampler::SamplePositive(double nominal, double sigmaFrac) {
    if (nominal < 0) {
        throw std::invalid_argument("[VariationSampler] nominal must be non-negative.");
    }
    if (sigmaFrac < 0) {
        throw std::invalid_argument("[VariationSampler] sigmaFrac must be non-negative.");
    }
    if (nominal == 0 || sigmaFrac == 0) {
        return nominal;
    }

    const double sigma = nominal * sigmaFrac;
    const double lowerBound = std::max(nominal * 1e-12, nominal - 3.0 * sigma);
    const double upperBound = nominal + 3.0 * sigma;
    std::normal_distribution<double> dist(nominal, sigma);

    // Use rejection sampling so the bounded distribution remains Gaussian
    // inside the admissible interval instead of accumulating clipped mass
    // at the edges.
    for (int attempt = 0; attempt < 256; ++attempt) {
        const double sample = dist(rng_);
        if (sample >= lowerBound && sample <= upperBound) {
            return sample;
        }
    }

    // Deterministic fallback if repeated draws fail.
    return std::min(std::max(dist(rng_), lowerBound), upperBound);
}

double VariationSampler::SampleResistance(double nominal, double sigmaFrac) {
    return SamplePositive(nominal, sigmaFrac);
}

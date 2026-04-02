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

    const double sigma2 = std::log(1.0 + sigmaFrac * sigmaFrac);
    const double sigma = std::sqrt(sigma2);
    const double mu = std::log(nominal) - 0.5 * sigma2;

    std::lognormal_distribution<double> dist(mu, sigma);
    return dist(rng_);
}

double VariationSampler::SampleResistance(double nominal, double sigmaFrac) {
    return SamplePositive(nominal, sigmaFrac);
}

#include "model/VariationSampler.h"

#include <cassert>
#include <cmath>
#include <iostream>

static void test_zero_sigma_returns_nominal() {
    VariationSampler sampler(12345);
    const double nominal = 2500.0;
    const double sampled = sampler.SampleResistance(nominal, 0.0);
    assert(sampled == nominal);
}

static void test_same_seed_same_sequence() {
    VariationSampler samplerA(7);
    VariationSampler samplerB(7);

    for (int i = 0; i < 8; i++) {
        const double a = samplerA.SampleResistance(1000.0, 0.1);
        const double b = samplerB.SampleResistance(1000.0, 0.1);
        assert(a == b);
    }
}

static void test_positive_samples() {
    VariationSampler sampler(99);

    for (int i = 0; i < 128; i++) {
        const double sampled = sampler.SampleResistance(1000.0, 0.5);
        assert(sampled > 0.0);
    }
}

static void test_mean_preserved_approximately() {
    VariationSampler sampler(123);
    const double nominal = 5000.0;
    const double sigmaFrac = 0.2;
    const int samples = 20000;
    double sum = 0.0;

    for (int i = 0; i < samples; i++) {
        sum += sampler.SampleResistance(nominal, sigmaFrac);
    }

    const double mean = sum / samples;
    const double relError = std::fabs(mean - nominal) / nominal;
    assert(relError < 0.03);
}

int main() {
    test_zero_sigma_returns_nominal();
    test_same_seed_same_sequence();
    test_positive_samples();
    test_mean_preserved_approximately();
    std::cout << "VariationSampler tests passed" << std::endl;
    return 0;
}

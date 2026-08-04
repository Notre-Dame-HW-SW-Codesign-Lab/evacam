#include "model/VariationSampler.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

static void test_zero_sigma_returns_nominal() {
    VariationSampler sampler(12345);
    const double nominal = 2500.0;
    assert(sampler.SamplePositive(nominal, 0.0) == nominal);
    assert(sampler.SampleResistance(nominal, 0.0) == nominal);
    assert(sampler.SamplePositive(0.0, 0.5) == 0.0);
}

static void test_positive_sampler_validates_arguments() {
    VariationSampler sampler(1);
    try {
        (void)sampler.SamplePositive(-1.0, 0.1);
        assert(false && "Negative nominal must throw.");
    } catch (const std::invalid_argument &) {
    }
    try {
        (void)sampler.SamplePositive(1.0, -0.1);
        assert(false && "Negative standard deviation fraction must throw.");
    } catch (const std::invalid_argument &) {
    }
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
        const double sampled = sampler.SamplePositive(1000.0, 0.5);
        assert(sampled > 0.0);
    }
}

static void test_samples_stay_within_bounds() {
    VariationSampler sampler(31415);
    const double nominal = 1000.0;
    const double stdevFrac = 0.2;
    const double stdev = nominal * stdevFrac;
    const double lowerBound = nominal - 3.0 * stdev;
    const double upperBound = nominal + 3.0 * stdev;

    for (int i = 0; i < 2048; i++) {
        const double sampled = sampler.SampleResistance(nominal, stdevFrac);
        assert(sampled >= lowerBound);
        assert(sampled <= upperBound);
    }
}

static void test_mean_preserved_approximately() {
    VariationSampler sampler(123);
    const double nominal = 5000.0;
    const double stdevFrac = 0.2;
    const int samples = 20000;
    double sum = 0.0;

    for (int i = 0; i < samples; i++) {
        sum += sampler.SampleResistance(nominal, stdevFrac);
    }

    const double mean = sum / samples;
    const double relError = std::fabs(mean - nominal) / nominal;
    assert(relError < 0.03);
}

int main() {
    test_zero_sigma_returns_nominal();
    test_positive_sampler_validates_arguments();
    test_same_seed_same_sequence();
    test_positive_samples();
    test_samples_stay_within_bounds();
    test_mean_preserved_approximately();
    std::cout << "VariationSampler tests passed" << std::endl;
    return 0;
}

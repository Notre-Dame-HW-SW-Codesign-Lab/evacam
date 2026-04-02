#ifndef VARIATIONSAMPLER_H_
#define VARIATIONSAMPLER_H_

#include <random>

class VariationSampler {
    public:
        explicit VariationSampler(unsigned int seed);

        double SamplePositive(double nominal, double sigmaFrac);
        double SampleResistance(double nominal, double sigmaFrac);

    private:
        std::mt19937 rng_;
};

#endif /* VARIATIONSAMPLER_H_ */

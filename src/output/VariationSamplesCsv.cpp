#include "output/VariationSamplesCsv.h"

#include <iomanip>
#include <ostream>

#include "Result.h"

void WriteVariationSamplesCsv(std::ostream &os, const Result &result) {
    os << "sample,matchline_delay_s,search_latency_s,search_dynamic_energy_j,"
       << "sense_margin_v,reference_delay_s\n";

    if (!result.bank || !result.bank->mat || !result.bank->mat->subarray) {
        return;
    }

    const auto &samples = result.bank->mat->subarray->monteCarloSamples;
    os << std::setprecision(17);
    for (const auto &sample : samples) {
        os << sample.sample << ","
           << sample.matchlineDelay << ","
           << sample.searchLatency << ","
           << sample.searchDynamicEnergy << ","
           << sample.senseMargin << ","
           << sample.referDelay << "\n";
    }
}

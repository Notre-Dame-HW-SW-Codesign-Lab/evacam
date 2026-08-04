#include "output/VariationSamplesCsv.h"

#include <iomanip>
#include <ostream>
#include <string>

#include "Result.h"

namespace {

std::string CsvString(const std::string &value) {
    std::string quoted = "\"";
    for (char c : value) {
        if (c == '"') {
            quoted += "\"\"";
        } else {
            quoted += c;
        }
    }
    quoted += "\"";
    return quoted;
}

}  // namespace

void WriteVariationSamplesCsv(std::ostream &os, const Result &result) {
    os << "sample,corner_label,memory_device_res_on_corner,"
       << "memory_device_res_off_corner,"
       << "matchline_delay_s,search_latency_s,search_dynamic_energy_j,"
       << "exact_match_sense_margin_v,reference_delay_s,nominal_matchline_delay_s,"
       << "nominal_search_latency_s,nominal_search_dynamic_energy_j,"
       << "nominal_exact_match_sense_margin_v,nominal_reference_delay_s\n";

    if (!result.bank || !result.bank->mat || !result.bank->mat->subarray) {
        return;
    }

    const auto &subarray = *result.bank->mat->subarray;
    const auto &samples = subarray.variationSamples;
    const auto &summary = subarray.variationSummary;
    os << std::setprecision(17);
    for (const auto &sample : samples) {
        os << sample.sample << ","
           << CsvString(sample.cornerLabel) << ","
           << CsvString(sample.memoryDeviceResOnCorner) << ","
           << CsvString(sample.memoryDeviceResOffCorner) << ","
           << sample.matchlineDelay << ","
           << sample.searchLatency << ","
           << sample.searchDynamicEnergy << ","
           << sample.senseMargin << ","
           << sample.referDelay << ","
           << summary.matchlineDelay.nominal << ","
           << summary.searchLatency.nominal << ","
           << summary.searchDynamicEnergy.nominal << ","
           << summary.senseMargin.nominal << ","
           << summary.referenceDelay.nominal << "\n";
    }
}

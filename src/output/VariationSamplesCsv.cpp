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
    os << "sample,corner_label,matchline_wire_res_corner,access_res_on_corner,"
       << "access_res_off_corner,match_res_on_corner,match_res_off_corner,"
       << "matchline_delay_s,search_latency_s,search_dynamic_energy_j,"
       << "sense_margin_v,reference_delay_s\n";

    if (!result.bank || !result.bank->mat || !result.bank->mat->subarray) {
        return;
    }

    const auto &samples = result.bank->mat->subarray->variationSamples;
    os << std::setprecision(17);
    for (const auto &sample : samples) {
        os << sample.sample << ","
           << CsvString(sample.cornerLabel) << ","
           << CsvString(sample.matchlineWireResCorner) << ","
           << CsvString(sample.accessResOnCorner) << ","
           << CsvString(sample.accessResOffCorner) << ","
           << CsvString(sample.matchResOnCorner) << ","
           << CsvString(sample.matchResOffCorner) << ","
           << sample.matchlineDelay << ","
           << sample.searchLatency << ","
           << sample.searchDynamicEnergy << ","
           << sample.senseMargin << ","
           << sample.referDelay << "\n";
    }
}

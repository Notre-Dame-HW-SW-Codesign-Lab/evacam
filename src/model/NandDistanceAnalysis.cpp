#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

#include "model/NandCamModel.h"

namespace {

void ValidateDistanceQuery(const NandDistanceContext &context,
        const std::vector<int> &query, bool allowGlobal) {
    const auto states = context.table.resistance.size();
    if (states < 2 || states > 16 || context.symbols <= 0) {
        throw std::invalid_argument("NAND distance analysis requires a resistance_table");
    }
    if (query.empty() && allowGlobal) return;
    if (query.size() != static_cast<std::size_t>(context.symbols)) {
        throw std::invalid_argument("NAND distance vectors must contain symbols_per_key entries");
    }
    for (int symbol : query) {
        if (symbol < 0 || symbol >= static_cast<int>(states)) {
            throw std::invalid_argument("NAND distance analysis requires ordinary state indices; wildcards are only supported by exact matching");
        }
    }
}

struct DistanceState {
    double minTau = std::numeric_limits<double>::infinity();
    double maxTau = -std::numeric_limits<double>::infinity();
    double minResistance = std::numeric_limits<double>::infinity();
    double maxResistance = -std::numeric_limits<double>::infinity();
};

}  // namespace

double NandSquaredDistance(const NandDistanceContext &context,
        const std::vector<int> &stored, const std::vector<int> &query) {
    ValidateDistanceQuery(context, stored, false);
    ValidateDistanceQuery(context, query, false);
    double distance = 0;
    for (std::size_t i = 0; i < stored.size(); ++i) {
        const double delta = stored[i] - query[i];
        distance += delta * delta;
    }
    return distance;
}

std::vector<EvaCAMDistanceVoltageBounds> NandTableDistanceBounds(
        const NandDistanceContext &context, const std::vector<int> &query,
        double decisionTime, double searchLatency) {
    ValidateDistanceQuery(context, query, true);
    const int states = static_cast<int>(context.table.resistance.size());
    if (context.firstKey < 0 || static_cast<std::size_t>(context.firstKey) > context.weights.size()
            || static_cast<std::size_t>(context.symbols) > (context.weights.size() - context.firstKey) / 2
            || !std::isfinite(context.baseTimeConstant) || context.baseTimeConstant <= 0
            || !std::isfinite(context.baseResistance) || context.baseResistance <= 0
            || !std::isfinite(context.prechargeVoltage) || context.prechargeVoltage <= 0
            || !std::isfinite(decisionTime) || decisionTime <= 0
            || !std::isfinite(searchLatency) || searchLatency < 0) {
        throw std::invalid_argument("NAND distance analysis requires valid RC weights, baseline, and sensing time");
    }
    // Bound work and witness memory before allocating. Never silently sample
    // or prune distances: a refused analysis must not masquerade as exact bounds.
    std::size_t maximum = 0, cells = 0, transitions = 0;
    for (int index = 0; index < context.symbols; ++index) {
        const int delta = query.empty() ? states - 1 : std::max(query[index], states - 1 - query[index]);
        transitions += (maximum + 1) * states;
        maximum += delta * delta;
        cells += maximum + 1;
        if (cells > 2000000 || transitions > 30000000) {
            throw std::invalid_argument("NAND exact distance bounds exceed the analysis budget; reduce symbol count or state count");
        }
    }
    std::vector<DistanceState> current(1);
    current[0] = {context.baseTimeConstant, context.baseTimeConstant,
            context.baseResistance, context.baseResistance};
    // Encoded (stored, query) choice for min/max tau at each layer/distance.
    std::vector<std::vector<std::array<short, 2>>> trace;
    trace.reserve(context.symbols);
    for (int index = 0; index < context.symbols; ++index) {
        struct Choice {
            DistanceState value;
            short minPair = -1;
            short maxPair = -1;
        };
        std::map<int, Choice> choices;
        const double firstWeight = context.weights[context.firstKey + 2 * index];
        const double secondWeight = context.weights[context.firstKey + 2 * index + 1];
        if (!std::isfinite(firstWeight) || !std::isfinite(secondWeight)
                || firstWeight < 0 || secondWeight < 0) {
            throw std::invalid_argument("NAND distance capacitance weights must be finite and nonnegative");
        }
        for (int stored = 0; stored < states; ++stored) {
            const int firstQuery = query.empty() ? 0 : query[index];
            const int lastQuery = query.empty() ? states - 1 : query[index];
            for (int searched = firstQuery; searched <= lastQuery; ++searched) {
                const auto pair = context.table.Pair(stored, searched, context.passResistance);
                const double resistance = pair.first + pair.second - 2 * context.passResistance;
                const double tau = (pair.first - context.passResistance) * firstWeight
                        + (pair.second - context.passResistance) * secondWeight;
                if (!std::isfinite(tau) || !std::isfinite(resistance) || tau < 0 || resistance < 0) {
                    throw std::invalid_argument("NAND distance table contribution must be finite and nonnegative");
                }
                auto &choice = choices[(stored - searched) * (stored - searched)];
                const short encoded = static_cast<short>(stored * states + searched);
                if (tau < choice.value.minTau) { choice.value.minTau = tau; choice.minPair = encoded; }
                if (tau > choice.value.maxTau) { choice.value.maxTau = tau; choice.maxPair = encoded; }
                choice.value.minResistance = std::min(choice.value.minResistance, resistance);
                choice.value.maxResistance = std::max(choice.value.maxResistance, resistance);
            }
        }
        const std::size_t size = current.size() + choices.rbegin()->first;
        std::vector<DistanceState> next(size);
        trace.emplace_back(size, std::array<short, 2>{-1, -1});
        for (std::size_t distance = 0; distance < current.size(); ++distance) {
            if (!std::isfinite(current[distance].minTau)) continue;
            for (const auto &entry : choices) {
                const auto &choice = entry.second;
                const auto target = distance + entry.first;
                auto &value = next[target];
                const double low = current[distance].minTau + choice.value.minTau;
                const double high = current[distance].maxTau + choice.value.maxTau;
                if (!std::isfinite(low) || !std::isfinite(high)
                        || !std::isfinite(current[distance].maxResistance + choice.value.maxResistance)) {
                    throw std::invalid_argument("NAND distance bounds overflowed");
                }
                if (low < value.minTau) { value.minTau = low; trace.back()[target][0] = choice.minPair; }
                if (high > value.maxTau) { value.maxTau = high; trace.back()[target][1] = choice.maxPair; }
                value.minResistance = std::min(value.minResistance,
                        current[distance].minResistance + choice.value.minResistance);
                value.maxResistance = std::max(value.maxResistance,
                        current[distance].maxResistance + choice.value.maxResistance);
            }
        }
        current.swap(next);
    }
    std::vector<EvaCAMDistanceVoltageBounds> result;
    for (std::size_t distance = 0; distance < current.size(); ++distance) {
        const auto &value = current[distance];
        if (!std::isfinite(value.minTau)) continue;
        if (!std::isfinite(value.maxTau) || !std::isfinite(value.maxResistance)) {
            throw std::invalid_argument("NAND distance bounds overflowed");
        }
        EvaCAMDistanceVoltageBounds bound;
        bound.squaredEuclideanDistance = static_cast<double>(distance);
        bound.minimumTimeConstant = value.minTau;
        bound.maximumTimeConstant = value.maxTau;
        bound.minimumVoltage = context.prechargeVoltage * std::exp(-decisionTime / value.minTau);
        bound.maximumVoltage = context.prechargeVoltage * std::exp(-decisionTime / value.maxTau);
        bound.minimumConductance = 1 / value.maxResistance;
        bound.maximumConductance = 1 / value.minResistance;
        bound.minimumSearchLatency = bound.maximumSearchLatency = searchLatency;
        bound.sensingTime = decisionTime;
        for (int endpoint = 0; endpoint < 2; ++endpoint) {
            auto &stored = endpoint == 0 ? bound.minimumVoltageStored : bound.maximumVoltageStored;
            auto &searched = endpoint == 0 ? bound.minimumVoltageQuery : bound.maximumVoltageQuery;
            stored.resize(context.symbols);
            searched.resize(context.symbols);
            auto remaining = distance;
            for (int index = context.symbols - 1; index >= 0; --index) {
                const int encoded = trace[index][remaining][endpoint];
                stored[index] = encoded / states;
                searched[index] = encoded % states;
                const int delta = stored[index] - searched[index];
                remaining -= delta * delta;
            }
        }
        result.push_back(std::move(bound));
    }
    return result;
}

void SetNandThresholdDiagnostics(EvaCAMMatchResult &result,
        const NandDistanceContext &context,
        const std::vector<EvaCAMDistanceVoltageBounds> &bounds,
        double maximumSquaredDistance, double decisionTime) {
    if (!std::isfinite(maximumSquaredDistance) || maximumSquaredDistance < 0 || bounds.empty()) {
        throw std::invalid_argument("NAND squared-distance threshold must be finite and nonnegative, with nonempty bounds");
    }
    double accepted = -std::numeric_limits<double>::infinity();
    double rejected = std::numeric_limits<double>::infinity();
    double fastestValid = std::numeric_limits<double>::infinity();
    for (const auto &bound : bounds) {
        fastestValid = std::min(fastestValid, bound.minimumTimeConstant);
        if (bound.squaredEuclideanDistance <= maximumSquaredDistance) {
            accepted = std::max(accepted, bound.maximumVoltage);
        } else {
            rejected = std::min(rejected, bound.minimumVoltage);
        }
    }
    // Invalid programmed entries are rejected even when every valid distance
    // is accepted. The binary marker adds the same RC increment to every word.
    const double invalidVoltage = context.prechargeVoltage
            * std::exp(-decisionTime / (fastestValid + context.invalidTimeIncrement));
    rejected = std::min(rejected, invalidVoltage);
    const double reference = context.configuredReference == 0 ? (accepted + rejected) / 2
            : context.configuredReference;
    result.hit = result.squaredEuclideanDistance <= maximumSquaredDistance;
    result.senseReferenceVoltage = reference;
    result.acceptedMaximumVoltage = accepted;
    result.rejectedMinimumVoltage = rejected;
    result.senseMargin = std::min(reference - accepted, rejected - reference) - context.senseOffset;
    result.requiredSenseMargin = context.requiredMargin;
    result.senseMarginSlack = result.senseMargin - result.requiredSenseMargin;
    result.senseMarginPass = std::isfinite(result.senseMarginSlack) && result.senseMarginSlack >= 0;
    result.senseMarginApplicable = true;
}

EvaCAMMatchResult NandCamBackend::EvaluateDistance(const std::vector<int> &stored,
        const std::vector<int> &query) const {
    Metrics();
    const double distance = NandSquaredDistance(distanceContext, stored, query);
    auto result = Evaluate(stored, query);
    result.squaredEuclideanDistance = distance;
    return result;
}

std::vector<EvaCAMDistanceVoltageBounds> NandCamBackend::DistanceVoltageBounds(
        const std::vector<int> &query) const {
    const auto &metrics = Metrics();
    return NandTableDistanceBounds(distanceContext, query, metrics.decisionTime, metrics.searchLatency);
}

EvaCAMMatchResult NandCamBackend::EvaluateDistanceThreshold(const std::vector<int> &stored,
        const std::vector<int> &query, double maximumSquaredDistance) const {
    if (!std::isfinite(maximumSquaredDistance) || maximumSquaredDistance < 0) {
        throw std::invalid_argument("NAND squared-distance threshold must be finite and nonnegative");
    }
    auto result = EvaluateDistance(stored, query);
    SetNandThresholdDiagnostics(result, distanceContext, DistanceVoltageBounds(query),
            maximumSquaredDistance, Metrics().decisionTime);
    return result;
}

#include "config/InputRuleValidator.h"

#include <limits>
#include <stdexcept>
#include <string>

#include "EvaCamConfig.h"

namespace {

long long CheckedMultiply(long long lhs, long long rhs, const char *what) {
    if (lhs <= 0 || rhs <= 0) {
        throw std::runtime_error(std::string("[Input] Error: ") + what + " factors must be positive.");
    }
    if (lhs > std::numeric_limits<long long>::max() / rhs) {
        throw std::runtime_error(std::string("[Input] Error: ") + what + " exceeds int64_t range.");
    }
    return lhs * rhs;
}

long long CheckedTotalProduct(const IntValueDomain &first, const IntValueDomain &second,
        const char *what) {
    return CheckedMultiply(first.Min(), second.Min(), what);
}

void ValidateDerivedInputs(const EvaCamConfig &config) {
    if (config.input.wordWidth <= 0) {
        throw std::runtime_error("[Input] Error: word_width must be > 0.");
    }
    if (config.input.capacity <= 0) {
        throw std::runtime_error(
                "[Input] Error: memory.capacity must be > 0 unless organization.subarray.dimensions derives it.");
    }
    const bool isWordWidthPow2 = (config.input.wordWidth & (config.input.wordWidth - 1)) == 0;
    if (!isWordWidthPow2 && config.runtimeSizing.realCapacity == 0) {
        throw std::runtime_error(
                "[Input] Error: non-power-of-two word_width requires extra.real_capacity to be set.");
    }
    if (config.runtimeSizing.realCapacity > 0) {
        if (config.runtimeSizing.realCapacity < config.input.capacity) {
            throw std::runtime_error("[Input] Error: extra.real_capacity must be >= memory.capacity.");
        }
        const long long denom =
            (long long)config.exploration.geometry.numRowSubarray.Min()
            * config.exploration.geometry.numColumnSubarray.Min()
            * config.exploration.geometry.numActiveMatPerRow.Min()
            * config.exploration.geometry.numActiveMatPerColumn.Min();
        if (denom <= 0) {
            throw std::runtime_error(
                    "[Input] Error: invalid organization geometry while validating extra.real_capacity.");
        }
        if ((config.runtimeSizing.realCapacity % denom) != 0) {
            throw std::runtime_error(
                    "[Input] Error: extra.real_capacity is incompatible with organization geometry.");
        }
        if (((config.runtimeSizing.realCapacity / denom) % config.input.wordWidth) != 0) {
            throw std::runtime_error(
                    "[Input] Error: extra.real_capacity is incompatible with word_width.");
        }
    }
}

void ValidateAndResolveExplicitSubarrayDimensions(EvaCamConfig &config) {
    if (!config.runtimeSizing.hasFixedSubarrayDimensions) {
        if (!config.runtimeSizing.hasExplicitCapacity || config.runtimeSizing.capacityIsAuto) {
            throw std::runtime_error(
                    "[Input] Error: memory.capacity is required unless organization.subarray.dimensions is supplied.");
        }
        return;
    }

    if (config.input.optimizationTarget == full_exploration || config.exploration.deepExploration) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions is only supported for fixed non-DSE configs.");
    }

    const int subarrayRows = config.runtimeSizing.fixedSubarrayRows;
    const int subarrayColumns = config.runtimeSizing.fixedSubarrayColumns;
    if (subarrayRows < 16 || subarrayRows > 512) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions row count must be between 16 and 512.");
    }
    if (subarrayColumns < 8 || subarrayColumns > 512) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions column count must be between 8 and 512.");
    }

    const long long banksTotal = CheckedTotalProduct(config.exploration.geometry.numRowMat,
            config.exploration.geometry.numColumnMat, "organization.banks.total");
    const long long banksActive = CheckedTotalProduct(config.exploration.geometry.numActiveMatPerColumn,
            config.exploration.geometry.numActiveMatPerRow, "organization.banks.active");
    const long long matsTotal = CheckedTotalProduct(config.exploration.geometry.numRowSubarray,
            config.exploration.geometry.numColumnSubarray, "organization.mats.total");
    const long long matsActive = CheckedTotalProduct(config.exploration.geometry.numActiveSubarrayPerColumn,
            config.exploration.geometry.numActiveSubarrayPerRow, "organization.mats.active");

    if (banksTotal % banksActive != 0 || matsTotal % matsActive != 0) {
        throw std::runtime_error(
                "[Input] Error: active bank/mat partitioning must divide total bank/mat geometry.");
    }

    const long long partitionFactor = CheckedMultiply(banksTotal / banksActive,
            matsTotal / matsActive, "active partitioning");
    if (config.input.wordWidth % partitionFactor != 0) {
        throw std::runtime_error(
                "[Input] Error: word_width is incompatible with active bank/mat partitioning.");
    }
    const long long effectiveSubarrayRows = config.input.wordWidth / partitionFactor;
    if (effectiveSubarrayRows != subarrayRows) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions row count is incompatible with word_width.");
    }
    if ((effectiveSubarrayRows & (effectiveSubarrayRows - 1)) != 0) {
        throw std::runtime_error(
                "[Input] Error: organization.subarray.dimensions row count must match a power-of-two effective row count.");
    }

    long long capacityBits = subarrayRows;
    capacityBits = CheckedMultiply(capacityBits, subarrayColumns, "derived capacity");
    capacityBits = CheckedMultiply(capacityBits, banksTotal, "derived capacity");
    capacityBits = CheckedMultiply(capacityBits, matsTotal, "derived capacity");
    if (capacityBits % 8 != 0) {
        throw std::runtime_error(
                "[Input] Error: derived capacity from organization.subarray.dimensions is not byte-addressable.");
    }
    const int64_t derivedCapacityBytes = capacityBits / 8;

    if (!config.runtimeSizing.hasExplicitCapacity || config.runtimeSizing.capacityIsAuto) {
        config.input.capacity = derivedCapacityBytes;
    } else if (config.input.capacity != derivedCapacityBytes) {
        throw std::runtime_error(
                "[Input] Error: memory.capacity does not match organization.subarray.dimensions.");
    }

    if (config.runtimeSizing.realCapacity > 0
            && config.runtimeSizing.realCapacity != derivedCapacityBytes) {
        throw std::runtime_error(
                "[Input] Error: extra.real_capacity must match capacity derived from organization.subarray.dimensions.");
    }
}

}  // namespace

void InputRuleValidator::Validate(EvaCamConfig &config) {
    ValidateAndResolveExplicitSubarrayDimensions(config);
    ValidateDerivedInputs(config);
}

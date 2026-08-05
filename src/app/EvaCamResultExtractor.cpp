#include "EvaCamResultExtractor.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

#include "Bank.h"
#include "Result.h"
#include "CAM_SubArray.h"
#include "EvaCamConfig.h"
#include "Mat.h"

namespace {

std::string OptimizationTargetName(OptimizationTarget target) {
    switch (target) {
        case read_latency_optimized: return "ReadLatency";
        case write_latency_optimized: return "WriteLatency";
        case read_energy_optimized: return "ReadDynamicEnergy";
        case write_energy_optimized: return "WriteDynamicEnergy";
        case read_edp_optimized: return "ReadEDP";
        case write_edp_optimized: return "WriteEDP";
        case leakage_optimized: return "LeakagePower";
        case area_optimized: return "Area";
        case search_latency_optimized: return "SearchLatency";
        case search_energy_optimized: return "SearchEnergy";
        case search_edp_optimized: return "SearchEDP";
        case full_exploration: return "Exploration";
    }
    return "Unknown";
}

double SafeCellArea(const Result &result) {
    if (!result.config || !result.config->technology.cell || !result.config->technology.tech) {
        return 0;
    }

    return result.config->technology.cell->area
        * result.config->technology.tech->featureSize()
        * result.config->technology.tech->featureSize();
}

double SafeRatio(double numerator, double denominator) {
    if (denominator == 0) {
        return 0;
    }
    return numerator / denominator;
}

double LocalSearchLatency(const Result &result) {
    const auto &bank = *result.bank;
    const auto &sub = *bank.mat->subarray;
    if (result.config->peripherals.noPrechargeInc) {
        return sub.matchlineDelay + sub.ColMux[sub.indexMatchline]->readLatency
            + sub.senseAmpLatency + sub.outputAcc->readLatency;
    }

    double latency = sub.searchLatency * bank.mat->muxSenseAmp
        - sub.inputBuf->readLatency * (bank.mat->muxSenseAmp - 1);
    if (result.config->peripherals.withOutputAcc) {
        latency *= result.config->input.wordWidth / bank.CAM_opt.BitSerialWidth;
    }
    return latency;
}

double LocalSearchEnergy(const Result &result) {
    const auto &bank = *result.bank;
    const auto &mat = *bank.mat;
    const auto &sub = *mat.subarray;
    double energy = sub.searchDynamicEnergy * mat.muxSenseAmp
        - (sub.inputBuf->readDynamicEnergy + sub.inputEnc->readDynamicEnergy)
        * (mat.muxSenseAmp - 1);
    if (result.config->peripherals.withOutputAcc) {
        energy *= result.config->input.wordWidth / bank.CAM_opt.BitSerialWidth;
    }
    return energy * bank.numRowMat * bank.numColumnMat
        * bank.numRowSubarray * bank.numColumnSubarray;
}

template <typename T>
bool HasUnit(const std::unique_ptr<T> &unit) {
    return static_cast<bool>(unit);
}

EvaCamMetricStatsDto ExtractMetricStats(const CAMMetricStats &stats) {
    EvaCamMetricStatsDto dto;
    dto.available = stats.available;
    dto.nominal = stats.nominal;
    dto.sample = stats.sample;
    dto.mean = stats.mean;
    dto.stddev = stats.stddev;
    dto.min = stats.min;
    dto.max = stats.max;
    dto.p95 = stats.p95;
    return dto;
}

EvaCamVariationDto ExtractVariation(const CAM_SubArray &subarray) {
    EvaCamVariationDto variation;
    variation.enabled = subarray.variationSummary.enabled;
    variation.mode = subarray.variationSummary.mode;
    variation.samples = subarray.variationSummary.samples;
    variation.matchlineDelay = ExtractMetricStats(subarray.variationSummary.matchlineDelay);
    variation.searchLatency = ExtractMetricStats(subarray.variationSummary.searchLatency);
    variation.searchDynamicEnergy = ExtractMetricStats(subarray.variationSummary.searchDynamicEnergy);
    variation.senseMargin = ExtractMetricStats(subarray.variationSummary.senseMargin);
    variation.referenceDelay = ExtractMetricStats(subarray.variationSummary.referenceDelay);

    variation.sampleData.reserve(subarray.variationSamples.size());
    for (const CAMVariationSample &sample : subarray.variationSamples) {
        EvaCamVariationSampleDto dto;
        dto.sample = sample.sample;
        dto.cornerLabel = sample.cornerLabel;
        dto.memoryDeviceResOnCorner = sample.memoryDeviceResOnCorner;
        dto.memoryDeviceResOffCorner = sample.memoryDeviceResOffCorner;
        dto.matchlineDelay = sample.matchlineDelay;
        dto.searchLatency = sample.searchLatency;
        dto.searchDynamicEnergy = sample.searchDynamicEnergy;
        dto.senseMargin = sample.senseMargin;
        dto.referenceDelay = sample.referDelay;
        variation.sampleData.push_back(dto);
    }
    return variation;
}

void AddCoreSummary(EvaCamDesignResultDto &dto, const Result &result) {
    const auto &bank = *result.bank;
    const auto &mat = *bank.mat;
    const auto &sub = *mat.subarray;

    dto.summary["area.total.width_m"] = bank.height;
    dto.summary["area.total.height_m"] = bank.width;
    dto.summary["area.total.area_m2"] = bank.area;
    dto.summary["area.mat.width_m"] = mat.height;
    dto.summary["area.mat.height_m"] = mat.width;
    dto.summary["area.mat.area_m2"] = mat.area;
    dto.summary["area.subarray.width_m"] = sub.height;
    dto.summary["area.subarray.height_m"] = sub.width;
    dto.summary["area.subarray.area_m2"] = sub.area;

    const double cellArea = SafeCellArea(result);
    dto.summary["area.mat.cell_area_utilization"] = SafeRatio(
            cellArea * bank.capacity / bank.numRowMat / bank.numColumnMat,
            mat.area);
    dto.summary["area.subarray.cell_area_utilization"] = SafeRatio(
            cellArea * bank.capacity / bank.numRowMat / bank.numColumnMat
                / bank.numRowSubarray / bank.numColumnSubarray,
            sub.area);
    dto.summary["area.efficiency"] = SafeRatio(cellArea * bank.capacity, bank.area);

    dto.summary["timing.read_latency_s"] = bank.readLatency;
    dto.summary["timing.write_latency_s"] = bank.writeLatency;
    dto.summary["timing.search_latency_s"] = bank.searchLatency;
    dto.summary["timing.reset_latency_s"] = bank.resetLatency;
    dto.summary["timing.set_latency_s"] = bank.setLatency;
    dto.summary["timing.mat_read_latency_s"] = mat.readLatency;
    dto.summary["timing.mat_write_latency_s"] = mat.writeLatency;
    dto.summary["timing.mat_search_latency_s"] = mat.subarray->searchLatency;
    dto.summary["timing.predecoder_latency_s"] = mat.predecoderLatency;
    dto.summary["timing.subarray_read_latency_s"] = sub.readLatency;
    dto.summary["timing.subarray_write_latency_s"] = sub.writeLatency;
    dto.summary["timing.subarray_search_latency_s"] = sub.searchLatency;
    dto.summary["timing.matchline_delay_s"] = sub.matchlineDelay;
    dto.summary["timing.sense_amp_latency_s"] = sub.senseAmpLatency;
    dto.summary["timing.reference_delay_s"] = sub.referDelay;
    dto.summary["timing.exact_match_sense_margin_v"] = sub.senseMargin;

    dto.summary["energy.read_dynamic_j"] = bank.readDynamicEnergy;
    dto.summary["energy.write_dynamic_j"] = bank.writeDynamicEnergy;
    dto.summary["energy.search_dynamic_j"] = bank.searchDynamicEnergy;
    dto.summary["energy.reset_dynamic_j"] = bank.resetDynamicEnergy;
    dto.summary["energy.set_dynamic_j"] = bank.setDynamicEnergy;
    dto.summary["power.leakage_w"] = bank.leakage;

    if (HasUnit(sub.rowDecoder) && HasUnit(sub.precharger)) {
        dto.summary["bandwidth.read_Bps"] = SafeRatio(
                static_cast<double>(bank.blockSize),
                sub.readLatency - sub.rowDecoder->readLatency + sub.precharger->readLatency) / 8;
    }
    dto.summary["bandwidth.write_Bps"] = SafeRatio(static_cast<double>(bank.blockSize), sub.writeLatency) / 8;
}

void AddGeometry(EvaCamDesignResultDto &dto, const Result &result) {
    const auto &bank = *result.bank;
    const auto &mat = *bank.mat;
    const auto &sub = *mat.subarray;

    dto.geometry["capacity_bits"] = static_cast<double>(bank.capacity);
    dto.geometry["block_size_bits"] = static_cast<double>(bank.blockSize);
    dto.geometry["num_row_mat"] = bank.numRowMat;
    dto.geometry["num_column_mat"] = bank.numColumnMat;
    dto.geometry["num_row_subarray"] = mat.numRowSubarray;
    dto.geometry["num_column_subarray"] = mat.numColumnSubarray;
    dto.geometry["subarray_rows"] = static_cast<double>(sub.numRow);
    dto.geometry["subarray_columns"] = static_cast<double>(sub.numColumn);
    dto.geometry["num_active_mat_per_row"] = bank.numActiveMatPerRow;
    dto.geometry["num_active_mat_per_column"] = bank.numActiveMatPerColumn;
    dto.geometry["num_active_subarray_per_row"] = mat.numActiveSubarrayPerRow;
    dto.geometry["num_active_subarray_per_column"] = mat.numActiveSubarrayPerColumn;
    dto.geometry["mux_sense_amp"] = bank.muxSenseAmp;
    dto.geometry["mux_output_level_1"] = bank.muxOutputLev1;
    dto.geometry["mux_output_level_2"] = bank.muxOutputLev2;
    dto.geometry["bit_serial_width"] = bank.CAM_opt.BitSerialWidth;
    dto.geometry["area_optimization_level"] = mat.areaOptimizationLevel;
    dto.geometry["row_driver_optimization_level"] = sub.DriverOptLevel;
    dto.geometry["priority_optimization_level"] = sub.PriorityOptLevel;
}

void AddBreakdown(EvaCamDesignResultDto &dto, const Result &result) {
    const auto &input = *result.config;
    const auto &bank = *result.bank;
    const auto &mat = *bank.mat;
    const auto &sub = *mat.subarray;
    const std::string routeKey = (input.input.routingMode == h_tree) ? "h_tree" : "non_h_tree";

    const double localSearchLatency = LocalSearchLatency(result);
    const double localSearchEnergy = LocalSearchEnergy(result);
    dto.breakdown["search_latency." + routeKey + "_s"] = bank.searchLatency - localSearchLatency;
    dto.breakdown["search_latency.mat_s"] = localSearchLatency;
    dto.breakdown["search_latency.predecoder_s"] = mat.predecoderLatency;
    if (HasUnit(sub.inputEnc)) {
        dto.breakdown["search_latency.input_encoder_s"] = sub.inputEnc->readLatency;
    }
    dto.breakdown["search_latency.row_decoder_s"] = sub.RowDecMergeNand->readLatency
        + sub.senseAmpMuxLev1Nand->readLatency + sub.senseAmpMuxLev2Nand->readLatency;

    double rowDriverLatency = 0;
    for (const auto &driver : sub.RowDriver) {
        if (driver) {
            rowDriverLatency = std::max(rowDriverLatency, driver->readLatency);
        }
    }
    dto.breakdown["search_latency.row_driver_s"] = rowDriverLatency;
    dto.breakdown["search_latency.precharger_s"] = sub.precharger->readLatency;
    dto.breakdown["search_latency.matchline_s"] = sub.matchlineDelay;

    double colMuxLatency = 0;
    for (const auto &mux : sub.ColMux) {
        if (mux) {
            colMuxLatency = std::max(colMuxLatency, mux->readLatency);
        }
    }
    dto.breakdown["search_latency.column_mux_s"] = colMuxLatency;
    dto.breakdown["search_latency.sense_amplifier_s"] = sub.senseAmpLatency;
    dto.breakdown["search_latency.mux_of_sa_s"] =
        sub.senseAmpMuxLev1->readLatency + sub.senseAmpMuxLev2->readLatency;
    if (input.peripherals.withOutputAcc) {
        dto.breakdown["search_latency.output_accumulator_s"] = sub.outputAcc->readLatency;
    }
    if (input.peripherals.withPriorityEnc) {
        dto.breakdown["search_latency.priority_encoder_s"] = sub.priorityEnc->readLatency;
    }

    dto.breakdown["read_dynamic_energy." + routeKey + "_j"] = bank.readDynamicEnergy
        - mat.readDynamicEnergy * bank.numActiveMatPerColumn * bank.numActiveMatPerRow;
    dto.breakdown["read_dynamic_energy.mat_j"] = mat.readDynamicEnergy;
    dto.breakdown["read_dynamic_energy.predecoder_j"] = mat.readDynamicEnergy
        - sub.readDynamicEnergy * bank.numActiveSubarrayPerRow * bank.numActiveSubarrayPerColumn;
    dto.breakdown["read_dynamic_energy.subarray_j"] = sub.readDynamicEnergy;

    dto.breakdown["write_dynamic_energy." + routeKey + "_j"] = bank.writeDynamicEnergy
        - mat.writeDynamicEnergy * bank.numActiveMatPerColumn * bank.numActiveMatPerRow;
    dto.breakdown["write_dynamic_energy.mat_j"] = mat.writeDynamicEnergy;
    dto.breakdown["write_dynamic_energy.predecoder_j"] = mat.writeDynamicEnergy
        - sub.writeDynamicEnergy * bank.numActiveSubarrayPerRow * bank.numActiveSubarrayPerColumn;
    dto.breakdown["write_dynamic_energy.subarray_j"] = sub.writeDynamicEnergy;

    dto.breakdown["search_dynamic_energy." + routeKey + "_j"] =
        bank.searchDynamicEnergy - localSearchEnergy;
    dto.breakdown["search_dynamic_energy.mat_j"] = localSearchEnergy;

    if (HasUnit(sub.inputEnc)) {
        dto.breakdown["search_dynamic_energy.input_encoder_j"] = sub.inputEnc->readDynamicEnergy;
    }
    dto.breakdown["search_dynamic_energy.row_decoder_j"] = sub.RowDecMergeNand->readDynamicEnergy;
    dto.breakdown["search_dynamic_energy.row_driver_j"] = sub.searchlineDriveDynamicEnergy;
    dto.breakdown["search_dynamic_energy.precharger_j"] = sub.precharger->readDynamicEnergy;
    dto.breakdown["search_dynamic_energy.cell_read_j"] = sub.cellReadEnergy;
    double colMuxReadDynamicEnergy = 0;
    for (const auto &mux : sub.ColMux) {
        if (mux) {
            colMuxReadDynamicEnergy += mux->readDynamicEnergy;
        }
    }
    dto.breakdown["search_dynamic_energy.column_mux_j"] = colMuxReadDynamicEnergy;
    dto.breakdown["search_dynamic_energy.sense_amplifier_j"] = sub.senseAmp->readDynamicEnergy;
    dto.breakdown["search_dynamic_energy.mux_of_sa_j"] =
        sub.senseAmpMuxLev1->readDynamicEnergy + sub.senseAmpMuxLev2->readDynamicEnergy;
    if (input.peripherals.withOutputAcc) {
        dto.breakdown["search_dynamic_energy.output_accumulator_j"] = sub.outputAcc->readDynamicEnergy;
    }
    if (input.peripherals.withPriorityEnc) {
        dto.breakdown["search_dynamic_energy.priority_encoder_j"] = sub.priorityEnc->readDynamicEnergy;
    }

    dto.breakdown["write_dynamic_energy.cell_reset_j"] = sub.cellResetEnergy;
    dto.breakdown["write_dynamic_energy.cell_set_j"] = sub.cellSetEnergy;
    if (HasUnit(sub.inputEnc)) {
        dto.breakdown["write_dynamic_energy.input_encoder_j"] = sub.inputEnc->writeDynamicEnergy;
    }
    dto.breakdown["write_dynamic_energy.row_decoder_j"] = sub.RowDecMergeNand->writeDynamicEnergy;
    double rowWriteDynamicEnergy = 0;
    for (const auto &driver : sub.RowDriver) {
        if (driver) {
            rowWriteDynamicEnergy += driver->writeDynamicEnergy * 16;
        }
    }
    dto.breakdown["write_dynamic_energy.row_driver_j"] = rowWriteDynamicEnergy;
    dto.breakdown["write_dynamic_energy.precharger_j"] = sub.precharger->writeDynamicEnergy;
    double colWriteDynamicEnergy = 0;
    for (const auto &mux : sub.ColMux) {
        if (mux) {
            colWriteDynamicEnergy += mux->writeDynamicEnergy;
        }
    }
    dto.breakdown["write_dynamic_energy.column_mux_j"] = colWriteDynamicEnergy;
    dto.breakdown["write_dynamic_energy.sense_amplifier_j"] = sub.senseAmp->writeDynamicEnergy;
    dto.breakdown["write_dynamic_energy.mux_of_sa_j"] =
        sub.senseAmpMuxLev1->writeDynamicEnergy + sub.senseAmpMuxLev2->writeDynamicEnergy;
    if (input.peripherals.withOutputAcc) {
        dto.breakdown["write_dynamic_energy.output_accumulator_j"] = sub.outputAcc->writeDynamicEnergy;
    }
    if (input.peripherals.withPriorityEnc) {
        dto.breakdown["write_dynamic_energy.priority_encoder_j"] = sub.priorityEnc->writeDynamicEnergy;
    }

    dto.breakdown["leakage." + routeKey + "_w"] = bank.leakage
        - mat.leakage * bank.numColumnMat * bank.numRowMat;
    dto.breakdown["leakage.mat_w"] = mat.leakage;
    dto.breakdown["leakage.predecoder_w"] = mat.rowPredecoderBlock1->leakage + mat.rowPredecoderBlock2->leakage
        + mat.bitlineMuxPredecoderBlock1->leakage + mat.bitlineMuxPredecoderBlock2->leakage
        + mat.senseAmpMuxLev1PredecoderBlock1->leakage + mat.senseAmpMuxLev1PredecoderBlock2->leakage
        + mat.senseAmpMuxLev2PredecoderBlock1->leakage + mat.senseAmpMuxLev2PredecoderBlock2->leakage;
    dto.breakdown["leakage.subarray_w"] = sub.leakage;
    if (HasUnit(sub.inputEnc)) {
        dto.breakdown["leakage.input_encoder_w"] = sub.inputEnc->leakage;
    }
    dto.breakdown["leakage.row_decoder_w"] = sub.RowDecMergeNand->leakage;
    double rowDriverLeakage = 0;
    for (const auto &driver : sub.RowDriver) {
        if (driver) {
            rowDriverLeakage += driver->leakage;
        }
    }
    dto.breakdown["leakage.row_driver_w"] = rowDriverLeakage;
    dto.breakdown["leakage.precharger_w"] = sub.precharger->leakage;
    double colMuxLeakage = 0;
    for (const auto &mux : sub.ColMux) {
        if (mux) {
            colMuxLeakage += mux->leakage;
        }
    }
    dto.breakdown["leakage.column_mux_w"] = colMuxLeakage;
    dto.breakdown["leakage.sense_amplifier_w"] = sub.senseAmp->leakage;
    dto.breakdown["leakage.mux_of_sa_w"] = sub.senseAmpMuxLev1->leakage + sub.senseAmpMuxLev2->leakage;
    if (input.peripherals.withOutputAcc) {
        dto.breakdown["leakage.output_accumulator_w"] = sub.outputAcc->leakage;
    }
    if (input.peripherals.withPriorityEnc) {
        dto.breakdown["leakage.priority_encoder_w"] = sub.priorityEnc->leakage;
    }

    dto.breakdown["subarray_area.total_cell_area_m2"] = sub.lenRow * sub.lenCol;
    if (HasUnit(sub.inputBuf)) {
        dto.breakdown["subarray_area.input_buffer_m2"] = sub.inputBuf->area * sub.numRow;
    }
    if (HasUnit(sub.inputEnc)) {
        dto.breakdown["subarray_area.input_encoder_m2"] = sub.inputEnc->area;
    }
    dto.breakdown["subarray_area.row_decoder_m2"] = sub.RowDecMergeNand->area;
    double rowDriverArea = 0;
    for (const auto &driver : sub.RowDriver) {
        if (driver) {
            rowDriverArea += driver->area;
        }
    }
    dto.breakdown["subarray_area.row_driver_m2"] = rowDriverArea;
    dto.breakdown["subarray_area.precharger_m2"] = sub.precharger->area;
    if (input.peripherals.withWriteDriver) {
        dto.breakdown["subarray_area.write_driver_m2"] = sub.WriteDriverArea;
    }
    double colMuxArea = 0;
    for (const auto &mux : sub.ColMux) {
        if (mux) {
            colMuxArea += mux->area;
        }
    }
    dto.breakdown["subarray_area.column_mux_m2"] = colMuxArea;
    dto.breakdown["subarray_area.sense_amplifier_m2"] = sub.senseAmp->area;
    dto.breakdown["subarray_area.mux_of_sa_m2"] = sub.senseAmpMuxLev1->area + sub.senseAmpMuxLev2->area
        + sub.senseAmpMuxLev1Nand->area + sub.senseAmpMuxLev2Nand->area;
    if (input.peripherals.withOutputAcc) {
        dto.breakdown["subarray_area.output_accumulator_m2"] = sub.outputAcc->area * sub.numColumn / sub.muxSenseAmp;
    }
    if (input.peripherals.withPriorityEnc) {
        dto.breakdown["subarray_area.priority_encoder_m2"] = sub.priorityEnc->area;
    }
    if (HasUnit(sub.outputBuf)) {
        dto.breakdown["subarray_area.output_buffer_m2"] = sub.outputBuf->area * sub.numColumn / sub.muxSenseAmp;
    }
}

EvaCamDesignResultDto ExtractDesignResult(const Result &result) {
    EvaCamDesignResultDto dto;
    dto.optimizationTarget = OptimizationTargetName(result.optimizationTarget);
    AddCoreSummary(dto, result);
    AddGeometry(dto, result);
    AddBreakdown(dto, result);
    dto.variation = ExtractVariation(*result.bank->mat->subarray);
    return dto;
}

}  // namespace

EvaCamRunResultDto ExtractEvaCamRunResult(
        long long numSolutions,
        const std::vector<std::shared_ptr<Result>> &bestResults,
        const std::string &explorationCsvPath,
        const std::string &outputYamlPath) {
    EvaCamRunResultDto dto;
    dto.numSolutions = numSolutions;
    dto.explorationCsvPath = explorationCsvPath;
    dto.outputYamlPath = outputYamlPath;

    if (numSolutions <= 0) {
        return dto;
    }

    for (const auto &result : bestResults) {
        if (!result || !result->bank || !result->bank->initialized
                || !std::isfinite(result->bank->area) || result->bank->area >= 1e40) {
            continue;
        }

        EvaCamDesignResultDto designResult = ExtractDesignResult(*result);
        dto.bestResults[designResult.optimizationTarget] = designResult;
    }

    return dto;
}

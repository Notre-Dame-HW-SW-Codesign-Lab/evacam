#include "output/ResultsYaml.h"

#include <functional>
#include <iomanip>
#include <string>
#include <unordered_map>

#include "UnitFormatter.h"
#include "EvaCamConfig.h"

namespace {

    class YamlWriter {
        public:
            explicit YamlWriter(std::ostream& os) : os_(os) {}

            void begin_map(const std::string& key) {
                indent();
                os_ << key << ":\n";
                indent_ += 2;
            }

            void end_map() {
                if (indent_ >= 2)
                    indent_ -= 2;
            }

            void line(const std::string& key, const std::string& value, const std::string& comment = "") {
                indent();
                os_ << key << ": " << value;
                if (!comment.empty())
                    os_ << "  # " << comment;
                os_ << "\n";
            }

        private:
            void indent() { os_ << std::string(indent_, ' '); }

            std::ostream& os_;
            int indent_ = 0;
    };

    std::string fmt_second(double x) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << ToSecond(x);
        return oss.str();
    }

    std::string fmt_bps(double x) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << ToBps(x);
        return oss.str();
    }

    std::string fmt_joule(double x) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << ToJoule(x);
        return oss.str();
    }

    std::string fmt_watt(double x) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << ToWatt(x);
        return oss.str();
    }

    std::string fmt_voltage(double x) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << x << "V";
        return oss.str();
    }

    std::string fmt_meter(double x) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << ToMeter(x);
        return oss.str();
    }

    std::string fmt_sqm(double x) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << ToSquareMeter(x);
        return oss.str();
    }

    std::string fmt_percent(double x) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << x << "%";
        return oss.str();
    }

    double safe_cell_area(const Result& result) {
        if (!result.config || !result.config->technology.cell || !result.config->technology.tech) {
            return 0;
        }

        return result.config->technology.cell->area
            * result.config->technology.tech->featureSize()
            * result.config->technology.tech->featureSize();
    }

    double safe_percent(double numerator, double denominator) {
        if (denominator == 0) {
            return 0;
        }
        return numerator / denominator * 100;
    }

    double local_search_latency(const Result& result) {
        const auto& bank = result.bank;
        const auto& sub = bank->mat->subarray;
        if (result.config->peripherals.noPrechargeInc) {
            return sub->matchlineDelay + sub->ColMux[sub->indexMatchline]->readLatency
                + sub->senseAmpLatency + sub->outputAcc->readLatency;
        }

        double latency = sub->searchLatency * bank->mat->muxSenseAmp
            - sub->inputBuf->readLatency * (bank->mat->muxSenseAmp - 1);
        if (result.config->peripherals.withOutputAcc) {
            latency *= result.config->input.wordWidth / bank->CAM_opt.BitSerialWidth;
        }
        return latency;
    }

    double local_search_energy(const Result& result) {
        const auto& bank = result.bank;
        const auto& mat = bank->mat;
        const auto& sub = mat->subarray;
        double energy = sub->searchDynamicEnergy * mat->muxSenseAmp
            - (sub->inputBuf->readDynamicEnergy + sub->inputEnc->readDynamicEnergy)
            * (mat->muxSenseAmp - 1);
        if (result.config->peripherals.withOutputAcc) {
            energy *= result.config->input.wordWidth / bank->CAM_opt.BitSerialWidth;
        }
        return energy * bank->numRowMat * bank->numColumnMat
            * bank->numRowSubarray * bank->numColumnSubarray;
    }

    std::string optimization_target_name(OptimizationTarget t) {
        switch (t) {
            case read_latency_optimized:
                return "ReadLatency";
            case write_latency_optimized:
                return "WriteLatency";
            case read_energy_optimized:
                return "ReadDynamicEnergy";
            case write_energy_optimized:
                return "WriteDynamicEnergy";
            case read_edp_optimized:
                return "ReadEDP";
            case write_edp_optimized:
                return "WriteEDP";
            case leakage_optimized:
                return "LeakagePower";
            case area_optimized:
                return "Area";
            case search_latency_optimized:
                return "SearchLatency";
            case search_energy_optimized:
                return "SearchEnergy";
            case search_edp_optimized:
                return "SearchEDP";
            case full_exploration:
                return "Exploration";
            default:
                return "Unknown";
        }
    }

    std::string roadmap_name(DeviceRoadmap roadmap) {
        switch (roadmap) {
            case HP: return "HP";
            case LSTP: return "LSTP";
            case LOP: return "LOP";
            case FEFET: return "FEFET";
            case LP: return "LP";
            default: return "Unknown";
        }
    }

    std::string bool_name(bool value) {
        return value ? "true" : "false";
    }

    void write_assumptions(YamlWriter& y, const EvaCamConfig &config) {
        y.begin_map("assumptions");
        y.line("model_identifier", "evacam-cam-v1");
        y.line("design_target", "CAM");
        y.line("routing", config.input.routingMode == h_tree ? "h_tree" : "non_h_tree");
        y.begin_map("technology");
        y.line("process_node", std::to_string(config.input.processNode) + "nm");
        y.line("roadmap", roadmap_name(config.input.deviceRoadmap));
        y.end_map();
        y.begin_map("modeling_options");
        y.line("exclude_precharge_latency", bool_name(config.peripherals.noPrechargeInc));
        y.line("include_leakage", bool_name(config.peripherals.includeLeakage));
        y.line("scaled_voltage", fmt_voltage(config.peripherals.scaledVoltage));
        y.end_map();
        y.line("limitations_reference", "docs/limitations.md");
        y.end_map();
    }

    void write_metric_stats(
            YamlWriter& y,
            const std::string& key,
            const CAMMetricStats& stats,
            const std::function<std::string(double)>& formatter) {
        if (!stats.available)
            return;
        y.begin_map(key);
        y.line("nominal", formatter(stats.nominal));
        y.line("mean", formatter(stats.mean));
        y.line("stddev", formatter(stats.stddev));
        y.line("min", formatter(stats.min));
        y.line("max", formatter(stats.max));
        y.line("p95", formatter(stats.p95));
        y.end_map();
    }

    void write_metric_sample(
            YamlWriter& y,
            const std::string& key,
            const CAMMetricStats& stats,
            const std::function<std::string(double)>& formatter) {
        if (!stats.available)
            return;
        y.begin_map(key);
        y.line("nominal", formatter(stats.nominal));
        y.line("sample", formatter(stats.sample));
        y.end_map();
    }

    void write_breakdown(YamlWriter& y, const Result& result) {
        const auto& input = result.config;
        const auto& bank = result.bank;
        const auto& mat = bank->mat;
        const auto& sub = mat->subarray;

        y.begin_map("breakdown");

        y.begin_map("subarray_area");
        y.line("total_cell_area", fmt_sqm(sub->lenRow * sub->lenCol));
        y.line("input_buffer_area", fmt_sqm(sub->inputBuf->area * sub->numRow));
        y.line("input_encoder_area", fmt_sqm(sub->inputEnc->area));
        y.line("row_decoder_area", fmt_sqm(sub->RowDecMergeNand->area));
        double row_driver_area = 0;
        for (int i = 0; i < input->technology.cell->camNumRow; i++)
            row_driver_area += sub->RowDriver[i]->area;
        y.line("row_driver_area", fmt_sqm(row_driver_area));
        y.line("precharger_area", fmt_sqm(sub->precharger->area));
        if (input->peripherals.withWriteDriver)
            y.line("write_driver_area", fmt_sqm(sub->WriteDriverArea));
        double col_mux_area = 0;
        for (int i = 0; i < input->technology.cell->camNumCol; i++)
            col_mux_area += sub->ColMux[i]->area;
        y.line("column_mux_area", fmt_sqm(col_mux_area));
        y.line("sense_amplifier_area", fmt_sqm(sub->senseAmp->area));
        y.line("mux_of_sa_area", fmt_sqm(sub->senseAmpMuxLev1->area + sub->senseAmpMuxLev2->area +
                    sub->senseAmpMuxLev1Nand->area + sub->senseAmpMuxLev2Nand->area));
        y.line("output_accumulator_area",
                fmt_sqm(sub->outputAcc->area * sub->numColumn / sub->muxSenseAmp));
        if (input->peripherals.withPriorityEnc)
            y.line("priority_encoder_area", fmt_sqm(sub->priorityEnc->area));
        y.line("output_buffer_area", fmt_sqm(sub->outputBuf->area * sub->numColumn / sub->muxSenseAmp));
        y.end_map();

        y.begin_map("search_latency");
        y.line("input_encoder", fmt_second(sub->inputEnc->readLatency));
        y.line("row_decoder", fmt_second(sub->RowDecMergeNand->readLatency + sub->senseAmpMuxLev1Nand->readLatency +
                    sub->senseAmpMuxLev2Nand->readLatency));
        double row_driver_latency = 0;
        for (int i = 0; i < input->technology.cell->camNumRow; i++)
            row_driver_latency = std::max(row_driver_latency, sub->RowDriver[i]->readLatency);
        y.line("row_driver", fmt_second(row_driver_latency));
        y.line("precharger", fmt_second(sub->precharger->readLatency));
        y.line("matchline", fmt_second(sub->matchlineDelay));
        double col_driver_latency = 0;
        for (int i = 0; i < input->technology.cell->camNumCol; i++)
            col_driver_latency = std::max(col_driver_latency, sub->ColMux[i]->readLatency);
        y.line("column_mux", fmt_second(col_driver_latency));
        y.line("sense_amplifier", fmt_second(sub->senseAmpLatency));
        y.line("mux_of_sa", fmt_second(sub->senseAmpMuxLev1->readLatency + sub->senseAmpMuxLev2->readLatency));
        if (input->peripherals.withOutputAcc)
            y.line("output_accumulator", fmt_second(sub->outputAcc->readLatency));
        if (input->peripherals.withPriorityEnc)
            y.line("priority_encoder", fmt_second(sub->priorityEnc->readLatency));
        y.end_map();

        y.begin_map("search_dynamic_energy");
        y.line("input_encoder", fmt_joule(sub->inputEnc->readDynamicEnergy));
        y.line("row_decoder", fmt_joule(sub->RowDecMergeNand->readDynamicEnergy));
        y.line("row_driver", fmt_joule(sub->searchlineDriveDynamicEnergy));
        y.line("precharger", fmt_joule(sub->precharger->readDynamicEnergy));
        y.line("cell_read", fmt_joule(sub->cellReadEnergy));
        double col_mux_read_dynamic_energy = 0;
        for (int i = 0; i < input->technology.cell->camNumCol; i++)
            col_mux_read_dynamic_energy += sub->ColMux[i]->readDynamicEnergy;
        y.line("column_mux", fmt_joule(col_mux_read_dynamic_energy));
        y.line("sense_amplifier", fmt_joule(sub->senseAmp->readDynamicEnergy));
        y.line("mux_of_sa", fmt_joule(sub->senseAmpMuxLev1->readDynamicEnergy + sub->senseAmpMuxLev2->readDynamicEnergy));
        if (input->peripherals.withOutputAcc)
            y.line("output_accumulator", fmt_joule(sub->outputAcc->readDynamicEnergy));
        if (input->peripherals.withPriorityEnc)
            y.line("priority_encoder", fmt_joule(sub->priorityEnc->readDynamicEnergy));
        y.end_map();

        y.begin_map("write_dynamic_energy");
        y.line("cell_reset", fmt_joule(sub->cellResetEnergy));
        y.line("cell_set", fmt_joule(sub->cellSetEnergy));
        y.line("input_encoder", fmt_joule(sub->inputEnc->writeDynamicEnergy));
        y.line("row_decoder", fmt_joule(sub->RowDecMergeNand->writeDynamicEnergy));
        double row_write_dynamic_energy = 0;
        for (int i = 0; i < input->technology.cell->camNumRow; i++)
            row_write_dynamic_energy += sub->RowDriver[i]->writeDynamicEnergy * 16;
        y.line("row_driver", fmt_joule(row_write_dynamic_energy));
        y.line("precharger", fmt_joule(sub->precharger->writeDynamicEnergy));
        double col_write_dynamic_energy = 0;
        for (int i = 0; i < input->technology.cell->camNumCol; i++)
            col_write_dynamic_energy += sub->ColMux[i]->writeDynamicEnergy;
        y.line("column_mux", fmt_joule(col_write_dynamic_energy));
        y.line("sense_amplifier", fmt_joule(sub->senseAmp->writeDynamicEnergy));
        y.line("mux_of_sa", fmt_joule(sub->senseAmpMuxLev1->writeDynamicEnergy + sub->senseAmpMuxLev2->writeDynamicEnergy));
        if (input->peripherals.withOutputAcc)
            y.line("output_accumulator", fmt_joule(sub->outputAcc->writeDynamicEnergy));
        if (input->peripherals.withPriorityEnc)
            y.line("priority_encoder", fmt_joule(sub->priorityEnc->writeDynamicEnergy));
        y.end_map();

        y.begin_map("leakage");
        y.line("input_encoder", fmt_watt(sub->inputEnc->leakage));
        y.line("row_decoder", fmt_watt(sub->RowDecMergeNand->leakage));
        double leakage = 0;
        for (int i = 0; i < input->technology.cell->camNumRow; i++)
            leakage += sub->RowDriver[i]->leakage;
        y.line("row_driver", fmt_watt(leakage));
        y.line("precharger", fmt_watt(sub->precharger->leakage));
        leakage = 0;
        for (int i = 0; i < input->technology.cell->camNumCol; i++)
            leakage += sub->ColMux[i]->leakage;
        y.line("column_mux", fmt_watt(leakage));
        y.line("sense_amplifier", fmt_watt(sub->senseAmp->leakage));
        y.line("mux_of_sa", fmt_watt(sub->senseAmpMuxLev1->leakage + sub->senseAmpMuxLev2->leakage));
        if (input->peripherals.withOutputAcc)
            y.line("output_accumulator", fmt_watt(sub->outputAcc->leakage));
        if (input->peripherals.withPriorityEnc)
            y.line("priority_encoder", fmt_watt(sub->priorityEnc->leakage));
        y.end_map();

        y.end_map();
    }

    void write_summary(YamlWriter& y, const Result& result,
            const std::string &variationSamplesFile = "",
            const std::string &variationPlotFile = "") {
        const auto& input = result.config;
        const auto& bank = result.bank;
        const std::string route_key = (input->input.routingMode == h_tree) ? "h_tree" : "non_h_tree";

        y.begin_map("summary");

        y.begin_map("area");
        y.begin_map("total");
        y.line("width", fmt_meter(bank->height));
        y.line("height", fmt_meter(bank->width));
        y.line("area", fmt_sqm(bank->area));
        y.end_map();
        const double cellArea = safe_cell_area(result);
        double mat_area_pct = safe_percent(
                cellArea * bank->capacity / bank->numRowMat / bank->numColumnMat,
                bank->mat->area);
        double subarray_area_pct = safe_percent(
                cellArea * bank->capacity / bank->numRowMat / bank->numColumnMat
                / bank->numRowSubarray / bank->numColumnSubarray,
                bank->mat->subarray->area);
        y.begin_map("mat");
        y.line("width", fmt_meter(bank->mat->height));
        y.line("height", fmt_meter(bank->mat->width));
        y.line("area", fmt_sqm(bank->mat->area));
        y.line("cell_area_utilization", fmt_percent(mat_area_pct));
        y.end_map();
        y.begin_map("subarray");
        y.line("dimensions", std::to_string(bank->mat->subarray->numRow) + "x" +
                std::to_string(bank->mat->subarray->numColumn));
        y.line("width", fmt_meter(bank->mat->subarray->height));
        y.line("height", fmt_meter(bank->mat->subarray->width));
        y.line("area", fmt_sqm(bank->mat->subarray->area));
        y.line("cell_area_utilization", fmt_percent(subarray_area_pct));
        y.end_map();
        double area_efficiency = safe_percent(cellArea * bank->capacity, bank->area);
        y.line("efficiency", fmt_percent(area_efficiency));
        y.end_map();

        y.begin_map("timing");
        const double localSearchLatency = local_search_latency(result);
        y.line("search_latency", fmt_second(bank->searchLatency));
        y.begin_map("search_latency_breakdown");
        y.line(route_key, fmt_second(bank->searchLatency - localSearchLatency));
        y.line("mat", fmt_second(localSearchLatency));
        y.end_map();

        const auto &subarray = *bank->mat->subarray;
        const double nominalSenseMargin = subarray.variationSummary.senseMargin.available
            ? subarray.variationSummary.senseMargin.nominal
            : subarray.senseMargin;
        y.line("exact_match_sense_margin", fmt_voltage(nominalSenseMargin));
        y.line("minimum_required_sense_margin", fmt_voltage(subarray.senseVoltage));

        if (input->technology.cell->memCellType == PCRAM || input->technology.cell->memCellType == FBRAM ||
                input->technology.cell->memCellType == FEFETRAM ||
                (input->technology.cell->memCellType == memristor &&
                 (input->technology.cell->accessType == CMOS_access || input->technology.cell->accessType == BJT_access))) {
            y.line("reset_latency", fmt_second(bank->resetLatency));
            y.begin_map("reset_latency_breakdown");
            y.line(route_key, fmt_second(bank->resetLatency - bank->mat->resetLatency));
            y.line("mat", fmt_second(bank->mat->resetLatency));
            y.line("predecoder", fmt_second(bank->mat->predecoderLatency));
            y.line("subarray", fmt_second(bank->mat->subarray->resetLatency));
            y.end_map();
            y.line("set_latency", fmt_second(bank->setLatency));
            y.begin_map("set_latency_breakdown");
            y.line(route_key, fmt_second(bank->setLatency - bank->mat->setLatency));
            y.line("mat", fmt_second(bank->mat->setLatency));
            y.line("predecoder", fmt_second(bank->mat->predecoderLatency));
            y.line("subarray", fmt_second(bank->mat->subarray->setLatency));
            y.end_map();
        } else if (input->technology.cell->memCellType == SLCNAND) {
            y.line("erase_latency", fmt_second(bank->resetLatency));
            y.begin_map("erase_latency_breakdown");
            y.line(route_key, fmt_second(bank->resetLatency - bank->mat->resetLatency));
            y.line("mat", fmt_second(bank->mat->resetLatency));
            y.end_map();
            y.line("program_latency", fmt_second(bank->setLatency));
            y.begin_map("program_latency_breakdown");
            y.line(route_key, fmt_second(bank->setLatency - bank->mat->setLatency));
            y.line("mat", fmt_second(bank->mat->setLatency));
            y.end_map();
        } else {
            y.line("write_latency", fmt_second(bank->writeLatency));
            y.begin_map("write_latency_breakdown");
            y.line(route_key, fmt_second(bank->writeLatency - bank->mat->writeLatency));
            y.line("mat", fmt_second(bank->mat->writeLatency));
            y.line("predecoder", fmt_second(bank->mat->predecoderLatency));
            y.line("subarray", fmt_second(bank->mat->subarray->writeLatency));
            y.end_map();
        }

        double read_bandwidth = (double)bank->blockSize /
            (bank->mat->subarray->readLatency - bank->mat->subarray->rowDecoder->readLatency +
             bank->mat->subarray->precharger->readLatency) / 8;
        y.line("read_bandwidth", fmt_bps(read_bandwidth));
        double write_bandwidth = (double)bank->blockSize / (bank->mat->subarray->writeLatency) / 8;
        y.line("write_bandwidth", fmt_bps(write_bandwidth));
        if (bank->mat->subarray->variationSummary.enabled) {
            y.begin_map("variation");
            y.line("mode", bank->mat->subarray->variationSummary.mode);
            y.line("samples", std::to_string(bank->mat->subarray->variationSummary.samples));
            if (!variationSamplesFile.empty()) {
                y.line("sample_file", variationSamplesFile);
            }
            if (!variationPlotFile.empty()) {
                y.line("plot_file", variationPlotFile);
            }
            if (bank->mat->subarray->variationSummary.mode == "single_point") {
                write_metric_sample(y, "matchline_delay", bank->mat->subarray->variationSummary.matchlineDelay, fmt_second);
                write_metric_sample(y, "search_latency", bank->mat->subarray->variationSummary.searchLatency, fmt_second);
                write_metric_sample(y, "search_dynamic_energy", bank->mat->subarray->variationSummary.searchDynamicEnergy, fmt_joule);
                write_metric_sample(y, "exact_match_sense_margin", bank->mat->subarray->variationSummary.senseMargin, fmt_voltage);
                write_metric_sample(y, "reference_delay", bank->mat->subarray->variationSummary.referenceDelay, fmt_second);
            } else {
                write_metric_stats(y, "matchline_delay", bank->mat->subarray->variationSummary.matchlineDelay, fmt_second);
                write_metric_stats(y, "search_latency", bank->mat->subarray->variationSummary.searchLatency, fmt_second);
                write_metric_stats(y, "search_dynamic_energy", bank->mat->subarray->variationSummary.searchDynamicEnergy, fmt_joule);
                write_metric_stats(y, "exact_match_sense_margin", bank->mat->subarray->variationSummary.senseMargin, fmt_voltage);
                write_metric_stats(y, "reference_delay", bank->mat->subarray->variationSummary.referenceDelay, fmt_second);
            }
            y.end_map();
        }
        y.end_map();

        y.begin_map("power");
        const double localSearchEnergy = local_search_energy(result);
        y.line("search_dynamic_energy", fmt_joule(bank->searchDynamicEnergy));
        y.begin_map("search_dynamic_energy_breakdown");
        y.line(route_key, fmt_joule(bank->searchDynamicEnergy - localSearchEnergy));
        y.line("mat", fmt_joule(localSearchEnergy));
        y.end_map();

        y.line("read_dynamic_energy", fmt_joule(bank->readDynamicEnergy));
        y.begin_map("read_dynamic_energy_breakdown");
        y.line(route_key, fmt_joule(bank->readDynamicEnergy - bank->mat->readDynamicEnergy *
                    bank->numActiveMatPerColumn * bank->numActiveMatPerRow));
        y.line("mat", fmt_joule(bank->mat->readDynamicEnergy), "per mat");
        y.line("predecoder", fmt_joule(bank->mat->readDynamicEnergy - bank->mat->subarray->readDynamicEnergy *
                    bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn));
        y.line("subarray", fmt_joule(bank->mat->subarray->readDynamicEnergy), "per active subarray");
        y.end_map();

        if (input->technology.cell->memCellType == PCRAM || input->technology.cell->memCellType == FBRAM ||
                (input->technology.cell->memCellType == memristor &&
                 (input->technology.cell->accessType == CMOS_access || input->technology.cell->accessType == BJT_access ||
                  input->technology.cell->memCellType == FEFETRAM))) {
            y.line("reset_dynamic_energy", fmt_joule(bank->resetDynamicEnergy));
            y.begin_map("reset_dynamic_energy_breakdown");
            y.line(route_key, fmt_joule(bank->resetDynamicEnergy - bank->mat->resetDynamicEnergy *
                        bank->numActiveMatPerColumn * bank->numActiveMatPerRow));
            y.line("mat", fmt_joule(bank->mat->resetDynamicEnergy), "per mat");
            y.line("predecoder", fmt_joule(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy *
                        bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn));
            y.line("subarray", fmt_joule(bank->mat->subarray->writeDynamicEnergy), "per active subarray");
            y.end_map();

            y.line("set_dynamic_energy", fmt_joule(bank->setDynamicEnergy));
            y.begin_map("set_dynamic_energy_breakdown");
            y.line(route_key, fmt_joule(bank->setDynamicEnergy - bank->mat->setDynamicEnergy *
                        bank->numActiveMatPerColumn * bank->numActiveMatPerRow));
            y.line("mat", fmt_joule(bank->mat->setDynamicEnergy), "per mat");
            y.line("predecoder", fmt_joule(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy *
                        bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn));
            y.line("subarray", fmt_joule(bank->mat->subarray->writeDynamicEnergy), "per active subarray");
            y.end_map();
        } else if (input->technology.cell->memCellType == SLCNAND) {
            y.line("erase_dynamic_energy", fmt_joule(bank->resetDynamicEnergy));
            y.begin_map("erase_dynamic_energy_breakdown");
            y.line(route_key, fmt_joule(bank->resetDynamicEnergy - bank->mat->resetDynamicEnergy *
                        bank->numActiveMatPerColumn * bank->numActiveMatPerRow));
            y.line("mat", fmt_joule(bank->mat->resetDynamicEnergy), "per mat");
            y.line("predecoder", fmt_joule(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy *
                        bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn));
            y.line("subarray", fmt_joule(bank->mat->subarray->writeDynamicEnergy), "per active subarray");
            y.end_map();

            y.line("program_dynamic_energy", fmt_joule(bank->setDynamicEnergy));
            y.begin_map("program_dynamic_energy_breakdown");
            y.line(route_key, fmt_joule(bank->setDynamicEnergy - bank->mat->setDynamicEnergy *
                        bank->numActiveMatPerColumn * bank->numActiveMatPerRow));
            y.line("mat", fmt_joule(bank->mat->setDynamicEnergy), "per mat");
            y.line("predecoder", fmt_joule(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy *
                        bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn));
            y.line("subarray", fmt_joule(bank->mat->subarray->writeDynamicEnergy), "per active subarray");
            y.end_map();
        } else {
            y.line("write_dynamic_energy", fmt_joule(bank->writeDynamicEnergy));
            y.begin_map("write_dynamic_energy_breakdown");
            y.line(route_key, fmt_joule(bank->writeDynamicEnergy - bank->mat->writeDynamicEnergy *
                        bank->numActiveMatPerColumn * bank->numActiveMatPerRow));
            y.line("mat", fmt_joule(bank->mat->writeDynamicEnergy), "per mat");
            y.line("predecoder", fmt_joule(bank->mat->writeDynamicEnergy - bank->mat->subarray->writeDynamicEnergy *
                        bank->numActiveSubarrayPerRow * bank->numActiveSubarrayPerColumn));
            y.line("subarray", fmt_joule(bank->mat->subarray->writeDynamicEnergy), "per active subarray");
            y.end_map();
        }

        y.line("leakage_power", fmt_watt(bank->leakage));
        y.begin_map("leakage_power_breakdown");
        y.line(route_key, fmt_watt(bank->leakage - bank->mat->leakage *
                    bank->numColumnMat * bank->numRowMat));
        y.line("mat", fmt_watt(bank->mat->leakage), "per mat");
        y.line("predecoder", fmt_watt(bank->mat->rowPredecoderBlock1->leakage + bank->mat->rowPredecoderBlock2->leakage +
                    bank->mat->bitlineMuxPredecoderBlock1->leakage + bank->mat->bitlineMuxPredecoderBlock2->leakage +
                    bank->mat->senseAmpMuxLev1PredecoderBlock1->leakage + bank->mat->senseAmpMuxLev1PredecoderBlock2->leakage +
                    bank->mat->senseAmpMuxLev2PredecoderBlock1->leakage + bank->mat->senseAmpMuxLev2PredecoderBlock2->leakage));
        y.line("subarray", fmt_watt(bank->mat->subarray->leakage), "per active subarray");
        y.end_map();

        y.end_map();
        y.end_map();
    }

    void write_results_body(YamlWriter& y, const Result& result,
            const std::string &variationSamplesFile = "",
            const std::string &variationPlotFile = "") {
        write_summary(y, result, variationSamplesFile, variationPlotFile);
        write_breakdown(y, result);
    }

} // namespace

void WriteResultsYaml(std::ostream& os, const Result& result,
        const std::string &variationSamplesFile,
        const std::string &variationPlotFile) {
    YamlWriter y(os);
    write_assumptions(y, *result.config);
    write_results_body(y, result, variationSamplesFile, variationPlotFile);
}

void WriteResultsYamlMulti(std::ostream& os, const std::vector<std::shared_ptr<Result>>& results,
        const std::unordered_map<OptimizationTarget, std::string> &variationSamplesFiles,
        const std::unordered_map<OptimizationTarget, std::string> &variationPlotFiles) {
    YamlWriter y(os);
    for (const auto& res : results) {
        if (res && res->config) {
            write_assumptions(y, *res->config);
            break;
        }
    }
    for (const auto& res : results) {
        if (!res || !res->bank || !res->bank->initialized)
            continue;
        std::string variationSamplesFile;
        const auto sampleFile = variationSamplesFiles.find(res->optimizationTarget);
        if (sampleFile != variationSamplesFiles.end()) {
            variationSamplesFile = sampleFile->second;
        }
        std::string variationPlotFile;
        const auto plotFile = variationPlotFiles.find(res->optimizationTarget);
        if (plotFile != variationPlotFiles.end()) {
            variationPlotFile = plotFile->second;
        }
        y.begin_map(optimization_target_name(res->optimizationTarget));
        write_summary(y, *res, variationSamplesFile, variationPlotFile);
        write_breakdown(y, *res);
        y.end_map();
    }
}

void WriteResultsYamlNoSolutions(std::ostream& os, const EvaCamConfig &config) {
    YamlWriter y(os);
    write_assumptions(y, config);
    y.line("status", "no_valid_solutions");
}

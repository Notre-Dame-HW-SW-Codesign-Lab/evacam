#include <iomanip>
#include <iostream>
#include <memory>

#include "EvaCamConfig.h"
#include "EvaCamExplorer.h"
#include "Result.h"
#include "config/ExplorationSpaceResolver.h"

// Read-only instrumentation for scripts/compare_legacy_evacam.py. The normal
// explorer selects and evaluates the design; this probe only exposes its fields.
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    auto config = std::make_shared<EvaCamConfig>();
    config->ReadConfigFromFile(argv[1]);
    config->logger.SetOutputEnabled(false);
    config->resolvedExploration = ExplorationSpaceResolver::Resolve(config->exploration);
    const auto run = EvaCamExplorer(config, 1).Run();
    if (run.numSolution == 0) {
        std::cout << "BASELINE feasible=0\n";
        return 0;
    }
    const auto &result = run.bestResults.at(config->input.optimizationTarget);
    const auto &bank = *result->bank;
    const auto &sub = *bank.mat->subarray;
    const auto &cell = *config->technology.cell;
    const auto &tech = *config->technology.tech;
    std::cout << std::scientific << std::setprecision(17);
#define FIELD(key, expression) std::cout << "BASELINE " key "=" << (expression) << '\n'
    FIELD("feasible", 1);
    FIELD("search_s", bank.searchLatency);
    FIELD("read_s", bank.readLatency);
    FIELD("energy_j", bank.searchDynamicEnergy);
    FIELD("area_m2", bank.area);
    FIELD("sub_area_m2", sub.area);
    FIELD("sub_search_s", sub.searchLatency);
    FIELD("ml_s", sub.matchlineDelay);
    FIELD("sa_s", sub.senseAmpLatency);
    FIELD("precharge_s", sub.precharger->readLatency);
    FIELD("decoder_s", sub.decoderLatency);
    FIELD("row_s", sub.RowDriver[0]->readLatency);
    FIELD("input_s", sub.inputBuf->readLatency);
    FIELD("encoder_s", sub.inputEnc->readLatency);
    FIELD("output_mux_s", sub.senseAmpMuxLev1->readLatency + sub.senseAmpMuxLev2->readLatency);
    FIELD("ml_cap_f", sub.Col[sub.indexMatchline].cap);
    FIELD("ml_res_ohm", sub.Col[sub.indexMatchline].res);
    FIELD("cell_cap_f", sub.capCellAccess);
    FIELD("sa_cap_f", sub.senseAmp->capLoad);
    FIELD("precharge_cap_f", sub.precharger->capOutputBitlinePrecharger);
    FIELD("cell_energy_j", sub.cellReadEnergy);
    FIELD("sa_energy_j", sub.senseAmp->readDynamicEnergy);
    FIELD("precharge_energy_j", sub.precharger->readDynamicEnergy);
    FIELD("write_driver_area_m2", sub.WriteDriverArea);
    FIELD("row_driver_area_m2", sub.RowDriver[0]->area);
    FIELD("sa_area_m2", sub.senseAmp->area);
    FIELD("path_on_ohm", sub.resMemCellOn);
    FIELD("path_off_ohm", sub.resMemCellOff);
    FIELD("sense_margin_v", sub.senseMargin);
    FIELD("input.node_nm", tech.featureSizeInNano());
    FIELD("input.feature_m", tech.featureSize());
    FIELD("input.vdd_v", tech.vdd());
    FIELD("input.temperature_k", config->input.temperature);
    FIELD("input.bits_per_ml", sub.numRow);
    FIELD("input.entries", sub.numColumn);
    FIELD("input.comparison_bits", sub.CAM_opt.BitSerialWidth);
    FIELD("input.cell_area_f2", cell.area);
    FIELD("input.cell_aspect", cell.aspectRatio);
    FIELD("input.ron_ohm", cell.resistanceOn);
    FIELD("input.roff_ohm", cell.resistanceOff);
    FIELD("input.read_voltage_v", cell.readVoltage);
    FIELD("input.read_current_a", cell.readCurrent);
    FIELD("input.read_power_w", cell.readPower);
    FIELD("input.minimum_sense_v", cell.minSenseVoltage);
    FIELD("input.additional_cap_f", config->peripherals.addCapOnML);
    FIELD("input.access_width_f", cell.widthAccessCMOS);
    FIELD("input.match_width_f", cell.camWidthMatchTran);
    FIELD("input.access_type", cell.accessType);
    FIELD("input.voltage_drop_v", cell.voltageDropAccessDevice);
    FIELD("input.row_ports", cell.camNumRow);
    FIELD("input.column_ports", cell.camNumCol);
    FIELD("input.write_driver", sub.withWriteDriver);
    FIELD("input.sa_mux", sub.muxSenseAmp);
    FIELD("input.output_mux1", sub.muxOutputLev1);
    FIELD("input.output_mux2", sub.muxOutputLev2);
    for (int axis = 0; axis < 2; ++axis) {
        const int count = axis == 0 ? cell.camNumRow : cell.camNumCol;
        for (int i = 0; i < count; ++i) {
            const auto &port = cell.camPort[axis][i];
            const std::string key = "BASELINE input.port" + std::to_string(axis) + "." + std::to_string(i);
            std::cout << key << ".width=" << port.widthCmos << '\n'
                << key << ".count=" << port.numCmos << '\n'
                << key << ".wire=" << port.widthWire << '\n'
                << key << ".type=" << (port.Type == Wordline ? 0 : port.Type == Searchline ? 1
                        : port.Type == Bitline ? 2 : port.Type == Matchline ? 3 : -1) << '\n'
                << key << ".region=" << port.ConnectedRegion << '\n'
                << key << ".is_nmos=" << port.isNMOS << '\n'
                << key << ".set_lrs=" << port.volSetLRS << '\n'
                << key << ".set_mrs=" << port.volSetMRS << '\n'
                << key << ".reset=" << port.volReset << '\n'
                << key << ".search0=" << port.volSearch0 << '\n'
                << key << ".search1=" << port.volSearch1 << '\n';
        }
    }
#undef FIELD
}

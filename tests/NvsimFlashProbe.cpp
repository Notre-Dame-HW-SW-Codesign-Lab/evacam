// Validation adapter for unmodified NVSim at the pinned revision in the report.
// Links to that source snapshot, not to EvaCAM; no copied replacement equations.
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "global.h"
#include "SubArray.h"
#include "constant.h"
#include "formula.h"

InputParameter *inputParameter;
Technology *tech;
MemCell *cell;
Wire *localWire;
Wire *globalWire;

int main(int argc, char **argv) {
    if (argc != 5) return 2; // cell file, page count, rows, columns
    InputParameter input;
    Technology technology;
    MemCell memory;
    Wire local, global;
    inputParameter = &input;
    tech = &technology;
    cell = &memory;
    localWire = &local;
    globalWire = &global;
    const long pages = std::strtol(argv[2], nullptr, 10);
    const long rows = std::strtol(argv[3], nullptr, 10);
    const long columns = std::strtol(argv[4], nullptr, 10);
    if (pages < 1 || rows < pages || columns < 2) return 2;
    input.designTarget = RAM_chip;
    input.processNode = 45;
    input.deviceRoadmap = HP;
    input.temperature = 350;
    input.pageSize = columns / 2; // odd/even bitlines, mux 2
    input.flashBlockSize = input.pageSize * pages;
    technology.Initialize(45, HP);
    memory.ReadCellFromFile(argv[1]);
    if (memory.memCellType != SLCNAND) return 2;
    local.Initialize(45, local_aggressive, repeated_none, 350, false);
    global.Initialize(45, global_aggressive, repeated_none, 350, false);
    SubArray array;
    array.Initialize(rows, columns, false, true, 2, true, 1, 1, latency_first);
    if (array.invalid) return 1;
    array.CalculateArea();
    array.CalculateLatency(1e20);
    array.CalculatePower();
    std::cout << std::setprecision(17) << "{\n";
#define FIELD(name, value) std::cout << "\"" name "\":" << (value) << ",\n"
    FIELD("pages", pages);
    FIELD("rows", rows);
    FIELD("columns", columns);
    FIELD("page_bits", input.pageSize);
    FIELD("block_bits", input.flashBlockSize);
    FIELD("vdd_v", technology.vdd);
    FIELD("precharge_v", array.voltagePrecharge);
    FIELD("sense_drop_v", array.senseVoltage);
    FIELD("string_resistance_ohm", array.resCellAccess);
    FIELD("bitline_resistance_ohm", array.resBitline);
    FIELD("cell_capacitance_f", array.capCellAccess);
    FIELD("bitline_capacitance_f", array.capBitline);
    FIELD("mux_delay_capacitance_f", array.bitlineMux.capForPreviousDelayCalculation);
    FIELD("mux_energy_capacitance_f", array.bitlineMux.capForPreviousPowerCalculation);
    FIELD("bitline_delay_s", array.bitlineDelay);
    FIELD("read_latency_s", array.readLatency);
    FIELD("program_latency_s", array.setLatency);
    FIELD("erase_latency_s", array.resetLatency);
    FIELD("read_energy_j", array.readDynamicEnergy);
    FIELD("program_energy_j", array.setDynamicEnergy);
    FIELD("erase_energy_j", array.resetDynamicEnergy);
    FIELD("write_energy_j", array.writeDynamicEnergy);
    FIELD("area_m2", array.area);
    FIELD("leakage_w", array.leakage);
    FIELD("program_voltage_v", memory.flashProgramVoltage);
    FIELD("erase_voltage_v", memory.flashEraseVoltage);
    FIELD("pass_voltage_v", memory.flashPassVoltage);
    FIELD("program_time_s", memory.flashProgramTime);
    FIELD("cell_area_m2", memory.area * technology.featureSize * technology.featureSize);
    FIELD("junction_capacitance_f_per_m2", technology.capJunction);
    FIELD("builtin_voltage_v", technology.buildInPotential);
    FIELD("tunnel_current_density_a_per_m2", TUNNEL_CURRENT_FLOW);
    FIELD("threshold_change_v", DELTA_V_TH);
    FIELD("precharge_driver_resistance_ohm", CalculateOnResistance(array.precharger.widthPMOSBitlinePrecharger, PMOS, 350, technology));
    FIELD("precharger_latency_s", array.precharger.readLatency);
    FIELD("row_decoder_ramp", array.rowDecoder.rampOutput);
    FIELD("minimum_nmos_gm_s", CalculateTransconductance(technology.featureSize, NMOS, technology));
    std::cout << "\"components\":{\n";
    const char *names[] = {"row_decoder", "bitline_mux_decoder", "sense_mux1_decoder", "sense_mux2_decoder",
        "precharger", "bitline_mux", "sense_amp", "sense_mux1", "sense_mux2"};
    const FunctionUnit *units[] = {&array.rowDecoder, &array.bitlineMuxDecoder,
        &array.senseAmpMuxLev1Decoder, &array.senseAmpMuxLev2Decoder, &array.precharger,
        &array.bitlineMux, &array.senseAmp, &array.senseAmpMuxLev1, &array.senseAmpMuxLev2};
    for (int i = 0; i < 9; ++i) {
        std::cout << '"' << names[i] << "\":{\"read_j\":" << units[i]->readDynamicEnergy
            << ",\"write_j\":" << units[i]->writeDynamicEnergy
            << ",\"set_j\":" << units[i]->setDynamicEnergy
            << ",\"reset_j\":" << units[i]->resetDynamicEnergy << "}" << (i == 8 ? "\n" : ",\n");
    }
    std::cout << "}}\n";
    return 0;
}

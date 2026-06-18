#include "Bank.h"
#include "formula.h"
#include "UnitFormatter.h"

namespace {

template <typename Accessor>
double SumRowDrivers(const CAM_SubArray &subarray, int rowCount, Accessor accessor) {
    double total = 0;
    for (int i = 0; i < rowCount; i++) {
        total += accessor(*subarray.RowDriver[i]);
    }
    return total;
}

template <typename Accessor>
double SumColumnMuxes(const CAM_SubArray &subarray, int columnCount, Accessor accessor) {
    double total = 0;
    for (int i = 0; i < columnCount; i++) {
        total += accessor(*subarray.ColMux[i]);
    }
    return total;
}

void PrintSubarrayAreaBreakdown(const Bank &bank, const CAM_SubArray &subarray) {
    const EvaCamConfig &config = *bank.config;

    std::cout << "========= Subarray Area Breakdown =========" << std::endl;
    std::cout << " |--- Total Cell Area         = " << ToSquareMeter(subarray.lenRow * subarray.lenCol) << std::endl;

    std::cout << " |--- Input Buffer Area       = " << ToSquareMeter(subarray.inputBuf->area * subarray.numRow) << std::endl;
    std::cout << " |--- Input Encoder Area      = " << ToSquareMeter(subarray.inputEnc->area) << std::endl;
    std::cout << " |--- Row Decoder Area        = " << ToSquareMeter(subarray.RowDecMergeNand->area) << std::endl;
    const double rowDriverArea = SumRowDrivers(
            subarray, config.technology.cell->camNumRow,
            [](const auto &rowDriver) { return rowDriver.area; });
    std::cout << " |--- Row Driver Area         = " << ToSquareMeter(rowDriverArea) << std::endl;
    std::cout << " |--- Precharger Area         = " << ToSquareMeter(subarray.precharger->area) << std::endl;
    if (config.peripherals.withWriteDriver) {
        std::cout << " |--- Write Driver Area       = " << ToSquareMeter(subarray.WriteDriverArea) << std::endl;
    }

    const double columnMuxArea = SumColumnMuxes(
            subarray, config.technology.cell->camNumCol,
            [](const auto &columnMux) { return columnMux.area; });
    std::cout << " |--- Column Mux Area         = " << ToSquareMeter(columnMuxArea) << std::endl;
    std::cout << " |--- Sense Amplifier Area    = " << ToSquareMeter(subarray.senseAmp->area) << std::endl;
    std::cout << " |--- Mux of SA               = " << ToSquareMeter(subarray.senseAmpMuxLev1->area + subarray.senseAmpMuxLev2->area + subarray.senseAmpMuxLev1Nand->area + subarray.senseAmpMuxLev2Nand->area) << std::endl;
    std::cout << " |--- Output Accumulator Area = " << ToSquareMeter(subarray.outputAcc->area * subarray.numColumn / subarray.muxSenseAmp) << std::endl;
    if (config.peripherals.withPriorityEnc) {
        std::cout << " |--- Priority Encoder Area   = " << ToSquareMeter(subarray.priorityEnc->area) << std::endl;
    }
    std::cout << " |--- Output Buffer Area      = " << ToSquareMeter(subarray.outputBuf->area * subarray.numColumn / subarray.muxSenseAmp) << std::endl;
    std::cout << std::endl;
}

void PrintSearchLatencyBreakdown(const Bank &bank, const CAM_SubArray &subarray) {
    const EvaCamConfig &config = *bank.config;

    std::cout << "========= Search Latency Breakdown =========" << std::endl;
    std::cout << " |--- Input Encoder Latency      = " << ToSecond(subarray.inputEnc->readLatency) << std::endl;
    std::cout << " |--- Row Decoder Latency        = " << ToSecond(subarray.RowDecMergeNand->readLatency + subarray.senseAmpMuxLev1Nand->readLatency + subarray.senseAmpMuxLev2Nand->readLatency) << std::endl;
    double rowDriverLatency = 0;
    for (int i = 0; i < config.technology.cell->camNumRow; i++) {
        rowDriverLatency = std::max(subarray.RowDriver[i]->readLatency, bank.readLatency);
    }
    std::cout << " |--- Row Driver Latency         = " << ToSecond(rowDriverLatency) << std::endl;
    std::cout << " |--- Precharger Latency         = " << ToSecond(subarray.precharger->readLatency) << std::endl;
    std::cout << " |--- Matchline Latency          = " << ToSecond(subarray.matchlineDelay) << std::endl;
    double columnDriverLatency = 0;
    for (int i = 0; i < config.technology.cell->camNumCol; i++) {
        columnDriverLatency = std::max(subarray.ColMux[i]->readLatency, bank.readLatency);
    }
    std::cout << " |--- Column Mux Latency         = " << ToSecond(columnDriverLatency) << std::endl;
    std::cout << " |--- Sense Amplifier Latency    = " << ToSecond(subarray.senseAmpLatency) << std::endl;
    std::cout << " |--- MUX of SA Latency          = " << ToSecond(subarray.senseAmpMuxLev1->readLatency + subarray.senseAmpMuxLev2->readLatency) << std::endl;
    if (config.peripherals.withOutputAcc) {
        std::cout << " |--- Output Accumulator Latency = " << ToSecond(subarray.outputAcc->readLatency) << std::endl;
    }
    if (config.peripherals.withPriorityEnc) {
        std::cout << " |--- Priority Encoder Latency   = " << ToSecond(subarray.priorityEnc->readLatency) << std::endl;
    }
    std::cout << std::endl;
}

void PrintSearchEnergyBreakdown(const Bank &bank, const CAM_SubArray &subarray) {
    const EvaCamConfig &config = *bank.config;

    std::cout << "========= Search Dynamic Energy Breakdown =========" << std::endl;
    std::cout << " |--- Input Encoder Dynamic Energy      = " << ToJoule(subarray.inputEnc->readDynamicEnergy) << std::endl;
    std::cout << " |--- Row Decoder Dynamic Energy        = " << ToJoule(subarray.RowDecMergeNand->readDynamicEnergy) << std::endl;
    std::cout << " |--- RowDriver Dynamic Energy          = "
              << ToJoule(subarray.searchlineDriveDynamicEnergy) << std::endl;
    std::cout << " |--- Precharger Dynamic Energy         = " << ToJoule(subarray.precharger->readDynamicEnergy) << std::endl;
    // TODO: not really the breakdown
    std::cout << " |--- Cell Read Energy                  = " << ToJoule(subarray.cellReadEnergy) << std::endl;
    const double columnMuxReadDynamicEnergy = SumColumnMuxes(
            subarray, config.technology.cell->camNumCol,
            [](const auto &columnMux) { return columnMux.readDynamicEnergy; });
    std::cout << " |--- Column Mux Dynamic Energy         = " << ToJoule(columnMuxReadDynamicEnergy) << std::endl;
    std::cout << " |--- Sense Amplifier Dynamic Energy    = " << ToJoule(subarray.senseAmp->readDynamicEnergy) << std::endl;
    std::cout << " |--- MUX of SA Dynamic Energy          = " << ToJoule(subarray.senseAmpMuxLev1->readDynamicEnergy + subarray.senseAmpMuxLev2->readDynamicEnergy) << std::endl;
    if (config.peripherals.withOutputAcc) {
        std::cout << " |--- Output Accumulator Dynamic Energy = " << ToJoule(subarray.outputAcc->readDynamicEnergy) << std::endl;
    }
    if (config.peripherals.withPriorityEnc) {
        std::cout << " |--- Priority Encoder Dynamic Energy   = " << ToJoule(subarray.priorityEnc->readDynamicEnergy) << std::endl;
    }
    std::cout << std::endl;
}

void PrintWriteEnergyBreakdown(const Bank &bank, const CAM_SubArray &subarray) {
    const EvaCamConfig &config = *bank.config;

    std::cout << "========= Write Dynamic Energy Breakdown =========" << std::endl;
    std::cout << " |--- Cell Reset Energy                  = " << ToJoule(subarray.cellResetEnergy) << std::endl;
    std::cout << " |--- Cell Set Energy                    = " << ToJoule(subarray.cellSetEnergy) << std::endl;
    std::cout << " |--- Input Encoder Dynamic Energy       = " << ToJoule(subarray.inputEnc->writeDynamicEnergy) << std::endl;
    std::cout << " |--- Row Decoder Dynamic Energy         = " << ToJoule(subarray.RowDecMergeNand->writeDynamicEnergy) << std::endl;
    const double rowWriteDynamicEnergy = SumRowDrivers(
            subarray, config.technology.cell->camNumRow,
            [](const auto &rowDriver) { return rowDriver.writeDynamicEnergy * 16; });
    std::cout << " |--- RowDriver Dynamic Energy           = " << ToJoule(rowWriteDynamicEnergy) << std::endl;
    std::cout << " |--- Precharger Dynamic Energy          = " << ToJoule(subarray.precharger->writeDynamicEnergy) << std::endl;
    // TODO: not really the breakdown
    const double columnWriteDynamicEnergy = SumColumnMuxes(
            subarray, config.technology.cell->camNumCol,
            [](const auto &columnMux) { return columnMux.writeDynamicEnergy; });
    std::cout << " |--- Column Mux Dynamic Energy          = " << ToJoule(columnWriteDynamicEnergy) << std::endl;
    std::cout << " |--- Sense Amplifier Dynamic Energy     = " << ToJoule(subarray.senseAmp->writeDynamicEnergy) << std::endl;
    std::cout << " |--- MUX of SA Dynamic Energy           = " << ToJoule(subarray.senseAmpMuxLev1->writeDynamicEnergy + subarray.senseAmpMuxLev2->writeDynamicEnergy) << std::endl;
    if (config.peripherals.withOutputAcc) {
        std::cout << " |--- Output Accumulator Dynamic Energy  = " << ToJoule(subarray.outputAcc->writeDynamicEnergy) << std::endl;
    }
    if (config.peripherals.withPriorityEnc) {
        std::cout << " |--- Priority Encoder Dynamic Energy    = " << ToJoule(subarray.priorityEnc->writeDynamicEnergy) << std::endl;
    }
    std::cout << std::endl;
}

void PrintLeakageBreakdown(const Bank &bank, const CAM_SubArray &subarray) {
    const EvaCamConfig &config = *bank.config;

    std::cout << "============ Leakage Breakdown ============" << std::endl;
    std::cout << " |--- Input Encoder Leakage      = " << ToWatt(subarray.inputEnc->leakage) << std::endl;
    std::cout << " |--- Row Decoder Leakage        = " << ToWatt(subarray.RowDecMergeNand->leakage) << std::endl;
    double rowDriverLeakage = SumRowDrivers(
            subarray, config.technology.cell->camNumRow,
            [](const auto &rowDriver) { return rowDriver.leakage; });
    std::cout << " |--- Row Driver Leakage         = " << ToWatt(rowDriverLeakage) << std::endl;
    std::cout << " |--- Precharger Leakage         = " << ToWatt(subarray.precharger->leakage) << std::endl;
    double columnMuxLeakage = SumColumnMuxes(
            subarray, config.technology.cell->camNumCol,
            [](const auto &columnMux) { return columnMux.leakage; });
    std::cout << " |--- Column Mux Leakage         = " << ToWatt(columnMuxLeakage) << std::endl;
    std::cout << " |--- Sense Amplifier Leakage    = " << ToWatt(subarray.senseAmp->leakage) << std::endl;
    std::cout << " |--- MUX of SA Leakage          = " << ToWatt(subarray.senseAmpMuxLev1->leakage + subarray.senseAmpMuxLev2->leakage) << std::endl;
    if (config.peripherals.withOutputAcc) {
        std::cout << " |--- Output Accumulator Leakage = " << ToWatt(subarray.outputAcc->leakage) << std::endl;
    }
    if (config.peripherals.withPriorityEnc) {
        std::cout << " |--- Priority Encoder Leakage   = " << ToWatt(subarray.priorityEnc->leakage) << std::endl;
    }
}

}  // namespace

void Bank::PrintProperty() {
    std::cout << "Bank Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}

bool Bank::match(const std::vector<int> &stored, const std::vector<int> &query) const {
    return evaluate(stored, query).hit;
}

EvaCAMMatchResult Bank::evaluate(const std::vector<int> &stored, const std::vector<int> &query) const {
    if (!mat || !mat->subarray) {
        throw std::runtime_error("[Bank] Error: bank is not initialized for matching.");
    }

    return mat->subarray->EvaluateBinaryMatch(stored, query);
}

void Bank::printbreakdown() {
    const CAM_SubArray &subarray = *mat->subarray;

    PrintSubarrayAreaBreakdown(*this, subarray);
    PrintSearchLatencyBreakdown(*this, subarray);
    PrintSearchEnergyBreakdown(*this, subarray);
    PrintWriteEnergyBreakdown(*this, subarray);
    PrintLeakageBreakdown(*this, subarray);
}

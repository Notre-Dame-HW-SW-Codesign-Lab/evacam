#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "model/NandCamBank.h"

namespace {

bool ValidPartition(int total, int active) {
    return total > 0 && active > 0 && active <= total && total % active == 0;
}

} // namespace

void NandCamBank::Initialize(int rows, int columns, long long capacityCells,
        long wordWidth, int activeColumns, int activeRows,
        int senseMux, bool internalSense, int outputMux1, int outputMux2,
        int subRows, int subColumns, int activeSubColumns, int activeSubRows,
        BufferDesignTarget optimization, CAMType type, SearchFunction search,
        std::shared_ptr<EvaCamConfig> configuration, const Wire &local,
        const Wire &global, const CAM_Opt &options) {
    initialized = false;
    invalid = false;
    config = std::move(configuration);
    localWire = local;
    globalWire = global;
    numRowMat = rows;
    numColumnMat = columns;
    numActiveMatPerRow = activeColumns;
    numActiveMatPerColumn = activeRows;
    numRowSubarray = subRows;
    numColumnSubarray = subColumns;
    numActiveSubarrayPerRow = activeSubColumns;
    numActiveSubarrayPerColumn = activeSubRows;
    capacity = capacityCells;
    blockSize = wordWidth;
    muxSenseAmp = senseMux;
    muxOutputLev1 = outputMux1;
    muxOutputLev2 = outputMux2;
    internalSenseAmp = internalSense;
    areaOptimizationLevel = optimization;
    camType = type;
    searchFunction = search;
    CAM_opt = options;
    CAM_opt.NormalizeComparisonColumns();
    numBitSerial = static_cast<int>(wordWidth);
    if (!config || !config->technology.cell || !config->technology.cell->nandString
            || !config->runtimeSizing.hasFixedSubarrayDimensions
            || type != TCAM || search != EX || !internalSense
            || outputMux1 != 1 || outputMux2 != 1
            || !ValidPartition(rows, activeRows) || !ValidPartition(columns, activeColumns)
            || !ValidPartition(subRows, activeSubRows) || !ValidPartition(subColumns, activeSubColumns)
            || wordWidth != config->input.wordWidth
            || CAM_opt.ComparisonColumns != wordWidth) {
        throw std::invalid_argument("[NAND bank] Unsupported topology, geometry, or search operation.");
    }
    const long double count = static_cast<long double>(rows) * columns * subRows * subColumns;
    if (count > 65536) {
        throw std::invalid_argument("[NAND bank] At most 65536 physical blocks are supported.");
    }
    totalBlocks = static_cast<long long>(count);
    blockRounds = static_cast<long long>(rows / activeRows) * (columns / activeColumns)
        * (subRows / activeSubRows) * (subColumns / activeSubColumns);
    if (config->input.pageSize < 8 || config->input.pageSize > 1048576) {
        throw std::invalid_argument("[NAND bank] Physical page requires 8..1048576 strings.");
    }
    const long long entriesPerBlock = config->technology.cell->memCellType == NAND3D
        ? config->runtimeSizing.fixedSubarrayRows : config->input.pageSize;
    if (entriesPerBlock <= 0 || entriesPerBlock > 1048576) {
        throw std::invalid_argument("[NAND bank] Entries per block must be between 1 and 1048576.");
    }
    const long long expectedEntries = totalBlocks * entriesPerBlock;
    if (wordWidth <= 0 || expectedEntries > std::numeric_limits<long long>::max() / wordWidth
            || capacityCells != expectedEntries * wordWidth) {
        throw std::invalid_argument("[NAND bank] Logical capacity does not match complete keys in all blocks.");
    }
    mat = std::make_unique<Mat>();
    mat->Initialize(subRows, subColumns, 1, wordWidth, false,
            activeSubColumns, activeSubRows, senseMux, internalSense, outputMux1,
            outputMux2, optimization, type, search, config, localWire, CAM_opt);
    initialized = true;
    invalid = mat->invalid;
    if (invalid) return;
    CalculateArea();
    CalculateLatencyAndPower();
}

void NandCamBank::BuildRoutes() {
    routeDelay = routeQueryEnergy = routeResultEnergy = routeLeakage = routeArea = 0;
    programRouteEnergy = eraseRouteEnergy = 0;
    const auto &metrics = mat->subarray->nandModel->Metrics();
    const int rows = numRowMat * numRowSubarray;
    const int columns = numColumnMat * numColumnSubarray;
    const double queryBits = metrics.queryBitCount > 0
        ? static_cast<double>(metrics.queryBitCount) : 2.0 * (metrics.keyWidth + 1);
    const double resultBits = static_cast<double>(metrics.entries);
    double maximumPathEnergy = 0;

    const auto segment = [&](double length, long long descendants,
            double &delay, double &energy) {
        double leakagePower = 0;
        delay = energy = 0;
        if (length <= 0) return;
        globalWire.CalculateLatencyAndPower(length, &delay, &energy, &leakagePower);
        const double outputBits = resultBits * descendants;
        routeQueryEnergy += energy * queryBits;
        routeResultEnergy += energy * outputBits;
        routeLeakage += leakagePower * (queryBits + outputBits);
        if (globalWire.wireRepeaterType != repeated_none) {
            const double repeaters = std::ceil(length / globalWire.repeaterSpacing);
            routeArea += repeaters * globalWire.repeaterWidth * globalWire.repeaterHeight
                * (queryBits + outputBits);
        }
    };

    if (config->input.routingMode == non_h_tree) {
        for (int row = 0; row < rows; ++row) {
            for (int column = 0; column < columns; ++column) {
                // Interface at the first block: dedicated Manhattan routes.
                const double length = row * metrics.height + column * metrics.width;
                double delay = 0, energy = 0;
                segment(length, 1, delay, energy);
                routeDelay = std::max(routeDelay, delay);
                maximumPathEnergy = std::max(maximumPathEnergy, energy);
            }
        }
    } else if (config->input.routingMode == h_tree) {
        // Binary orthogonal tree with shared query wires and separate return
        // bits. Each split spans one quarter of its current rectangle.
        const auto walk = [&](const auto &self, int rowCount, int columnCount,
                double pathDelay, double pathEnergy) -> void {
            if (rowCount == 1 && columnCount == 1) {
                routeDelay = std::max(routeDelay, pathDelay);
                maximumPathEnergy = std::max(maximumPathEnergy, pathEnergy);
                return;
            }
            const bool horizontal = columnCount > 1
                && (rowCount == 1 || columnCount * metrics.width >= rowCount * metrics.height);
            const int extent = horizontal ? columnCount : rowCount;
            const int first = extent / 2;
            const int second = extent - first;
            for (int child : {first, second}) {
                const int childRows = horizontal ? rowCount : child;
                const int childColumns = horizontal ? child : columnCount;
                const double length = (extent - child) * 0.5
                    * (horizontal ? metrics.width : metrics.height);
                double delay = 0, energy = 0;
                segment(length, static_cast<long long>(childRows) * childColumns, delay, energy);
                self(self, childRows, childColumns, pathDelay + delay, pathEnergy + energy);
            }
        };
        walk(walk, rows, columns, 0, 0);
    } else {
        throw std::invalid_argument("[NAND bank] Unsupported routing type.");
    }
    // One command plus address bits. Only programming transfers a page payload;
    // erasing addresses a block. Search returns one bit per logical entry.
    const double blockAddressBits = std::ceil(std::log2(static_cast<double>(totalBlocks)));
    const double wordlineAddressBits = std::ceil(std::log2(
            static_cast<double>(metrics.physicalBlockBits / metrics.physicalPageBits)));
    programRouteEnergy = maximumPathEnergy
        * (metrics.physicalPageBits + blockAddressBits + wordlineAddressBits + 1);
    eraseRouteEnergy = maximumPathEnergy * (blockAddressBits + 1);
}

void NandCamBank::CalculateArea() {
    if (!initialized) ThrowInitializationError("[NAND bank]");
    if (invalid) return;
    BuildRoutes();
    width = mat->width * numColumnMat;
    height = mat->height * numRowMat;
    area = width * height + routeArea;
    height = area / width;
    if (!std::isfinite(area) || !std::isfinite(width) || !std::isfinite(height)) {
        throw std::invalid_argument("[NAND bank] Non-finite aggregate area.");
    }
}

void NandCamBank::CalculateRC() {
    if (!initialized) ThrowInitializationError("[NAND bank]");
    if (!invalid) BuildRoutes();
}

void NandCamBank::CalculateLatencyAndPower() {
    if (!initialized) ThrowInitializationError("[NAND bank]");
    if (invalid) return;
    mat->CalculateLatency(0);
    mat->CalculatePower();
    const auto &metrics = mat->subarray->nandModel->Metrics();
    searchLatency = blockRounds * (metrics.searchLatency + 2 * routeDelay);
    // Conservative scheduling broadcasts query rails every block round;
    // each stored entry's result is transferred once per complete search.
    searchDynamicEnergy = totalBlocks * metrics.searchEnergy
        + blockRounds * routeQueryEnergy + routeResultEnergy;
    readLatency = readDynamicEnergy = cellReadEnergy = 0;
    writeLatency = setLatency = metrics.programPageLatency + routeDelay;
    resetLatency = metrics.eraseBlockLatency + routeDelay;
    writeDynamicEnergy = setDynamicEnergy = metrics.programPageEnergy + programRouteEnergy;
    resetDynamicEnergy = metrics.eraseBlockEnergy + eraseRouteEnergy;
    cellSetEnergy = metrics.programPageEnergy;
    cellResetEnergy = metrics.eraseBlockEnergy;
    leakage = totalBlocks * metrics.leakage + routeLeakage;
    for (double metric : {searchLatency, searchDynamicEnergy, writeLatency,
            resetLatency, writeDynamicEnergy, resetDynamicEnergy, leakage}) {
        if (!std::isfinite(metric) || metric < 0) {
            throw std::invalid_argument("[NAND bank] Non-finite aggregate timing or power.");
        }
    }
}

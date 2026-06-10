#include "BankWithHtree.h"


BankWithHtree::BankWithHtree() {
    initialized = false;
    invalid = false;
}

void BankWithHtree::MarkInvalid(const char *reason) {
    invalid = true;
    config->logger.Verbose() << reason;
}

void BankWithHtree::MarkInvalidInitialized(const char *reason) {
    MarkInvalid(reason);
    initialized = true;
}

int BankWithHtree::TotalHorizontalBits(int level) const {
    return horizontalLevels[level].addressBits
        + horizontalLevels[level].dataDistributeBits
        + horizontalLevels[level].dataBroadcastBits;
}

int BankWithHtree::TotalVerticalBits(int level) const {
    return verticalLevels[level].addressBits
        + verticalLevels[level].dataDistributeBits
        + verticalLevels[level].dataBroadcastBits;
}

BankWithHtree::WireAreaModel BankWithHtree::GetWireAreaModel() const {
    WireAreaModel model;
    if (globalWire.wireRepeaterType == repeated_none) {
        model.sharingWidth = 1;
        model.effectivePitch = 0;
    } else {
        model.sharingWidth = (int)floor(globalWire.repeaterSpacing / globalWire.repeaterHeight);
        model.effectivePitch = globalWire.repeatedWirePitch;
    }
    return model;
}

void BankWithHtree::AccumulateHtreeLevelLatencyAndPower(const HtreeLevel &level,
        int totalBits, int beta) {
    double latency;
    double energy;
    double leakageWire;

    globalWire.CalculateLatencyAndPower(level.length, &latency, &energy, &leakageWire);
    readLatency += latency * 2;						/* 2 due to in/out */
    writeLatency += latency;						/* only in */
    resetLatency += latency;
    setLatency += latency;

    /* Read and write energy for H-tree should be the same; each wire is
     * activated on exactly one way.
     */
    readDynamicEnergy += energy * level.activeWireGroups * totalBits;
    writeDynamicEnergy += energy * level.activeWireGroups * totalBits / beta;
    resetDynamicEnergy += energy * level.activeWireGroups * totalBits / beta;
    setDynamicEnergy += energy * level.activeWireGroups * totalBits / beta;
    leakage += leakageWire * level.totalWireGroups * totalBits;

    if (config->input.designTarget == CAM_chip) {
        if (!config->peripherals.noPrechargeInc)
            searchLatency += latency * 2;
        searchDynamicEnergy += mat->subarray->searchDynamicEnergy
            + energy * level.activeWireGroups * totalBits;
    }
}

bool BankWithHtree::HasRoutableBits(const RoutingState &state) const {
    return state.dataDistributeBitsToRoute + state.dataBroadcastBitsToRoute != 0
        && state.addressBitsToRoute != 0;
}

BankWithHtree::RoutingState BankWithHtree::CreateInitialRoutingState() const {
    RoutingState state;
    state.horizontalLevelsRemaining = levelHorizontal;
    state.verticalLevelsRemaining = levelVertical;
    state.activeRowsRemaining = numActiveMatPerColumn;
    state.activeColumnsRemaining = numActiveMatPerRow;
    state.addressBitsToRoute = numAddressBit;
    state.dataDistributeBitsToRoute = numDataDistributeBit;
    state.dataBroadcastBitsToRoute = numDataBroadcastBit;
    return state;
}

bool BankWithHtree::InitializeFirstHorizontalLevel(RoutingState &state) {
    if (state.horizontalLevelsRemaining <= 0) {
        return true;
    }
    if (!HasRoutableBits(state)) {
        MarkInvalidInitialized("H>0");
        return false;
    }

    horizontalLevels[0].addressBits = state.addressBitsToRoute;
    horizontalLevels[0].dataDistributeBits = state.dataDistributeBitsToRoute;
    horizontalLevels[0].dataBroadcastBits = state.dataBroadcastBitsToRoute;
    horizontalLevels[0].wireGroups = 1;
    horizontalLevels[0].totalWireGroups = 1;
    horizontalLevels[0].activeWireGroups = 1;
    state.horizontalLevelsRemaining--;
    return true;
}

bool BankWithHtree::ReduceExtraHorizontalLevels(RoutingState &state) {
    while (state.horizontalLevelsRemaining > state.verticalLevelsRemaining) {
        if (!HasRoutableBits(state)) {
            MarkInvalidInitialized("H>V");
            return false;
        }

        int level = levelHorizontal - state.horizontalLevelsRemaining;
        if (state.activeColumnsRemaining > 1) {
            state.dataDistributeBitsToRoute /= 2;
            state.activeColumnsRemaining /= 2;
            horizontalLevels[level].activeWireGroups =
                2 * horizontalLevels[level - 1].activeWireGroups;
        } else {
            state.addressBitsToRoute--;
            horizontalLevels[level].activeWireGroups = horizontalLevels[level - 1].activeWireGroups;
        }

        horizontalLevels[level].addressBits = state.addressBitsToRoute;
        horizontalLevels[level].dataDistributeBits = state.dataDistributeBitsToRoute;
        horizontalLevels[level].dataBroadcastBits = state.dataBroadcastBitsToRoute;
        horizontalLevels[level].wireGroups = 1;
        horizontalLevels[level].totalWireGroups = 2 * horizontalLevels[level - 1].totalWireGroups;
        state.horizontalLevelsRemaining--;
        state.verticalWireTier *= 2;
    }
    return true;
}

bool BankWithHtree::ReduceExtraVerticalLevels(RoutingState &state) {
    while (state.verticalLevelsRemaining > state.horizontalLevelsRemaining) {
        if (!HasRoutableBits(state)) {
            MarkInvalidInitialized("V>H");
            return false;
        }

        int level = levelVertical - state.verticalLevelsRemaining;
        if (state.activeRowsRemaining > 1) {
            state.dataDistributeBitsToRoute /= 2;
            state.activeRowsRemaining /= 2;
            if (state.verticalLevelsRemaining == levelVertical) {
                verticalLevels[0].activeWireGroups = 2;
            } else {
                verticalLevels[level].activeWireGroups =
                    2 * verticalLevels[level - 1].activeWireGroups;
            }
        } else {
            state.addressBitsToRoute--;
            if (state.verticalLevelsRemaining == levelVertical) {
                verticalLevels[0].activeWireGroups = 1;
            } else {
                verticalLevels[level].activeWireGroups = verticalLevels[level - 1].activeWireGroups;
            }
        }

        verticalLevels[level].addressBits = state.addressBitsToRoute;
        verticalLevels[level].dataDistributeBits = state.dataDistributeBitsToRoute;
        verticalLevels[level].dataBroadcastBits = state.dataBroadcastBitsToRoute;
        verticalLevels[level].wireGroups = 1;
        if (state.verticalLevelsRemaining == levelVertical) {
            verticalLevels[0].totalWireGroups = 2;
        } else {
            verticalLevels[level].totalWireGroups = 2 * verticalLevels[level - 1].totalWireGroups;
        }
        state.verticalLevelsRemaining--;
        state.horizontalWireTier *= 2;
    }
    return true;
}

bool BankWithHtree::ReducePairedHorizontalAndVerticalLevels(RoutingState &state) {
    while (state.horizontalLevelsRemaining > 0) {
        if (!HasRoutableBits(state)) {
            MarkInvalidInitialized("Reduce H an V to zero");
            return false;
        }

        int horizontalLevel = levelHorizontal - state.horizontalLevelsRemaining;
        int verticalLevel = levelVertical - state.verticalLevelsRemaining;

        if (state.activeColumnsRemaining > 1) {
            state.dataDistributeBitsToRoute /= 2;
            state.activeColumnsRemaining /= 2;
            if (state.verticalLevelsRemaining == levelVertical) {
                horizontalLevels[horizontalLevel].activeWireGroups =
                    2 * horizontalLevels[horizontalLevel - 1].activeWireGroups;
            } else {
                horizontalLevels[horizontalLevel].activeWireGroups =
                    2 * verticalLevels[verticalLevel - 1].activeWireGroups;
            }
        } else {
            state.addressBitsToRoute--;
            if (state.verticalLevelsRemaining == levelVertical) {
                horizontalLevels[horizontalLevel].activeWireGroups =
                    horizontalLevels[horizontalLevel - 1].activeWireGroups;
            } else {
                horizontalLevels[horizontalLevel].activeWireGroups =
                    verticalLevels[verticalLevel - 1].activeWireGroups;
            }
        }

        horizontalLevels[horizontalLevel].addressBits = state.addressBitsToRoute;
        horizontalLevels[horizontalLevel].dataDistributeBits = state.dataDistributeBitsToRoute;
        horizontalLevels[horizontalLevel].dataBroadcastBits = state.dataBroadcastBitsToRoute;
        horizontalLevels[horizontalLevel].wireGroups = state.horizontalWireTier;

        if (state.verticalLevelsRemaining == levelVertical) {
            horizontalLevels[horizontalLevel].totalWireGroups =
                2 * horizontalLevels[horizontalLevel - 1].totalWireGroups;
        } else {
            horizontalLevels[horizontalLevel].totalWireGroups =
                2 * verticalLevels[verticalLevel - 1].totalWireGroups;
        }

        if (!HasRoutableBits(state)) {
            MarkInvalidInitialized("numDataDistributeBitToRoute + "
                    "numDataBroadcastBitToRoute == 0 || numAddressBitToRoute == 0");
            return false;
        }

        if (state.activeRowsRemaining > 1) {
            state.dataDistributeBitsToRoute /= 2;
            state.activeRowsRemaining /= 2;
            verticalLevels[verticalLevel].activeWireGroups =
                2 * horizontalLevels[horizontalLevel].activeWireGroups;
        } else {
            state.addressBitsToRoute--;
            verticalLevels[verticalLevel].activeWireGroups =
                horizontalLevels[horizontalLevel].activeWireGroups;
        }

        verticalLevels[verticalLevel].addressBits = state.addressBitsToRoute;
        verticalLevels[verticalLevel].dataDistributeBits = state.dataDistributeBitsToRoute;
        verticalLevels[verticalLevel].dataBroadcastBits = state.dataBroadcastBitsToRoute;
        if (levelHorizontal == 2) {
            verticalLevels[verticalLevel].wireGroups = state.verticalWireTier;
        } else {
            verticalLevels[verticalLevel].wireGroups = 2 * state.verticalWireTier;
        }
        verticalLevels[verticalLevel].totalWireGroups =
            2 * horizontalLevels[horizontalLevel].totalWireGroups;

        state.horizontalLevelsRemaining--;
        state.verticalLevelsRemaining--;
        state.horizontalWireTier *= 2;
        state.verticalWireTier *= 2;
    }
    return true;
}

bool BankWithHtree::FinalizeMatRoutingBits(RoutingState &state) {
    if (!HasRoutableBits(state)) {
        MarkInvalidInitialized("numDataDistributeBitToRoute");
        return false;
    }

    if (state.activeColumnsRemaining > 1) {
        state.dataDistributeBitsToRoute /= 2;
        state.activeColumnsRemaining /= 2;
    } else if (levelHorizontal > 0) {
        state.addressBitsToRoute--;
    }

    return true;
}

bool BankWithHtree::DetermineMatRouting(const RoutingState &state, MatRouting &matRouting) {
    if (memoryType == mem_data) {
        if (numRowPerSet > (int)pow(2, state.dataBroadcastBitsToRoute)) {
            MarkInvalidInitialized("no multiple rows");
            return false;
        }

        matRouting.blockSize = state.dataDistributeBitsToRoute;
        matRouting.numWay = (int)pow(2, state.dataBroadcastBitsToRoute);

        int numWayPerRow = matRouting.numWay / numRowPerSet;
        if (numWayPerRow > 1) {
            int numWayPerRowInLog = (int)(log2((double)numWayPerRow) + 0.1);
            if (config->technology.cell->memCellType == DRAM
                    || config->technology.cell->memCellType == eDRAM) {
                int extraMuxOutputLev2 = (int)pow(2, numWayPerRowInLog / 2);
                int extraMuxOutputLev1 = numWayPerRow / extraMuxOutputLev2;
                muxOutputLev1 *= extraMuxOutputLev1;
                muxOutputLev2 *= extraMuxOutputLev2;
            } else {
                int extraMuxOutputLev2 = (int)pow(2, numWayPerRowInLog / 3);
                int extraMuxOutputLev1 = extraMuxOutputLev2;
                int extraMuxSenseAmp = numWayPerRow / extraMuxOutputLev1 / extraMuxOutputLev2;
                muxSenseAmp *= extraMuxSenseAmp;
                muxOutputLev1 *= extraMuxOutputLev1;
                muxOutputLev2 *= extraMuxOutputLev2;
            }
        }
    } else {
        matRouting.blockSize = state.dataBroadcastBitsToRoute;
        matRouting.numWay = 1;
    }

    return true;
}

void BankWithHtree::Initialize(int _numRowMat, int _numColumnMat, long long _capacity,
        long _blockSize, int _associativity, int _numRowPerSet, int _numActiveMatPerRow,
        int _numActiveMatPerColumn, int _muxSenseAmp, bool _internalSenseAmp, int _muxOutputLev1,
        int _muxOutputLev2, int _numRowSubarray, int _numColumnSubarray,
        int _numActiveSubarrayPerRow, int _numActiveSubarrayPerColumn,
        BufferDesignTarget _areaOptimizationLevel, MemoryType _memoryType, CAMType _camType, 
        SearchFunction _searchFunction, std::shared_ptr<EvaCamConfig> _config,
        const Wire &_localWire, const Wire &_globalWire, const CAM_Opt &_CAM_opt) {

    config = _config;
    localWire = _localWire;
    globalWire = _globalWire;

    if (initialized) {
        initialized = false;
        invalid = false;
    }

    if (!_internalSenseAmp) {
        MarkInvalid("[Bank] Htree organization does not support external sense "
                "amplification scheme");
        return;
    }
    if (initialized)
        config->logger.Verbose() << "[Bank] Warning: Already initialized!";

    numRowMat = _numRowMat;
    numColumnMat = _numColumnMat;
    capacity = _capacity;
    blockSize = _blockSize;
    associativity = _associativity;
    numRowPerSet = _numRowPerSet;
    internalSenseAmp = _internalSenseAmp;
    areaOptimizationLevel = _areaOptimizationLevel;
    memoryType = _memoryType;
    camType = _camType;
    searchFunction = _searchFunction;
    CAM_opt = _CAM_opt;

    /* Calculate the physical signals that are required in routing */
    numAddressBit = (int)(log2((double)capacity / blockSize / associativity) + 0.1);

    /* use double during the calculation to avoid overflow */
    if (memoryType == mem_data) {
        numDataDistributeBit = blockSize;
        numDataBroadcastBit = (int)(log2(associativity));	/* TODO: this is not the only way */

    } else {	/* CAM */
        numDataDistributeBit = 0;
        numDataBroadcastBit = blockSize;
    }

    if (_numActiveMatPerRow > numColumnMat) {
        config->logger.Log() << "[Bank] Warning: The number of active subarray "
            "per row is larger than the number of subarray per row!";
        config->logger.Log() << _numActiveMatPerRow << " > " << numColumnMat;
        numActiveMatPerRow = numColumnMat;

    } else {
        numActiveMatPerRow = _numActiveMatPerRow;
    }

    if (_numActiveMatPerColumn > numRowMat) {
        config->logger.Log() << "[Bank] Warning: The number of active subarray "
            "per column is larger than the number of subarray per column!";
        config->logger.Log() << _numActiveMatPerColumn << " > " << numRowMat;
        numActiveMatPerColumn = numRowMat;

    } else {
        numActiveMatPerColumn = _numActiveMatPerColumn;
    }

    muxSenseAmp = _muxSenseAmp;
    muxOutputLev1 = _muxOutputLev1;
    muxOutputLev2 = _muxOutputLev2;
    numRowSubarray = _numRowSubarray;
    numColumnSubarray = _numColumnSubarray;

    if (_numActiveSubarrayPerRow > numColumnSubarray) {
        config->logger.Log() << "[Bank] Warning: The number of active subarray "
            "per row is larger than the number of subarray per row!";
        config->logger.Log() << _numActiveSubarrayPerRow << " > " << numColumnSubarray;
        numActiveSubarrayPerRow = numColumnSubarray;

    } else {
        numActiveSubarrayPerRow = _numActiveSubarrayPerRow;
    }

    if (_numActiveSubarrayPerColumn > numRowSubarray) {
        config->logger.Log() << "[Bank] Warning: The number of active subarray "
            "per column is larger than the number of subarray per column!";
        config->logger.Log() << _numActiveSubarrayPerColumn << " > " << numRowSubarray;
        numActiveSubarrayPerColumn = numRowSubarray;

    } else {
        numActiveSubarrayPerColumn = _numActiveSubarrayPerColumn;
    }

    levelHorizontal = (int)(log2(numColumnMat)+0.1);
    levelVertical = (int)(log2(numRowMat)+0.1);

    if (levelHorizontal > 0) {
        horizontalLevels.resize(levelHorizontal);
    }

    if (levelVertical > 0) {
        verticalLevels.resize(levelVertical);	
    }

    RoutingState state = CreateInitialRoutingState();
    if (!InitializeFirstHorizontalLevel(state)
            || !ReduceExtraHorizontalLevels(state)
            || !ReduceExtraVerticalLevels(state)
            || !ReducePairedHorizontalAndVerticalLevels(state)
            || !FinalizeMatRoutingBits(state)) {
        return;
    }

    MatRouting matRouting;
    if (!DetermineMatRouting(state, matRouting)) {
        return;
    }

    mat = std::make_unique<Mat>();
    mat->Initialize(numRowSubarray, numColumnSubarray, state.addressBitsToRoute,
            matRouting.blockSize, matRouting.numWay, numRowPerSet, false,
            numActiveSubarrayPerRow, numActiveSubarrayPerColumn,
            muxSenseAmp, internalSenseAmp, muxOutputLev1, muxOutputLev2, areaOptimizationLevel, 
            memoryType, camType, searchFunction, config, localWire, CAM_opt);

    /* Check if mat is under a legal configuration */
    if (mat->invalid) {
        MarkInvalidInitialized("[Bank] invalid mat configurations!");
        return;
    }

    /* Reset the mux values for correct printing */
    muxSenseAmp = _muxSenseAmp;
    muxOutputLev1 = _muxOutputLev1;
    muxOutputLev2 = _muxOutputLev2;

    initialized = true;
    CalculateArea();
    CalculateLatencyAndPower();
}

void BankWithHtree::CalculateArea() {
    if (invalid) {
        height = width = area = 1e41;
    } else {
        height = mat->height * numRowMat;
        width = mat->width * numColumnMat;

        WireAreaModel wireArea = GetWireAreaModel();

        for (int i = 0; i < levelHorizontal; i++) {
            height += ceil((double)TotalHorizontalBits(i)
                    * horizontalLevels[i].wireGroups / wireArea.sharingWidth)
                * wireArea.effectivePitch ;
        }

        for (int i = 0; i < levelVertical; i++) {
            width += ceil((double)TotalVerticalBits(i)
                    * verticalLevels[i].wireGroups / wireArea.sharingWidth)
                * wireArea.effectivePitch;
        }

        /* Determine if the aspect ratio meets the constraint */
        if (memoryType == mem_data)
            if (height / width > CONSTRAINT_ASPECT_RATIO_BANK
                    || width / height > CONSTRAINT_ASPECT_RATIO_BANK) {
                /* illegal */
                MarkInvalid("aspect ratio doesn't meet the constraint");
                config->logger.Verbose() << "height / width = " << height / width
                    << "|| width / height = " << width / height;
                height = width = area = 1e41;
                return;
            }

        area = height * width;

        /* Calculate the length of each H-tree wire */
        int h = levelHorizontal - 1;
        int v = levelVertical - 1;
        while (v > h) {
            if (v == levelVertical - 1)
                verticalLevels[v].length = mat->height / 2;
            else
                verticalLevels[v].length = verticalLevels[v + 1].length * 2;
            v--;
        }

        double numHorizontalBitToRoute = 0;
        double numVerticalBitToRoute = 0;

        while (v >= 0) {
            if (v == levelVertical - 1) {
                verticalLevels[v].length = mat->height / 2;
            } else {
                if (h == levelHorizontal - 1)
                    verticalLevels[v].length = verticalLevels[v + 1].length * 2;
                else {
                    numHorizontalBitToRoute = TotalHorizontalBits(h + 1);
                    verticalLevels[v].length = verticalLevels[v + 1].length * 2
                        + ceil((double)numHorizontalBitToRoute / wireArea.sharingWidth)
                        * wireArea.effectivePitch / 2;
                }
            }

            if (h == levelHorizontal - 1) {
                horizontalLevels[h].length = mat->width;
                for (int i = v; i < levelVertical; i++) {
                    numVerticalBitToRoute = TotalVerticalBits(i);
                    horizontalLevels[h].length += ceil((double)numVerticalBitToRoute
                            / wireArea.sharingWidth) * wireArea.effectivePitch / 2;
                }
            } else {
                numVerticalBitToRoute = TotalVerticalBits(v);
                horizontalLevels[h].length = horizontalLevels[h + 1].length * 2
                    + ceil((double)numVerticalBitToRoute / wireArea.sharingWidth)
                    * wireArea.effectivePitch / 2;
            }

            v--;
            h--;
        }

        while (h >=  0) {
            if (h == levelHorizontal - 1)
                horizontalLevels[h].length = mat->width;
            else
                horizontalLevels[h].length = horizontalLevels[h + 1].length * 2;
            h--;
        }
    }
}

void BankWithHtree::CalculateRC() {
    if (!initialized) {
        throw std::runtime_error("[Bank] Error: Require initialization first!");
    } else if (!invalid) {
        /* No bank-level RC model is required for H-tree banks.
         * Mat RC is calculated during Mat::Initialize(), and H-tree wire
         * effects are handled by Wire::CalculateLatencyAndPower().
         */
    } else { 
        // If this happens, the size parameters provided from the loop were invalid and skipped.
        config->logger.Verbose()
            << "[Bank] Error: Cannot calculate RC while initialized and invalid.";
    }
}

void BankWithHtree::CalculateLatencyAndPower() {
    if (!initialized) {
        throw std::runtime_error("[Bank] Error: Require initialization first!");

    } else if (invalid) {
        readLatency = writeLatency = 1e41;
        readDynamicEnergy = writeDynamicEnergy = 1e41;
        leakage = 1e41;

    } else {
        /* For fast access mode cache, beta is equal to associativity, which
         * means only 1/beta interconnect wires are activated.
         */
        int beta = 1;

        mat->CalculateLatency(1e41 /* means Inf */);
        mat->CalculatePower();
        readLatency = mat->readLatency;
        writeLatency = mat->writeLatency;
        readDynamicEnergy = mat->readDynamicEnergy * numActiveMatPerRow * numActiveMatPerColumn;
        writeDynamicEnergy = mat->writeDynamicEnergy * numActiveMatPerRow * numActiveMatPerColumn;
        leakage = mat->leakage * numRowMat * numColumnMat;

        /* energy consumption on cells */
        cellReadEnergy = mat->cellReadEnergy * numActiveMatPerRow * numActiveMatPerColumn;
        cellSetEnergy = mat->cellSetEnergy * numActiveMatPerRow * numActiveMatPerColumn;
        cellResetEnergy = mat->cellResetEnergy * numActiveMatPerRow * numActiveMatPerColumn;

        /* for asymmetric RESET/SET only */
        resetLatency = mat->resetLatency;
        setLatency = mat->setLatency;
        resetDynamicEnergy = mat->resetDynamicEnergy * numActiveMatPerRow * numActiveMatPerColumn;
        setDynamicEnergy = mat->setDynamicEnergy * numActiveMatPerRow * numActiveMatPerColumn;

        if (config->input.designTarget == CAM_chip) {

            if (config->peripherals.noPrechargeInc) {
                searchLatency = mat->subarray->matchlineDelay
                    + mat->subarray->ColMux[mat->subarray->indexMatchline]->readLatency
                    + mat->subarray->senseAmpLatency + mat->subarray->outputAcc->readLatency;

                if (config->peripherals.withOutputAcc || mat->muxSenseAmp > 1) {
                    config->logger.Verbose()
                        << "[Bank] Warning: Bit serial or Mux on SA design, "
                        << "but latency only shows a single sense.";
                }

            } else {
                searchLatency = mat->subarray->searchLatency * mat->muxSenseAmp
                    - (mat->subarray->inputBuf->readLatency) * (mat->muxSenseAmp - 1);
                if (config->peripherals.withOutputAcc) {
                    searchLatency *= config->input.wordWidth / CAM_opt.BitSerialWidth;
                }
            }

            searchDynamicEnergy = mat->subarray->searchDynamicEnergy * mat->muxSenseAmp
                - (mat->subarray->inputBuf->readDynamicEnergy
                        + mat->subarray->inputEnc->readDynamicEnergy)
                * (mat->muxSenseAmp - 1);

            if (config->peripherals.withOutputAcc) {
                searchDynamicEnergy *= config->input.wordWidth / CAM_opt.BitSerialWidth;
            }

            numBitSerial = CAM_opt.BitSerialWidth;
        }

        for (int i = 0; i < levelHorizontal; i++) {
            AccumulateHtreeLevelLatencyAndPower(horizontalLevels[i], TotalHorizontalBits(i), beta);
        }

        for (int i = 0; i < levelVertical; i++) {
            AccumulateHtreeLevelLatencyAndPower(verticalLevels[i], TotalVerticalBits(i), beta);
        }
    }
}

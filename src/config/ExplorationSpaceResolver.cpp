#include "config/ExplorationSpaceResolver.h"

#include <stdexcept>

namespace {

void ValidateNotEmpty(const std::vector<int> &values, const char *label) {
    if (values.empty()) {
        throw std::runtime_error(std::string("Exploration domain resolved to no values: ") + label);
    }
}

}  // namespace

ResolvedExplorationSpace ExplorationSpaceResolver::Resolve(const ExplorationSpec &spec) {
    ResolvedExplorationSpace resolved;

    resolved.geometry.numRowMatValues = spec.geometry.numRowMat.Values();
    resolved.geometry.numColumnMatValues = spec.geometry.numColumnMat.Values();
    resolved.geometry.numRowSubarrayValues = spec.geometry.numRowSubarray.Values();
    resolved.geometry.numColumnSubarrayValues = spec.geometry.numColumnSubarray.Values();
    resolved.geometry.muxSenseAmpValues = spec.geometry.muxSenseAmp.Values();
    resolved.geometry.muxOutputLev1Values = spec.geometry.muxOutputLev1.Values();
    resolved.geometry.muxOutputLev2Values = spec.geometry.muxOutputLev2.Values();
    resolved.geometry.numRowPerSetValues = spec.geometry.numRowPerSet.Values();

    resolved.wires.localWireTypeValues = spec.wires.localWireType.Values();
    resolved.wires.globalWireTypeValues = spec.wires.globalWireType.Values();
    resolved.wires.localWireRepeaterTypeValues = spec.wires.localWireRepeaterType.Values();
    resolved.wires.globalWireRepeaterTypeValues = spec.wires.globalWireRepeaterType.Values();
    resolved.wires.isLocalWireLowSwingValues = spec.wires.isLocalWireLowSwing.Values();
    resolved.wires.isGlobalWireLowSwingValues = spec.wires.isGlobalWireLowSwing.Values();

    resolved.cam.areaOptimizationLevelValues = spec.cam.areaOptimizationLevel.Values();
    resolved.cam.rowDriverOptLevelValues = spec.cam.rowDriverOptLevel.Values();
    resolved.cam.priorityOptLevelValues = spec.cam.priorityOptLevel.Values();
    resolved.cam.bitSerialWidthValues = spec.cam.bitSerialWidth.Values();

    ValidateNotEmpty(resolved.geometry.numRowMatValues, "numRowMat");
    ValidateNotEmpty(resolved.geometry.numColumnMatValues, "numColumnMat");
    ValidateNotEmpty(resolved.geometry.numRowSubarrayValues, "numRowSubarray");
    ValidateNotEmpty(resolved.geometry.numColumnSubarrayValues, "numColumnSubarray");
    ValidateNotEmpty(resolved.geometry.muxSenseAmpValues, "muxSenseAmp");
    ValidateNotEmpty(resolved.geometry.muxOutputLev1Values, "muxOutputLev1");
    ValidateNotEmpty(resolved.geometry.muxOutputLev2Values, "muxOutputLev2");
    ValidateNotEmpty(resolved.geometry.numRowPerSetValues, "numRowPerSet");
    ValidateNotEmpty(resolved.wires.localWireTypeValues, "localWireType");
    ValidateNotEmpty(resolved.wires.globalWireTypeValues, "globalWireType");
    ValidateNotEmpty(resolved.wires.localWireRepeaterTypeValues, "localWireRepeaterType");
    ValidateNotEmpty(resolved.wires.globalWireRepeaterTypeValues, "globalWireRepeaterType");
    ValidateNotEmpty(resolved.wires.isLocalWireLowSwingValues, "isLocalWireLowSwing");
    ValidateNotEmpty(resolved.wires.isGlobalWireLowSwingValues, "isGlobalWireLowSwing");
    ValidateNotEmpty(resolved.cam.areaOptimizationLevelValues, "areaOptimizationLevel");
    ValidateNotEmpty(resolved.cam.rowDriverOptLevelValues, "rowDriverOptLevel");
    ValidateNotEmpty(resolved.cam.priorityOptLevelValues, "priorityOptLevel");
    ValidateNotEmpty(resolved.cam.bitSerialWidthValues, "bitSerialWidth");

    return resolved;
}

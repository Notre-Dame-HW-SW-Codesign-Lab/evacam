#ifndef WIRE_PROCESS_TABLE_H_
#define WIRE_PROCESS_TABLE_H_

#include "typedef.h"

struct WireProcessSpec {
    double featureSize;
    double barrierThickness;
    double horizontalDielectric;
    double wirePitch;
    double aspectRatio;
    double ildThickness;
    double copperResistivity;
};

bool FindWireProcessSpec(
        int featureSizeInNano,
        WireType wireType,
        WireProcessSpec *spec);

#endif /* WIRE_PROCESS_TABLE_H_ */

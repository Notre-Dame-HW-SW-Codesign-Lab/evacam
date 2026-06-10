#ifndef FORMULA_H_
#define FORMULA_H_

#include "Technology.h"
#include "constant.h"
#include <math.h>

#define MAX(a,b) (((a)> (b))?(a):(b))
#define MIN(a,b) (((a)< (b))?(a):(b))

bool isPow2(int n);

int intLog2(int n);

// Calculate the gate capacitance.
double CalculateGateCap(double width, const Technology &tech);

double CalculateGateArea(
        int gateType, int numInput,
        double widthNMOS, double widthPMOS,
        double heightTransistorRegion, const Technology &tech,
        double *height, double *width, bool UseUpdatedWidth);

// Calculate the capacitance of a gate.
void CalculateGateCapacitance(
        int gateType, int numInput,
        double widthNMOS, double widthPMOS,
        double heightTransistorRegion, const Technology &tech,
        double *capInput, double *capOutput);

double CalculateDrainCap(
        double width, int type,
        double heightTransistorRegion, const Technology &tech);

// Calculate the capacitance of a FBRAM.
double CalculateFBRAMGateCap(double width, double thicknessFactor, const Technology &tech);

double CalculateFBRAMDrainCap(double width, const Technology &tech);

double CalculateGateLeakage(
        int gateType, int numInput,
        double widthNMOS, double widthPMOS,
        double temperature, const Technology &tech);

double CalculateOnResistance(double width, int type, double temperature, const Technology &tech);

double CalculateTransconductance(double width, int type, const Technology &tech);

double horowitz(double tr, double beta, double rampInput, double *rampOutput);

double CalculateWireResistance(
        double resistivity, double wireWidth, double wireThickness,
        double barrierThickness, double dishingThickness, double alphaScatter);

double CalculateWireCapacitance(
        double permittivity, double wireWidth, double wireThickness, double wireSpacing,
        double ildThickness, double millerValue, double horizontalDielectric,
        double verticalDielectric, double fringeCap);


#endif // FORMULA_H_

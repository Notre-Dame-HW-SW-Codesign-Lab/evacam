#ifndef FORMULA_H_
#define FORMULA_H_

#include "Technology.h"
#include "constant.h"
#include <math.h>

#define MAX(a,b) (((a)> (b))?(a):(b))
#define MIN(a,b) (((a)< (b))?(a):(b))

bool isPow2(int n);

int intLog2(int n);

/* calculate the gate capacitance */
double CalculateGateCap(double width, std::shared_ptr<Technology> tech);

double CalculateGateArea(
		int gateType, int numInput,
		double widthNMOS, double widthPMOS,
		double heightTransistorRegion, std::shared_ptr<Technology> tech,
		double *height, double *width, bool UseUpdatedWidth);

/* calculate the capacitance of a gate */
void CalculateGateCapacitance(
		int gateType, int numInput,
		double widthNMOS, double widthPMOS,
		double heightTransistorRegion, std::shared_ptr<Technology> tech,
		double *capInput, double *capOutput);

double CalculateDrainCap(
		double width, int type,
		double heightTransistorRegion, std::shared_ptr<Technology> tech);

double CAM_CalculateSourceCap(
		double width, int type,
		double heightTransistorRegion, std::shared_ptr<Technology> tech);

/* calculate the capacitance of a FBRAM */
double CalculateFBRAMGateCap(double width, double thicknessFactor, std::shared_ptr<Technology> tech);

double CalculateFBRAMDrainCap(double width, std::shared_ptr<Technology> tech);

double CalculateGateLeakage(
		int gateType, int numInput,
		double widthNMOS, double widthPMOS,
		double temperature, std::shared_ptr<Technology> tech);

double CalculateOnResistance(double width, int type, double temperature, std::shared_ptr<Technology> tech);

double CalculateTransconductance(double width, int type, std::shared_ptr<Technology> tech);

double horowitz(double tr, double beta, double rampInput, double *rampOutput);

double CalculateWireResistance(
		double resistivity, double wireWidth, double wireThickness,
		double barrierThickness, double dishingThickness, double alphaScatter);

double CalculateWireCapacitance(
		double permittivity, double wireWidth, double wireThickness, double wireSpacing,
		double ildThickness, double millarValue, double horizontalDielectric,
		double verticalDielectic, double fringeCap);


#endif /* FORMULA_H_ */

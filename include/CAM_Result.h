#ifndef CAM_RESULT_H_
#define CAM_RESULT_H_

#include "BankWithHtree.h"
#include "BankWithoutHtree.h"
#include "Wire.h"
#include "Result.h"

class CAM_Result : public Result{
public:
	CAM_Result();
	//CAM_Result(const CAM_Result&) {}
	~CAM_Result();

	/* Functions */
	void print();
	//void printAsCache(Result &tagBank, CacheAccessMode cacheAccessMode);
	void reset();
	//void printToCsvFile(ofstream &outputFile);
	//void printAsCacheToCsvFile(Result &tagBank, CacheAccessMode cacheAccessMode, ofstream &outputFile);
	void compareAndUpdate(std::shared_ptr<Result> newResult);

        //std::shared_ptr<Bank> bank;
        //std::shared_ptr<Wire> localWire;
        //std::shared_ptr<Wire> globalWire;

//	OptimizationTarget optimizationTarget;	/* Exploration should not be assigned here */
//
//	Bank * bank;
//	Wire * localWire;		
//	Wire * globalWire;
//
//	double limitReadLatency;			/* The maximum allowable read latency, Unit: s */
//	double limitWriteLatency;			/* The maximum allowable write latency, Unit: s */
//	double limitReadDynamicEnergy;		/* The maximum allowable read dynamic energy, Unit: J */
//	double limitWriteDynamicEnergy;		/* The maximum allowable write dynamic energy, Unit: J */
//	double limitReadEdp;				/* The maximum allowable read EDP, Unit: s-J */
//	double limitWriteEdp;				/* The maximum allowable write EDP, Unit: s-J */
//	double limitArea;					/* The maximum allowable area, Unit: m^2 */
//	double limitLeakage;				/* The maximum allowable leakage power, Unit: W */


};

#endif /* CAM_RESULT_H_ */

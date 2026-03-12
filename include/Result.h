#ifndef RESULT_H_
#define RESULT_H_

#include <iosfwd>
#include <memory>

#include "Bank.h"
#include "Wire.h"
#include "typedef.h"

class EvaCamConfig;

class Result {
    public:
        Result() { initialized = false; }
        Result(const Result&) {}
        virtual ~Result() {};

        /* Functions */
        virtual void Initialize(std::shared_ptr<EvaCamConfig> _config);
        void print();
        void printAsCache(Result &tagBank, CacheAccessMode cacheAccessMode);
        void reset();
        void printToCsvFile(std::ofstream &outputFile);
        void printAsCacheToCsvFile(Result &tagBank, CacheAccessMode cacheAccessMode, std::ofstream &outputFile);
        void compareAndUpdate(std::shared_ptr<Result> newResult);

        OptimizationTarget optimizationTarget;	/* Exploration should not be assigned here */

        std::shared_ptr<EvaCamConfig> config;

        std::shared_ptr<Bank> bank;
        std::shared_ptr<Wire> localWire;		/* TODO: this one has the same name as one of the global variables */
        std::shared_ptr<Wire> globalWire;            // No more globals fixes this

        bool initialized;
        double limitReadLatency;			/* The maximum allowable read latency, Unit: s */
        double limitWriteLatency;			/* The maximum allowable write latency, Unit: s */
        double limitReadDynamicEnergy;		/* The maximum allowable read dynamic energy, Unit: J */
        double limitWriteDynamicEnergy;		/* The maximum allowable write dynamic energy, Unit: J */
        double limitReadEdp;				/* The maximum allowable read EDP, Unit: s-J */
        double limitWriteEdp;				/* The maximum allowable write EDP, Unit: s-J */
        double limitArea;					/* The maximum allowable area, Unit: m^2 */
        double limitLeakage;				/* The maximum allowable leakage power, Unit: W */
};

#endif /* RESULT_H_ */

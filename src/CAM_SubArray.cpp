/*
 * CAM_SubArray->cpp
 *
 */
#include "../include/CAM_SubArray.h"
#include "../include/formula.h"
#include "../include/global.h"
#include "../include/constant.h"
#include "../include/CAM_Line.h"
#include "../include/MemCell.h"
#include "../include/macros.h"

#include <math.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <random>

/*
CAM_SubArray::CAM_SubArray() {
	// TODO Auto-generated constructor stub
	initialized = false;
	invalid = false;
}

CAM_SubArray::~CAM_SubArray() {
	// TODO Auto-generated destructor stub
}
*/
void CAM_SubArray::Initialize(long long _numRow, long long _numColumn, bool _multipleRowPerSet, bool _split,
		int _muxSenseAmp, bool _internalSenseAmp, int _muxOutputLev1, int _muxOutputLev2,
		BufferDesignTarget _DecMergeOptLevel, BufferDesignTarget _DriverOptLevel,
		bool _withInputEnc, TypeOfInputEncoder _typeInputEnc, bool _customInputEnc,
		TypeOfSenseAmp _typeSenseAmp, bool _customSenseAmp, bool _withWriteDriver,
		bool _withOutputAcc, bool _withPriorityEnc, BufferDesignTarget _PriorityOptLevel,
		bool _withInputBuf, bool _withOutputBuf, CAMType _camType, SearchFunction _searchFunction, 
                bool _withVariation, std::shared_ptr<InputParameter> _inputParameter, std::shared_ptr<Wire> _localWire,
                std::shared_ptr<CAM_Opt> _CAM_opt) {
	if (initialized)
		std::cout << "[CAM_SubArray] Warning: Already initialized!" << std::endl;
	numRow = _numRow;
	numColumn = _numColumn;
	multipleRowPerSet = _multipleRowPerSet;
	split = _split;
	muxSenseAmp = _muxSenseAmp;
	muxOutputLev1 = _muxOutputLev1;
	muxOutputLev2 = _muxOutputLev2;
	internalSenseAmp = _internalSenseAmp;
	DecMergeOptLevel = _DecMergeOptLevel;
	DriverOptLevel = _DriverOptLevel;
	withInputEnc = _withInputEnc;
	typeInputEnc = _typeInputEnc;
	customInputEnc = _customInputEnc;
	withWriteDriver = _withWriteDriver;
	typeSenseAmp = _typeSenseAmp;
	customSenseAmp = _customSenseAmp;
	withOutputAcc = _withOutputAcc;
	withPriorityEnc = _withPriorityEnc;
	PriorityOptLevel = _PriorityOptLevel;
	withInputBuf = _withInputBuf;
	withOutputBuf = _withOutputBuf;
	camType = _camType;
	searchFunction = _searchFunction;
	withVariation = _withVariation;
        inputParameter = _inputParameter;
        localWire = _localWire;
        CAM_opt = _CAM_opt;

        rowDecoder = std::make_shared<RowDecoder>();
        bitlineMuxDecoder = std::make_shared<RowDecoder>();
        senseAmpMuxLev1Decoder = std::make_shared<RowDecoder>();
        senseAmpMuxLev2Decoder = std::make_shared<RowDecoder>();
        bitlineMux = std::make_shared<Mux>();

        inputBuf = std::make_shared<CAM_DataBuffer>();
        outputBuf = std::make_shared<CAM_DataBuffer>();
        inputLS = std::make_shared<CAM_LevelShifter>();
        outputLS = std::make_shared<CAM_LevelShifter>();
        inputEnc = std::make_shared<CAM_InputEncoder>();
        RowDecMergeNand = std::make_shared<CAM_RowNand>();

        precharger = std::make_shared<CAM_Precharger>();

        senseAmp = std::make_shared<CAM_SenseAmp>();

        ColDecMergeNand = std::make_shared<RowDecoder>();

        senseAmpMuxLev1Nand = std::make_shared<RowDecoder>();
        senseAmpMuxLev2Nand = std::make_shared<RowDecoder>();
        senseAmpMuxLev1 = std::make_shared<Mux>();
        senseAmpMuxLev2 = std::make_shared<Mux>();

        outputAcc = std::make_shared<CAM_OutputAccumulator>();
        priorityEnc = std::make_shared<CAM_PriorityEncoder>();

        RowDriver.resize(MAX_PORT);
        WriteDriver.resize(MAX_PORT);
        ColMux.resize(MAX_PORT);
        Row.resize(MAX_PORT);
        Col.resize(MAX_PORT);

	//////////////////////////////////////////////////////////////////////////////////
	//		   						 input check: 									//
	//////////////////////////////////////////////////////////////////////////////////

	if (inputParameter->realCapacity != inputParameter->capacity && inputParameter->realCapacity != 0) {
		// deal with the ASP-DAC12 72-bit word
		numRow = inputParameter->realCapacity / inputParameter->minNumRowSubarray / inputParameter->minNumColumnSubarray
				/  inputParameter->minNumActiveMatPerRow / inputParameter->minNumActiveMatPerColumn / numColumn;
	}

			// std::cout << __FILE__ << ": " << __LINE__ << " row: " << numRow << " col: " << numColumn << std::endl;

	if (inputParameter->cell->camNumRow < 1 || inputParameter->cell->camNumCol < 1) {
		invalid = true;
		std::cout <<"[CAM_SubArray] [Error]: array configuration error" <<std::endl;
		return;
	}

	//////////////////////////////////////////////////////////////////////////////////
	//		   						 defination: 									//
	//////////////////////////////////////////////////////////////////////////////////


	/* Derived parameters */
	numSenseAmp = numColumn / muxSenseAmp; // # of SA
	lenRow = (double)numColumn * inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize; /* length of row */
	lenCol = (double)numRow * inputParameter->cell->heightInFeatureSize * inputParameter->tech->featureSize; /* length of column */
	/* Add stitching overhead if necessary */
	if (inputParameter->cell->stitching) {
		lenRow += ((numColumn - 1) / inputParameter->cell->stitching + 1) * STITCHING_OVERHEAD * inputParameter->tech->featureSize;
	}
	//////////////////////////////////////////////////////////////////////////////////
	//		   			   calculation for SA: 										//
	//////////////////////////////////////////////////////////////////////////////////

	// 1. setting SA sense mode
	// TODO: note that cmos-based/doide-based has to be current-in voltage sensing
	// 5. calc precharge voltage
	// 6. calc for sensing: resMemCellOff, voltageMemCellOff

	voltageSense = bool(inputParameter->cell->readMode > 0);
	senseVoltage = inputParameter->cell->minSenseVoltage;

	
	if (!internalSenseAmp && inputParameter->cell->memCellType != SRAM) {
		invalid = true;
		std::cout << "[CAM_SubArray] Error: nvTCAM does not support external sense amplifiers!" << std::endl;
		return;
	}
	for(int i=0;i<inputParameter->cell->camNumCol;i++) {
                Col[i] = std::make_shared<CAM_Line>();
		Col[i]->Initialize(false, i, lenCol, numRow, inputParameter, localWire);
	}

	
	// Variation generation model

    // std::mt19937 rngResistanceOff(std::random_device{}());
	// std::mt19937 rngResistanceOn(std::random_device{}());
    // // std::normal_distribution<double> distResistanceOff(inputParameter->cell->resistanceOff, inputParameter->cell->resistanceOff*inputParameter->cell->resistanceOffVariation);
	// std::normal_distribution<double> distResistanceOff(6.67e3,148.8);
	// std::normal_distribution<double> distResistanceOn(inputParameter->cell->resistanceOn, inputParameter->cell->resistanceOff*inputParameter->cell->resistanceOnVariation);
    // for (int i = 0; i < 100; i++) {
    //     std::cout << distResistanceOff(rngResistanceOff) << std::endl;
    // }


	/* Caculate the CAM cell resistance and capaciatence on the matchline. */
	indexMatchline = 10;
	for(int i=0;i<inputParameter->cell->camNumCol;i++){
		indexMatchline = i;
		if (Col[i]->CellPort.Type == Matchline || Col[i]->CellPort.Type == Matchline_Bitline) {
			if(Col[i]->CellPort.ConnectedRegion == gate){
				invalid = true;
				std::cout << "[Warning]: Impractical matchline connection (gate)." << std::endl;
				return;
			}
			else if(Col[i]->CellPort.ConnectedRegion == drain||Col[i]->CellPort.ConnectedRegion == source){

				if(Col[i]->CellPort.isNMOS){
					if(inputParameter->cell->isNVMdischarge){
						resMemCellOff = inputParameter->tech->vdd / inputParameter->tech->currentOffNmos[inputParameter->temperature - 300]/ inputParameter->tech->featureSize / Col[i]->CellPort.numCmos
		                			     + inputParameter->cell->resistanceOff/2;
						resMemCellOn  = CalculateOnResistance(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, NMOS, inputParameter->temperature, inputParameter->tech)*Col[i]->CellPort.numCmos + inputParameter->cell->resistanceOn;
						// Update for HV
						capCellAccess = (CalculateDrainCap(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize*3 /* Update for HV */, NMOS, inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize*3 /* Update for HV */, inputParameter->tech)+inputParameter->cell->capacitanceOff)*Col[i]->CellPort.numCmos;
					}else{
									// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
						resMemCellOff = inputParameter->tech->vdd / inputParameter->tech->currentOffNmos[inputParameter->temperature - 300] / inputParameter->tech->featureSize / Col[i]->CellPort.numCmos;
						resMemCellOn  = CalculateOnResistance(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, NMOS, inputParameter->temperature, inputParameter->tech);
						capCellAccess = CalculateDrainCap(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, NMOS, inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize, inputParameter->tech)*Col[i]->CellPort.numCmos;
									// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
					}
				}else{
					if(inputParameter->cell->isNVMdischarge){
									// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
						resMemCellOff = (inputParameter->tech->vdd / inputParameter->tech->currentOffPmos[inputParameter->temperature - 300]/ inputParameter->tech->featureSize / Col[i]->CellPort.numCmos
		                			     + inputParameter->cell->resistanceOff)/2;
						resMemCellOn  = CalculateOnResistance(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, PMOS, inputParameter->temperature, inputParameter->tech) + inputParameter->cell->resistanceOn;
						capCellAccess = CalculateDrainCap(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, PMOS, inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize, inputParameter->tech)*Col[i]->CellPort.numCmos;
									// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
					}else{
						resMemCellOff = inputParameter->tech->vdd / inputParameter->tech->currentOffPmos[inputParameter->temperature - 300]/ inputParameter->tech->featureSize / Col[i]->CellPort.numCmos;
						resMemCellOn  = CalculateOnResistance(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, PMOS, inputParameter->temperature, inputParameter->tech)*Col[i]->CellPort.numCmos;
						capCellAccess = CalculateDrainCap(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, PMOS, inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize, inputParameter->tech)*Col[i]->CellPort.numCmos;
					}
				}
			}else if(Col[i]->CellPort.ConnectedRegion == diode){
				indexMatchline = i; 
				if(Col[i]->CellPort.isNMOS){
					if(inputParameter->cell->isNVMdischarge){
						resMemCellOff = (inputParameter->tech->vdd / inputParameter->tech->currentOffNmos[inputParameter->temperature - 300]/ inputParameter->tech->featureSize / Col[i]->CellPort.widthCmos*Col[i]->CellPort.numCmos/2
		                			     + inputParameter->cell->resistanceOff)/2;
						resMemCellOn  = CalculateOnResistance(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, NMOS, inputParameter->temperature, inputParameter->tech)+inputParameter->cell->resistanceOn;
						capCellAccess = CalculateDrainCap(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, NMOS, inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize, inputParameter->tech)*Col[i]->CellPort.numCmos
										+CalculateGateCap(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, inputParameter->tech);
					}else{
						resMemCellOff = inputParameter->tech->vdd / inputParameter->tech->currentOffNmos[inputParameter->temperature - 300]/ inputParameter->tech->featureSize / Col[i]->CellPort.widthCmos*Col[i]->CellPort.numCmos;
						resMemCellOn  = CalculateOnResistance(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, NMOS, inputParameter->temperature, inputParameter->tech)*Col[i]->CellPort.numCmos;
						capCellAccess = CalculateDrainCap(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, NMOS, inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize, inputParameter->tech)*Col[i]->CellPort.numCmos
									    +CalculateGateCap(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, inputParameter->tech);
					}
				}else{
					if(inputParameter->cell->isNVMdischarge){
						resMemCellOff = (inputParameter->tech->vdd / inputParameter->tech->currentOffNmos[inputParameter->temperature - 300]/ inputParameter->tech->featureSize / Col[i]->CellPort.widthCmos*Col[i]->CellPort.numCmos/2
		                			     + inputParameter->cell->resistanceOff)/2;
						resMemCellOn  = CalculateOnResistance(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, NMOS, inputParameter->temperature, inputParameter->tech)*Col[i]->CellPort.numCmos+inputParameter->cell->resistanceOn;
						capCellAccess = CalculateDrainCap(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, NMOS, inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize, inputParameter->tech)*Col[i]->CellPort.numCmos
										+CalculateGateCap(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, inputParameter->tech);
					}else{
						resMemCellOff = inputParameter->tech->vdd / inputParameter->tech->currentOffPmos[inputParameter->temperature - 300]/ inputParameter->tech->featureSize / Col[i]->CellPort.widthCmos*Col[i]->CellPort.numCmos;
						resMemCellOn  = CalculateOnResistance(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, PMOS, inputParameter->temperature, inputParameter->tech)*Col[i]->CellPort.numCmos;
						capCellAccess = CalculateDrainCap(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, PMOS, inputParameter->cell->widthInFeatureSize * inputParameter->tech->featureSize, inputParameter->tech)*Col[i]->CellPort.numCmos
									    +CalculateGateCap(Col[i]->CellPort.widthCmos*inputParameter->tech->featureSize, inputParameter->tech);
					}
				}

			}else if(Col[i]->CellPort.ConnectedRegion == none){			
				if(inputParameter->cell->memCellType == FEFETRAM){
					capCellAccess += CalculateDrainCap(Col[i]->CellPort.widthCmos * inputParameter->FEFET_tech->featureSize, NMOS, inputParameter->cell->widthInFeatureSize * inputParameter->FEFET_tech->featureSize, inputParameter->FEFET_tech)*Col[i]->CellPort.numCmos;
				}
				resMemCellOff = inputParameter->cell->resistanceOff/Col[i]->CellPort.numCmos;
				resMemCellOn = inputParameter->cell->resistanceOn;
				//std::cout << "OnOffRes " << resMemCellOff << " : " << resMemCellOn << std::endl; 
			}else{
				std::cout << "[CAM_SubArray] Error: Unsupported access type." <<std::endl;
			}
			break;
                /* need to account for other cases for some variables to be initialized */ 
		} else if (Col[i]->CellPort.Type == Bitline) {
                        //TODO: Actually calculate this correctly, put a placeholder for uninitialized value errors
                        std::cout << "[CAM_SubArray] Warning: Placeholders used for calculations." << std::endl;
                        resMemCellOn = 0.001;
                        resMemCellOff = 0.001;
                        capCellAccess = 0.001;
                }
	}
	// std::cout << "res&cap: " << resMemCellOff << std::endl;
	// 	std::cout << "indexML: " << indexMatchline << std::endl;
	/* Terminate if CAM device type is not supported. */
		if(inputParameter->cell->memCellType != SRAM && inputParameter->cell->memCellType != FEFETRAM && inputParameter->cell->memCellType != memristor && inputParameter->cell->memCellType != PCRAM && inputParameter->cell->memCellType != MRAM) {
		invalid = true;
		std::cout <<"[CAM_SubArray] Error: NVM device type is not supported." <<std::endl;
		return;
	}	
	// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
	// double check = 0;
	// /* Terminate if CAM cell resistance or capacitance calcualtion is wrong */
	// 	if(resMemCellOff = check||resMemCellOn = check||capCellAccess =0) {
	// 	invalid = true;
	// 	std::cout <<"[CAM_SubArray] Error: Invalid CAM cell structure, no cap/res on ML found." <<std::endl;
	// 	return;
	// }

	if(indexMatchline == 10) {
		invalid = true;
		std::cout <<"[CAM_SubArray] Error: no matchline found." <<std::endl;
		return;
	}
	
	/* Assume the precharge voltage as Vdd to be more simplified and generalized, some other precharge voltage designs also exist*/
	/* E.g. diode access design many just need to charge to Vmatch+vth
	 * CMOS access design could be half swing
	 * None access design could be n*volatgeMemOff
	 */
	voltagePrecharge = inputParameter->tech->vdd;

	
	//////////////////////////////////////////////////////////////////////////////////
	//		   			   calculation for driver: 									//
	//////////////////////////////////////////////////////////////////////////////////

	// 2. line resistance caclualtion
	// 3. Mux load calculation: extended from signel bl to cols
	// 4. transistor's impact on res and cap of the lines


	for(int i=0;i<inputParameter->cell->camNumRow;i++) {
                Row[i] = std::make_shared<CAM_Line>();
		Row[i]->Initialize(true, i, lenRow, numColumn, inputParameter, localWire);
	}
	for(int i=0;i<inputParameter->cell->camNumCol;i++) {
		Col[i]->Initialize(false, i, lenCol, numRow, inputParameter, localWire);
		if (Col[i]->minMuxWidth > inputParameter->maxNmosSize * inputParameter->tech->featureSize) {
			invalid = true;
			std::cout <<"[CAM_SubArray] Error: Column mux width too large" <<std::endl;
			return;
		}
	}

// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
	//////////////////////////////////////////////////////////////////////////////////
	//		   					 validation check: 									//
	//////////////////////////////////////////////////////////////////////////////////

	// 2. ML length: match (all-Ih) v.s. 1-miss (Il+all-Ih) for search

	// if (inputParameter->cell->accessType == CMOS_access || inputParameter->cell->accessType == diode_access){
		// ML is connected with the drain of NMOS, like ISSCC15-3t1r
		// ML is connected with the diode (both gate and drain of nmos), like VSLIT12-4t2r
		// the leakage cannot be too large
	// 	if(numRow * inputParameter->tech->currentOffNmos[inputParameter->temperature - 300] / BITLINE_LEAKAGE_TOLERANCE >
	// 		inputParameter->tech->currentOnNmos[inputParameter->temperature - 300]) {
	// 		invalid = true;
	// 		std::cout <<"[CAM_SubArray] Error: too large ML leakage" <<std::endl;
	// 		return;
	// 	}
	// } else if (inputParameter->cell->accessType == none_access) {
	// 				if(inputParameter->cell->resistanceOff/numRow < inputParameter->cell->resistanceOn) {
	// 		invalid = true;
	// 		std::cout <<"[CAM_SubArray] Error: too large ML leakage" <<std::endl;
	// 		return;
	// 	}
		// ML is connected with cell, like JSSC11-2t2r and also for 2FEFET TCAM design
		// TODO sth for 2FEFET TCAM
		// std::cout << "numRow: " << numRow;
	// 	if( inputParameter->withOutputAcc == false ) {
	// 		double Rh = numRow * resMemCellOff;
	// 		double Rl = resMemCellOn;
	// 		senseMargin = 2 * senseVoltage * (Rh - Rl) / (Rh + Rl);
	// 		if ( senseMargin < MIN_SENSE_MARGIN ) {
	// 			// TODO: the minimal sense margin is defined in the constant
	// 			invalid = true;
	// 			std::cout <<"[CAM_SubArray] Error: too many rows for sense" <<std::endl;
	// 			return;
	// 		}
	// 	}
	// } else {
	// 	invalid = true;
	// 	std::cout <<"[CAM_SubArray] Error: access type input error" <<std::endl;
	// 	return;
	// }

	//////////////////////////////////////////////////////////////////////////////////
	//		   						 Peripherals initialization									//
	//////////////////////////////////////////////////////////////////////////////////

	double capNandInput, tmp;

	// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
	if(withInputBuf) {
		CalculateGateCapacitance(NAND, 2, 2 * MIN_NMOS_SIZE /* Update for HV */ * inputParameter->tech->featureSize, inputParameter->tech->pnSizeRatio * MIN_NMOS_SIZE * inputParameter->tech->featureSize,
					inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT * 3 /* Update for HV */, inputParameter->tech, &capNandInput, &tmp);
		inputBuf->Initialize(true /*TODO*/, capNandInput, 0, inputParameter);
	}

		CalculateGateCapacitance(NAND, 2, 2 * MIN_NMOS_SIZE /* Update for HV */ * inputParameter->tech->featureSize, inputParameter->tech->pnSizeRatio * MIN_NMOS_SIZE * inputParameter->tech->featureSize,
					inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT * 3 /* Update for HV */, inputParameter->tech, &capNandInput, &tmp);
		inputLS->Initialize(true /*TODO*/, capNandInput, 0, inputParameter);
		CalculateGateCapacitance(NAND, 2, 2 * MIN_NMOS_SIZE /* Update for HV */ * inputParameter->tech->featureSize, inputParameter->tech->pnSizeRatio * MIN_NMOS_SIZE * inputParameter->tech->featureSize,
					inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT * 3 /* Update for HV */, inputParameter->tech, &capNandInput, &tmp);
		outputLS->Initialize(true /*TODO*/, capNandInput, 0, inputParameter);
	if(withInputEnc) {
		CalculateGateCapacitance(NAND, 2, 2 * MIN_NMOS_SIZE /* Update for HV */ * inputParameter->tech->featureSize, inputParameter->tech->pnSizeRatio * MIN_NMOS_SIZE * inputParameter->tech->featureSize,
					inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT *3 /* Update for HV */, inputParameter->tech, &capNandInput, &tmp);
		inputEnc->Initialize(encoding_two_bit, false, capNandInput, 0, inputParameter/* TODO*/);
		inputEnc->CalculateRC();
	}

	// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
	// this NAND merges pre-decoder's result, output the WL activation signal
	CalculateGateCapacitance(NAND, 2, 2 * MIN_NMOS_SIZE /* Update for HV */ * inputParameter->tech->featureSize, inputParameter->tech->pnSizeRatio * MIN_NMOS_SIZE * inputParameter->tech->featureSize,
				inputParameter->tech->featureSize* MAX_TRANSISTOR_HEIGHT * 3 /* Update for HV */ , inputParameter->tech, &capNandInput, &tmp);
	if(inputParameter->cell->memCellType != SRAM) {
		
		RowDecMergeNand->Initialize(numRow * 2, capNandInput, 0, false/*TODO*/, true, DecMergeOptLevel, 0, inputParameter /*TODO*/);
	} else {
		RowDecMergeNand->Initialize(numRow, capNandInput, 0, false/*TODO*/, true, DecMergeOptLevel, 0, inputParameter /*TODO*/);
	}
	//RowDecMergeNand->CalculateRC();
	// those NAND merges the WL and SL signal
	//COMMENT RowDriver = new CAM_RowNand [inputParameter->cell->camNumRow];
	for(int i=0;i<inputParameter->cell->camNumRow;i++){
                RowDriver[i] = std::make_shared<CAM_RowNand>();
		RowDriver[i]->Initialize(numRow, Row[i]->cap*1.6, Row[i]->res, false/*TODO*/, false, DriverOptLevel, Row[i]->maxCurrent, inputParameter);
		//RowDriver[i]->CalculateRC();
	}
	// if(inputParameter->cell->memCellType == FEFETRAM){
	// 	for(int i=0;i<inputParameter->cell->camNumRow;i++){
	// 	RowDriver[i]->Initialize(numRow, Row[i]->cap /* * 10 */, Row[i]->res, false/*TODO*/, false, DriverOptLevel, Row[i]->maxCurrent);
	// 	RowDriver[i]->CalculateRC();
	// 	}
	// }
        
        precharger = std::make_shared<CAM_Precharger>();
	precharger->Initialize(voltagePrecharge, numColumn, Col[indexMatchline]->cap, Col[indexMatchline]->res, 
                inputParameter, localWire);
	//precharger->CalculateRC();

// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
	// colmn decoder signal merge
	// TODO: I am too lazy to calculate very one, just use the index zero
        Row[inputParameter->cell->camNumRow] = std::make_shared<CAM_Line>();
	Row[inputParameter->cell->camNumRow]->Initialize(lenRow, numColumn, Col[0]->minMuxWidth, 
                inputParameter, localWire);
	ColDecMergeNand->Initialize(inputParameter->cell->camNumCol * muxSenseAmp, Row[inputParameter->cell->camNumRow]->cap, Row[inputParameter->cell->camNumRow]->res, false, DecMergeOptLevel, 0, inputParameter);
	//ColDecMergeNand->CalculateRC();

	senseAmpMuxLev1Nand->Initialize(muxOutputLev1, Row[inputParameter->cell->camNumRow]->cap*1.6, Row[inputParameter->cell->camNumRow]->res, false, DecMergeOptLevel, 0, inputParameter);
	//senseAmpMuxLev1Nand->CalculateRC();
	senseAmpMuxLev2Nand->Initialize(muxOutputLev2, Row[inputParameter->cell->camNumRow]->cap*1.6, Row[inputParameter->cell->camNumRow]->res, false, DecMergeOptLevel, 0, inputParameter);
	//senseAmpMuxLev2Nand->CalculateRC();

	// MUX
	senseAmpMuxLev2->Initialize(muxOutputLev2, numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2,
			0, 0, Col[indexMatchline]->maxCurrent, inputParameter);
	//senseAmpMuxLev2->CalculateRC();
	senseAmpMuxLev1->Initialize(muxOutputLev1, numColumn / muxSenseAmp / muxOutputLev1,
			senseAmpMuxLev2->capForPreviousDelayCalculation, senseAmpMuxLev2->capForPreviousPowerCalculation, Col[indexMatchline]->maxCurrent, inputParameter);
	//senseAmpMuxLev1->CalculateRC();

	if (internalSenseAmp) {
		senseAmp->Initialize(numColumn / muxSenseAmp, typeSenseAmp, customSenseAmp, senseVoltage, lenRow / numColumn * muxSenseAmp, inputParameter->fileCustomSA, inputParameter);
		//senseAmp->CalculateRC();
		for(int i=0;i<inputParameter->cell->camNumCol;i++){
                        ColMux[i] = std::make_shared<Mux>();
			ColMux[i]->Initialize(muxSenseAmp, numColumn / muxSenseAmp, senseAmp->capLoad, senseAmp->capLoad, Col[i]->maxCurrent, inputParameter);
			//ColMux[i]->CalculateRC();
		}
	} else {
		for(int i=0;i<inputParameter->cell->camNumCol;i++){
			ColMux[i]->Initialize(muxSenseAmp, numColumn / muxSenseAmp, 
                                senseAmpMuxLev1->capForPreviousDelayCalculation, 
                                senseAmpMuxLev1->capForPreviousPowerCalculation, Col[i]->maxCurrent, inputParameter);
			//ColMux[i]->CalculateRC();
		}
	}

	if (withWriteDriver) {
		for(int i=0;i<inputParameter->cell->camNumCol;i++) {
			if(inputParameter->cell->camPort[1][i].Type == Matchline) {
                                WriteDriver[i] = std::make_shared<RowDecoder>();
			} else {
                                WriteDriver[i] = std::make_shared<RowDecoder>();
				WriteDriver[i]->Initialize(numColumn / muxSenseAmp, Col[i]->cap, Col[i]->res, false, DriverOptLevel, Col[i]->maxCurrent, inputParameter);
				//WriteDriver[i]->CalculateRC();
			}
		}
	}

	if (withPriorityEnc) {
		priorityEnc->Initialize(numColumn, PriorityOptLevel, 0, 0, inputParameter /*TODO: no output driver*/);
	}

	if (withOutputAcc) {
		if (withPriorityEnc)
			outputAcc->Initialize(priorityEnc->MMR.BasicMMR.capIn, 0, inputParameter);
		else
			outputAcc->Initialize(0, 0, inputParameter /*TODO: no output driver*/);
		//outputAcc->CalculateRC();
	}

	if(withOutputBuf) {
		CalculateGateCapacitance(NAND, 2, 8 * MIN_NMOS_SIZE /* Update for HV */* inputParameter->tech->featureSize, inputParameter->tech->pnSizeRatio * MIN_NMOS_SIZE * inputParameter->tech->featureSize,
					inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT * 3 /* Update for HV */, inputParameter->tech, &capNandInput, &tmp);
		outputBuf->Initialize(true /*TODO*/, capNandInput, 0, inputParameter);
	}
        
	initialized = true;
        CalculateArea();
}

void CAM_SubArray::CalculateArea() {
	if (!initialized) {
		std::cout << "[CAM_SubArray] Error: Require initialization first!" << std::endl;
	} else if (invalid) {
		std::cout << "[CAM_SubArray] Error: invalid!" << std::endl;
		height = width = area = 1e41;
	} else {
		double addWidthArea = 0, addHeightArea = 0;


		width = lenRow;
		height = lenCol;
	//	std::cout << "width: height: " << width * 1e6 << ": " << height*1e6 << std::endl;
		area = height * width;

		if (withInputBuf) {
			//inputBuf->CalculateArea();
			area += (inputBuf->area * numRow * 4);
			addWidthArea += (inputBuf->area * numRow * 4);
		}
		//inputLS->CalculateArea();
		area += (inputLS->area * numRow);
		addWidthArea += (inputBuf->area * numRow);

		//outputLS->CalculateArea();
		area += (outputLS->area * numRow);
		addWidthArea += (outputLS->area * numColumn * 2);

		if (withInputEnc) {
			//inputEnc->CalculateArea();
			area += (inputEnc->area * numRow);
			addWidthArea += (inputEnc->area * numRow);
		}

		//RowDecMergeNand->CalculateArea();
		area += (RowDecMergeNand->area * 4);
		addWidthArea += (RowDecMergeNand->area * 4);


		for (int i=0;i<inputParameter->cell->camNumRow;i++){
			//RowDriver[i]->CalculateArea();
			area += (RowDriver[i]->area * 4);
			addWidthArea += (RowDriver[i]->area * 4);
		}

		// colmn decoder signal merge
		//ColDecMergeNand->CalculateArea();
		area += (ColDecMergeNand->area * 4);
		addWidthArea += (ColDecMergeNand->area * 4);

		//senseAmpMuxLev1Nand->CalculateArea();
		area += (senseAmpMuxLev1Nand->area );
		addWidthArea += (senseAmpMuxLev1Nand->area );
		//senseAmpMuxLev2Nand->CalculateArea();
		area += (senseAmpMuxLev2Nand->area );
		addWidthArea += (senseAmpMuxLev2Nand->area);

		//precharger->CalculateArea();
		area += (precharger->area);
		addHeightArea += (precharger->area);

		for(int i=0;i<inputParameter->cell->camNumCol;i++){
			//ColMux[i]->CalculateArea();
			area += (ColMux[i]->area);
			addHeightArea += (ColMux[i]->area* 3);
		}

		// MUX
		//senseAmpMuxLev2->CalculateArea();
		area += (senseAmpMuxLev2->area* 3);
		addHeightArea += (senseAmpMuxLev2->area);
	        //senseAmpMuxLev1->CalculateArea();
		area += (senseAmpMuxLev1->area* 9);
		addHeightArea += (senseAmpMuxLev1->area);

		if (internalSenseAmp) {
			//senseAmp->CalculateArea();
			area += (senseAmp->area);
			addHeightArea += (senseAmp->area);
		}

		WriteDriverArea = 0;
		if (withWriteDriver) {
			for(int i=0;i<inputParameter->cell->camNumCol;i++){
				if (WriteDriver[i]->initialized) {
					//WriteDriver[i]->CalculateArea();
					if (i > 0 && Col[i]->CellPort.Type == Bitline && Col[i-1]->CellPort.Type == Bitline) {
						WriteDriverArea += (WriteDriver[i]->outputDriver->area);
						WriteDriverArea += (WriteDriver[i]->area* 4);
					} else {
						WriteDriverArea += (WriteDriver[i]->area* 4);
					}
				}
			}
		}
		area += WriteDriverArea;
		addHeightArea += WriteDriverArea;

		if (withOutputAcc) {
			//outputAcc->CalculateArea();
			area += (outputAcc->area * numColumn / muxSenseAmp);
			addHeightArea += (outputAcc->area * numColumn / muxSenseAmp);
		}

		if (withPriorityEnc) {
			//priorityEnc->CalculateArea();
			area += (priorityEnc->area);
			addHeightArea += (priorityEnc->area);
		}

		if (withOutputBuf) {
			//outputBuf->CalculateArea();
			area += (outputBuf->area * numColumn / muxSenseAmp);
			addWidthArea += (outputBuf->area * numColumn / muxSenseAmp);
		}

		// TODO: a prefect layout
		width = addWidthArea / lenCol + lenCol;
		height = area / width;
//		std::cout << "width: height: 22 " << width * 1e6 << ": " << height*1e6 << std::endl;
	}
}

void CAM_SubArray::CalculateLatency(double _rampInput) {
	if (!initialized) {
		throw std::runtime_error("[CAM_SubArray] Error: Require initialization first!");
	} else if (invalid) {
		std::cout << "[CAM_SubArray] Error: invalid!" << std::endl;
		searchLatency = readLatency = writeLatency = 1e41;
	} else {

		if (withInputBuf) {
			inputBuf->CalculateLatency(_rampInput);
		} else {
			inputBuf->readLatency = 0;
			inputBuf->rampOutput = _rampInput;
		}
		inputLS->CalculateLatency(_rampInput);
		outputLS->CalculateLatency(_rampInput);
		if(withInputEnc) {
			inputEnc->CalculateLatency(_rampInput);
		} else {
			inputEnc->readLatency = 0;
			inputEnc->rampOutput = _rampInput;
		}

		// this NAND merges pre-decoder's result, output the WL activation signal
		RowDecMergeNand->CalculateLatency(_rampInput);
		
                // those NAND merges the WL and SL signal
		double maxRowDriver = 0;
		int indexMaxRowDriver = 0;
		for(int i=0;i<inputParameter->cell->camNumRow;i++){
                        RowDriver[i]->CalculateLatency(MAX(inputEnc->rampOutput, RowDecMergeNand->rampOutput));
                        
                        //std::cout << "i = " << i << std::endl;

			if (RowDriver[i]->readLatency > maxRowDriver) {
				maxRowDriver = RowDriver[i]->readLatency;
				indexMaxRowDriver = i;
                                //std::cout << "indexMaxRowDriver = " << indexMaxRowDriver << std::endl;
			}
		}

		// precharge
		precharger->CalculateLatency(_rampInput);

		// colmn decoder signal merge
		ColDecMergeNand->CalculateLatency(_rampInput);

		senseAmpMuxLev1Nand->CalculateLatency(_rampInput);
		senseAmpMuxLev2Nand->CalculateLatency(_rampInput);

		columnDecoderLatency = MAX(MAX(ColDecMergeNand->readLatency, senseAmpMuxLev1Nand->readLatency), senseAmpMuxLev2Nand->readLatency);
		decoderLatency = MAX(RowDecMergeNand->readLatency + maxRowDriver, columnDecoderLatency);

		//////////////////////////////////////////////////////////////////////////////////
		//	                    calc matchline latency				//
		//////////////////////////////////////////////////////////////////////////////////

                if (inputParameter->cell->camType == TCAM){
			double tau, gm, beta;
				// Estimate the ML latency for 1-miss case

                                //std::cout << resMemCellOn << std::endl;
                                //std::cout << resMemCellOff << std::endl;

				resTotalCell = (resMemCellOn * resMemCellOff) / ((CAM_opt->BitSerialWidth-1)*resMemCellOn + resMemCellOff);
				capTotalCell = capCellAccess * CAM_opt->BitSerialWidth;
                                
                                //std::cout << "[CAM_SubArray] Why nan prints" << std::endl;
                                //std::cout << resTotalCell << std::endl;
                                //std::cout << capTotalCell << std::endl;
                                //std::cout << ColMux[indexMatchline]->capForPreviousDelayCalculation << std::endl;
                                //std::cout << inputParameter->AddCapOnML << std::endl;
                                //std::cout << precharger->capOutputBitlinePrecharger << std::endl;
                                //std::cout << senseAmp->capLoad << std::endl;
                                //std::cout << Col[indexMatchline]->res << std::endl;
                                //std::cout << Col[indexMatchline]->cap << std::endl;

				tau = resTotalCell 
                                    * (capTotalCell 
                                        + ColMux[indexMatchline]->capForPreviousDelayCalculation 
                                        + inputParameter->AddCapOnML 
                                        + precharger->capOutputBitlinePrecharger 
                                        + senseAmp->capLoad) 
                                    + Col[indexMatchline]->res 
                                    * (ColMux[indexMatchline]->capForPreviousDelayCalculation 
                                        + inputParameter->AddCapOnML 
                                        + precharger->capOutputBitlinePrecharger 
                                        + senseAmp->capLoad 
                                        + Col[indexMatchline]->cap / 2);

                                //std::cout << "[CAM_SubArray] tau: " << tau << std::endl;

				// tau = resTotalCell * capTotalCell + Col[indexMatchline]->res * (ColMux[indexMatchline]->capForPreviousDelayCalculation);
				// referDelay = tau * log((voltagePrecharge) / (inputParameter->cell->readVoltage)); // Too hard for user to provide read voltage
				// referDelay = tau * log(2);
				// beta = resMemCellOff / CAM_opt->BitSerialWidth / resTotalCell;
				gm = CalculateTransconductance(Col[indexMatchline]->CellPort.widthCmos*inputParameter->tech->featureSize, NMOS, inputParameter->tech);
				beta = 1 / gm / resTotalCell;
				matchlineDelay = horowitz(tau, beta, RowDriver[indexMaxRowDriver]->rampOutput, &matchlineRamp);
				PRINT_VERBOSE("matchlineDelay = " << matchlineDelay * 1e12 << " ps");

				// Estimate the ML latency for all-match case
				resTotalCell = resMemCellOff / CAM_opt->BitSerialWidth;//  
				tau = resTotalCell * (Col[indexMatchline]->cap + ColMux[indexMatchline]->capForPreviousDelayCalculation)
						+ Col[indexMatchline]->res * (ColMux[indexMatchline]->capForPreviousDelayCalculation + Col[indexMatchline]->cap / 2);
                                //TODO: Need to get referDelay to be an expected value, took the following line from a commented out line above
                                referDelay = tau * log(2);
				volMatchDrop = voltagePrecharge - voltagePrecharge * exp(-referDelay * tau);

			// Primary sense margin check
			senseMargin = voltagePrecharge/2 - volMatchDrop;	
			if (senseMargin < senseVoltage) {
				std::cout << "[CAM_Subarray] Error: matchline too long to be sensed!" << std::endl;
				invalid = true;
				searchLatency = readLatency = writeLatency = 1e41;
				return;
			}

			for(int i=0;i<inputParameter->cell->camNumCol;i++){
				ColMux[i]->CalculateLatency(matchlineRamp);
			}
			if (internalSenseAmp) {
				senseAmp->CalculateLatency(ColMux[indexMatchline]->rampOutput);
				senseAmpMuxLev1->CalculateLatency(1e20);
				senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);
			} else {
				senseAmpMuxLev1->CalculateLatency(ColMux[indexMatchline]->rampOutput);
				senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);
			}

			if (withOutputAcc) {
				outputAcc->CalculateLatency(1e20);
			} else {
				outputAcc->readLatency = 0;
				outputAcc->rampOutput = 1e20;
			}

			if (withPriorityEnc) {
				priorityEnc->CalculateLatency(outputAcc->rampOutput);
				rampOutput = priorityEnc->rampOutput;
			} else {
				priorityEnc->readLatency = 0;
				priorityEnc->rampOutput = outputAcc->rampOutput;
				rampOutput = priorityEnc->rampOutput;
			}

			if(withOutputBuf) {
				outputBuf->CalculateLatency(outputAcc->rampOutput);
			} else {
				outputBuf->readLatency = 0;
			}

			// searchLatency = inputBuf->readLatency + precharger->readLatency + maxRowDriver + inputEnc->readLatency + matchlineDelay
			// 		+ ColMux[indexMatchline]->readLatency + senseAmp->readLatency + outputAcc->readLatency + priorityEnc->readLatency
			// 		+ outputBuf->readLatency;
                            
                            
                            //std::cout << std::endl << inputBuf->readLatency << std::endl; 
                            //std::cout << MAX(precharger->readLatency, decoderLatency + inputEnc->readLatency)  << std::endl;
                            //std::cout << matchlineDelay << std::endl;
			    //std::cout << ColMux[indexMatchline]->readLatency  << std::endl;
                            //std::cout << senseAmp->readLatency  << std::endl;
                            //std::cout << senseAmpMuxLev1->readLatency  << std::endl;
                            //std::cout << senseAmpMuxLev2->readLatency << std::endl;
			    //std::cout << outputAcc->readLatency << std::endl;
                            //std::cout << priorityEnc->readLatency  << std::endl;
                            //std::cout << outputBuf->readLatency  << std::endl;
                            //std::cout << outputLS->readLatency << std::endl;

			searchLatency = inputBuf->readLatency 
                            + MAX(precharger->readLatency, decoderLatency + inputEnc->readLatency) 
                            + matchlineDelay
			    + ColMux[indexMatchline]->readLatency 
                            + senseAmp->readLatency 
                            + senseAmpMuxLev1->readLatency 
                            + senseAmpMuxLev2->readLatency
			    + outputAcc->readLatency 
                            + priorityEnc->readLatency 
                            + outputBuf->readLatency 
                            + inputLS->readLatency 
                            + outputLS->readLatency;
                        
                        //std::cout << "[CAM_SubArray] searchLatency: " << searchLatency << std::endl;

			senseAmpLatency = senseAmp->readLatency;
                        
                            //std::cout << std::endl << inputBuf->readLatency << std::endl; 
                            //std::cout << MAX(precharger->readLatency, decoderLatency + inputEnc->readLatency)  << std::endl;
                            //std::cout << matchlineDelay << std::endl;
			    //std::cout << ColMux[indexMatchline]->readLatency  << std::endl;
                            //std::cout << senseAmp->readLatency  << std::endl;
                            //std::cout << senseAmpMuxLev1->readLatency  << std::endl;
                            //std::cout << senseAmpMuxLev2->readLatency << std::endl;
			    //std::cout << outputAcc->readLatency << std::endl;
                            //std::cout << priorityEnc->readLatency  << std::endl;
                            //std::cout << outputBuf->readLatency  << std::endl;
                            //std::cout << outputLS->readLatency << std::endl;

			readLatency = inputBuf->readLatency 
                            + MAX(precharger->readLatency, decoderLatency + inputEnc->readLatency) 
                            + matchlineDelay
			    + ColMux[indexMatchline]->readLatency 
                            + senseAmp->readLatency 
                            + senseAmpMuxLev1->readLatency 
                            + senseAmpMuxLev2->readLatency
			    + outputAcc->readLatency 
                            + priorityEnc->readLatency 
                            + outputBuf->readLatency 
                            + outputLS->readLatency;


			
			// Hamming distance-based approximate match
			if(inputParameter->searchFunction == BE || inputParameter->searchFunction == TH){
				for(int k = 1; k<CAM_opt->BitSerialWidth; k++){

					double resTemp0 = (resMemCellOn * resMemCellOff) / ((CAM_opt->BitSerialWidth-k)*resMemCellOn + resMemCellOff*k);
					double resTemp1 = (resMemCellOn * resMemCellOff) / ((CAM_opt->BitSerialWidth-k-1)*resMemCellOn + resMemCellOff*(k+1));

					capTotalCell = capCellAccess * CAM_opt->BitSerialWidth;

                                        //std::cout << "capTotalCell " << capTotalCell << std::endl;

					double tauTemp0 = resTemp0 * (capTotalCell + ColMux[indexMatchline]->capForPreviousDelayCalculation + inputParameter->AddCapOnML + precharger->capOutputBitlinePrecharger + senseAmp->capLoad)
						+ Col[indexMatchline]->res * (ColMux[indexMatchline]->capForPreviousDelayCalculation + inputParameter->AddCapOnML + precharger->capOutputBitlinePrecharger + senseAmp->capLoad + Col[indexMatchline]->cap / 2);
					double tauTemp1 = resTemp1 * (capTotalCell + ColMux[indexMatchline]->capForPreviousDelayCalculation + inputParameter->AddCapOnML + precharger->capOutputBitlinePrecharger + senseAmp->capLoad)
						+ Col[indexMatchline]->res * (ColMux[indexMatchline]->capForPreviousDelayCalculation + inputParameter->AddCapOnML + precharger->capOutputBitlinePrecharger + senseAmp->capLoad + Col[indexMatchline]->cap / 2);

					gm = CalculateTransconductance(Col[indexMatchline]->CellPort.widthCmos*inputParameter->tech->featureSize, NMOS, inputParameter->tech);

					double beta0 = 1 / gm / resTemp0;
					double beta1 = 1 / gm / resTemp1;
					double matchlineRamptemp;

					double delayTemp0 = horowitz(tauTemp0, beta0, RowDriver[indexMaxRowDriver]->rampOutput, &matchlineRamp);
					double delayTemp1 = horowitz(tauTemp1, beta1, RowDriver[indexMaxRowDriver]->rampOutput, &matchlineRamptemp);
					//PRINT_VERBOSE("delayTemp0 = " << delayTemp0*1e12);
					//PRINT_VERBOSE("delayTemp1 = " << delayTemp1*1e12);
					//PRINT_VERBOSE("distance = " << k << " margin = " 
                                                //<< (delayTemp0-delayTemp1)*1e12);

					if(delayTemp0 - delayTemp1 >= inputParameter->MatchlineSenseMargin){
						matchlineDelayForApprox[k] = delayTemp0;
						MaxDetectCellNumber = k;
						searchLatencyForApprox[k] = inputBuf->readLatency + MAX(precharger->readLatency, decoderLatency + inputEnc->readLatency) + matchlineDelayForApprox[k]
										+ ColMux[indexMatchline]->readLatency + senseAmp->readLatency + senseAmpMuxLev1->readLatency + senseAmpMuxLev2->readLatency
										+ outputAcc->readLatency + priorityEnc->readLatency + outputBuf->readLatency;
						continue;
					}else{
						break;
					}	
				}	
			}
		} else if(inputParameter->cell->camType == MCAM){
			if(inputParameter->cell->memCellType != FEFETRAM){
				invalid = true;
				throw std::runtime_error("Only 2FeFET MCAM design is supported.");
			}else{
                                std::cout << "Warning: 2FeFET MCAM design is not properly supported and will return inaccurate results for some metrics." << std::endl;
                                // TODO: fix this placeholder
                                capTotalCell = 0.001;
                                searchLatency = 0.001;
                                senseAmpLatency = 0.001;
			}

		} else if(inputParameter->cell->camType == ACAM){
                        throw std::runtime_error("ACAM is not supported at this time.");
		}



		// for(int i=0;i<inputParameter->cell->camNumCol;i++){
		// 	ColMux[i]->CalculateLatency(matchlineRamp);
		// }
		// if (internalSenseAmp) {
		// 	senseAmp->CalculateLatency(ColMux[indexMatchline]->rampOutput);
		// 	senseAmpMuxLev1->CalculateLatency(1e20);
		// 	senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);
		// } else {
		// 	senseAmpMuxLev1->CalculateLatency(ColMux[indexMatchline]->rampOutput);
		// 	senseAmpMuxLev2->CalculateLatency(senseAmpMuxLev1->rampOutput);
		// }

		// if (withOutputAcc) {
		// 	outputAcc->CalculateLatency(1e20);
		// } else {
		// 	outputAcc->readLatency = 0;
		// 	outputAcc->rampOutput = 1e20;
		// }

		// if (withPriorityEnc) {
		// 	priorityEnc->CalculateLatency(outputAcc->rampOutput);
		// 	rampOutput = priorityEnc->rampOutput;
		// } else {
		// 	priorityEnc->readLatency = 0;
		// 	priorityEnc->rampOutput = outputAcc->rampOutput;
		// 	rampOutput = priorityEnc->rampOutput;
		// }

		// if(withOutputBuf) {
		// 	outputBuf->CalculateLatency(outputAcc->rampOutput);
		// } else {
		// 	outputBuf->readLatency = 0;
		// }

		// // searchLatency = inputBuf->readLatency + precharger->readLatency + maxRowDriver + inputEnc->readLatency + matchlineDelay
		// // 		+ ColMux[indexMatchline]->readLatency + senseAmp->readLatency + outputAcc->readLatency + priorityEnc->readLatency
		// // 		+ outputBuf->readLatency;
		// searchLatency = inputBuf->readLatency + MAX(precharger->readLatency, decoderLatency + inputEnc->readLatency) + matchlineDelay
		// 		+ ColMux[indexMatchline]->readLatency + senseAmp->readLatency + senseAmpMuxLev1->readLatency + senseAmpMuxLev2->readLatency
		// 		+ outputAcc->readLatency + priorityEnc->readLatency + outputBuf->readLatency;
		// senseAmpLatency = senseAmp->readLatency;
		// readLatency = inputBuf->readLatency + MAX(precharger->readLatency, decoderLatency + inputEnc->readLatency) + matchlineDelay
		// 		+ ColMux[indexMatchline]->readLatency + senseAmp->readLatency + senseAmpMuxLev1->readLatency + senseAmpMuxLev2->readLatency
		// 		+ outputAcc->readLatency + priorityEnc->readLatency + outputBuf->readLatency;
		// for(k=1, k<=MaxDetectCellNumber, k++){
		// 	searchLatencyForApprox[k] = inputBuf->readLatency + MAX(precharger->readLatency, decoderLatency + inputEnc->readLatency) + matchlineDelayForApprox[k]
		// 		+ ColMux[indexMatchline]->readLatency + senseAmp->readLatency + senseAmpMuxLev1->readLatency + senseAmpMuxLev2->readLatency
		// 		+ outputAcc->readLatency + priorityEnc->readLatency + outputBuf->readLatency;
		}

		// for write
		double capPassTransistor = ColMux[indexMatchline]->capNMOSPassTransistor +
				senseAmpMuxLev1->capNMOSPassTransistor + senseAmpMuxLev2->capNMOSPassTransistor;
		double resPassTransistor = ColMux[indexMatchline]->resNMOSPassTransistor +
				senseAmpMuxLev1->resNMOSPassTransistor + senseAmpMuxLev2->resNMOSPassTransistor;
		double tauChargeLatency = resPassTransistor * (capPassTransistor + Col[indexMatchline]->cap) +
				Col[indexMatchline]->res * Col[indexMatchline]->cap / 2;
		chargeLatency = horowitz(tauChargeLatency, 0, 1e20, NULL);

		WriteDriverLatency = 0;
		if (inputParameter->writeScheme == write_and_verify) {
			/*TODO: write and verify programming */
		} else {
			if (withWriteDriver) {
				for(int i=0;i<inputParameter->cell->camNumCol;i++){
					if (WriteDriver[i]->initialized) {
						WriteDriver[i]->CalculateLatency(1e20);
						WriteDriverLatency = MAX(WriteDriver[i]->writeLatency, WriteDriverLatency);
					}
				}
			}
			writeLatency = MAX(decoderLatency, columnDecoderLatency + WriteDriverLatency + chargeLatency);
			resetLatency = (writeLatency + inputParameter->cell->resetPulse)*numColumn*2 * muxSenseAmp * muxOutputLev1 * muxOutputLev2;
			setLatency = (writeLatency + inputParameter->cell->setPulse)*numColumn*2 * muxSenseAmp * muxOutputLev1 * muxOutputLev2;
			writeLatency += MAX(inputParameter->cell->resetPulse, inputParameter->cell->setPulse);
		}
	}


void CAM_SubArray::CalculatePower() {
	if (!initialized) {
		std::cout << "[CAM_SubArray] Error: Require initialization first!" << std::endl;
	} else if (invalid) {
		std::cout << "[CAM_SubArray] Error: invalid!" << std::endl;
		readDynamicEnergy = writeDynamicEnergy = leakage = 1e41;
	} else {

		//////////////////////////////////////////////////////////////////////////////////
		//		   					calc components										//
		//////////////////////////////////////////////////////////////////////////////////

		readDynamicEnergy = writeDynamicEnergy = leakage = 0;

		if(withInputBuf) {
			//inputBuf->CalculatePower();
		} else {
			inputBuf->readDynamicEnergy = 0;
			inputBuf->leakage = 0;
		}
			//inputLS->CalculatePower();
			//outputLS->CalculatePower();
		if(withInputEnc) {
			//inputEnc->CalculatePower();
		} else {
			inputEnc->readDynamicEnergy = 0;
			inputEnc->leakage = 0;
		}

		// this NAND merges pre-decoder's result, output the WL activation signal
		//RowDecMergeNand->CalculatePower();
		// those NAND merges the WL and SL signal
		for(int i=0;i<inputParameter->cell->camNumRow;i++){
			//RowDriver[i]->CalculatePower();
		}

		// precharge
		//precharger->CalculatePower();

		// colmn decoder signal merge
		//ColDecMergeNand->CalculatePower();

		//senseAmpMuxLev1Nand->CalculatePower();
		//senseAmpMuxLev2Nand->CalculatePower();

		if (internalSenseAmp) {
			//senseAmp->CalculatePower();
		}
		//senseAmpMuxLev1->CalculatePower();
		//senseAmpMuxLev2->CalculatePower();

		if (withOutputAcc) {
			//outputAcc->CalculatePower();
		}

		if (withPriorityEnc) {
			//priorityEnc->CalculatePower();
		}

		if(withOutputBuf) {
			//outputBuf->CalculatePower();
		} else {
			outputBuf->readDynamicEnergy = 0;
			outputBuf->leakage = 0;
		}

		//////////////////////////////////////////////////////////////////////////////////
		//		   					calc read and search								//
		//////////////////////////////////////////////////////////////////////////////////
		if (typeSenseAmp == discharge) {
			searchDynamicEnergy = (Col[indexMatchline]->cap 
                                + ColMux[indexMatchline]->capForPreviousPowerCalculation + capTotalCell)
				* (voltagePrecharge * voltagePrecharge - inputParameter->cell->readVoltage * inputParameter->cell->readVoltage) * numColumn / muxSenseAmp;
                                        //std::cout << "[CAM_Subarray] searchDynamicEnergy " << searchDynamicEnergy << std::endl;
		} else {
			if (inputParameter->cell->memCellType == SRAM || inputParameter->cell->memCellType == FEFETRAM) {
				/* Codes below calculate the SRAM matchline power */
                                //std::cout << "Col[indexMatchline]->cap: " << Col[indexMatchline]->cap << std::endl;
                                //std::cout << "Col[indexMatchline]->capForPreviousPowerCalculation: " << ColMux[indexMatchline]->capForPreviousPowerCalculation << std::endl;
                                //std::cout << "capTotalCell: " << capTotalCell << std::endl;
                                //std::cout << "voltagePrecharge: " << voltagePrecharge << std::endl;
                                //std::cout << "numColumn: " << numColumn << std::endl;
                                //std::cout << "muxSenseAmp: " << muxSenseAmp << std::endl;
				searchDynamicEnergy = (Col[indexMatchline]->cap + ColMux[indexMatchline]->capForPreviousPowerCalculation + capTotalCell)
						* voltagePrecharge * voltagePrecharge * numColumn / muxSenseAmp;
                                        //std::cout << "[CAM_Subarray] searchDynamicEnergy " << searchDynamicEnergy << std::endl;
			} else if (inputParameter->cell->memCellType == MRAM || inputParameter->cell->memCellType == PCRAM || inputParameter->cell->memCellType == memristor) {
				if (inputParameter->cell->readMode == false) {	/* current-sensing */
					/* Use ICCAD 2009 model */
					double resMatchlineMux = ColMux[indexMatchline]->resNMOSPassTransistor;
					double vpreMin = inputParameter->cell->readVoltage * resMatchlineMux / (resMatchlineMux + Col[indexMatchline]->res + resMemCellOn);
					double vpreMax = inputParameter->cell->readVoltage * (resMatchlineMux + Col[indexMatchline]->res) /
							(resMatchlineMux + Col[indexMatchline]->res + resMemCellOn);
					searchDynamicEnergy = capTotalCell * vpreMax * vpreMax + ColMux[indexMatchline]->capForPreviousPowerCalculation
							* vpreMin * vpreMin + Col[indexMatchline]->cap * (vpreMax * vpreMax + vpreMin * vpreMin + vpreMax * vpreMin) / 3;
                                        //std::cout << "[CAM_Subarray] searchDynamicEnergy " << searchDynamicEnergy << std::endl;
					searchDynamicEnergy *= numColumn / muxSenseAmp;
                                        //std::cout << "[CAM_Subarray] searchDynamicEnergy " << searchDynamicEnergy << std::endl;

				} else {				/* voltage-sensing */
                                        /*std::cout << "[CAM_Subarray]" << capTotalCell << std::endl;
                                        std::cout << "[CAM_Subarray]" << Col[indexMatchline]->cap << std::endl;
                                        std::cout << "[CAM_Subarray]" << ColMux[indexMatchline]->capForPreviousPowerCalculation << std::endl;
                                        std::cout << "[CAM_Subarray]" << voltagePrecharge << std::endl;
                                        std::cout << "[CAM_Subarray]" << voltageMemCellOn << std::endl;
                                        std::cout << "[CAM_Subarray]" << numColumn << std::endl;
                                        std::cout << "[CAM_Subarray]" << muxSenseAmp << std::endl;*/
					searchDynamicEnergy = (capTotalCell 
                                                + Col[indexMatchline]->cap 
                                                + ColMux[indexMatchline]->capForPreviousPowerCalculation) 
                                                * (voltagePrecharge * voltagePrecharge - voltageMemCellOn 
                                                * voltageMemCellOn ) * numColumn / muxSenseAmp;
                                        //std::cout << "[CAM_Subarray] searchDynamicEnergy " << searchDynamicEnergy << std::endl;
				}
			// } else if (inputParameter->cell->memCellType ==FEFETRAM){
			// 	double FEFETCap = CalculateDrainCap(inputParameter->cell->widthAccessCMOS * inputParameter->FEFET_tech->featureSize, NMOS, inputParameter->cell->widthInFeatureSize * inputParameter->FEFET_tech->featureSize, inputParameter->FEFET_tech);
			// 	searchDynamicEnergy = (Col[indexMatchline]->cap + ColMux[indexMatchline]->capForPreviousPowerCalculation + FEFETCap) *
			// 			(voltagePrecharge * voltagePrecharge) * numColumn / muxSenseAmp;
			}// }
		}
		if (inputParameter->cell->readEnergy != 0) {
			cellReadEnergy = inputParameter->cell->readEnergy * CAM_opt->BitSerialWidth;
                       // std::cout << "error out plz" << cellReadEnergy << std::endl;
		} else if (inputParameter->cell->readPower != 0) {
                      //  std::cout << "error out" << inputParameter->cell->readPower << std::endl;
                      //  std::cout << "error out" << CAM_opt->BitSerialWidth << std::endl;
                     //   std::cout << "error out" << senseAmp->readLatency << std::endl;
                     //   std::cout << "error out" << matchlineDelay << std::endl;
			cellReadEnergy = inputParameter->cell->readPower 
                            * CAM_opt->BitSerialWidth 
                            * (senseAmp->readLatency + matchlineDelay);
                       // std::cout << "error out plz" << cellReadEnergy << std::endl;
		} else if (inputParameter->cell->memCellType == SRAM) {
			cellReadEnergy = capCellAccess * voltagePrecharge * voltagePrecharge * CAM_opt->BitSerialWidth;
		} else if (inputParameter->cell->readMode) {	/* voltage-sensing */
			if (inputParameter->cell->readVoltage == 0) { /* Current-in voltage sensing */
				cellReadEnergy = inputParameter->tech->vdd * inputParameter->cell->readCurrent * (senseAmp->readLatency + matchlineDelay) * CAM_opt->BitSerialWidth;
			}
			if (inputParameter->cell->readCurrent == 0) { /*Voltage-divider sensing */
				double resInSerialForSenseAmp, maxMatchlineCurrent;
				resInSerialForSenseAmp = sqrt(inputParameter->cell->resistanceOn * inputParameter->cell->resistanceOff);
				maxMatchlineCurrent = (inputParameter->cell->readVoltage - inputParameter->cell->voltageDropAccessDevice) / (inputParameter->cell->resistanceOn + resInSerialForSenseAmp);
				cellReadEnergy = inputParameter->tech->vdd * maxMatchlineCurrent * (senseAmp->readLatency + matchlineDelay) * CAM_opt->BitSerialWidth;
                        //std::cout << "error out plz" << cellReadEnergy << std::endl;
			}
		} else { /* current-sensing */
			double maxMatchlineCurrent = (inputParameter->cell->readVoltage - inputParameter->cell->voltageDropAccessDevice) / inputParameter->cell->resistanceOn;
			cellReadEnergy = inputParameter->tech->vdd * maxMatchlineCurrent * (senseAmp->readLatency + matchlineDelay) * CAM_opt->BitSerialWidth;
                        //std::cout << "error out plz" << cellReadEnergy << std::endl;
		}
		cellReadEnergy *= numColumn / muxSenseAmp;
                        //std::cout << "error out plz" << cellReadEnergy << std::endl;

		energyDriveSearch0 = 0;
		energyDriveSearch1 = 0;

		for (int i = 0; i < inputParameter->cell->camNumRow; i++) {

			energyDriveSearch0 += RowDriver[i]->readDynamicEnergy / inputParameter->tech->vdd 
                            / inputParameter->tech->vdd * Row[i]->CellPort.volSearch0 * Row[i]->CellPort.volSearch0;

			energyDriveSearch1 += RowDriver[i]->readDynamicEnergy / inputParameter->tech->vdd 
                            / inputParameter->tech->vdd * Row[i]->CellPort.volSearch1 * Row[i]->CellPort.volSearch1;
		}

                searchDynamicEnergy += (energyDriveSearch0 + energyDriveSearch1)/2;
                //std::cout << "[CAM_Subarray] searchDynamicEnergy " << searchDynamicEnergy << std::endl;

		readDynamicEnergy = searchDynamicEnergy + inputBuf->readDynamicEnergy * numRow 
                    + inputEnc->readDynamicEnergy * numRow
		    + cellReadEnergy + ColDecMergeNand->readDynamicEnergy + precharger->readDynamicEnergy
		    + senseAmpMuxLev1Nand->readDynamicEnergy + senseAmpMuxLev2Nand->readDynamicEnergy
                    + ColMux[indexMatchline]->readDynamicEnergy + senseAmp->readDynamicEnergy 
                    + senseAmpMuxLev1->readDynamicEnergy + senseAmpMuxLev2->readDynamicEnergy
		    + outputAcc->readDynamicEnergy + priorityEnc->readDynamicEnergy 
                    + outputBuf->readDynamicEnergy * numColumn //removed ; here to include the following line as code that actually does things, unsure if that's completely expected
		    + inputLS->readDynamicEnergy * numRow + outputLS->readDynamicEnergy * numColumn;
                    
                /*
                std::cout << "[CAM_Subarray] searchDynamicEnergy " << searchDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] inputBuf->readDynamicEnergy " << inputBuf->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] inputEnc->readDynamicEnergy " << inputEnc->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] ColDecMergeNand->readDynamicEnergy " 
                    << ColDecMergeNand->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] precharger->readDynamicEnergy " << precharger->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] senseAmpMuxLev1Nand->readDynamicEnergy " 
                    << senseAmpMuxLev1Nand->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] senseAmpMuxLev1Nand->readDynamicEnergy " 
                    << senseAmpMuxLev1Nand->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] ColMux[indexMatchline]->readDynamicEnergy " 
                    << ColMux[indexMatchline]->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] senseAmp->readDynamicEnergy " << senseAmp->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] senseAmpMuxLev1->readDynamicEnergy " 
                    << senseAmpMuxLev1->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] senseAmpMuxLev2->readDynamicEnergy " 
                    << senseAmpMuxLev2->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] outputAcc->readDynamicEnergy " << outputAcc->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] priorityEnc->readDynamicEnergy " << priorityEnc->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] outputBuf->readDynamicEnergy " << outputBuf->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] inputLS->readDynamicEnergy " << inputLS->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] outputLS->readDynamicEnergy " << outputLS->readDynamicEnergy << std::endl;
                std::cout << "[CAM_Subarray] numRow " << numRow << std::endl;
                std::cout << "[CAM_Subarray] cellReadEnergy " << cellReadEnergy << std::endl;
                std::cout << "[CAM_Subarray] numColumn " << numColumn << std::endl;
                
                std::cout << "[CAM_SubArray] readDynamicEnergy " << readDynamicEnergy << std::endl;
                */
                
 	// std::cout << __FILE__ << ": " << __LINE__ << "read eng" << inputBuf->readDynamicEnergy << ": " << inputEnc->readDynamicEnergy << ": " << cellReadEnergy  << ": " << ColDecMergeNand->readDynamicEnergy << ": " << precharger->readDynamicEnergy << ": " << senseAmpMuxLev1Nand->readDynamicEnergy + senseAmpMuxLev2Nand->readDynamicEnergy+ ColMux[indexMatchline]->readDynamicEnergy+ senseAmp->readDynamicEnergy + senseAmpMuxLev1->readDynamicEnergy + senseAmpMuxLev2->readDynamicEnergy << std::endl;	
        
		searchDynamicEnergy +=  inputBuf->readDynamicEnergy * numRow + inputEnc->readDynamicEnergy * numRow
				+ cellReadEnergy + ColDecMergeNand->readDynamicEnergy + precharger->readDynamicEnergy
				+ ColMux[indexMatchline]->readDynamicEnergy
				+ senseAmp->readDynamicEnergy
				+ outputAcc->readDynamicEnergy + priorityEnc->readDynamicEnergy 
                                + outputBuf->readDynamicEnergy * numColumn;

                //std::cout << readDynamicEnergy << std::endl;
		//////////////////////////////////////////////////////////////////////////////////
		//		   							calc write									//
		//////////////////////////////////////////////////////////////////////////////////
		numBitline = 0;
		indexBitline = 0;
		// default status is using ML as the BL, as it is in the case of JSSC 2t2r
		for(int i=0;i<inputParameter->cell->camNumCol;i++){
			if (Col[i]->CellPort.Type == Bitline) {
				indexBitline = i;
				numBitline++;
			}
		}
		if (inputParameter->cell->memCellType == SRAM) {
			// since the SRAM cell is not flexible, we can make the coding simpler
			double capSRAMin;
			double capSRAMout;
			// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
			CalculateGateCapacitance(INV, 1, inputParameter->cell->widthSRAMCellNMOS, inputParameter->cell->widthSRAMCellPMOS,
					inputParameter->tech->featureSize*MAX_TRANSISTOR_HEIGHT, inputParameter->tech, &capSRAMin, &capSRAMout);
			cellResetEnergy = (capSRAMin + capSRAMout) * inputParameter->tech->vdd * inputParameter->tech->vdd;
			cellResetEnergy += (Col[indexBitline]->cap + ColMux[indexBitline]->capForPreviousPowerCalculation)
					* inputParameter->tech->vdd  * inputParameter->tech->vdd;
			cellSetEnergy = cellResetEnergy;
			// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
		} else if (inputParameter->cell->memCellType == MRAM || inputParameter->cell->memCellType == PCRAM || inputParameter->cell->memCellType == memristor || inputParameter->cell->memCellType == FEFETRAM) {
			/* Ignore the dynamic transition during the SET/RESET operation */
			/* Assume that the cell resistance keeps high for worst-case power estimation */
			inputParameter->cell->CalculateWriteEnergy();

			// TODO: MLC MRS set
			resetEnergyPerBit = inputParameter->cell->resetEnergy;
			setEnergyPerBit = inputParameter->cell->setEnergy;
			for(int i=0;i<inputParameter->cell->camNumCol;i++){
				// since each line has a description of the set/reset voltage already, we do not need setMode and resetMode in original nvsim anymore
				// for current set/reset mode, it has to be converted to the description of voltage set/reset in the cell configuration file
				// for example, the matchline voltage will be zero when writing in ISSCC'15 3t1r
				setEnergyPerBit += (capCellAccess + Col[indexBitline]->cap + ColMux[i]->capForPreviousPowerCalculation)
										* Col[i]->CellPort.volSetLRS * Col[i]->CellPort.volSetLRS;
				resetEnergyPerBit += (capCellAccess + Col[indexBitline]->cap + ColMux[i]->capForPreviousPowerCalculation)
										* Col[i]->CellPort.volReset * Col[i]->CellPort.volReset;

			}
			

			if (inputParameter->cell->memCellType == PCRAM) { //PCRAM write energy
				if (inputParameter->writeScheme == write_and_verify) {
					/*TODO: write and verify programming */
				} else {
					cellResetEnergy = resetEnergyPerBit / SHAPER_EFFICIENCY_CONSERVATIVE;
					cellSetEnergy = setEnergyPerBit / SHAPER_EFFICIENCY_CONSERVATIVE;  /* Due to the shaper inefficiency */
				}
			} else { //MRAM and memristor + FEFET write energy

				cellResetEnergy = resetEnergyPerBit / SHAPER_EFFICIENCY_AGGRESSIVE;
				cellSetEnergy = setEnergyPerBit / SHAPER_EFFICIENCY_AGGRESSIVE;  /* Due to the shaper inefficiency */
			}
			leakage = 0;                       //TODO: cell leaks during read/write operation
		}
		writeDynamicEnergy = MAX(cellResetEnergy, cellSetEnergy);
		cellResetEnergy *= numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
		cellSetEnergy *= numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;
		writeDynamicEnergy *= numColumn / muxSenseAmp / muxOutputLev1 / muxOutputLev2;

		// TODO: does not calculate MLC case
		setDynamicEnergy = resetDynamicEnergy = 0;
		for(int i=0;i<inputParameter->cell->camNumRow;i++){
			setDynamicEnergy += RowDriver[i]->writeDynamicEnergy / inputParameter->tech->vdd / inputParameter->tech->vdd
					* Row[i]->CellPort.volSetLRS * Row[i]->CellPort.volSetLRS;
			resetDynamicEnergy += RowDriver[i]->writeDynamicEnergy / inputParameter->tech->vdd / inputParameter->tech->vdd
					* Row[i]->CellPort.volReset * Row[i]->CellPort.volReset;
			writeDynamicEnergy += RowDriver[i]->writeDynamicEnergy /*/ inputParameter->tech->vdd / inputParameter->tech->vdd
				* (Row[i]->CellPort.volReset + Row[i]->CellPort.volSetLRS) * (Row[i]->CellPort.volSetLRS + Row[i]->CellPort.volReset)*/;
			
		}
		for(int i=0;i<inputParameter->cell->camNumCol;i++){
			if(indexBitline == i && i > 0) {
				// this is skipping the matchline that is not used in writing
				continue;
			}
			setDynamicEnergy += ColMux[i]->writeDynamicEnergy;
			resetDynamicEnergy += ColMux[i]->writeDynamicEnergy;
			writeDynamicEnergy += ColMux[i]->writeDynamicEnergy;
		}
		if (indexBitline == 0) {
			// ML is also used in the writing operations
			setDynamicEnergy += senseAmp->writeDynamicEnergy;
			resetDynamicEnergy += senseAmp->writeDynamicEnergy;
			writeDynamicEnergy += senseAmp->writeDynamicEnergy;
		}

		WriteDriverDyn = 0;
		WriteDriverLeakage = 0;
		if (withWriteDriver) {
			for(int i=0;i<inputParameter->cell->camNumCol;i++){
				if (WriteDriver[i]->initialized) {
					//WriteDriver[i]->CalculatePower();
					WriteDriverDyn += WriteDriver[i]->writeDynamicEnergy;
				}
			}
		}

		writeDynamicEnergy += ColDecMergeNand->writeDynamicEnergy + WriteDriverDyn
				+ senseAmpMuxLev1Nand->writeDynamicEnergy + senseAmpMuxLev2Nand->writeDynamicEnergy
				+ senseAmpMuxLev1->writeDynamicEnergy + senseAmpMuxLev2->writeDynamicEnergy
				+ inputLS->writeDynamicEnergy * numRow ;
		// std::cout << "Write Dynamic Energy in Subarray: " << writeDynamicEnergy << std::endl;
		/* for assymetric RESET and SET latency calculation only */
		setDynamicEnergy += cellSetEnergy + ColDecMergeNand->writeDynamicEnergy
				+ senseAmpMuxLev1Nand->writeDynamicEnergy + senseAmpMuxLev2Nand->writeDynamicEnergy
				+ senseAmpMuxLev1->writeDynamicEnergy + senseAmpMuxLev2->writeDynamicEnergy;
		resetDynamicEnergy += setDynamicEnergy + ColDecMergeNand->writeDynamicEnergy
				+ senseAmpMuxLev1Nand->writeDynamicEnergy + senseAmpMuxLev2Nand->writeDynamicEnergy
				+ senseAmpMuxLev1->writeDynamicEnergy + senseAmpMuxLev2->writeDynamicEnergy;

		// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
		//////////////////////////////////////////////////////////////////////////////////
		//		   							calc leakage								//
		//////////////////////////////////////////////////////////////////////////////////

		// leakage inside the cell
		if (inputParameter->cell->memCellType == SRAM) {
			leakage = CalculateGateLeakage(INV, 1, inputParameter->cell->widthSRAMCellNMOS * inputParameter->tech->featureSize,
					inputParameter->cell->widthSRAMCellPMOS * inputParameter->tech->featureSize, inputParameter->temperature, inputParameter->tech)
							* inputParameter->tech->vdd * 2;	/* two inverters per SRAM cell */
			leakage += CalculateGateLeakage(INV, 1, inputParameter->cell->widthAccessCMOS * inputParameter->tech->featureSize, 0,
							inputParameter->temperature, inputParameter->tech) * inputParameter->tech->vdd;	/* two accesses NMOS, but combined as one with vdd crossed */
			leakage += CalculateGateLeakage(INV, 1, inputParameter->cell->widthAccessCMOS * inputParameter->tech->featureSize, 0,
										inputParameter->temperature, inputParameter->tech) * inputParameter->tech->vdd;	/* two accesses NMOS, but combined as one with vdd crossed */
			leakage += CalculateGateLeakage(INV, 1, inputParameter->cell->camWidthMatchTran * inputParameter->tech->featureSize, 0,
										inputParameter->temperature, inputParameter->tech) * inputParameter->tech->vdd;	/* two accesses NMOS, but combined as one with vdd crossed */
			leakage *= 2;
		} else if (inputParameter->cell->memCellType == MRAM || inputParameter->cell->memCellType == PCRAM || inputParameter->cell->memCellType == memristor || inputParameter->cell->memCellType == FEFETRAM) {
			// basically count the transistors in the cell
			// the trick here is that every transistor in the cell should be connected by some line to the gate to control it
			// the exception is the matchline transistor in cmos-access, like ISSCC'15 3t1r
			leakage = 0;
			for(int i=0;i<inputParameter->cell->camNumRow;i++) {
                                //TODO: for some reason leak wasn't defined, setting it to true on a whim
                                //TODO: Row[i]->CellPort.ConnectedRegion also not defined...
                                Row[i]->CellPort.leak = false;
				if (Row[i]->CellPort.ConnectedRegion == gate && Row[i]->CellPort.leak) {
					if (Row[i]->CellPort.isNMOS)
						leakage += CalculateGateLeakage(INV, 1, 
                                                        Row[i]->CellPort.widthCmos * inputParameter->tech->featureSize,
                                                        0, 
                                                        inputParameter->temperature, 
                                                        inputParameter->tech) * inputParameter->tech->vdd;
					else
						leakage += CalculateGateLeakage(INV, 1, 0, 
                                                        Row[i]->CellPort.widthCmos * inputParameter->tech->featureSize,
							inputParameter->temperature, 
                                                        inputParameter->tech) * inputParameter->tech->vdd;
				}
			}
			for (int i=0; i<inputParameter->cell->camNumCol; i++) {
                                //TODO: for some reason leak wasn't defined, setting it to true on a whim
                                Col[i]->CellPort.leak = false;
				if (Col[i]->CellPort.ConnectedRegion == gate && Col[i]->CellPort.leak) {
					if (Col[i]->CellPort.isNMOS)
						leakage += CalculateGateLeakage(INV, 1, Col[i]->CellPort.widthCmos * inputParameter->tech->featureSize, 0,
							inputParameter->temperature, inputParameter->tech) * inputParameter->tech->vdd;
					else
						leakage += CalculateGateLeakage(INV, 1, 0, Col[i]->CellPort.widthCmos * inputParameter->tech->featureSize,
													inputParameter->temperature, inputParameter->tech) * inputParameter->tech->vdd;
				}
			}
			if (inputParameter->cell->accessType == CMOS_access) {
				leakage += CalculateGateLeakage(INV, 1, inputParameter->cell->camWidthMatchTran * inputParameter->tech->featureSize,  0,
						inputParameter->temperature, inputParameter->tech) * inputParameter->tech->vdd;
			}
		} else {
			invalid = true;
			PRINT_VERBOSE("[CAM_SubArray] Error: cell type input error");
			return;
		}
		leakage *= numRow * numColumn;

		//////////////////////////
		double leak = 0;
		for(int i=0;i<inputParameter->cell->camNumRow;i++){
			leakage += RowDriver[i]->leakage;
			leak += RowDriver[i]->leakage;
		}
		for(int i=0;i<inputParameter->cell->camNumCol;i++){
			leakage += ColMux[i]->leakage;
			leak += ColMux[i]->leakage;
		}

		leakage += inputBuf->leakage * numColumn + inputEnc->leakage + precharger->leakage + senseAmpMuxLev1Nand->leakage
				+ senseAmpMuxLev2Nand->leakage + ColDecMergeNand->leakage + WriteDriverLeakage + senseAmp->leakage
				+ senseAmpMuxLev1->leakage + senseAmpMuxLev2->leakage + outputAcc->leakage + priorityEnc->leakage
				+ outputBuf->leakage * numColumn;
		leak += inputBuf->leakage * numColumn + inputEnc->leakage + precharger->leakage + senseAmpMuxLev1Nand->leakage
				+ senseAmpMuxLev2Nand->leakage + ColDecMergeNand->leakage + WriteDriverLeakage + senseAmp->leakage
				+ senseAmpMuxLev1->leakage + senseAmpMuxLev2->leakage + outputAcc->leakage + priorityEnc->leakage
				+ outputBuf->leakage * numColumn;
		//std::cout << "leakage energy of peripherals: " << leak * 1e9 << std::endl;
		// std::cout << __FILE__ << ": " << __LINE__ << std::endl;
	}

}

void CAM_SubArray::PrintProperty() {
	std::cout << "CAMSubarray Properties:" << std::endl;
	//FunctionUnit::PrintProperty();
	std::cout << "numRow:" << numRow << " numColumn:" << numColumn << std::endl;
	// area wise
	std::cout << "Input Encoder Area:" << inputEnc->area*1e12 << " um^2 (" << inputEnc->area/area*100 << "%)" << std::endl;
	for(int i=0;i<inputParameter->cell->camNumRow;i++){
		std::cout << "Row Driver " << i << " Area:" << RowDriver[i]->area*1e12 << " um^2 (" << RowDriver[i]->area/area*100 << "%)" <<std::endl;
	}
	std::cout << "lenWordline * lenBitline = " << lenRow*1e6 << " um * " << lenCol*1e6 << " um = " << lenRow * lenCol * 1e12
			<< " um^2 (" << lenRow * lenCol/area*100 << "%)" << std::endl;
	std::cout << "MergeDecoderNand Area:" << RowDecMergeNand->area*1e12 << " um^2 (" << RowDecMergeNand->area/area*100 << "%)" << std::endl;
	for(int i=0;i<inputParameter->cell->camNumCol;i++){
		std::cout << "Col Mux " << i << " Area:" << ColMux[i]->area*1e12 << " um^2 (" << ColMux[i]->area/area*100 << "%)" << std::endl;
	}
	std::cout << "Write Driver Area:" << WriteDriverArea*1e12 << " um^2 (" << WriteDriverArea/area*100 << "%)" << std::endl;
	std::cout << "Mux Area:" << inputEnc->area*1e12 << " um^2" << std::endl;
	std::cout << "Sense Amplifier Area:" << senseAmp->area*1e12 << " um^2 (" << senseAmp->area/area*100 << "%)" << std::endl;
	std::cout << "Output Acc Area:" << outputAcc->area*1e12 << " um^2 (" << outputAcc->area/area*100 << "%)" << std::endl;
	std::cout << "Priority Encoder Area:" << priorityEnc->area*1e12 << " um^2 (" << priorityEnc->area/area*100 << "%)" << std::endl;

	// TODO: not done with debug interface yet
	std::cout << "matchlineDelay: " << matchlineDelay*1e12 << "ps" << std::endl;
	std::cout << "chargeLatency: " << chargeLatency*1e12 << "ps" << std::endl;
	std::cout << "columnDecoderLatency: " << columnDecoderLatency*1e12 << "ps" << std::endl;
	std::cout << "errors exist here!" << std::endl;
}

CAM_SubArray & CAM_SubArray::operator=(const CAM_SubArray &rhs) {
	// TODO: did not go through clearly
	height = rhs.height;
	width = rhs.width;
	area = rhs.area;
	readLatency = rhs.readLatency;
	writeLatency = rhs.writeLatency;
	readDynamicEnergy = rhs.readDynamicEnergy;
	writeDynamicEnergy = rhs.writeDynamicEnergy;
	resetLatency = rhs.resetLatency;
	setLatency = rhs.setLatency;
	resetDynamicEnergy = rhs.resetDynamicEnergy;
	setDynamicEnergy = rhs.setDynamicEnergy;
	cellReadEnergy = rhs.cellReadEnergy;
	cellResetEnergy = rhs.cellResetEnergy;
	cellSetEnergy = rhs.cellSetEnergy;
	leakage = rhs.leakage;
	initialized = rhs.initialized;
	numRow = rhs.numRow;
	numColumn = rhs.numColumn;
	multipleRowPerSet = rhs.multipleRowPerSet;
	split = rhs.split;
	muxSenseAmp = rhs.muxSenseAmp;
	internalSenseAmp = rhs.internalSenseAmp;
	muxOutputLev1 = rhs.muxOutputLev1;
	muxOutputLev2 = rhs.muxOutputLev2;

	voltageSense = rhs.voltageSense;
	senseVoltage = rhs.senseVoltage;
	numSenseAmp = rhs.numSenseAmp;
	resCellAccess = rhs.resCellAccess;
	capCellAccess = rhs.capCellAccess;
	matchlineDelay = rhs.matchlineDelay;
	chargeLatency = rhs.chargeLatency;
	columnDecoderLatency = rhs.columnDecoderLatency;
	// bitlineDelayOn = rhs.bitlineDelayOn;
	// bitlineDelayOff = rhs.bitlineDelayOff;
	/////////////////////////////
	// tau = rhs.tau;
	// Basetau = rhs.Basetau;
	// SenseTime = rhs.SenseTime;
	// MatchlineSenseMargin = rhs.MatchlineSenseMargin;
	//////////////////////////////
	resInSerialForSenseAmp = rhs.resInSerialForSenseAmp;
	resEquivalentOn = rhs.resEquivalentOn;
	resEquivalentOff = rhs.resEquivalentOff;
	resMemCellOff = rhs.resMemCellOff;
	resMemCellOn = rhs.resMemCellOn;

	senseAmpMuxLev1 = rhs.senseAmpMuxLev1;
	senseAmpMuxLev2 = rhs.senseAmpMuxLev2;
	precharger = rhs.precharger;
	senseAmp = rhs.senseAmp;

	inputEnc = rhs.inputEnc;
	RowDecMergeNand = rhs.RowDecMergeNand;
	for(int i=0;i<MAX_PORT;i++){
		RowDriver[i] = rhs.RowDriver[i];
	}
	precharger = rhs.precharger;
	ColDecMergeNand = rhs.ColDecMergeNand;
	for(int i=0;i<MAX_PORT;i++){
		ColMux[i] = rhs.ColMux[i];
	}
	senseAmpMuxLev1Nand = rhs.senseAmpMuxLev1Nand;
	senseAmpMuxLev2Nand = rhs.senseAmpMuxLev2Nand;
	outputAcc = rhs.outputAcc;
	priorityEnc = rhs.priorityEnc;

	numRow = rhs.numRow;
	numColumn = rhs.numColumn;
	lenRow = rhs.lenRow;
	lenCol = rhs.lenCol;
	indexMatchline = rhs.indexMatchline;
	indexBitline = rhs.indexBitline;
	numBitline = rhs.numBitline;
	for(int i=0;i<MAX_PORT;i++){
		Row[i] = rhs.Row[i];
		Col[i] = rhs.Col[i];
	}
	withInputEnc = rhs.withInputEnc;
	typeInputEnc = rhs.typeInputEnc;
	customInputEnc = rhs.customInputEnc;
	DecMergeOptLevel = rhs.DecMergeOptLevel;
	RowDecMergeInv = rhs.RowDecMergeInv;
	DriverOptLevel = rhs.DriverOptLevel;
	voltagePrecharge = rhs.voltagePrecharge;
	typeSenseAmp = rhs.typeSenseAmp;
	customSenseAmp = rhs.customSenseAmp;
	senseMargin = rhs.senseMargin;
	withOutputAcc = rhs.withOutputAcc;
	withPriorityEnc = rhs.withPriorityEnc;
	PriorityOptLevel = rhs.PriorityOptLevel;
	withWriteDriver = rhs.withWriteDriver;
	WriteDriverArea = rhs.WriteDriverArea;
	WriteDriverLatency = rhs.WriteDriverLatency;
	WriteDriverDyn = rhs.WriteDriverDyn;
	WriteDriverLeakage = rhs.WriteDriverLeakage;
	withInputBuf = rhs.withInputBuf;
	withOutputBuf = rhs.withOutputBuf;
	senseAmpLatency = rhs.senseAmpLatency;
	referDelay = rhs.referDelay;
	volMatchDrop = rhs.volMatchDrop;
	// volAllMissDrop = rhs.volAllMissDrop;

	decoderLatency = rhs.decoderLatency;
	matchlineDelay = rhs.matchlineDelay;
	matchlineRamp = rhs.matchlineRamp;
	chargeLatency = rhs.chargeLatency;
	searchLatency = rhs.searchLatency;
	searchDynamicEnergy = rhs.searchDynamicEnergy;
	energyDriveSearch0 = rhs.energyDriveSearch0;
	energyDriveSearch1 = rhs.energyDriveSearch1;
	rampOutput = rhs.rampOutput;
	resetEnergyPerBit = rhs.resetEnergyPerBit;
	setEnergyPerBit = rhs.setEnergyPerBit;
	return *this;
}



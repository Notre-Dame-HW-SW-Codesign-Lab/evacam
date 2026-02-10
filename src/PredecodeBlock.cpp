#include "../include/PredecodeBlock.h"
#include "../include/formula.h"
#include "../include/global.h"
#include "../include/macros.h"
/*
PredecodeBlock::PredecodeBlock() {
	// TODO Auto-generated constructor stub
	initialized = false;
	//rowDecoderStage1A = NULL;
	//rowDecoderStage1B = NULL;
	//rowDecoderStage1C = NULL;
	//rowDecoderStage2 = NULL;
	//basicDecoderA1 = NULL;
	//basicDecoderA2 = NULL;
	//basicDecoderB = NULL;
	//basicDecoderC = NULL;
}

PredecodeBlock::~PredecodeBlock() {
	// TODO Auto-generated destructor stub
	 smart pointers
        if (rowDecoderStage1A) delete rowDecoderStage1A;
	if (rowDecoderStage1B) delete rowDecoderStage1B;
	if (rowDecoderStage1C) delete rowDecoderStage1C;
	if (rowDecoderStage2)  delete rowDecoderStage2;
        
	if (basicDecoderA1)    delete basicDecoderA1;
	if (basicDecoderA2)    delete basicDecoderA2;
	if (basicDecoderB)     delete basicDecoderB;
	if (basicDecoderC)     delete basicDecoderC;
        
}
*/
void PredecodeBlock::Initialize(int _numAddressBit, double _capLoad, double _resLoad,
        std::shared_ptr<InputParameter> _inputParameter) {
	if (initialized)
		std::cout << "[Predecoder Block] Warning: Already initialized!" << std::endl;

        inputParameter = _inputParameter;
	numAddressBit =_numAddressBit;
	if (numAddressBit > 27 ) {
		std::cout << "[Predecoder Block] Error: Invalid number of address bits" << std::endl;
		throw std::runtime_error("Error");
	} else if (numAddressBit == 0) {
		height = width = area = 0;
		readLatency = writeLatency = 0;
		readDynamicEnergy = writeDynamicEnergy = 0;
		leakage = 0;
		initialized = true;
	} else {
		capLoad = _capLoad;
		resLoad = _resLoad;

		numDecoder12 = numDecoder24 = numDecoder38 = 0;
		numOutputAddressBit = 1 << numAddressBit;
		if (numAddressBit == 1) {
			numDecoder12 = 1;
		} else {
			int numAddressMod3 = numAddressBit % 3;
			if (numAddressMod3 == 2) {
				numDecoder24 = 1;
			} else if (numAddressMod3 == 1) {
				numDecoder24 = 2;
			}
			numDecoder38 = (numAddressBit - 2 * numDecoder24) / 3;
		}
		int numBasicDecoder = numDecoder12 + numDecoder24 + numDecoder38;
		if (numBasicDecoder <= 1){
			rowDecoderStage1A = NULL;
			rowDecoderStage1B = NULL;
			rowDecoderStage1C = NULL;
			rowDecoderStage2 = NULL;
		} else if (numBasicDecoder <= 3) {
			numNandInputStage1A = numBasicDecoder;
			rowDecoderStage2 = NULL;
			rowDecoderStage1B = NULL;
			rowDecoderStage1C = NULL;
			rowDecoderStage1A = std::make_shared<RowDecoder>();
			rowDecoderStage1A->Initialize(numOutputAddressBit, capLoad, resLoad, numNandInputStage1A == 3, latency_first, 0, inputParameter);
			//rowDecoderStage1A->CalculateRC();
		} else {
			rowDecoderStage2 = std::make_shared<RowDecoder>();
			double capLoadStage1A, capLoadStage1B, capLoadStage1C;
			if (numBasicDecoder <= 6) {
				rowDecoderStage2->Initialize(numOutputAddressBit, capLoad, resLoad, false, latency_first, 0, inputParameter);
				//rowDecoderStage2->CalculateRC();
				numNandInputStage1B = numBasicDecoder / 2;
				numNandInputStage1A = numBasicDecoder - numNandInputStage1B;
				numAddressBitStage1A = numAddressBitStage1B = 1;
				int i = 3 * numNandInputStage1A - numDecoder24;
				numAddressBitStage1A <<= i;
				numAddressBitStage1B <<= 3 * numNandInputStage1B;
				capLoadStage1A = numAddressBitStage1B * rowDecoderStage2->capNandInput;
				capLoadStage1B = numAddressBitStage1A * rowDecoderStage2->capNandInput;
				rowDecoderStage1C = NULL;
				rowDecoderStage1A = std::make_shared<RowDecoder>();
			    rowDecoderStage1A->Initialize(numAddressBitStage1A, capLoadStage1A, 0 /* TODO */, numNandInputStage1A == 3, latency_first, 0, inputParameter);
			    //rowDecoderStage1A->CalculateRC();
			    rowDecoderStage1B = std::make_shared<RowDecoder>();
			    rowDecoderStage1B->Initialize(numAddressBitStage1B, capLoadStage1B, 0 /* TODO */, numNandInputStage1B == 3, latency_first, 0, inputParameter);
			    //rowDecoderStage1B->CalculateRC();
			} else if (numBasicDecoder <= 9){
				rowDecoderStage2->Initialize(numOutputAddressBit, capLoad, resLoad, true, latency_first, 0, inputParameter);
				//rowDecoderStage2->CalculateRC();
				if (numBasicDecoder == 7) {
					numNandInputStage1A = 3;
					numNandInputStage1B = 2;
					numNandInputStage1C = 2;
					numAddressBitStage1B = 64;
					numAddressBitStage1C = 64;
				} else if (numBasicDecoder == 8) {
					numNandInputStage1A = 3;
					numNandInputStage1B = 3;
				    numNandInputStage1C = 2;
				    numAddressBitStage1B = 512;
				    numAddressBitStage1C = 64;
				} else {
					numNandInputStage1A = 3;
					numNandInputStage1B = 3;
				    numNandInputStage1C = 3;
				    numAddressBitStage1B = 512;
				    numAddressBitStage1C = 512;
				}
				int i = 3 * numNandInputStage1A - numDecoder24;
				numAddressBitStage1A <<= i;
				capLoadStage1A = numAddressBitStage1B * numAddressBitStage1C * rowDecoderStage2->capNandInput;
				capLoadStage1B = numAddressBitStage1A * numAddressBitStage1C * rowDecoderStage2->capNandInput;
				capLoadStage1C = numAddressBitStage1A * numAddressBitStage1B * rowDecoderStage2->capNandInput;
				rowDecoderStage1A = std::make_shared<RowDecoder>();
				rowDecoderStage1A->Initialize(numAddressBitStage1A, capLoadStage1A, 0 /* TODO */, numNandInputStage1A == 3, latency_first, 0, inputParameter);
				//rowDecoderStage1A->CalculateRC();
				rowDecoderStage1B = std::make_shared<RowDecoder>();
				rowDecoderStage1B->Initialize(numAddressBitStage1B, capLoadStage1B, 0 /* TODO */, numNandInputStage1B == 3, latency_first, 0, inputParameter);
				//rowDecoderStage1B->CalculateRC();
				rowDecoderStage1C = std::make_shared<RowDecoder>();
				rowDecoderStage1C->Initialize(numAddressBitStage1C, capLoadStage1C, 0 /* TODO */, numNandInputStage1C == 3, latency_first, 0, inputParameter);
				//rowDecoderStage1C->CalculateRC();
			}
		}
		if (rowDecoderStage1C != NULL) {
			if (numNandInputStage1C == 2){
				capLoadBasicDecoderC = 8 * rowDecoderStage1C->capNandInput;
			} else {
				capLoadBasicDecoderC = 64 * rowDecoderStage1C->capNandInput;
			}
			basicDecoderC = std::make_shared<BasicDecoder>();
			basicDecoderC->Initialize(3, capLoadBasicDecoderC, 0, inputParameter /* TODO */);
		} else {
			basicDecoderC = NULL;
		}
		if (rowDecoderStage1B != NULL) {
			if (numNandInputStage1B == 2){
				capLoadBasicDecoderB = 8 * rowDecoderStage1B->capNandInput;
			} else {
				capLoadBasicDecoderB = 64 * rowDecoderStage1B->capNandInput;
			}
			basicDecoderB = std::make_shared<BasicDecoder>();
			basicDecoderB->Initialize(3, capLoadBasicDecoderB, 0, inputParameter /* TODO */);
		} else {
			basicDecoderB = NULL;
		}
		if (rowDecoderStage1A != NULL) {
			int numCapNandA1, numCapNandA2;
			if (numDecoder24 == 0) {
				numBasicDecoderA1 = numNandInputStage1A;
				numBasicDecoderA2 = 0;
				numCapNandA1 = 1 << ( 3* (numNandInputStage1A - 1));
				capLoadBasicDecoderA1 = numCapNandA1 * rowDecoderStage1A->capNandInput;
				basicDecoderA1 = std::make_shared<BasicDecoder>();
				basicDecoderA1->Initialize(3, capLoadBasicDecoderA1, 0, inputParameter /* TODO */);
				basicDecoderA2 = NULL;
			} else if (numDecoder24 == 1) {
				numBasicDecoderA1 = 1;
				numBasicDecoderA2 = numNandInputStage1A - numBasicDecoderA1;
				numCapNandA1 = 1 << (3 * numBasicDecoderA2);
				numCapNandA2 = 1 << (2 + 3 * (numBasicDecoderA2 - 1));
				capLoadBasicDecoderA1 = numCapNandA1 * rowDecoderStage1A->capNandInput;
				capLoadBasicDecoderA2 = numCapNandA2 * rowDecoderStage1A->capNandInput;
				basicDecoderA1 = std::make_shared<BasicDecoder>();
			    basicDecoderA1->Initialize(2, capLoadBasicDecoderA1, 0, inputParameter /* TODO */);
			    basicDecoderA2 = std::make_shared<BasicDecoder>();
			    basicDecoderA2->Initialize(3, capLoadBasicDecoderA2, 0, inputParameter /* TODO */);
			} else if (numDecoder24 == 2) {
				if (numNandInputStage1A == 2) {
		    		numBasicDecoderA1 = 2;
		    		numBasicDecoderA2 = 0;
		    		basicDecoderA1 = std::make_shared<BasicDecoder>();
		    		basicDecoderA1->Initialize(2, 4 * rowDecoderStage1A->capNandInput, 0, inputParameter /* TODO */);
		    		basicDecoderA2 = NULL;
				} else {
		    		numBasicDecoderA1 = 2;
		    		numBasicDecoderA2 = 1;
		    		basicDecoderA1 = std::make_shared<BasicDecoder>();
		    		basicDecoderA1->Initialize(2, 32 * rowDecoderStage1A->capNandInput, 0, inputParameter /* TODO */);
		    		basicDecoderA2 = std::make_shared<BasicDecoder>();
		    		basicDecoderA2->Initialize(3, 16 * rowDecoderStage1A->capNandInput, 0, inputParameter /* TODO */);
				}
			}
		} else {
			numBasicDecoderA1 = 1;
			numBasicDecoderA2 = 0;
			basicDecoderA1 = std::make_shared<BasicDecoder>();
			basicDecoderA2 = NULL;
			if (numDecoder12 == 1) {
				basicDecoderA1->Initialize(1, capLoad, resLoad, inputParameter);
			} else if (numDecoder24 == 1) {
				basicDecoderA1->Initialize(2, capLoad, resLoad, inputParameter);
			} else if (numDecoder38 == 1)
				basicDecoderA1->Initialize(3, capLoad, resLoad, inputParameter);
		}
	}
	initialized = true;
        //CalculateArea();
        //CalculateRC();
        //CalculatePower();
}

void PredecodeBlock::CalculateArea() {
	if (!initialized) {
		std::cout << "[Predecoder Block] Error: Require initialization first!" << std::endl;
	} else if (numAddressBit == 0) {
		height = width = area = 0;
	} else {
		double hTemp,wTemp;
		hTemp = wTemp = 0;
		if (basicDecoderA1 != NULL) {
			//basicDecoderA1->CalculateArea();
			wTemp = std::max(wTemp, basicDecoderA1->width);
			hTemp += numBasicDecoderA1 * basicDecoderA1->height;
			if (basicDecoderA2 != NULL) {
				//basicDecoderA2->CalculateArea();
				wTemp = std::max(wTemp, basicDecoderA2->width);
				hTemp += numBasicDecoderA2 * basicDecoderA2->height;
			}
			if (basicDecoderB != NULL) {
				//basicDecoderB->CalculateArea();
				wTemp = std::max(wTemp, basicDecoderB->width);
				hTemp += numNandInputStage1B * basicDecoderB->height;
				if (basicDecoderC != NULL) {
					//basicDecoderC->CalculateArea();
					wTemp = std::max(wTemp, basicDecoderC->width);
					hTemp += numNandInputStage1C * basicDecoderC->height;
				}
			}
		}
		width = wTemp;
		height = hTemp;
		hTemp = wTemp = 0;
		if (rowDecoderStage1A != NULL) {
			//rowDecoderStage1A->CalculateArea();
			wTemp = std::max(wTemp, rowDecoderStage1A->width);
			hTemp += rowDecoderStage1A->height;
			if (rowDecoderStage1B != NULL) {
				//rowDecoderStage1B->CalculateArea();
				wTemp = std::max(wTemp, rowDecoderStage1B->width);
				hTemp += rowDecoderStage1B->height;
				if (rowDecoderStage1C != NULL) {
					//rowDecoderStage1C->CalculateArea();
					wTemp = std::max(wTemp, rowDecoderStage1C->width);
					hTemp += rowDecoderStage1C->height;
				}
			}
			if (rowDecoderStage2 != NULL) {
				//rowDecoderStage2->CalculateArea();
				wTemp += rowDecoderStage2->width;
				hTemp = std::max(hTemp, rowDecoderStage2->height);
			}
		}
		width += wTemp;
		height = std::max(height, hTemp);
		area = width * height;
	}
}

void PredecodeBlock::CalculateRC() {
	if (!initialized) {
		std::cout << "[Predecoder Block] Error: Require initialization first!" << std::endl;
	} else if (numAddressBit > 0) {
		if (basicDecoderA1 != NULL) {
			//basicDecoderA1->CalculateRC();
			if (basicDecoderA2 != NULL) {
				//basicDecoderA2->CalculateRC();
			}
			if (basicDecoderB != NULL) {
				//basicDecoderB->CalculateRC();
				if (basicDecoderC != NULL) {
					//basicDecoderC->CalculateRC();
				}
			}
		}
	}
}

void PredecodeBlock::CalculateLatency(double _rampInput) {
	if (!initialized) {
		std::cout << "[Predecoder Block] Error: Require initialization first!" << std::endl;
	} else if (numAddressBit == 0) {
		readLatency = writeLatency = 0;
		rampOutput = _rampInput;
	} else {
		rampInput = _rampInput;
		double delayA1, delayA2, delayB, delayC;
		double maxRampOutput = 0;
		delayA1 = delayA2 = delayB = delayC = 0;
		rampOutput = 0;
		readLatency = writeLatency = 0;
		if (basicDecoderA1 != NULL) {
			basicDecoderA1->CalculateLatency(rampInput);
			delayA1 += basicDecoderA1->readLatency;
			maxRampOutput = basicDecoderA1->rampOutput;
			if (rowDecoderStage1A != NULL) {
				rowDecoderStage1A->CalculateLatency(basicDecoderA1->rampOutput);
				delayA1 += rowDecoderStage1A->readLatency;
				maxRampOutput = rowDecoderStage1A->rampOutput;
				if (rowDecoderStage2 != NULL) {
					rowDecoderStage2->CalculateLatency(rowDecoderStage1A->rampOutput);
					delayA1 += rowDecoderStage2->readLatency;
					maxRampOutput = rowDecoderStage2->rampOutput;
				}
			}
		}
		rampOutput = std::max(rampOutput, maxRampOutput);
		readLatency = std::max(readLatency, delayA1);
		maxRampOutput = 0;
		if (basicDecoderA2 != NULL) {
			basicDecoderA2->CalculateLatency(rampInput);
			delayA2 += basicDecoderA1->readLatency;
			rowDecoderStage1A->CalculateLatency(basicDecoderA2->rampOutput);
			delayA2 += rowDecoderStage1A->readLatency;
			maxRampOutput = rowDecoderStage1A->rampOutput;
			if (rowDecoderStage2 != NULL) {
				rowDecoderStage2->CalculateLatency(rowDecoderStage1A->rampOutput);
				delayA2 += rowDecoderStage2->readLatency;
				maxRampOutput = rowDecoderStage2->rampOutput;
			}
		}
		rampOutput = std::max(rampOutput, maxRampOutput);
		readLatency = std::max(readLatency, delayA2);
		maxRampOutput = 0;
		if (basicDecoderB !=NULL) {
			basicDecoderB->CalculateLatency(rampInput);
			delayB += basicDecoderB->readLatency;
			rowDecoderStage1B->CalculateLatency(basicDecoderB->rampOutput);
			delayB += rowDecoderStage1B->readLatency;
			rowDecoderStage2->CalculateLatency(rowDecoderStage1B->rampOutput);
			delayB += rowDecoderStage2->readLatency;
			maxRampOutput = rowDecoderStage2->rampOutput;
		}
		rampOutput = std::max(rampOutput, maxRampOutput);
		readLatency = std::max(readLatency, delayB);
		maxRampOutput = 0;
		if (basicDecoderC !=NULL) {
			basicDecoderC->CalculateLatency(rampInput);
			delayC += basicDecoderC->readLatency;
			rowDecoderStage1C->CalculateLatency(basicDecoderC->rampOutput);
			delayC += rowDecoderStage1C->readLatency;
			rowDecoderStage2->CalculateLatency(rowDecoderStage1C->rampOutput);
			delayC += rowDecoderStage2->readLatency;
			maxRampOutput = rowDecoderStage2->rampOutput;
		}
		rampOutput = std::max(rampOutput, maxRampOutput);
		readLatency = std::max(readLatency, delayC);
		writeLatency = readLatency;
	}
}


void PredecodeBlock::CalculatePower() {
	if (!initialized) {
		std::cout << "[Predecoder Block] Error: Require initialization first!" << std::endl;
	} else if (numAddressBit == 0) {
		leakage = readDynamicEnergy = writeDynamicEnergy = 0;
	} else {
		leakage = readDynamicEnergy = 0;
		if (basicDecoderA1 != NULL) {
			//basicDecoderA1->CalculatePower();
			leakage += basicDecoderA1->leakage;
			readDynamicEnergy += basicDecoderA1->readDynamicEnergy;
			if (basicDecoderA2 != NULL) {
				//basicDecoderA2->CalculatePower();
				leakage += basicDecoderA2->leakage;
				readDynamicEnergy += basicDecoderA2->readDynamicEnergy;
			}
			if (basicDecoderB != NULL) {
				//basicDecoderB->CalculatePower();
				leakage += basicDecoderB->leakage;
				readDynamicEnergy += basicDecoderB->readDynamicEnergy;
				if (basicDecoderC != NULL) {
					//basicDecoderC->CalculatePower();
					leakage += basicDecoderC->leakage;
					readDynamicEnergy += basicDecoderC->readDynamicEnergy;
				}
			}
		}
		if (rowDecoderStage1A != NULL) {
			//rowDecoderStage1A->CalculatePower();
			leakage += rowDecoderStage1A->leakage;
			readDynamicEnergy += rowDecoderStage1A->readDynamicEnergy;
			if (rowDecoderStage1B != NULL) {
				//rowDecoderStage1B->CalculatePower();
				leakage += rowDecoderStage1B->leakage;
				readDynamicEnergy += rowDecoderStage1B->readDynamicEnergy;
				if (rowDecoderStage1C != NULL) {
					//rowDecoderStage1C->CalculatePower();
					leakage += rowDecoderStage1C->leakage;
					readDynamicEnergy += rowDecoderStage1C->readDynamicEnergy;
				}
			}
			if (rowDecoderStage2 != NULL) {
				leakage += rowDecoderStage2->leakage;
				readDynamicEnergy += rowDecoderStage2->readDynamicEnergy;
			}
		}
		writeDynamicEnergy = readDynamicEnergy;
	}
}

void PredecodeBlock::PrintProperty() {
	std::cout << "Predecoding Block Properties:" << std::endl;
	FunctionUnit::PrintProperty();
}

PredecodeBlock & PredecodeBlock::operator=(const PredecodeBlock &rhs) {
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
	cellSetEnergy = rhs.cellSetEnergy;
	cellResetEnergy = rhs.cellResetEnergy;
	leakage = rhs.leakage;

	initialized = rhs.initialized;
	numNandInputStage1A = rhs.numNandInputStage1A;
	numNandInputStage1B = rhs.numNandInputStage1B;
	numNandInputStage1C = rhs.numNandInputStage1C;
	numAddressBitStage1A = rhs.numAddressBitStage1A;
	numAddressBitStage1B = rhs.numAddressBitStage1B;
	numAddressBitStage1C = rhs.numAddressBitStage1C;
	capLoad = rhs.capLoad;
	resLoad = rhs.resLoad;
	numAddressBit = rhs.numAddressBit;
	numOutputAddressBit = rhs.numOutputAddressBit;
	numDecoder12 = rhs.numDecoder12;
	numDecoder24 = rhs.numDecoder24;
	numDecoder38 = rhs.numDecoder38;
	numBasicDecoderA1 = rhs.numBasicDecoderA1;
	numBasicDecoderA2 = rhs.numBasicDecoderA2;
	capLoadBasicDecoderA1 = rhs.capLoadBasicDecoderA1;
	capLoadBasicDecoderA2 = rhs.capLoadBasicDecoderA2;
	capLoadBasicDecoderB = rhs.capLoadBasicDecoderB;
	capLoadBasicDecoderC = rhs.capLoadBasicDecoderC;
	rampInput = rhs.rampInput;
	rampOutput = rhs.rampOutput;
	return *this;
}

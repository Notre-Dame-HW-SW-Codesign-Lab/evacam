#ifndef PREDECODEBLOCK_H_
#define PREDECODEBLOCK_H_

#include <memory>

#include "FunctionUnit.h"
#include "RowDecoder.h"
#include "BasicDecoder.h"

class PredecodeBlock: public FunctionUnit {
    public:
        PredecodeBlock() {
            initialized = false;
        }
        PredecodeBlock(const PredecodeBlock&) {}
        virtual ~PredecodeBlock() {}

        /* Functions */
        void PrintProperty();
        void Initialize(int _numAddressBit, double _capLoad, double _resLoad, 
                std::shared_ptr<EvaCamConfig> _config);
        void CalculateArea();
        void CalculateRC();
        void CalculateLatency(double _rampInput);
        void CalculatePower();
        PredecodeBlock & operator=(const PredecodeBlock &);
        std::unique_ptr<FunctionUnit> clone() const override {
            return std::make_unique<PredecodeBlock>(*this);
        }

        /* Properties */
        bool initialized;	/* Initialization flag */

        std::shared_ptr<RowDecoder> rowDecoderStage1A;
        std::shared_ptr<RowDecoder> rowDecoderStage1B;
        std::shared_ptr<RowDecoder> rowDecoderStage1C;
        std::shared_ptr<RowDecoder> rowDecoderStage2;
        std::shared_ptr<BasicDecoder> basicDecoderA1;
        std::shared_ptr<BasicDecoder> basicDecoderA2;
        std::shared_ptr<BasicDecoder> basicDecoderB;
        std::shared_ptr<BasicDecoder> basicDecoderC;

        int numNandInputStage1A, numNandInputStage1B, numNandInputStage1C;
        int numAddressBitStage1A, numAddressBitStage1B, numAddressBitStage1C;
        double capLoad;		/* Load capacitance Unit: F */
        double resLoad;     /* Load resistance Unit: ohm */
        int numAddressBit;   /* Number of Address Bits assigned to the block */
        int numOutputAddressBit;
        int numDecoder12;          /* Number of 1 to 2 Decoders */
        int numDecoder24;          /* Number of 2 to 4 Decoders */
        int numDecoder38;          /* Number of 3 to 8 Decoders */
        int numBasicDecoderA1, numBasicDecoderA2;
        double capLoadBasicDecoderA1, capLoadBasicDecoderA2, capLoadBasicDecoderB, capLoadBasicDecoderC;
        double rampInput, rampOutput;
        /* TODO: Predecoder so far does not take OptPriority input because the output driver is already quite fixed in this module */
};

#endif /* PREDECODEBLOCK_H_ */

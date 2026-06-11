#include "Bank.h"
#include "formula.h"
#include "UnitFormatter.h"

void Bank::PrintProperty() {
    std::cout << "Bank Properties:" << std::endl;
    FunctionUnit::PrintProperty();
}

void Bank::debug() {
    std::cout << & mat << std::endl;
    std::cout << & numActiveSubarrayPerRow << std::endl;
}

bool Bank::match(const std::vector<int> &stored, const std::vector<int> &query) const {
    return evaluate(stored, query).hit;
}

EvaCAMMatchResult Bank::evaluate(const std::vector<int> &stored, const std::vector<int> &query) const {
    if (!mat || !mat->subarray)
        throw std::runtime_error("[Bank] Error: bank is not initialized for matching.");

    return mat->subarray->EvaluateBinaryMatch(stored, query);
}

void Bank::printbreakdown() {
    double latency = 0;
    if (config->peripherals.noPrechargeInc) {
        latency = mat->subarray->matchlineDelay + mat->subarray->ColMux[mat->subarray->indexMatchline]->readLatency
            + mat->subarray->senseAmpLatency + mat->subarray->outputAcc->readLatency;
    } else {
        latency = mat->subarray->searchLatency;
    }

    double energy = mat->subarray->searchDynamicEnergy;
    if (config->peripherals.includeLeakage) {
        energy += mat->leakage * latency;
    }
    if (config->peripherals.scaledVoltage > 0) {
        energy = energy / config->technology.tech->vdd() / config->technology.tech->vdd() * config->peripherals.scaledVoltage * config->peripherals.scaledVoltage;
    }




    // ///////////////       AREA              //////////////

    // std::cout << std::endl;
    std::cout << "========= Subarray Area Breakdown =========" << std::endl;
    std::cout << " |--- Total Cell Area         = " << ToSquareMeter(mat->subarray->lenRow * mat->subarray->lenCol) << std::endl;

    std::cout << " |--- Input Buffer Area       = " << ToSquareMeter(mat->subarray->inputBuf->area * mat->subarray->numRow) << std::endl;
    std::cout << " |--- Input Encoder Area      = " << ToSquareMeter(mat->subarray->inputEnc->area) << std::endl;
    std::cout << " |--- Row Decoder Area        = " << ToSquareMeter(mat->subarray->RowDecMergeNand->area) << std::endl;
    double RowDriverArea = 0;
    for(int i=0;i<config->technology.cell->camNumRow;i++){
        RowDriverArea += mat->subarray->RowDriver[i]->area;
    }
    std::cout << " |--- Row Driver Area         = " << ToSquareMeter(RowDriverArea) << std::endl;
    std::cout << " |--- Precharger Area         = " << ToSquareMeter(mat->subarray->precharger->area) << std::endl;
    if(config->peripherals.withWriteDriver == true){
        std::cout << " |--- Write Driver Area       = " << ToSquareMeter(mat->subarray->WriteDriverArea) << std::endl;
    }

    double ColMuxArea = 0;
    for(int i=0;i<config->technology.cell->camNumCol;i++){
        ColMuxArea += mat->subarray->ColMux[i]->area;
    }
    std::cout << " |--- Column Mux Area         = " << ToSquareMeter(ColMuxArea)<< std::endl;
    std::cout << " |--- Sense Amplifier Area    = " << ToSquareMeter(mat->subarray->senseAmp->area) << std::endl;
    std::cout << " |--- Mux of SA               = " << ToSquareMeter(mat->subarray->senseAmpMuxLev1->area + mat->subarray->senseAmpMuxLev2->area + mat->subarray->senseAmpMuxLev1Nand->area + mat->subarray->senseAmpMuxLev2Nand->area) << std::endl;
    std::cout << " |--- Output Accumulator Area = " << ToSquareMeter(mat->subarray->outputAcc->area * mat->subarray->numColumn / mat->subarray->muxSenseAmp) << std::endl;
    if(config->peripherals.withPriorityEnc == true){
        std::cout << " |--- Priority Encoder Area   = " << ToSquareMeter(mat->subarray->priorityEnc->area) << std::endl;
    }
    std::cout << " |--- Output Buffer Area      = " << ToSquareMeter(mat->subarray->outputBuf->area * mat->subarray->numColumn / mat->subarray->muxSenseAmp) << std::endl;
    std::cout << std::endl;

    ///////////////       latency              //////////////

    std::cout << "========= Search Latency Breakdown =========" << std::endl;
    // std::cout << std::endl;
    // std::cout << " |- Mat Search Latency = " << ToSecond(mat->subarray->readLatency)  << std::endl;
    // std::cout << " 	|-- Subarray Search Latency   = " << ToSecond(mat->subarray->readLatency) << std::endl;
    // std::cout << " 	|-- Predecoder Search Latency = " << ToSecond(mat->rowPredecoderBlock1->readLatency + mat->rowPredecoderBlock2->readLatency + mat->bitlineMuxPredecoderBlock1->readLatency + mat->bitlineMuxPredecoderBlock2->readLatency +
    // 		mat->senseAmpMuxLev1PredecoderBlock1->readLatency + mat->senseAmpMuxLev1PredecoderBlock2->readLatency +
    // 		mat->senseAmpMuxLev2PredecoderBlock1->readLatency + mat->senseAmpMuxLev2PredecoderBlock2->readLatency) << std::endl;
    std::cout << " |--- Input Encoder Latency      = " << ToSecond(mat->subarray->inputEnc->readLatency) << std::endl;
    std::cout << " |--- Row Decoder Latency        = " << ToSecond(mat->subarray->RowDecMergeNand->readLatency +mat->subarray->senseAmpMuxLev1Nand->readLatency +mat->subarray->senseAmpMuxLev2Nand->readLatency) << std::endl;
    double RowDriverLatency = 0;
    for(int i=0;i<config->technology.cell->camNumRow;i++){
        RowDriverLatency = std::max(mat->subarray->RowDriver[i]->readLatency, readLatency);
    }
    std::cout << " |--- Row Driver Latency         = " << ToSecond(RowDriverLatency) << std::endl;
    std::cout << " |--- Precharger Latency         = " << ToSecond(mat->subarray->precharger->readLatency) << std::endl;
    std::cout << " |--- Matchline Latency          = " << ToSecond(mat->subarray->matchlineDelay) << std::endl;
    double ColDriverLatency = 0;
    for(int i=0;i<config->technology.cell->camNumCol;i++){
        ColDriverLatency = std::max(mat->subarray->ColMux[i]->readLatency, readLatency);
    }
    std::cout << " |--- Column Mux Latency         = " << ToSecond(ColDriverLatency) << std::endl;
    std::cout << " |--- Sense Amplifier Latency    = " << ToSecond(mat->subarray->senseAmpLatency) << std::endl;
    std::cout << " |--- MUX of SA Latency          = " << ToSecond(mat->subarray->senseAmpMuxLev1->readLatency + mat->subarray->senseAmpMuxLev2->readLatency) << std::endl;
    if(config->peripherals.withOutputAcc == true){
        std::cout << " |--- Output Accumulator Latency = " << ToSecond(mat->subarray->outputAcc->readLatency) << std::endl;
    }
    if(config->peripherals.withPriorityEnc == true){
        std::cout << " |--- Priority Encoder Latency   = " << ToSecond(mat->subarray->priorityEnc->readLatency) << std::endl;
    }
    std::cout << std::endl;

    ///////////////       energy              //////////////
    std::cout << "========= Search Dynamic Energy Breakdown =========" << std::endl;
    // std::cout << " |-- Mat Dynamic Search Energy  = " << ToJoule(mat->readDynamicEnergy) << std::endl;
    // std::cout << " 	|-- Subarray Search Energy   = " << ToJoule(mat->subarray->readDynamicEnergy) << std::endl;
    //std::cout << "Search Dynamic Energy" << ToJoule(mat->subarray->searchAverage) << std::endl;
    // std::cout << " 	|-- Predecoder Energy = " << ToJoule(mat->rowPredecoderBlock1->readDynamicEnergy + mat->rowPredecoderBlock2->readDynamicEnergy + mat->bitlineMuxPredecoderBlock1->readDynamicEnergy + mat->bitlineMuxPredecoderBlock2->readDynamicEnergy +
    // 		mat->senseAmpMuxLev1PredecoderBlock1->readDynamicEnergy + mat->senseAmpMuxLev1PredecoderBlock2->readDynamicEnergy +
    // 		mat->senseAmpMuxLev2PredecoderBlock1->readDynamicEnergy + mat->senseAmpMuxLev2PredecoderBlock2->readDynamicEnergy) << std::endl;
    std::cout << " |--- Input Encoder Dynamic Energy      = " << ToJoule(mat->subarray->inputEnc->readDynamicEnergy) << std::endl;
    std::cout << " |--- Row Decoder Dynamic Energy        = " << ToJoule(mat->subarray->RowDecMergeNand->readDynamicEnergy) << std::endl;
    double RowSearchDynamicEnergy = 0;
    for(int i=0;i<config->technology.cell->camNumRow;i++){
        RowSearchDynamicEnergy += mat->subarray->RowDriver[i]->readDynamicEnergy;
    }
    std::cout << " |--- RowDriver Dynamic Energy          = " << ToJoule(RowSearchDynamicEnergy) << std::endl; //For FeFET *16
    std::cout << " |--- Precharger Dynamic Energy         = " << ToJoule(mat->subarray->precharger->readDynamicEnergy) << std::endl;
    // TODO: not ly the breakdown
    std::cout << " |--- Cell Read Energy                  = " << ToJoule(mat->subarray->cellReadEnergy) << std::endl;
    double ColMuxreadDynamicEnergy = 0;
    for(int i=0;i<config->technology.cell->camNumCol;i++){
        ColMuxreadDynamicEnergy += mat->subarray->ColMux[i]->readDynamicEnergy;
    }
    std::cout << " |--- Column Mux Dynamic Energy         = " << ToJoule(ColMuxreadDynamicEnergy) << std::endl;
    std::cout << " |--- Sense Amplifier Dynamic Energy    = " << ToJoule(mat->subarray->senseAmp->readDynamicEnergy) << std::endl;
    std::cout << " |--- MUX of SA Dynamic Energy          = " << ToJoule(mat->subarray->senseAmpMuxLev1->readDynamicEnergy + mat->subarray->senseAmpMuxLev2->readDynamicEnergy) << std::endl;
    if(config->peripherals.withOutputAcc == true){
        std::cout << " |--- Output Accumulator Dynamic Energy = " << ToJoule(mat->subarray->outputAcc->readDynamicEnergy) << std::endl;
    }
    if(config->peripherals.withPriorityEnc == true){
        std::cout << " |--- Priority Encoder Dynamic Energy   = " << ToJoule(mat->subarray->priorityEnc->readDynamicEnergy) << std::endl;
    }
    std::cout << std::endl;


    ///////////////////xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx////////////////////////////

    ///////////////       energy              ///////////>///
    std::cout << "========= Write Dynamic Energy Breakdown =========" << std::endl;
    // std::cout << " |- Mat Write Search Energy  = " << ToJoule(mat->writeDynamicEnergy) << std::endl;
    // std::cout << "  |-- Subarray Dynamic Energy            = " << ToJoule(mat->subarray->writeDynamicEnergy) << std::endl;
    // std::cout << "  |-- Predecoder Dynamic Energy          = " << ToJoule(mat->rowPredecoderBlock1->writeDynamicEnergy + mat->rowPredecoderBlock2->writeDynamicEnergy + mat->bitlineMuxPredecoderBlock1->writeDynamicEnergy+ mat->bitlineMuxPredecoderBlock2->writeDynamicEnergy+
    // 		mat->senseAmpMuxLev1PredecoderBlock1->writeDynamicEnergy+ mat->senseAmpMuxLev1PredecoderBlock2->writeDynamicEnergy +
    // 		mat->senseAmpMuxLev2PredecoderBlock1->writeDynamicEnergy+ mat->senseAmpMuxLev2PredecoderBlock2->writeDynamicEnergy) << std::endl;
    std::cout << " |--- Cell Reset Energy                  = " << ToJoule(mat->subarray->cellResetEnergy) << std::endl;
    std::cout << " |--- Cell Set Energy                    = " << ToJoule(mat->subarray->cellSetEnergy) << std::endl;
    std::cout << " |--- Input Encoder Dynamic Energy       = " << ToJoule(mat->subarray->inputEnc->writeDynamicEnergy) << std::endl;
    std::cout << " |--- Row Decoder Dynamic Energy         = " << ToJoule(mat->subarray->RowDecMergeNand->writeDynamicEnergy) << std::endl;
    double RowWriteDynamicEnergy = 0;
    for(int i=0;i<config->technology.cell->camNumRow;i++){
        RowWriteDynamicEnergy += mat->subarray->RowDriver[i]->writeDynamicEnergy*16;
    }
    std::cout << " |--- RowDriver Dynamic Energy           = " << ToJoule(RowWriteDynamicEnergy) << std::endl;
    std::cout << " |--- Precharger Dynamic Energy          = " << ToJoule(mat->subarray->precharger->writeDynamicEnergy) << std::endl;
    // TODO: not really the breakdown
    double ColWriteDynamicEnergy = 0;
    for(int i=0;i<config->technology.cell->camNumCol;i++){
        ColWriteDynamicEnergy += mat->subarray->ColMux[i]->writeDynamicEnergy;
    }
    std::cout << " |--- Column Mux Dynamic Energy          = " << ToJoule(ColWriteDynamicEnergy) << std::endl;
    std::cout << " |--- Sense Amplifier Dynamic Energy     = " << ToJoule(mat->subarray->senseAmp->writeDynamicEnergy) << std::endl;
    std::cout << " |--- MUX of SA Dynamic Energy           = " << ToJoule(mat->subarray->senseAmpMuxLev1->writeDynamicEnergy + mat->subarray->senseAmpMuxLev2->writeDynamicEnergy) << std::endl;
    if(config->peripherals.withOutputAcc == true){
        std::cout << " |--- Output Accumulator Dynamic Energy  = " << ToJoule(mat->subarray->outputAcc->writeDynamicEnergy) << std::endl;
    }
    if(config->peripherals.withPriorityEnc == true){
        std::cout << " |--- Priority Encoder Dynamic Energy    = " << ToJoule(mat->subarray->priorityEnc->writeDynamicEnergy) << std::endl;		
    }
    std::cout << std::endl;


    ///////////////////xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx////////////////////////////

    ///////////////       leakage              //////////////
    std::cout << "============ Leakage Breakdown ============" << std::endl;
    // std::cout << " |- Mat Leakage =  " << ToWatt(mat->leakage) << std::endl;
    // std::cout << " 	|-- Subarray Leakage   = " << ToWatt(mat->subarray->leakage) << std::endl;
    // std::cout << " 	|-- Predocoder Leakage = " << ToWatt(mat->rowPredecoderBlock1->leakage + mat->rowPredecoderBlock2->leakage +
    // 		mat->bitlineMuxPredecoderBlock1->leakage + mat->bitlineMuxPredecoderBlock2->leakage +
    // 		mat->senseAmpMuxLev1PredecoderBlock1->leakage + mat->senseAmpMuxLev1PredecoderBlock2->leakage +
    // 		mat->senseAmpMuxLev2PredecoderBlock1->leakage + mat->senseAmpMuxLev2PredecoderBlock2->leakage ) << std::endl;

    std::cout << " |--- Input Encoder Leakage      = " << ToWatt(mat->subarray->inputEnc->leakage) << std::endl;
    std::cout << " |--- Row Decoder Leakage        = " << ToWatt(mat->subarray->RowDecMergeNand->leakage) << std::endl;
    double leakage = 0;
    for(int i=0;i<config->technology.cell->camNumRow;i++){
        leakage += mat->subarray->RowDriver[i]->leakage;
    }
    std::cout << " |--- Row Driver Leakage         = " << ToWatt(leakage) << std::endl;
    std::cout << " |--- Precharger Leakage         = " << ToWatt(mat->subarray->precharger->leakage) << std::endl;
    leakage = 0;
    for(int i=0;i<config->technology.cell->camNumCol;i++){
        leakage += mat->subarray->ColMux[i]->leakage;
    }
    std::cout << " |--- Column Mux Leakage         = " << ToWatt(leakage) << std::endl;
    std::cout << " |--- Sense Amplifier Leakage    = " << ToWatt(mat->subarray->senseAmp->leakage) << std::endl;
    std::cout << " |--- MUX of SA Leakage          = " << ToWatt(mat->subarray->senseAmpMuxLev1->leakage + mat->subarray->senseAmpMuxLev2->leakage) << std::endl;
    if(config->peripherals.withOutputAcc == true){
        std::cout << " |--- Output Accumulator Leakage = " << ToWatt(mat->subarray->outputAcc->leakage) << std::endl;
    }
    if(config->peripherals.withPriorityEnc == true){
        std::cout << " |--- Priority Encoder Leakage   = " << ToWatt(mat->subarray->priorityEnc->leakage) << std::endl;		
    }		



}

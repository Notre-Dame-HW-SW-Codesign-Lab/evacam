#include "formula.h"

#include <iostream>
#include <stdexcept>

void CalcAreaForCostomSA(int designNum, double widthTransistorRegion, std::shared_ptr<Technology> tech, 
        double *_width, double *_height, bool UseUpdatedLib) {
    double width = 0;
    double height = 0;
    double tempHeight = 0;
    double tempWidth = 0;
    double tempParallelWidth = 0;
    double tempParallelHeight = 0;
    //**************************************************************************************************************
    //                             [Shuangchen] DASR-VSA (3k-7.5k)
    //**************************************************************************************************************
    if(designNum == 1){
        //////////////////////////////////////////////////////////
        double W_ref_P = 0.5;
        double W_ref_n = 25;
        double W_refpre_p = 30;
        double W_pre_p = 20;
        double W_amp_p = 2;
        double W_amp_n = 20;
        double W_amppre_p = 40;
        double W_ampen_n = 30;
        //////////////////////////////////////////////////////////
        // from pre-running we see the width constraint is 47F

        // Mrefp + Mrefn (0.5+20<47), horizental
        tempParallelWidth = 0;
        CalculateGateArea(INV, 1, 0, W_ref_P * tech->featureSize(), widthTransistorRegion, tech, 
                &tempWidth, &tempHeight, UseUpdatedLib);
        tempParallelWidth = tempWidth;
        tempParallelHeight = tempHeight;

        tempParallelWidth += MIN_GAP_BET_P_AND_N_DIFFS * tech->featureSize();
        CalculateGateArea(INV, 1, W_ref_n * tech->featureSize(), 0, widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        tempParallelWidth += tempWidth;
        tempParallelHeight = MAX(tempParallelHeight, tempHeight);

        width = MAX(width, tempParallelWidth);
        height += tempParallelHeight;
        if(width > widthTransistorRegion) {
            throw std::runtime_error("[Custom SA] Area calculation exceeds the width constraint and needs folding.");
        }

        // Mrefpre (30<47)
        height += MIN_GAP_BET_P_AND_N_DIFFS * tech->featureSize();
        CalculateGateArea(INV, 1, 0, W_refpre_p * tech->featureSize(), widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        width = MAX(width, tempWidth);
        height += tempHeight;

        // Mpre + Mampp (20+2<47) x2
        height += MIN_GAP_BET_SAME_TYPE_DIFFS * tech->featureSize();
        CalculateGateArea(INV, 1, 0, W_pre_p * tech->featureSize(), widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        tempParallelWidth = tempWidth;
        tempParallelHeight = tempHeight;

        tempParallelWidth += MIN_GAP_BET_SAME_TYPE_DIFFS * tech->featureSize();
        CalculateGateArea(INV, 1, 0, W_amp_p * tech->featureSize(), widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        tempParallelWidth += tempWidth;
        tempParallelHeight = MAX(tempParallelHeight, tempHeight);

        width = MAX(width, tempParallelWidth);
        height += tempParallelHeight;
        if(width > widthTransistorRegion) {
            throw std::runtime_error("[Custom SA] Area calculation exceeds the width constraint and needs folding.");
        }

        height += MIN_GAP_BET_SAME_TYPE_DIFFS * tech->featureSize();
        height += tempParallelHeight;

        // Mampn x2 (20+20<47)
        height += MIN_GAP_BET_P_AND_N_DIFFS * tech->featureSize();
        CalculateGateArea(INV, 1, W_amp_n * tech->featureSize(), 0, widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        tempParallelWidth = tempWidth;
        tempParallelHeight = tempHeight;

        tempParallelWidth += MIN_GAP_BET_SAME_TYPE_DIFFS * tech->featureSize();
        CalculateGateArea(INV, 1, W_amp_n * tech->featureSize(), 0, widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        tempParallelWidth += tempWidth;
        tempParallelHeight = MAX(tempParallelHeight, tempHeight);

        width = MAX(width, tempParallelWidth);
        height += tempParallelHeight;
        if(width > widthTransistorRegion) {
            throw std::runtime_error("[Custom SA] Area calculation exceeds the width constraint and needs folding.");
        }

        // W_amppre_p (40<47)
        height += MIN_GAP_BET_P_AND_N_DIFFS * tech->featureSize();
        CalculateGateArea(INV, 1, 0, W_amppre_p * tech->featureSize(), widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        width = MAX(width, tempWidth);
        height += tempHeight;

        // W_ampen_n (30<47)
        height += MIN_GAP_BET_SAME_TYPE_DIFFS * tech->featureSize();
        CalculateGateArea(INV, 1, W_ampen_n * tech->featureSize(), 0, widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        width = MAX(width, tempWidth);
        height += tempHeight;

        // second amplifier with inv x2 & MUX all in horizental
        CalculateGateArea(INV, 1, 1 * tech->featureSize(), 2 * tech->featureSize(), widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        width = MAX(width, tempWidth);
        height += tempHeight;
    }

    //**************************************************************************************************************

    //**************************************************************************************************************
    //                             [Shuangchen] CSB-SA (3k-7.5k)
    //**************************************************************************************************************
    if(designNum == 2){
        //////////////////////////////////////////////////////////
        double W_ref_P = 20;
        double Cs = 1/1e15;
        //////////////////////////////////////////////////////////
        // from pre-running we see the width constraint is 47F

        // Mrefp + Mrefn (20+20<47), horizental
        CalculateGateArea(INV, 1, 0, W_ref_P * tech->featureSize(), widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        tempParallelWidth += MIN_GAP_BET_P_AND_N_DIFFS * tech->featureSize();
        width = MAX(width, tempWidth);
        height += tempHeight;
        if(width > widthTransistorRegion) {
            throw std::runtime_error("[Custom SA] Area calculation exceeds the width constraint and needs folding.");
        }

        tempHeight = Cs / CalculateGateCap(widthTransistorRegion, tech) * tech->featureSize();
        height += 2*tempHeight;

        // latch and enable in one line
        height += MIN_GAP_BET_P_AND_N_DIFFS * tech->featureSize();
        CalculateGateArea(INV, 1, 1 * tech->featureSize(), 0, widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        tempWidth *= 3;
        tempWidth += MIN_GAP_BET_SAME_TYPE_DIFFS * tech->featureSize();
        width = MAX(width, tempWidth);
        height += tempHeight;
        if(width > widthTransistorRegion) {
            throw std::runtime_error("[Custom SA] Area calculation exceeds the width constraint and needs folding.");
        }

        // switches, 4 in one line
        height += MIN_GAP_BET_P_AND_N_DIFFS * tech->featureSize();
        CalculateGateArea(INV, 1, 1 * tech->featureSize(), 2 * tech->featureSize(), widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        tempWidth *= 4;
        tempWidth += MIN_GAP_BET_P_AND_N_DIFFS * tech->featureSize();
        width = MAX(width, tempWidth);
        height += tempHeight;
        if(width > widthTransistorRegion) {
            throw std::runtime_error("[Custom SA] Area calculation exceeds the width constraint and needs folding.");
        }

        // 2-stage amplify, in one line
        height += MIN_GAP_BET_P_AND_N_DIFFS * tech->featureSize();
        CalculateGateArea(INV, 1, 1 * tech->featureSize(), 2 * tech->featureSize(), widthTransistorRegion, tech, &tempWidth, &tempHeight, UseUpdatedLib);
        tempWidth *= 2;
        tempWidth += MIN_GAP_BET_P_AND_N_DIFFS * tech->featureSize();
        width = MAX(width, tempWidth);
        height += tempHeight;
        if(width > widthTransistorRegion) {
            throw std::runtime_error("[Custom SA] Area calculation exceeds the width constraint and needs folding.");
        }
    }

    //**************************************************************************************************************

    *_width = width;
    *_height = height;
}

void CalcCapForCostomSA(int designNum, double widthTransistorRegion, std::shared_ptr<Technology> tech, double *CapLoad) {
    if(designNum == 1){
        //////////////////////////////////////////////////////////
        // TODO: find out why the commented out variables below are unused
        //double W_ref_P = 0.5;
        double W_ref_n = 25;
        //double W_refpre_p = 30;
        double W_pre_p = 20;
        double W_amp_p = 2;
        double W_amp_n = 20;
        //double W_amppre_p = 40;
        //double W_ampen_n = 30;
        //////////////////////////////////////////////////////////
        *CapLoad = 	CalculateGateCap((W_amp_p + W_amp_n) * tech->featureSize(), tech)
            + CalculateGateCap((W_ref_n) * tech->featureSize(), tech)
            + CalculateDrainCap(W_amp_p * tech->featureSize(), PMOS, widthTransistorRegion, tech)
            + CalculateDrainCap(W_pre_p * tech->featureSize(), PMOS, widthTransistorRegion, tech)
            + CalculateDrainCap(W_pre_p * tech->featureSize(), PMOS, widthTransistorRegion, tech)
            + CalculateDrainCap(1 * tech->featureSize(), NMOS, widthTransistorRegion, tech)
            + CalculateDrainCap(2 * tech->featureSize(), PMOS, widthTransistorRegion, tech)
            + CalculateGateCap((1 + 2) * tech->featureSize(), tech);
    }
    if(designNum == 2){
        //////////////////////////////////////////////////////////
        double W_ref_P = 20;
        double Cs = 1/1e15;
        //////////////////////////////////////////////////////////
        *CapLoad = 	Cs
            + CalculateGateCap(1 * tech->featureSize(), tech)
            + CalculateDrainCap(1 * tech->featureSize(), NMOS, widthTransistorRegion, tech)
            + CalculateDrainCap(2 * tech->featureSize(), PMOS, widthTransistorRegion, tech)
            + CalculateDrainCap(1 * tech->featureSize(), NMOS, widthTransistorRegion, tech)
            + CalculateDrainCap(W_ref_P * tech->featureSize(), PMOS, widthTransistorRegion, tech);
    }
}

/*
 * CAM_CustomSAArea.h
 *  custom SA design interface
 */

#ifndef CAM_CUSTOMSAAREA_H_
#define CAM_CUSTOMSAAREA_H_

#include "formula.h"
#include "constant.h"

extern void CalcAreaForCostomSA(int designNum, double widthTransistorRegion, 
        const Technology &tech, double *width, double *height, bool UseUpdatedLib);

extern void CalcCapForCostomSA(int designNum, double widthTransistorRegion, 
        const Technology &tech, double *CapLoad);

#endif /* CAM_CUSTOMSAAREA_H_ */

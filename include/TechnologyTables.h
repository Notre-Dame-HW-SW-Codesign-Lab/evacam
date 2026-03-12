#ifndef TECHNOLOGYTABLES_H_
#define TECHNOLOGYTABLES_H_

#include "TechnologySpec.h"

const TechnologySpec *FindTechnologySpec(
        int featureSizeInNano,
        DeviceRoadmap roadmap,
        bool useUpdatedLib);

#endif /* TECHNOLOGYTABLES_H_ */

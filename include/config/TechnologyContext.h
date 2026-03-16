#ifndef EVACAM_CONFIG_TECHNOLOGYCONTEXT_H_
#define EVACAM_CONFIG_TECHNOLOGYCONTEXT_H_

#include <memory>

class Technology;
class MemCell;

struct TechnologyContext {
    std::shared_ptr<Technology> tech;
    std::shared_ptr<Technology> fefetTech;
    std::shared_ptr<MemCell> cell;
};

#endif  // EVACAM_CONFIG_TECHNOLOGYCONTEXT_H_

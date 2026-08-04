#ifndef INPUT_PHYSICALDOMAINVALIDATORS_H_
#define INPUT_PHYSICALDOMAINVALIDATORS_H_

class MemCell;
class Technology;

namespace PhysicalDomainValidators {

void ValidateMemCell(const MemCell& cell);
void ValidateTechnology(const Technology& technology);

}  // namespace PhysicalDomainValidators

#endif  // INPUT_PHYSICALDOMAINVALIDATORS_H_

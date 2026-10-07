#ifndef INPUT_PHYSICALDOMAINVALIDATORS_H_
#define INPUT_PHYSICALDOMAINVALIDATORS_H_

class MemCell;
class Technology;
struct Nand3dMemoryDevice;

namespace PhysicalDomainValidators {

void ValidateNand3dGeometry(const Nand3dMemoryDevice& spec);
void ValidateMemCell(const MemCell& cell);
void ValidateTechnology(const Technology& technology);

}  // namespace PhysicalDomainValidators

#endif  // INPUT_PHYSICALDOMAINVALIDATORS_H_

#include "MemCell.h"
#include "input/AccessDeviceYamlLoader.h"
#include "input/MemoryDeviceYamlLoader.h"

#include <cassert>
#include <cmath>

static bool Near(double a, double b) { return std::fabs(a - b) < 1e-18; }

int main() {
    MemCell fefetSplit;
    fefetSplit.ReadCellFromFile("config/2FeFET_TCAM/2FeFET_TCAM.cell.yaml", CAM_chip, 1.0);

    assert(fefetSplit.memCellType == FEFETRAM);
    assert(fefetSplit.camType == TCAM);
    assert(fefetSplit.processNode == 45);
    assert(Near(fefetSplit.area, 300));
    assert(Near(fefetSplit.aspectRatio, 1.76));
    assert(fefetSplit.accessType == none_access);
    assert(fefetSplit.camNumRow == 2);
    assert(fefetSplit.camNumCol == 1);
    assert(fefetSplit.camPort[0][0].Type == Searchline);
    assert(fefetSplit.camPort[0][0].ConnectedRegion == gate);
    assert(fefetSplit.camPort[1][0].Type == Matchline);
    assert(fefetSplit.camPort[1][0].ConnectedRegion == drain);

    MemCell accessSplit;
    accessSplit.ReadCellFromFile("config/10T-BCAM_28nm/10T-BCAM_28nm.cell.yaml", CAM_chip, 1.0);

    assert(accessSplit.memCellType == SRAM);
    assert(accessSplit.camType == TCAM);
    assert(accessSplit.processNode == 28);
    assert(Near(accessSplit.area, 950));
    assert(Near(accessSplit.aspectRatio, 4.897));
    assert(accessSplit.accessType == CMOS_access);
    assert(accessSplit.camNumRow == 4);
    assert(accessSplit.camNumCol == 2);
    assert(accessSplit.camPort[0][0].Type == Searchline);
    assert(accessSplit.camPort[0][0].ConnectedRegion == gate);
    assert(accessSplit.camPort[0][2].Type == Bitline);
    assert(accessSplit.camPort[0][2].ConnectedRegion == drain);
    assert(accessSplit.camPort[1][0].Type == Wordline);
    assert(accessSplit.camPort[1][0].numCmos == 4);
    assert(accessSplit.camPort[1][1].Type == Matchline);
    assert(accessSplit.camPort[1][1].numCmos == 2);
    return 0;
}

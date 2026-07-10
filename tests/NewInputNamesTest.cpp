#include "config/EvaCamConfig.h"
#include "config/EvaCamYamlLoader.h"

#include <cassert>

int main() {
    EvaCamConfig config;
    config.ReadConfigFromFile("config/2FeFET_TCAM/2FeFET_TCAM.config.yaml");
    assert(config.input.processNode == 45);
    assert(config.technology.cell != nullptr);
    assert(config.technology.tech != nullptr);
    assert(config.technology.cell->camType == TCAM);
    return 0;
}

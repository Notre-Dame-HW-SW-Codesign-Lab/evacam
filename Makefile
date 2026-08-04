CC=g++
CPP_FLAGS=-std=c++17 -O0 -Wall -Wextra -Wpedantic -g3 -fno-omit-frame-pointer -fopenmp -MMD -MP \
	-I$(ROOT_DIR)/include \
	-I$(ROOT_DIR)/include/app \
	-I$(ROOT_DIR)/include/cam \
	-I$(ROOT_DIR)/include/circuit \
	-I$(ROOT_DIR)/include/config \
	-I$(ROOT_DIR)/include/factories \
	-I$(ROOT_DIR)/include/input \
	-I$(ROOT_DIR)/include/model \
	-I$(ROOT_DIR)/include/output \
	-I$(ROOT_DIR)/include/technology \
	-I/usr/include/yaml-cpp
LD_LIBS= -lyaml-cpp 

VALGRIND_FLAGS=--leak-check=full --show-leak-kinds=all --track-origins=yes --suppressions=.valgrind.supp

ROOT_DIR=$(shell pwd)
SRC_DIR=$(ROOT_DIR)/src
OBJ_DIR=$(ROOT_DIR)/obj
RES_DIR=$(ROOT_DIR)/results
TEST_DEP_DIR=$(OBJ_DIR)/tests
TEST_BIN_DIR=$(ROOT_DIR)/test-bin

BIN=EvaCAM
TEST_YAML_BIN=$(TEST_BIN_DIR)/YamlHelpersTest
TEST_TOP_LEVEL_BIN=$(TEST_BIN_DIR)/TopLevelConfigParserTest
TEST_CELL_LOADER_BIN=$(TEST_BIN_DIR)/CellYamlLoaderTest
TEST_CLI_OPTIONS_BIN=$(TEST_BIN_DIR)/CliOptionsTest
TEST_CUSTOM_SA_LOADER_BIN=$(TEST_BIN_DIR)/CustomSenseAmpYamlLoaderTest
TEST_TECHNOLOGY_LOADER_BIN=$(TEST_BIN_DIR)/TechnologyYamlLoaderTest
TEST_NEW_INPUT_NAMES_BIN=$(TEST_BIN_DIR)/NewInputNamesTest
TEST_GENERATED_V2_CONFIGS_BIN=$(TEST_BIN_DIR)/GeneratedV2ConfigsTest
TEST_INPUT_VALIDATION_BIN=$(TEST_BIN_DIR)/InputValidationTest
TEST_OUTPUT_PATH_BUILDER_BIN=$(TEST_BIN_DIR)/OutputPathBuilderTest
TEST_EXPLORATION_BIN=$(TEST_BIN_DIR)/ExplorationDomainTest
TEST_VARIATION_BIN=$(TEST_BIN_DIR)/VariationSamplerTest
TEST_MONTECARLO_BIN=$(TEST_BIN_DIR)/MonteCarloRegressionTest
TEST_CORNER_BIN=$(TEST_BIN_DIR)/CornerVariationRegressionTest
TEST_WIRE_BIN=$(TEST_BIN_DIR)/WireCopyTest
TEST_FORMULA_BIN=$(TEST_BIN_DIR)/FormulaTest
TEST_MATCH_BIN=$(TEST_BIN_DIR)/MatchTest
TEST_MAT_DECODER_BIN=$(TEST_BIN_DIR)/MatDecoderRegressionTest
TEST_HTREE_ROUTING_BIN=$(TEST_BIN_DIR)/HtreeRoutingRegressionTest
TEST_EXHAUSTIVE_SEARCH_BIN=$(TEST_BIN_DIR)/ExhaustiveSearchRegressionTest
TEST_SUPPORT_BIN=$(TEST_BIN_DIR)/TestSupportTest
TEST_DERIVED_VALUES_BIN=$(TEST_BIN_DIR)/DerivedValueHelpersTest
TEST_CONFIG_NORMALIZER_BIN=$(TEST_BIN_DIR)/ConfigNormalizerTest
TEST_CONFIG_SECTIONS_BIN=$(TEST_BIN_DIR)/ConfigSectionReadersTest
TEST_OUTPUT_FILE_LOCK_BIN=$(TEST_BIN_DIR)/OutputFileLockTest
TEST_EVACAM_CONFIG_BIN=$(TEST_BIN_DIR)/EvaCamConfigTest
TEST_CONFIG_VALIDATORS_BIN=$(TEST_BIN_DIR)/ConfigValidatorsTest
TEST_TECHNOLOGY_VARIATION_CONFIG_BIN=$(TEST_BIN_DIR)/TechnologyAndVariationConfigTest
TEST_YAML_PRIMITIVE_COVERAGE_BIN=$(TEST_BIN_DIR)/YamlPrimitiveCoverageTest
TEST_PHYSICAL_DOMAIN_VALIDATORS_BIN=$(TEST_BIN_DIR)/PhysicalDomainValidatorsTest
TEST_CELL_MEMORY_LOADER_BRANCHES_BIN=$(TEST_BIN_DIR)/CellAndMemoryLoaderBranchesTest
TEST_SENSE_AMP_LOADER_BRANCHES_BIN=$(TEST_BIN_DIR)/SenseAmpLoaderBranchesTest
TEST_TECHNOLOGY_YAML_BRANCHES_BIN=$(TEST_BIN_DIR)/TechnologyYamlLoaderBranchesTest
TEST_TECHNOLOGY_BIN=$(TEST_BIN_DIR)/TechnologyTest
TEST_MEM_CELL_BIN=$(TEST_BIN_DIR)/MemCellTest
TEST_FORMULA_COVERAGE_BIN=$(TEST_BIN_DIR)/FormulaCoverageTest
TEST_WIRE_FACTORY_BIN=$(TEST_BIN_DIR)/WireAndFactoryTest
TEST_FUNCTION_UNIT_BIN=$(TEST_BIN_DIR)/FunctionUnitTest
TEST_DECODER_COMPONENTS_BIN=$(TEST_BIN_DIR)/DecoderComponentsTest
TEST_DRIVER_MUX_COMPONENTS_BIN=$(TEST_BIN_DIR)/DriverMuxComponentsTest
TEST_CHARGING_SENSING_COMPONENTS_BIN=$(TEST_BIN_DIR)/ChargingAndSensingComponentsTest
PYBIND_MODULE_BASE=evacam_py
PYBIND_MODULE=$(PYBIND_MODULE_BASE)$(shell python3-config --extension-suffix)
PYBIND_OBJ_DIR=$(OBJ_DIR)/pybind
PYBIND_BINDING_OBJECT=$(PYBIND_OBJ_DIR)/bindings/EvaCAM_Pybind.o
PYBIND_CPP_FLAGS=$(CPP_FLAGS) $(shell python3-config --includes) -fPIC
UML_TEX=docs/repo_uml.tex
UML_PDF=repo_uml.pdf
UML_SLIDE_TEX=docs/repo_uml_slide.tex
UML_SLIDE_PDF=repo_uml_slide.pdf

# Automatically find all CPP files in the source tree
SOURCES=$(shell find $(SRC_DIR) -type f -name '*.cpp' | sort)
# Create corresponding OBJ file paths in the object directory
OBJECTS=$(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))
OBJECTS_NO_MAIN=$(filter-out $(OBJ_DIR)/app/main.o, $(OBJECTS))
PYBIND_OBJECTS=$(patsubst $(SRC_DIR)/%.cpp, $(PYBIND_OBJ_DIR)/%.o, $(filter-out $(SRC_DIR)/app/main.cpp, $(SOURCES)))
DEPS=$(OBJECTS:.o=.d)
PYBIND_DEPS=$(PYBIND_OBJECTS:.o=.d) $(PYBIND_BINDING_OBJECT:.o=.d)


CONFIG_STEM=$(basename $(notdir $(CONFIG_FILE)))
RESULT_BASE=$(patsubst %.config,%,$(patsubst %_config,%,$(patsubst %-config,%,$(CONFIG_STEM))))

RES_YAML=$(RES_DIR)/$(RESULT_BASE)_results.yaml
RUN_LOG=$(RES_DIR)/$(RESULT_BASE)_run.log
MATCH_CONFIG_FILE ?= config/2FeFET_TCAM/2FeFET_TCAM_match.config.yaml

all: $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(CPP_FLAGS) -o $@ $^ $(LD_LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CPP_FLAGS) -c $< -o $@

-include $(DEPS)
-include $(PYBIND_DEPS)

$(PYBIND_OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(PYBIND_CPP_FLAGS) -c $< -o $@

$(PYBIND_OBJ_DIR)/bindings/%.o: bindings/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(PYBIND_CPP_FLAGS) -c $< -o $@

$(PYBIND_MODULE): $(PYBIND_BINDING_OBJECT) $(PYBIND_OBJECTS)
	$(CC) $(PYBIND_CPP_FLAGS) -shared -o $@ $^ $(LD_LIBS)

.PHONY: sync-python-package-data unit-test-inventory check-unit-test-inventory test-unit test-regression test-test-support test-derived-values test-config-normalizer test-config-sections test-output-file-lock test-evacam-config test-config-validators test-technology-variation-config test-yaml-primitives test-physical-domain-validators test-cell-memory-loader-branches test-sense-amp-loader-branches test-technology-yaml-branches test-technology test-mem-cell test-formula-coverage test-wire-factory test-function-unit test-decoder-components test-driver-mux-components test-charging-sensing-components test-yaml test-top-level-parser test-cell-loader test-cli-options test-custom-sa-loader test-technology-loader test-new-input-names test-generated-v2-configs test-input-validation test-output-path-builder test-exploration test-variation test-montecarlo test-corner test-wire test-formula test-match test-mat-decoder test-htree-routing test-exhaustive-search test-python-package-data test-pybind-match test-pybind-run uml uml-slide open-uml
sync-python-package-data:
	python3 scripts/sync_python_package_config_lib.py

unit-test-inventory:
	python3 scripts/generate_unit_test_inventory.py

check-unit-test-inventory:
	python3 scripts/generate_unit_test_inventory.py --check

test-unit: test-test-support test-derived-values test-config-normalizer test-config-sections \
		test-output-file-lock \
		test-evacam-config test-config-validators test-technology-variation-config \
		test-yaml-primitives test-physical-domain-validators test-cell-memory-loader-branches \
		test-sense-amp-loader-branches test-technology-yaml-branches test-technology \
		test-mem-cell test-formula-coverage test-wire-factory test-function-unit \
		test-decoder-components test-driver-mux-components test-charging-sensing-components \
		test-yaml test-top-level-parser test-cell-loader test-cli-options \
		test-custom-sa-loader test-technology-loader test-new-input-names \
		test-input-validation test-output-path-builder test-exploration \
		test-variation test-wire test-formula test-python-package-data

test-regression: test-generated-v2-configs test-montecarlo test-corner test-match \
		test-mat-decoder test-htree-routing test-exhaustive-search test-pybind-match \
		test-pybind-run

test-test-support: $(OBJECTS_NO_MAIN) tests/TestSupportTest.cpp tests/TestSupport.h tests/TestModelBuilders.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_SUPPORT_BIN)).d -MT $(TEST_SUPPORT_BIN) -o $(TEST_SUPPORT_BIN) tests/TestSupportTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_SUPPORT_BIN)

test-derived-values: $(OBJECTS_NO_MAIN) tests/DerivedValueHelpersTest.cpp tests/TestSupport.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_DERIVED_VALUES_BIN)).d -MT $(TEST_DERIVED_VALUES_BIN) -o $(TEST_DERIVED_VALUES_BIN) tests/DerivedValueHelpersTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_DERIVED_VALUES_BIN)

test-config-normalizer: $(OBJECTS_NO_MAIN) tests/ConfigNormalizerTest.cpp tests/TestSupport.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_CONFIG_NORMALIZER_BIN)).d -MT $(TEST_CONFIG_NORMALIZER_BIN) -o $(TEST_CONFIG_NORMALIZER_BIN) tests/ConfigNormalizerTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_CONFIG_NORMALIZER_BIN)

test-config-sections: $(OBJECTS_NO_MAIN) tests/ConfigSectionReadersTest.cpp tests/TestSupport.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_CONFIG_SECTIONS_BIN)).d -MT $(TEST_CONFIG_SECTIONS_BIN) -o $(TEST_CONFIG_SECTIONS_BIN) tests/ConfigSectionReadersTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_CONFIG_SECTIONS_BIN)

test-output-file-lock: $(OBJECTS_NO_MAIN) tests/OutputFileLockTest.cpp tests/TestSupport.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_OUTPUT_FILE_LOCK_BIN)).d -MT $(TEST_OUTPUT_FILE_LOCK_BIN) -o $(TEST_OUTPUT_FILE_LOCK_BIN) tests/OutputFileLockTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_OUTPUT_FILE_LOCK_BIN)

test-evacam-config: $(OBJECTS_NO_MAIN) tests/EvaCamConfigTest.cpp tests/TestSupport.h tests/TestModelBuilders.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_EVACAM_CONFIG_BIN)).d -MT $(TEST_EVACAM_CONFIG_BIN) -o $(TEST_EVACAM_CONFIG_BIN) tests/EvaCamConfigTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_EVACAM_CONFIG_BIN)

test-config-validators: $(OBJECTS_NO_MAIN) tests/ConfigValidatorsTest.cpp tests/TestSupport.h tests/TestModelBuilders.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_CONFIG_VALIDATORS_BIN)).d -MT $(TEST_CONFIG_VALIDATORS_BIN) -o $(TEST_CONFIG_VALIDATORS_BIN) tests/ConfigValidatorsTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_CONFIG_VALIDATORS_BIN)

test-technology-variation-config: $(OBJECTS_NO_MAIN) tests/TechnologyAndVariationConfigTest.cpp tests/TestSupport.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_TECHNOLOGY_VARIATION_CONFIG_BIN)).d -MT $(TEST_TECHNOLOGY_VARIATION_CONFIG_BIN) -o $(TEST_TECHNOLOGY_VARIATION_CONFIG_BIN) tests/TechnologyAndVariationConfigTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_TECHNOLOGY_VARIATION_CONFIG_BIN)

test-yaml-primitives: $(OBJECTS_NO_MAIN) tests/YamlPrimitiveCoverageTest.cpp tests/TestSupport.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_YAML_PRIMITIVE_COVERAGE_BIN)).d -MT $(TEST_YAML_PRIMITIVE_COVERAGE_BIN) -o $(TEST_YAML_PRIMITIVE_COVERAGE_BIN) tests/YamlPrimitiveCoverageTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_YAML_PRIMITIVE_COVERAGE_BIN)

test-physical-domain-validators: $(OBJECTS_NO_MAIN) tests/PhysicalDomainValidatorsTest.cpp tests/TestSupport.h tests/TestModelBuilders.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_PHYSICAL_DOMAIN_VALIDATORS_BIN)).d -MT $(TEST_PHYSICAL_DOMAIN_VALIDATORS_BIN) -o $(TEST_PHYSICAL_DOMAIN_VALIDATORS_BIN) tests/PhysicalDomainValidatorsTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_PHYSICAL_DOMAIN_VALIDATORS_BIN)

test-cell-memory-loader-branches: $(OBJECTS_NO_MAIN) tests/CellAndMemoryLoaderBranchesTest.cpp tests/TestSupport.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_CELL_MEMORY_LOADER_BRANCHES_BIN)).d -MT $(TEST_CELL_MEMORY_LOADER_BRANCHES_BIN) -o $(TEST_CELL_MEMORY_LOADER_BRANCHES_BIN) tests/CellAndMemoryLoaderBranchesTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_CELL_MEMORY_LOADER_BRANCHES_BIN)

test-sense-amp-loader-branches: $(OBJECTS_NO_MAIN) tests/SenseAmpLoaderBranchesTest.cpp tests/TestSupport.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_SENSE_AMP_LOADER_BRANCHES_BIN)).d -MT $(TEST_SENSE_AMP_LOADER_BRANCHES_BIN) -o $(TEST_SENSE_AMP_LOADER_BRANCHES_BIN) tests/SenseAmpLoaderBranchesTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_SENSE_AMP_LOADER_BRANCHES_BIN)

test-technology-yaml-branches: $(OBJECTS_NO_MAIN) tests/TechnologyYamlLoaderBranchesTest.cpp tests/TestSupport.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_TECHNOLOGY_YAML_BRANCHES_BIN)).d -MT $(TEST_TECHNOLOGY_YAML_BRANCHES_BIN) -o $(TEST_TECHNOLOGY_YAML_BRANCHES_BIN) tests/TechnologyYamlLoaderBranchesTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_TECHNOLOGY_YAML_BRANCHES_BIN)

test-technology: $(OBJECTS_NO_MAIN) tests/TechnologyTest.cpp tests/TestSupport.h tests/TestModelBuilders.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_TECHNOLOGY_BIN)).d -MT $(TEST_TECHNOLOGY_BIN) -o $(TEST_TECHNOLOGY_BIN) tests/TechnologyTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_TECHNOLOGY_BIN)

test-mem-cell: $(OBJECTS_NO_MAIN) tests/MemCellTest.cpp tests/TestSupport.h tests/TestModelBuilders.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_MEM_CELL_BIN)).d -MT $(TEST_MEM_CELL_BIN) -o $(TEST_MEM_CELL_BIN) tests/MemCellTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_MEM_CELL_BIN)

test-formula-coverage: $(OBJECTS_NO_MAIN) tests/FormulaCoverageTest.cpp tests/TestSupport.h tests/TestModelBuilders.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_FORMULA_COVERAGE_BIN)).d -MT $(TEST_FORMULA_COVERAGE_BIN) -o $(TEST_FORMULA_COVERAGE_BIN) tests/FormulaCoverageTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_FORMULA_COVERAGE_BIN)

test-wire-factory: $(OBJECTS_NO_MAIN) tests/WireAndFactoryTest.cpp tests/TestSupport.h tests/TestModelBuilders.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_WIRE_FACTORY_BIN)).d -MT $(TEST_WIRE_FACTORY_BIN) -o $(TEST_WIRE_FACTORY_BIN) tests/WireAndFactoryTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_WIRE_FACTORY_BIN)

test-function-unit: $(OBJECTS_NO_MAIN) tests/FunctionUnitTest.cpp tests/TestSupport.h tests/TestModelBuilders.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_FUNCTION_UNIT_BIN)).d -MT $(TEST_FUNCTION_UNIT_BIN) -o $(TEST_FUNCTION_UNIT_BIN) tests/FunctionUnitTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_FUNCTION_UNIT_BIN)

test-decoder-components: $(OBJECTS_NO_MAIN) tests/DecoderComponentsTest.cpp tests/TestSupport.h tests/TestModelBuilders.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_DECODER_COMPONENTS_BIN)).d -MT $(TEST_DECODER_COMPONENTS_BIN) -o $(TEST_DECODER_COMPONENTS_BIN) tests/DecoderComponentsTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_DECODER_COMPONENTS_BIN)

test-driver-mux-components: $(OBJECTS_NO_MAIN) tests/DriverMuxComponentsTest.cpp tests/TestSupport.h tests/TestModelBuilders.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_DRIVER_MUX_COMPONENTS_BIN)).d -MT $(TEST_DRIVER_MUX_COMPONENTS_BIN) -o $(TEST_DRIVER_MUX_COMPONENTS_BIN) tests/DriverMuxComponentsTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_DRIVER_MUX_COMPONENTS_BIN)

test-charging-sensing-components: $(OBJECTS_NO_MAIN) tests/ChargingAndSensingComponentsTest.cpp tests/TestSupport.h tests/TestModelBuilders.h
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_CHARGING_SENSING_COMPONENTS_BIN)).d -MT $(TEST_CHARGING_SENSING_COMPONENTS_BIN) -o $(TEST_CHARGING_SENSING_COMPONENTS_BIN) tests/ChargingAndSensingComponentsTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_CHARGING_SENSING_COMPONENTS_BIN)

test-yaml: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_YAML_BIN)).d -MT $(TEST_YAML_BIN) -o $(TEST_YAML_BIN) tests/YamlHelpersTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_YAML_BIN)

test-top-level-parser: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_TOP_LEVEL_BIN)).d -MT $(TEST_TOP_LEVEL_BIN) -o $(TEST_TOP_LEVEL_BIN) tests/TopLevelConfigParserTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_TOP_LEVEL_BIN)

test-cell-loader: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_CELL_LOADER_BIN)).d -MT $(TEST_CELL_LOADER_BIN) -o $(TEST_CELL_LOADER_BIN) tests/CellYamlLoaderTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_CELL_LOADER_BIN)

test-cli-options:
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_CLI_OPTIONS_BIN)).d -MT $(TEST_CLI_OPTIONS_BIN) -o $(TEST_CLI_OPTIONS_BIN) tests/CliOptionsTest.cpp src/input/CliOptions.cpp $(LD_LIBS)
	$(TEST_CLI_OPTIONS_BIN)

test-custom-sa-loader: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_CUSTOM_SA_LOADER_BIN)).d -MT $(TEST_CUSTOM_SA_LOADER_BIN) -o $(TEST_CUSTOM_SA_LOADER_BIN) tests/CustomSenseAmpYamlLoaderTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_CUSTOM_SA_LOADER_BIN)

test-technology-loader: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_TECHNOLOGY_LOADER_BIN)).d -MT $(TEST_TECHNOLOGY_LOADER_BIN) -o $(TEST_TECHNOLOGY_LOADER_BIN) tests/TechnologyYamlLoaderTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_TECHNOLOGY_LOADER_BIN)

test-new-input-names: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_NEW_INPUT_NAMES_BIN)).d -MT $(TEST_NEW_INPUT_NAMES_BIN) -o $(TEST_NEW_INPUT_NAMES_BIN) tests/NewInputNamesTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_NEW_INPUT_NAMES_BIN)

test-generated-v2-configs: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_GENERATED_V2_CONFIGS_BIN)).d -MT $(TEST_GENERATED_V2_CONFIGS_BIN) -o $(TEST_GENERATED_V2_CONFIGS_BIN) tests/GeneratedV2ConfigsTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_GENERATED_V2_CONFIGS_BIN)

test-input-validation: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_INPUT_VALIDATION_BIN)).d -MT $(TEST_INPUT_VALIDATION_BIN) -o $(TEST_INPUT_VALIDATION_BIN) tests/InputValidationTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_INPUT_VALIDATION_BIN)

test-output-path-builder: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_OUTPUT_PATH_BUILDER_BIN)).d -MT $(TEST_OUTPUT_PATH_BUILDER_BIN) -o $(TEST_OUTPUT_PATH_BUILDER_BIN) tests/OutputPathBuilderTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_OUTPUT_PATH_BUILDER_BIN)

test-exploration:
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_EXPLORATION_BIN)).d -MT $(TEST_EXPLORATION_BIN) -o $(TEST_EXPLORATION_BIN) tests/ExplorationDomainTest.cpp \
		src/config/IntValueDomain.cpp src/config/ExplorationSpec.cpp src/config/ExplorationSpaceResolver.cpp $(LD_LIBS)
	$(TEST_EXPLORATION_BIN)

test-variation:
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_VARIATION_BIN)).d -MT $(TEST_VARIATION_BIN) -o $(TEST_VARIATION_BIN) tests/VariationSamplerTest.cpp \
		src/model/VariationSampler.cpp $(LD_LIBS)
	$(TEST_VARIATION_BIN)

test-montecarlo: $(BIN) $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_MONTECARLO_BIN)).d -MT $(TEST_MONTECARLO_BIN) -o $(TEST_MONTECARLO_BIN) tests/MonteCarloRegressionTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_MONTECARLO_BIN)

test-corner: $(BIN) $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_CORNER_BIN)).d -MT $(TEST_CORNER_BIN) -o $(TEST_CORNER_BIN) tests/CornerVariationRegressionTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_CORNER_BIN)

test-wire: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_WIRE_BIN)).d -MT $(TEST_WIRE_BIN) -o $(TEST_WIRE_BIN) tests/WireCopyTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_WIRE_BIN)

test-formula: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_FORMULA_BIN)).d -MT $(TEST_FORMULA_BIN) -o $(TEST_FORMULA_BIN) tests/FormulaTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_FORMULA_BIN)

test-match: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_MATCH_BIN)).d -MT $(TEST_MATCH_BIN) -o $(TEST_MATCH_BIN) tests/MatchTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_MATCH_BIN) $(MATCH_CONFIG_FILE)

test-mat-decoder: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_MAT_DECODER_BIN)).d -MT $(TEST_MAT_DECODER_BIN) -o $(TEST_MAT_DECODER_BIN) tests/MatDecoderRegressionTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_MAT_DECODER_BIN)

test-htree-routing: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_HTREE_ROUTING_BIN)).d -MT $(TEST_HTREE_ROUTING_BIN) -o $(TEST_HTREE_ROUTING_BIN) tests/HtreeRoutingRegressionTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_HTREE_ROUTING_BIN)

test-exhaustive-search: $(OBJECTS_NO_MAIN)
	@mkdir -p $(TEST_DEP_DIR) $(TEST_BIN_DIR)
	$(CC) $(CPP_FLAGS) -MF $(TEST_DEP_DIR)/$(notdir $(TEST_EXHAUSTIVE_SEARCH_BIN)).d -MT $(TEST_EXHAUSTIVE_SEARCH_BIN) -o $(TEST_EXHAUSTIVE_SEARCH_BIN) tests/ExhaustiveSearchRegressionTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	$(TEST_EXHAUSTIVE_SEARCH_BIN)

test-python-package-data:
	python3 tests/test_python_package_data.py

test-pybind-match: $(PYBIND_MODULE)
	python3 tests/test_pybind_match.py $(MATCH_CONFIG_FILE)

test-pybind-run: $(PYBIND_MODULE)
	python3 tests/test_pybind_run.py $(MATCH_CONFIG_FILE)

uml:
	@if ! command -v pdflatex >/dev/null 2>&1; then \
		echo "pdflatex not found"; \
		exit 1; \
	fi
	pdflatex -interaction=nonstopmode -halt-on-error $(UML_TEX)

uml-slide:
	@if ! command -v pdflatex >/dev/null 2>&1; then \
		echo "pdflatex not found"; \
		exit 1; \
	fi
	pdflatex -interaction=nonstopmode -halt-on-error $(UML_SLIDE_TEX)

open-uml: uml
	@if ! command -v xdg-open >/dev/null 2>&1; then \
		echo "xdg-open not found; built $(UML_PDF)"; \
		exit 0; \
	fi
	xdg-open $(UML_PDF) >/dev/null 2>&1 &

.PHONY: clean
clean:
	@rm -rf $(OBJ_DIR) $(TEST_BIN_DIR) $(BIN) \
		$(PYBIND_MODULE_BASE)*.so $(PYBIND_MODULE_BASE)*.d \
		tests/tmp_cell_config.yaml tests/tmp_cell_variation.yaml tests/tmp_variation_cell_config.yaml tests/tmp_variation_system_config.yaml \
		tests/tmp_top_level.config.yaml tests/tmp_top_level.architecture.yaml \
		tests/tmp_top_level.cell.yaml tests/tmp_top_level_cell_config.yaml \
		tests/tmp_top_level.memory_device.yaml tests/tmp_top_level.sensing.yaml \
		tests/tmp_top_level_system_config.yaml tests/tmp_top_level_architecture_config.yaml tests/tmp_top_level_tool_config.yaml \
		tests/tmp_top_level_custom_sa.yaml tests/tmp_top_level.sense_amp.yaml \
		tests/tmp_cell_loader_cell_config.yaml tests/tmp_cell_loader.memory_device.yaml tests/tmp_cell_loader_missing.yaml \
		tests/tmp_custom_sense_amp_loader.yaml tests/tmp_custom_sense_amp_loader_missing.yaml \
		tests/tmp_input_validation.config.yaml tests/tmp_input_validation.architecture.yaml \
		tests/tmp_input_validation.cell.yaml tests/tmp_input_validation.memory_device.yaml \
		tests/tmp_input_validation.sensing.yaml \
		tests/tmp_input_validation_cell_config.yaml tests/tmp_input_validation_system_config.yaml \
		tests/tmp_input_validation_custom_sa.yaml \
		tests/tmp_yaml_helpers.config.yaml tests/tmp_yaml_helpers.architecture.yaml \
		tests/tmp_yaml_helpers.cell.yaml tests/tmp_yaml_helpers.memory_device.yaml \
		tests/tmp_yaml_helpers.sensing.yaml \
		tests/tmp_explicit_subarray_system_config.yaml tests/tmp_organization_system_config.yaml \
		$(UML_PDF) $(UML_SLIDE_PDF) \
		repo_uml.aux repo_uml.log repo_uml_slide.aux repo_uml_slide.log

run: $(BIN)
	@if [ -z "$(CONFIG_FILE)" ]; then \
		echo "Usage: make run CONFIG_FILE=path/to/config.yaml"; \
		exit 1; \
	fi
	@mkdir -p $(RES_DIR)
	@if [ -f $(CONFIG_FILE) ]; then \
		echo "Running $(BIN) with $(CONFIG_FILE)..."; \
		./$(BIN) -o $(RES_YAML) $(CONFIG_FILE) 2>&1 | tee $(RUN_LOG); \
		echo "Results YAML: $(RES_YAML)"; \
		echo "Console log:  $(RUN_LOG)"; \
	else \
		echo "Config file not found: $(CONFIG_FILE)!"; \
		exit 1; \
	fi

test: $(BIN)
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/2FeFET_TCAM/2FeFET_TCAM.config.yaml

test-all-valgrind: $(BIN)
# pass valgrind
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/2FeFET_TCAM/2FeFET_TCAM.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/10T-BCAM_28nm/10T-BCAM_28nm.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/SRAM_16T_28nm/SRAM_16T_28nm.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/SRAM-16T-ESSCIRC15/SRAM-16T-ESSCIRC15.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/8T-BCAM_65nm/8T-BCAM_65nm.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/MRAM-4T2R-VLSIC12/MRAM-4T2R-VLSIC12.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/PCM-2T2R-JSSC11/PCM-2T2R-JSSC11.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-2.5T1R-ISSCC16/ReRAM-2.5T1R-ISSCC16.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-2T2R/ReRAM-2T2R.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-3T1R-ISSCC15/ReRAM-3T1R-ISSCC15.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/FeFET-2Fe1T-DATE-2021/FeFET-2Fe1T-DATE-2021.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-4T2R-VLSIC14/ReRAM-4T2R-VLSIC14.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/MRAM-2T2R-ASPDAC12/MRAM-2T2R-ASPDAC12.config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/MRAM-6T2R-VLSIC11/MRAM-6T2R-VLSIC11.config.yaml > /dev/null
	# ReRAM-2T2R-VLSI21 has only a RAM bitline port and is not a valid CAM configuration.
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/2FeFET_MCAM/2FeFET_MCAM.config.yaml > /dev/null

# the following takes 15 mins to pass valgrind, runs much faster without valgrind turned on
#valgrind $(VALGRIND_FLAGS) ./EvaCAM config/2FeFET_TCAM_DSE/2FeFET_TCAM_DSE.config.yaml > /dev/null

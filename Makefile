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

BIN=EvaCAM
TEST_YAML_BIN=YamlHelpersTest
TEST_TOP_LEVEL_BIN=TopLevelConfigParserTest
TEST_CELL_LOADER_BIN=CellYamlLoaderTest
TEST_INPUT_VALIDATION_BIN=InputValidationTest
TEST_EXPLORATION_BIN=ExplorationDomainTest
TEST_VARIATION_BIN=VariationSamplerTest
TEST_MONTECARLO_BIN=MonteCarloRegressionTest
TEST_WIRE_BIN=WireCopyTest
UML_TEX=docs/repo_uml.tex
UML_PDF=repo_uml.pdf
UML_SLIDE_TEX=docs/repo_uml_slide.tex
UML_SLIDE_PDF=repo_uml_slide.pdf

# Automatically find all CPP files in the source tree
SOURCES=$(shell find $(SRC_DIR) -type f -name '*.cpp' | sort)
# Create corresponding OBJ file paths in the object directory
OBJECTS=$(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))
OBJECTS_NO_MAIN=$(filter-out $(OBJ_DIR)/app/main.o, $(OBJECTS))
DEPS=$(OBJECTS:.o=.d)


CONFIG_STEM=$(basename $(notdir $(CONFIG_FILE)))
RESULT_BASE=$(patsubst %_config,%,$(patsubst %-config,%,$(CONFIG_STEM)))

RES_YAML=$(RES_DIR)/$(RESULT_BASE)_results.yaml
RUN_LOG=$(RES_DIR)/$(RESULT_BASE)_run.log

all: $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(CPP_FLAGS) -o $@ $^ $(LD_LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CPP_FLAGS) -c $< -o $@

-include $(DEPS)

.PHONY: test-yaml test-top-level-parser test-cell-loader test-input-validation test-exploration test-variation test-montecarlo test-wire uml uml-slide open-uml
test-yaml: $(OBJECTS_NO_MAIN)
	$(CC) $(CPP_FLAGS) -o $(TEST_YAML_BIN) tests/YamlHelpersTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	./$(TEST_YAML_BIN)

test-top-level-parser: $(OBJECTS_NO_MAIN)
	$(CC) $(CPP_FLAGS) -o $(TEST_TOP_LEVEL_BIN) tests/TopLevelConfigParserTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	./$(TEST_TOP_LEVEL_BIN)

test-cell-loader: $(OBJECTS_NO_MAIN)
	$(CC) $(CPP_FLAGS) -o $(TEST_CELL_LOADER_BIN) tests/CellYamlLoaderTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	./$(TEST_CELL_LOADER_BIN)

test-input-validation: $(OBJECTS_NO_MAIN)
	$(CC) $(CPP_FLAGS) -o $(TEST_INPUT_VALIDATION_BIN) tests/InputValidationTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	./$(TEST_INPUT_VALIDATION_BIN)

test-exploration:
	$(CC) $(CPP_FLAGS) -o $(TEST_EXPLORATION_BIN) tests/ExplorationDomainTest.cpp \
		src/config/IntValueDomain.cpp src/config/ExplorationSpec.cpp src/config/ExplorationSpaceResolver.cpp $(LD_LIBS)
	./$(TEST_EXPLORATION_BIN)

test-variation:
	$(CC) $(CPP_FLAGS) -o $(TEST_VARIATION_BIN) tests/VariationSamplerTest.cpp \
		src/model/VariationSampler.cpp $(LD_LIBS)
	./$(TEST_VARIATION_BIN)

test-montecarlo: $(BIN) $(OBJECTS_NO_MAIN)
	$(CC) $(CPP_FLAGS) -o $(TEST_MONTECARLO_BIN) tests/MonteCarloRegressionTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	./$(TEST_MONTECARLO_BIN)

test-wire: $(OBJECTS_NO_MAIN)
	$(CC) $(CPP_FLAGS) -o $(TEST_WIRE_BIN) tests/WireCopyTest.cpp $(OBJECTS_NO_MAIN) $(LD_LIBS)
	./$(TEST_WIRE_BIN)

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
	@rm -rf $(OBJ_DIR) $(RES_DIR) $(BIN) $(TEST_YAML_BIN) $(TEST_YAML_BIN).d \
		$(TEST_TOP_LEVEL_BIN) $(TEST_TOP_LEVEL_BIN).d \
		$(TEST_CELL_LOADER_BIN) $(TEST_CELL_LOADER_BIN).d \
		$(TEST_INPUT_VALIDATION_BIN) $(TEST_INPUT_VALIDATION_BIN).d \
		$(TEST_EXPLORATION_BIN) $(TEST_EXPLORATION_BIN).d $(TEST_VARIATION_BIN) $(TEST_VARIATION_BIN).d \
		$(TEST_MONTECARLO_BIN) $(TEST_MONTECARLO_BIN).d \
		$(TEST_WIRE_BIN) $(TEST_WIRE_BIN).d \
		tests/tmp_cell.yaml tests/tmp_cell_variation.yaml tests/tmp_variation_cell.yaml tests/tmp_variation_config.yaml \
		tests/tmp_top_level_cell.yaml tests/tmp_top_level_config.yaml \
		tests/tmp_cell_loader_cell.yaml tests/tmp_cell_loader_missing.yaml \
		tests/tmp_input_validation_cell.yaml tests/tmp_input_validation_config.yaml \
		tests/tmp_explicit_subarray_config.yaml tests/tmp_organization_config.yaml \
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
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/2FeFET_TCAM/2FeFET_TCAM_config.yaml

test-all-valgrind: $(BIN)
# pass valgrind
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/2FeFET_TCAM/2FeFET_TCAM_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/10T-BCAM_28nm/10T-BCAM_28nm_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/SRAM_16T_28nm/SRAM_16T_28nm_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/SRAM-16T-ESSCIRC15/SRAM-16T-ESSCIRC15_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/8T-BCAM_65nm/8T-BCAM_65nm_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/MRAM-4T2R-VLSIC12/MRAM-4T2R-VLSIC12_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/PCM-2T2R-JSSC11/PCM-2T2R-JSSC11_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-2.5T1R-ISSCC16/ReRAM-2.5T1R-ISSCC16_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-2T2R/ReRAM-2T2R_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-3T1R-ISSCC15/ReRAM-3T1R-ISSCC15_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/FeFET-2Fe1T-DATE-2021/FeFET-2Fe1T-DATE-2021_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-4T2R-VLSIC14/ReRAM-4T2R-VLSIC14_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/MRAM-2T2R-ASPDAC12/MRAM-2T2R-ASPDAC12_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/MRAM-6T2R-VLSIC11/MRAM-6T2R-VLSIC11_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-2T2R-VLSI21/ReRAM-2T2R-VLSI21_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/2FeFET_MCAM/2FeFET_MCAM_config.yaml > /dev/null

# the following takes 15 mins to pass valgrind, runs much faster without valgrind turned on
#valgrind $(VALGRIND_FLAGS) ./EvaCAM config/2FeFET_TCAM_DSE/2FeFET_TCAM_DSE_config.yaml > /dev/null

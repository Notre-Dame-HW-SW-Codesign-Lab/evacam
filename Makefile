CC=g++
CPP_FLAGS=-std=c++17 -O0 -Wall -Wextra -Wpedantic -g3 -fno-omit-frame-pointer -fopenmp -MMD -MP -I/usr/include/yaml-cpp
LD_LIBS= -lyaml-cpp 

VALGRIND_FLAGS=--leak-check=full --show-leak-kinds=all --track-origins=yes --suppressions=.valgrind.supp

ROOT_DIR=$(shell pwd)
SRC_DIR=$(ROOT_DIR)/src
OBJ_DIR=$(ROOT_DIR)/obj
RES_DIR=$(ROOT_DIR)/results

BIN=EvaCAM
TEST_YAML_BIN=YamlHelpersTest
UML_TEX=docs/repo_uml.tex
UML_PDF=repo_uml.pdf

# Automatically find all CPP files in the source directory
SOURCES=$(wildcard $(SRC_DIR)/*.cpp)
# Create corresponding OBJ file paths in the object directory
OBJECTS=$(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))
DEPS=$(OBJECTS:.o=.d)


BASE_NAME = $(basename $(notdir $(CONFIG_FILE)))

RES_FILE=$(RES_DIR)/$(BASE_NAME)_results.txt

all: $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(CPP_FLAGS) -o $@ $^ $(LD_LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CPP_FLAGS) -c $< -o $@

-include $(DEPS)

.PHONY: test-yaml uml open-uml
test-yaml:
	$(CC) $(CPP_FLAGS) -o $(TEST_YAML_BIN) tests/YamlHelpersTest.cpp src/YamlHelpers.cpp src/MemCell.cpp $(LD_LIBS)
	./$(TEST_YAML_BIN)

uml:
	@if ! command -v pdflatex >/dev/null 2>&1; then \
		echo "pdflatex not found"; \
		exit 1; \
	fi
	pdflatex -interaction=nonstopmode -halt-on-error $(UML_TEX)

open-uml: uml
	@if ! command -v xdg-open >/dev/null 2>&1; then \
		echo "xdg-open not found; built $(UML_PDF)"; \
		exit 0; \
	fi
	xdg-open $(UML_PDF) >/dev/null 2>&1 &

.PHONY: clean
clean:
	@rm -rf $(OBJ_DIR) $(RES_DIR) $(BIN) $(TEST_YAML_BIN) $(TEST_YAML_BIN).d \
		$(UML_PDF) repo_uml.aux repo_uml.log

run: $(BIN)
	@if [ -z "$(CONFIG_FILE)" ]; then \
		echo "Usage: make run CONFIG_FILE=path/to/config.yaml"; \
		exit 1; \
	fi
	@mkdir -p $(RES_DIR)
	@if [ -f $(CONFIG_FILE) ]; then \
		echo "Running $(BIN) with $(CONFIG_FILE)..."; \
		./$(BIN) $(CONFIG_FILE) 2>&1 | tee $(RES_FILE); \
		echo "Results are written into $(RES_FILE)"; \
	else \
		echo "Config file not found: $(CONFIG_FILE)!"; \
		exit 1; \
	fi

test: $(BIN)
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/2FeFET_TCAM_config.yaml

test-all-valgrind: $(BIN)
# pass valgrind
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/2FeFET_TCAM_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/10T-BCAM_28nm_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/SRAM_16T_28nm_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/SRAM-16T-ESSCIRC15_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/8T-BCAM_65nm_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/MRAM-4T2R-VLSIC12_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/PCM-2T2R-JSSC11_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/ReRAM-2.5T1R-ISSCC16_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/ReRAM-2T2R_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/ReRAM-3T1R-ISSCC15_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/FeFET-2Fe1T-DATE-2021_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/ReRAM-4T2R-VLSIC14_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/MRAM-2T2R-ASPDAC12_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/MRAM-6T2R-VLSIC11_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/ReRAM-2T2R-VLSI21_config.yaml > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/2FeFET_MCAM_config.yaml > /dev/null

# the following takes 15 mins to pass valgrind, runs much faster without valgrind turned on
#valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/2FeFET_TCAM_DSE_config.yaml > /dev/null

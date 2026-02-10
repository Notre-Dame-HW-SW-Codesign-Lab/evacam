CC=g++
CPP_FLAGS=-std=c++17 -O0 -Wall -Wextra -Wpedantic -g3 -fno-omit-frame-pointer -fopenmp -I/usr/include/yaml-cpp
LD_LIBS= -lyaml-cpp 

VALGRIND_FLAGS=--leak-check=full --show-leak-kinds=all --track-origins=yes --suppressions=.valgrind.supp

ROOT_DIR=$(shell pwd)
SRC_DIR=$(ROOT_DIR)/src
OBJ_DIR=$(ROOT_DIR)/obj
RES_DIR=$(ROOT_DIR)/results
CONFIG_DIR=$(ROOT_DIR)/config
CONFIG_SELECT_FILE=$(ROOT_DIR)/config.txt

BIN=EvaCAM

# Automatically find all CPP files in the source directory
SOURCES=$(wildcard $(SRC_DIR)/*.cpp)
# Create corresponding OBJ file paths in the object directory
OBJECTS=$(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SOURCES))


CONFIG_FILE_NAME = $(shell awk '!/^\/\// && NF {print $$1; exit}' "$(CONFIG_SELECT_FILE)")
CONFIG_FILE      = $(CONFIG_FILE_NAME)
BASE_NAME        = $(basename $(notdir $(CONFIG_FILE_NAME)))

RES_FILE=$(RES_DIR)/$(BASE_NAME)_results.txt

all: $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(CPP_FLAGS) -o $@ $^ $(LD_LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CPP_FLAGS) -c $< -o $@

.PHONY: clean
clean:
	@rm -r $(OBJ_DIR) $(RES_DIR) $(BIN)

run: $(BIN)
	@mkdir -p $(RES_DIR)
	@if [ -f $(CONFIG_FILE) ]; then \
		echo "Running $(BIN) with $(CONFIG_FILE_NAME)..."; \
		./$(BIN) $(CONFIG_FILE) 2>&1 | tee $(RES_FILE); \
		echo "Results are written into $(RES_FILE)"; \
	else \
		echo "Config file not found: $(CONFIG_FILE)!"; \
	fi

test: $(BIN)
	valgrind $(VALGRIND_FLAGS) ./EvaCAM yaml/config/2FeFET_TCAM_config.yaml
	#valgrind $(VALGRIND_FLAGS) ./EvaCAM config/2FeFET_TCAM/2FeFET_TCAM.cfg
	#valgrind $(VALGRIND_FLAGS) ./EvaCAM config/8T-BCAM/8T-BCAM_65nm.cfg

test-all-valgrind: $(BIN)
# pass valgrind
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/2FeFET_TCAM/2FeFET_TCAM.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/10T-BCAM/10T-BCAM_28nm.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/16T-TCAM/SRAM_16T_28nm.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/16T-TCAM/SRAM-16T-ESSCIRC15.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/8T-BCAM/8T-BCAM_65nm.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/MRAM-4T2R/MRAM-4T2R-VLSIC12.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/PCM-2T2R/PCM-2T2R-JSSC11.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-2.5T1R/ReRAM-2.5T1R-ISSCC16.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-2T2R/ReRAM-2T2R.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-3T1R/ReRAM-3T1R-ISSCC15.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/FeFET-2Fe1T/FeFET-2Fe1T-DATE-2021.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-4T2R/ReRAM-4T2R-VLSIC14.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/MRAM_2T2R/MRAM-2T2R-ASPDAC12.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/MRAM-6T2R/MRAM-6T2R-VLSIC11.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/ReRAM-2T2R/ReRAM-2T2R-VLSI21.cfg > /dev/null
	valgrind $(VALGRIND_FLAGS) ./EvaCAM config/2FeFET_MCAM/2FeFET_MCAM.cfg > /dev/null

# the following takes 15 mins to pass valgrind, runs much faster without valgrind turned on
#valgrind $(VALGRIND_FLAGS) ./EvaCAM config/2FeFET_TCAM_DSE/2FeFET_TCAM_DSE.cfg > /dev/null

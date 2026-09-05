CXX ?= c++
FAST_IO_ROOT ?= ../..
BUILD_DIR ?= /tmp/fast_io_cpo_matrix
TAG ?= new
SOURCE ?= mixed
PACK ?= 8
LINE ?= 0
OUTPUT ?= obuffer
RESULT ?= std
CXXFLAGS ?= -O3 -march=native -std=c++20 -DNDEBUG

SOURCE_ID_f := 0
SOURCE_ID_d := 1
SOURCE_ID_p := 2
SOURCE_ID_s := 3
SOURCE_ID_a := 4
SOURCE_ID_mixed := 5
SOURCE_ID_bs := 6
SOURCE_ID_mixed_b := 7
SOURCE_ID_bd := 8
SOURCE_ID_pp := 9
SOURCE_ID_ss := 10
SOURCE_ID_mixed_p := 11
SOURCE_ID := $(SOURCE_ID_$(SOURCE))

OUTPUT_ID_obuffer := 0
OUTPUT_ID_raw := 1
OUTPUT_ID := $(OUTPUT_ID_$(OUTPUT))

RESULT_ID_std := 0
RESULT_ID_fast := 1
RESULT_ID := $(RESULT_ID_$(RESULT))

ifeq ($(SOURCE_ID),)
$(error SOURCE must be one of: f d p s bs a mixed mixed_b bd pp ss mixed_p)
endif
ifeq ($(OUTPUT_ID),)
$(error OUTPUT must be one of: obuffer raw)
endif
ifeq ($(RESULT_ID),)
$(error RESULT must be one of: std fast)
endif
ifeq ($(filter $(PACK),1 2 8 9 16 32 64),)
$(error PACK must be one of: 1 2 8 9 16 32 64)
endif
ifeq ($(filter $(LINE),0 1),)
$(error LINE must be 0 or 1)
endif

BIN_DIR := $(BUILD_DIR)/$(TAG)
COMMON_DEFINES := \
	-DFAST_IO_CPO_MATRIX_SOURCE=$(SOURCE_ID) \
	-DFAST_IO_CPO_MATRIX_PACK=$(PACK) \
	-DFAST_IO_CPO_MATRIX_LINE=$(LINE)
CPPFLAGS += -I$(FAST_IO_ROOT)/include $(COMMON_DEFINES)

PRINT_BIN := $(BIN_DIR)/print-$(SOURCE)-n$(PACK)-l$(LINE)-$(OUTPUT)
CONCAT_BIN := $(BIN_DIR)/concat-$(SOURCE)-n$(PACK)-l$(LINE)-$(RESULT)

.PHONY: all print concat print-path concat-path

all: print concat

print: $(PRINT_BIN)

concat: $(CONCAT_BIN)

$(BIN_DIR):
	mkdir -p $@

$(PRINT_BIN): print_case.cc common_sources.h common_outputs.h independent_oracle.h byte_oracle.h case_driver.h | $(BIN_DIR)
	$(CXX) $(CPPFLAGS) -DFAST_IO_CPO_MATRIX_OUTPUT=$(OUTPUT_ID) $(CXXFLAGS) $< -o $@

$(CONCAT_BIN): concat_case.cc common_sources.h independent_oracle.h byte_oracle.h case_driver.h | $(BIN_DIR)
	$(CXX) $(CPPFLAGS) -DFAST_IO_CPO_MATRIX_RESULT=$(RESULT_ID) $(CXXFLAGS) $< -o $@

print-path:
	@printf '%s\n' '$(PRINT_BIN)'

concat-path:
	@printf '%s\n' '$(CONCAT_BIN)'

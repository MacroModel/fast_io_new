CXX ?= c++
FAST_IO_ROOT ?= ../..
BUILD_DIR ?= /tmp/fast_io_state_machine_cpo
TAG ?= new
INPUT ?= contiguous
RECEIVER ?= int10
TO ?= text-int
TRANSCODER ?= adapter
# A direction cell owns one engine/adapter boundary and one bounded-window
# policy.  Keeping these axes in the artifact name prevents stale cross-cell
# binaries from being mistaken for a requested protocol instantiation.
TRANSCODER_DIRECTION_MODE ?= engine-decode-member
TRANSCODER_DIRECTION_CHUNK ?= bulk
TRANSCODER_DIRECTION_BUFFER ?= default-cold
TRANSCODER_DIRECTION_BASELINE ?= new
TO_PROTOCOL_SOURCE ?= f
TO_PROTOCOL_PACK ?= 1
TO_PROTOCOL_FRONTDOOR ?= to
TO_PROTOCOL_TARGET ?= builtin
TO_PROTOCOL_MODE ?= runtime
CXXFLAGS ?= -O3 -march=native -std=c++20 -DNDEBUG

INPUT_ID_contiguous := 0
INPUT_ID_chunk1 := 1
INPUT_ID_chunk3 := 2
INPUT_ID_chunk7 := 3
INPUT_ID := $(INPUT_ID_$(INPUT))

RECEIVER_ID_int10 := 0
RECEIVER_ID_int16 := 1
RECEIVER_ID_double := 2
RECEIVER_ID_string := 3
RECEIVER_ID := $(RECEIVER_ID_$(RECEIVER))

TO_ID_text-int := 0
TO_ID_text-double := 1
TO_ID_scalar-string := 2
TO_ID_inplace-int := 3
TO_ID := $(TO_ID_$(TO))

TRANSCODER_ID_adapter := 0
TRANSCODER_ID_staged := 1
TRANSCODER_ID := $(TRANSCODER_ID_$(TRANSCODER))

TRANSCODER_DIRECTION_MODE_ID_engine-decode-member := 0
TRANSCODER_DIRECTION_MODE_ID_engine-decode-cpo := 1
TRANSCODER_DIRECTION_MODE_ID_engine-encode-member := 2
TRANSCODER_DIRECTION_MODE_ID_engine-encode-cpo := 3
TRANSCODER_DIRECTION_MODE_ID_input-adapter := 4
TRANSCODER_DIRECTION_MODE_ID_output-adapter := 5
TRANSCODER_DIRECTION_MODE_ID_full-transmit := 6
TRANSCODER_DIRECTION_MODE_ID_staged-control := 7
TRANSCODER_DIRECTION_MODE_ID := $(TRANSCODER_DIRECTION_MODE_ID_$(TRANSCODER_DIRECTION_MODE))

TRANSCODER_DIRECTION_CHUNK_ID_1 := 0
TRANSCODER_DIRECTION_CHUNK_ID_2 := 1
TRANSCODER_DIRECTION_CHUNK_ID_3 := 2
TRANSCODER_DIRECTION_CHUNK_ID_63 := 3
TRANSCODER_DIRECTION_CHUNK_ID_64 := 4
TRANSCODER_DIRECTION_CHUNK_ID_65 := 5
TRANSCODER_DIRECTION_CHUNK_ID_bulk := 6
TRANSCODER_DIRECTION_CHUNK_ID := $(TRANSCODER_DIRECTION_CHUNK_ID_$(TRANSCODER_DIRECTION_CHUNK))

TRANSCODER_DIRECTION_BUFFER_ID_default-cold := 0
TRANSCODER_DIRECTION_BUFFER_ID_buffer64-cold := 1
TRANSCODER_DIRECTION_BUFFER_ID := $(TRANSCODER_DIRECTION_BUFFER_ID_$(TRANSCODER_DIRECTION_BUFFER))

TRANSCODER_DIRECTION_BASELINE_ID_new := 0
TRANSCODER_DIRECTION_BASELINE_ID_official-old-engine := 1
TRANSCODER_DIRECTION_BASELINE_ID := $(TRANSCODER_DIRECTION_BASELINE_ID_$(TRANSCODER_DIRECTION_BASELINE))

TO_PROTOCOL_SOURCE_ID_f := 0
TO_PROTOCOL_SOURCE_ID_d := 1
TO_PROTOCOL_SOURCE_ID_pp := 9
TO_PROTOCOL_SOURCE_ID_ss := 10
TO_PROTOCOL_SOURCE_ID_mixed-proof := 11
TO_PROTOCOL_SOURCE_ID_literal := 0
TO_PROTOCOL_SOURCE_ID := $(TO_PROTOCOL_SOURCE_ID_$(TO_PROTOCOL_SOURCE))

TO_PROTOCOL_FRONTDOOR_ID_to := 0
TO_PROTOCOL_FRONTDOOR_ID_inplace := 1
TO_PROTOCOL_FRONTDOOR_ID := $(TO_PROTOCOL_FRONTDOOR_ID_$(TO_PROTOCOL_FRONTDOOR))

TO_PROTOCOL_TARGET_ID_builtin := 0
TO_PROTOCOL_TARGET_ID_context := 1
TO_PROTOCOL_TARGET_ID := $(TO_PROTOCOL_TARGET_ID_$(TO_PROTOCOL_TARGET))

TO_PROTOCOL_MODE_ID_runtime := 0
TO_PROTOCOL_MODE_ID_literal := 1
TO_PROTOCOL_MODE_ID := $(TO_PROTOCOL_MODE_ID_$(TO_PROTOCOL_MODE))

ifeq ($(INPUT_ID),)
$(error INPUT must be one of: contiguous chunk1 chunk3 chunk7)
endif
ifeq ($(RECEIVER_ID),)
$(error RECEIVER must be one of: int10 int16 double string)
endif
ifeq ($(TO_ID),)
$(error TO must be one of: text-int text-double scalar-string inplace-int)
endif
ifeq ($(TRANSCODER_ID),)
$(error TRANSCODER must be one of: adapter staged)
endif
ifeq ($(TRANSCODER_DIRECTION_MODE_ID),)
$(error TRANSCODER_DIRECTION_MODE must be one of: engine-decode-member engine-decode-cpo engine-encode-member engine-encode-cpo input-adapter output-adapter full-transmit staged-control)
endif
ifeq ($(TRANSCODER_DIRECTION_CHUNK_ID),)
$(error TRANSCODER_DIRECTION_CHUNK must be one of: 1 2 3 63 64 65 bulk)
endif
ifeq ($(TRANSCODER_DIRECTION_BUFFER_ID),)
$(error TRANSCODER_DIRECTION_BUFFER must be one of: default-cold buffer64-cold)
endif
ifeq ($(TRANSCODER_DIRECTION_BASELINE_ID),)
$(error TRANSCODER_DIRECTION_BASELINE must be one of: new official-old-engine)
endif
ifeq ($(TRANSCODER_DIRECTION_BASELINE),official-old-engine)
# The official tree predates both the bounded CPO and the stream adapters.
# Rejecting those labels is part of the comparison contract, not a build skip.
ifeq ($(filter $(TRANSCODER_DIRECTION_MODE),engine-decode-member engine-encode-member),)
$(error official-old-engine supports only direct-member engine direction rows)
endif
ifneq ($(TRANSCODER_DIRECTION_BUFFER),default-cold)
$(error official-old-engine rows have no adapter buffer-policy axis)
endif
endif
ifeq ($(TO_PROTOCOL_SOURCE_ID),)
$(error TO_PROTOCOL_SOURCE must be one of: f d pp ss mixed-proof literal)
endif
ifeq ($(filter $(TO_PROTOCOL_PACK),1 2 8 32),)
$(error TO_PROTOCOL_PACK must be one of: 1 2 8 32)
endif
ifeq ($(TO_PROTOCOL_FRONTDOOR_ID),)
$(error TO_PROTOCOL_FRONTDOOR must be one of: to inplace)
endif
ifeq ($(TO_PROTOCOL_TARGET_ID),)
$(error TO_PROTOCOL_TARGET must be one of: builtin context)
endif
ifeq ($(TO_PROTOCOL_MODE_ID),)
$(error TO_PROTOCOL_MODE must be one of: runtime literal)
endif
ifeq ($(TO_PROTOCOL_MODE),runtime)
ifeq ($(filter $(TO_PROTOCOL_SOURCE),f d pp ss mixed-proof),)
$(error runtime to-protocol cells require TO_PROTOCOL_SOURCE=f, d, pp, ss, or mixed-proof)
endif
else
ifneq ($(TO_PROTOCOL_SOURCE),literal)
$(error literal to-protocol cells require TO_PROTOCOL_SOURCE=literal)
endif
ifneq ($(TO_PROTOCOL_TARGET),builtin)
$(error literal to-protocol cells require TO_PROTOCOL_TARGET=builtin)
endif
endif

BIN_DIR := $(BUILD_DIR)/$(TAG)
CPPFLAGS += -I$(FAST_IO_ROOT)/include
SCAN_BIN := $(BIN_DIR)/scan-$(INPUT)-$(RECEIVER)
TO_BIN := $(BIN_DIR)/to-$(TO)
TRANSCODER_BIN := $(BIN_DIR)/transcoder-$(TRANSCODER)
TRANSCODER_DIRECTION_BIN := $(BIN_DIR)/transcoder-direction-$(TRANSCODER_DIRECTION_BASELINE)-$(TRANSCODER_DIRECTION_MODE)-chunk$(TRANSCODER_DIRECTION_CHUNK)-$(TRANSCODER_DIRECTION_BUFFER)
TRANSCODER_DIRECTION_DEFINES := \
	-DFAST_IO_STATE_TRANSCODER_DIRECTION_MODE=$(TRANSCODER_DIRECTION_MODE_ID) \
	-DFAST_IO_STATE_TRANSCODER_DIRECTION_CHUNK=$(TRANSCODER_DIRECTION_CHUNK_ID) \
	-DFAST_IO_STATE_TRANSCODER_DIRECTION_BUFFER_POLICY=$(TRANSCODER_DIRECTION_BUFFER_ID) \
	-DFAST_IO_STATE_TRANSCODER_DIRECTION_OLD=$(TRANSCODER_DIRECTION_BASELINE_ID)
ifeq ($(TRANSCODER_DIRECTION_BUFFER),buffer64-cold)
# The source validates this exact byte budget again, so an inherited conflicting
# definition cannot silently corrupt the emitted experiment label.
TRANSCODER_DIRECTION_DEFINES += -DFAST_IO_BUFFER_SIZE=64
endif
TO_PROTOCOL_BIN := $(BIN_DIR)/to-protocol-$(TO_PROTOCOL_MODE)-$(TO_PROTOCOL_SOURCE)-p$(TO_PROTOCOL_PACK)-$(TO_PROTOCOL_FRONTDOOR)-$(TO_PROTOCOL_TARGET)
TO_PROTOCOL_OBJ := $(TO_PROTOCOL_BIN).o
TO_PROTOCOL_DEFINES := \
	-DFAST_IO_CPO_MATRIX_SOURCE=$(TO_PROTOCOL_SOURCE_ID) \
	-DFAST_IO_CPO_MATRIX_PACK=$(TO_PROTOCOL_PACK) \
	-DFAST_IO_TO_PROTOCOL_FRONTDOOR=$(TO_PROTOCOL_FRONTDOOR_ID) \
	-DFAST_IO_TO_PROTOCOL_TARGET=$(TO_PROTOCOL_TARGET_ID) \
	-DFAST_IO_TO_PROTOCOL_MODE=$(TO_PROTOCOL_MODE_ID)
# Linker selection is not a compile input and Clang diagnoses it as unused for
# `-c`; remove only that driver flag so strict object-metric cells remain clean.
TO_PROTOCOL_OBJECT_CXXFLAGS := $(filter-out -fuse-ld=%,$(CXXFLAGS))

.PHONY: all scan to transcoder transcoder-direction to-protocol to-protocol-object scan-path to-path transcoder-path transcoder-direction-path to-protocol-path to-protocol-object-path

all: scan to transcoder to-protocol

scan: $(SCAN_BIN)

to: $(TO_BIN)

transcoder: $(TRANSCODER_BIN)

transcoder-direction: $(TRANSCODER_DIRECTION_BIN)

to-protocol: $(TO_PROTOCOL_BIN)

to-protocol-object: $(TO_PROTOCOL_OBJ)

$(BIN_DIR):
	mkdir -p $@

$(SCAN_BIN): scan_case.cc case_driver.h fixture.h independent_oracle.h | $(BIN_DIR)
	$(CXX) $(CPPFLAGS) \
		-DFAST_IO_STATE_SCAN_INPUT=$(INPUT_ID) \
		-DFAST_IO_STATE_SCAN_RECEIVER=$(RECEIVER_ID) \
		$(CXXFLAGS) $< -o $@

$(TO_BIN): to_case.cc case_driver.h fixture.h independent_oracle.h | $(BIN_DIR)
	$(CXX) $(CPPFLAGS) -DFAST_IO_STATE_TO_OPERATION=$(TO_ID) \
		$(CXXFLAGS) $< -o $@

$(TRANSCODER_BIN): transcoder_case.cc case_driver.h fixture.h independent_oracle.h | $(BIN_DIR)
	$(CXX) $(CPPFLAGS) -DFAST_IO_STATE_TRANSCODER_MODE=$(TRANSCODER_ID) \
		$(CXXFLAGS) $< -o $@

$(TRANSCODER_DIRECTION_BIN): transcoder_direction_case.cc fixture.h independent_oracle.h ../0026.cpo_matrix/case_driver.h | $(BIN_DIR)
	$(CXX) $(CPPFLAGS) $(TRANSCODER_DIRECTION_DEFINES) \
		$(CXXFLAGS) $< -o $@

$(TO_PROTOCOL_BIN): to_protocol_case.cc ../0026.cpo_matrix/case_driver.h ../0026.cpo_matrix/common_sources.h | $(BIN_DIR)
	$(CXX) $(CPPFLAGS) $(TO_PROTOCOL_DEFINES) $(CXXFLAGS) $< -o $@

$(TO_PROTOCOL_OBJ): to_protocol_case.cc ../0026.cpo_matrix/case_driver.h ../0026.cpo_matrix/common_sources.h | $(BIN_DIR)
	$(CXX) $(CPPFLAGS) $(TO_PROTOCOL_DEFINES) $(TO_PROTOCOL_OBJECT_CXXFLAGS) -c $< -o $@

scan-path:
	@printf '%s\n' '$(SCAN_BIN)'

to-path:
	@printf '%s\n' '$(TO_BIN)'

transcoder-path:
	@printf '%s\n' '$(TRANSCODER_BIN)'

transcoder-direction-path:
	@printf '%s\n' '$(TRANSCODER_DIRECTION_BIN)'

to-protocol-path:
	@printf '%s\n' '$(TO_PROTOCOL_BIN)'

to-protocol-object-path:
	@printf '%s\n' '$(TO_PROTOCOL_OBJ)'

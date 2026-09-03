CXX ?= c++
ROOT ?= ../..
OUT ?= numeric_matrix
BASE ?= 10
PACK ?= 1
BITS ?= 64
TRANSMIT_KIND ?= 3
TRANSMIT_CHUNK ?= 23
TRANSMIT_OUTPUT ?= 0
CXXFLAGS ?= -O3 -march=native -std=c++20 -DNDEBUG

CPPFLAGS += -I$(ROOT)/include \
	-DFAST_IO_OLD_NEW_NUMERIC_BASE=$(BASE) \
	-DFAST_IO_OLD_NEW_NUMERIC_PACK=$(PACK) \
	-DFAST_IO_OLD_NEW_NUMERIC_BITS=$(BITS)

.PHONY: all floating transmit clean

all: $(OUT)

floating: floating_scan_matrix

transmit: transmit_matrix

$(OUT): numeric_matrix.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

floating_scan_matrix: floating_scan_matrix.cc
	$(CXX) -I$(ROOT)/include $(CXXFLAGS) $< -o $@

transmit_matrix: transmit_matrix.cc
	$(CXX) -I$(ROOT)/include \
		-DFAST_IO_OLD_NEW_TRANSMIT_KIND=$(TRANSMIT_KIND) \
		-DFAST_IO_OLD_NEW_TRANSMIT_CHUNK=$(TRANSMIT_CHUNK) \
		-DFAST_IO_OLD_NEW_TRANSMIT_OUTPUT=$(TRANSMIT_OUTPUT) \
		$(CXXFLAGS) $< -o $@

clean:
	$(RM) $(OUT) floating_scan_matrix transmit_matrix

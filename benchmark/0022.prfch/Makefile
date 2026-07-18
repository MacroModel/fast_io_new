CXX ?= c++
ROOT ?= ../..
BUILD_DIR ?= /tmp/fast_io_prfch_bench
OUT ?= $(BUILD_DIR)/scatter_chain_bench
OBJECT ?= $(BUILD_DIR)/scatter_chain_bench.o
OBJDUMP ?= objdump

CPPFLAGS += -I$(ROOT)/include -DFAST_IO_DISABLE_FLOATING_POINT \
	-DFAST_IO_PRFCH_BENCH_MIN_CURRENT_BYTES=$(MIN_CURRENT_BYTES)
CXXFLAGS ?= -O3 -march=native -std=c++20 -DNDEBUG
MIN_CURRENT_BYTES ?= 256

.PHONY: all codegen clean FORCE

all: $(OUT)

$(BUILD_DIR):
	mkdir -p $@

$(OUT): scatter_chain_bench.cc FORCE | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(OBJECT): scatter_chain_bench.cc FORCE | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Disassemble only the four measured kernels.  The command-line driver intentionally
# remains in the same translation unit so the executable and code-shape probe use
# exactly the same compiler flags and kernel definitions.
codegen: $(OBJECT)
	$(OBJDUMP) -drC --disassemble=fast_io_prfch_read_baseline $(OBJECT)
	$(OBJDUMP) -drC --disassemble=fast_io_prfch_read_next_nonempty $(OBJECT)
	$(OBJDUMP) -drC --disassemble=fast_io_prfch_write_baseline $(OBJECT)
	$(OBJDUMP) -drC --disassemble=fast_io_prfch_write_next_nonempty $(OBJECT)

clean:
	$(RM) $(OUT) $(OBJECT)

FORCE:

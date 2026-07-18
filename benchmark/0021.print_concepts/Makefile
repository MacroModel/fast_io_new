CXX ?= c++
ROOT ?= ../..
OUT ?= composition_bench
ITERATIONS ?= 100000

CPPFLAGS += -I$(ROOT)/include -DFAST_IO_DISABLE_FLOATING_POINT \
	-DFAST_IO_PRINT_CONCEPT_BENCH_ITERATIONS=$(ITERATIONS)
CXXFLAGS ?= -O3 -march=native -std=c++20 -DNDEBUG

.PHONY: all fmt clean

all: $(OUT)

$(OUT): composition_bench.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

# Optional: excluded from `all`, so the fast_io harness never acquires a fmt dependency.
fmt: fmt_comparison

fmt_comparison: fmt_comparison.cc
	$(CXX) -DFAST_IO_PRINT_CONCEPT_BENCH_ITERATIONS=$(ITERATIONS) -DFMT_HEADER_ONLY $(CXXFLAGS) $< -o $@

clean:
	$(RM) $(OUT) fmt_comparison

CXX ?= c++
ROOT ?= ../..
OUT ?= composition_bench
ITERATIONS ?= 100000
GOOGLE_BENCHMARK_ROOT ?= /tmp/google-benchmark
GOOGLE_BENCHMARK_BUILD ?= /tmp/google-benchmark-build
RUNTIME_FLOAT_SCATTER_OUT ?= runtime_float_scatter_bench
STATIC_PRECISION_FLOAT_WIDTH_OUT ?= static_precision_float_width_bench
STATIC_PRECISION_FLOAT_WIDTH_ASM_OUT ?= static_precision_float_width_asm.o
COMPILER_CONSTANT_BRIDGE_OUTLINE_OUT ?= compiler_constant_bridge_outline_bench

CPPFLAGS += -I$(ROOT)/include -DFAST_IO_DISABLE_FLOATING_POINT \
	-DFAST_IO_PRINT_CONCEPT_BENCH_ITERATIONS=$(ITERATIONS)
CXXFLAGS ?= -O3 -march=native -std=c++20 -DNDEBUG

.PHONY: all fmt runtime-float-scatter static-precision-float-width \
	static-precision-float-width-asm compiler-constant-float-asm-gate \
	compiler-constant-bridge-outline \
	compiler-constant-integer-rodata-asm-gate \
	compiler-constant-integer-facade-asm-gate \
	static-fragment-hot-policy-asm-gate \
	static-argument-fixed-width-asm-gate clean

all: $(OUT)

$(OUT): composition_bench.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

# Optional: excluded from `all`, so the fast_io harness never acquires a fmt dependency.
fmt: fmt_comparison

fmt_comparison: fmt_comparison.cc
	$(CXX) -DFAST_IO_PRINT_CONCEPT_BENCH_ITERATIONS=$(ITERATIONS) -DFMT_HEADER_ONLY $(CXXFLAGS) $< -o $@

# Optional: this numeric transport calibration depends on a separately built Google Benchmark tree.
runtime-float-scatter: $(RUNTIME_FLOAT_SCATTER_OUT)

$(RUNTIME_FLOAT_SCATTER_OUT): runtime_float_scatter_bench.cc
	$(CXX) -I$(ROOT)/include -I$(GOOGLE_BENCHMARK_ROOT)/include $(CXXFLAGS) $< \
		$(GOOGLE_BENCHMARK_BUILD)/src/libbenchmark.a -lpthread -o $@

# Optional: isolates static width/precision floating fields on a reusable put area.
static-precision-float-width: $(STATIC_PRECISION_FLOAT_WIDTH_OUT)

$(STATIC_PRECISION_FLOAT_WIDTH_OUT): static_precision_float_width_bench.cc
	$(CXX) -I$(ROOT)/include -I$(GOOGLE_BENCHMARK_ROOT)/include $(CXXFLAGS) $< \
		$(GOOGLE_BENCHMARK_BUILD)/src/libbenchmark.a -lpthread -o $@

# Optional: calibrates the run-time source-normalization bridge and its
# 128-format instruction-working-set tradeoff.
compiler-constant-bridge-outline: $(COMPILER_CONSTANT_BRIDGE_OUTLINE_OUT)

$(COMPILER_CONSTANT_BRIDGE_OUTLINE_OUT): compiler_constant_bridge_outline_bench.cc
	$(CXX) -I$(ROOT)/include -I$(GOOGLE_BENCHMARK_ROOT)/include $(CXXFLAGS) $< \
		$(GOOGLE_BENCHMARK_BUILD)/src/libbenchmark.a -lpthread -o $@

static-precision-float-width-asm: $(STATIC_PRECISION_FLOAT_WIDTH_ASM_OUT)

$(STATIC_PRECISION_FLOAT_WIDTH_ASM_OUT): static_precision_float_width_asm.cc
	$(CXX) -I$(ROOT)/include $(CXXFLAGS) -c $< -o $@

compiler-constant-float-asm-gate:
	ROOT="$(abspath $(ROOT))" CXX="$${GCC15_CXX:-g++-15}" \
		bash ./check_compiler_constant_float_posix_asm.sh

compiler-constant-integer-rodata-asm-gate:
	ROOT="$(abspath $(ROOT))" \
		bash ./check_compiler_constant_integer_facade_asm.sh

compiler-constant-integer-facade-asm-gate: compiler-constant-integer-rodata-asm-gate

static-fragment-hot-policy-asm-gate:
	ROOT="$(abspath $(ROOT))" bash ./check_static_fragment_hot_policy_asm.sh

static-argument-fixed-width-asm-gate:
	ROOT="$(abspath $(ROOT))" bash ./check_static_argument_fixed_width_asm.sh

clean:
	$(RM) $(OUT) fmt_comparison $(RUNTIME_FLOAT_SCATTER_OUT) \
		$(STATIC_PRECISION_FLOAT_WIDTH_OUT) $(STATIC_PRECISION_FLOAT_WIDTH_ASM_OUT) \
		$(COMPILER_CONSTANT_BRIDGE_OUTLINE_OUT)

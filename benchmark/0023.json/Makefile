CXX ?= g++-15
CC ?= gcc-15
ROOT ?= ../..
DEPS_ROOT ?= /tmp/fast_io_json_deps
BUILD_DIR ?= /tmp/fast_io_json_bench_build
OUT ?= $(BUILD_DIR)/json_bench
CPU ?= 4

YYJSON_ROOT := $(DEPS_ROOT)/yyjson
RAPIDJSON_ROOT := $(DEPS_ROOT)/rapidjson
SIMDJSON_ROOT := $(DEPS_ROOT)/simdjson
GLAZE_ROOT := $(DEPS_ROOT)/glaze

CPPFLAGS += -I$(ROOT)/include \
	-I$(YYJSON_ROOT)/src \
	-I$(RAPIDJSON_ROOT)/include \
	-I$(SIMDJSON_ROOT)/singleheader \
	-I$(GLAZE_ROOT)/include
CXXFLAGS ?= -O3 -march=native -std=c++23 -DNDEBUG -Wall -Wextra -Wpedantic
CFLAGS ?= -O3 -march=native -std=c17 -DNDEBUG -Wall -Wextra -Wpedantic
DEPFLAGS := -MMD -MP
LDLIBS += -pthread

CXX_SOURCES := json_bench.cc fast_io_adapter.cc yyjson_adapter.cc \
	rapidjson_adapter.cc simdjson_adapter.cc glaze_adapter.cc
CXX_OBJECTS := $(addprefix $(BUILD_DIR)/,$(CXX_SOURCES:.cc=.o))

.PHONY: all fetch run clean FORCE

all: $(OUT)

fetch:
	FAST_IO_JSON_BENCH_DEPS="$(DEPS_ROOT)" bash ./fetch_dependencies.sh

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/yyjson.o: $(YYJSON_ROOT)/src/yyjson.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(YYJSON_ROOT)/src -c $< -o $@

$(BUILD_DIR)/%.o: %.cc benchmark_adapter.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/simdjson_library.o: $(SIMDJSON_ROOT)/singleheader/simdjson.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(OUT): $(CXX_OBJECTS) $(BUILD_DIR)/simdjson_library.o $(BUILD_DIR)/yyjson.o FORCE | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CXX_OBJECTS) $(BUILD_DIR)/simdjson_library.o \
		$(BUILD_DIR)/yyjson.o $(LDLIBS) -o $@

run: $(OUT)
	taskset -c $(CPU) $(OUT)

clean:
	$(RM) -r $(BUILD_DIR)

FORCE:

-include $(CXX_OBJECTS:.o=.d) $(BUILD_DIR)/simdjson_library.d

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
LDLIBS += -pthread

.PHONY: all fetch run clean FORCE

all: $(OUT)

fetch:
	FAST_IO_JSON_BENCH_DEPS="$(DEPS_ROOT)" bash ./fetch_dependencies.sh

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/yyjson.o: $(YYJSON_ROOT)/src/yyjson.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(YYJSON_ROOT)/src -c $< -o $@

$(OUT): json_bench.cc $(BUILD_DIR)/yyjson.o \
	$(SIMDJSON_ROOT)/singleheader/simdjson.cpp FORCE | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) json_bench.cc \
		$(SIMDJSON_ROOT)/singleheader/simdjson.cpp $(BUILD_DIR)/yyjson.o \
		$(LDLIBS) -o $@

run: $(OUT)
	taskset -c $(CPU) $(OUT)

clean:
	$(RM) -r $(BUILD_DIR)

FORCE:


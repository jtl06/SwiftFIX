# Convenience wrapper around scripts/*.sh. Targets are phony; real build
# state lives in build/release (Release) and build/debug (Debug+sanitizers).
#
# Usage:
#   make                 # Release build
#   make test            # ctest on build/release
#   make bench           # bench on corpus/valid (quick sanity run)
#   make bench-stream    # bench on corpus/bulk.stream (headline numbers)
#   make bench-perf      # bench-stream + libpfm4 perf counters
#   make debug           # Debug build with ASan+UBSan
#   make clean           # wipe build/

SHELL        := /usr/bin/env bash
REPO_ROOT    := $(shell cd "$(dir $(abspath $(lastword $(MAKEFILE_LIST))))" && pwd)
BUILD_REL    := $(REPO_ROOT)/build/release
BUILD_DBG    := $(REPO_ROOT)/build/debug
CORPUS_VALID := $(REPO_ROOT)/corpus/valid
CORPUS_BULK  := $(REPO_ROOT)/corpus/bulk.stream
BENCH_ARGS   ?= --benchmark_min_time=2s
PERF_COUNTERS ?= INSTRUCTIONS,CYCLES,BRANCHES,L1-DCACHE-LOAD-MISSES

.PHONY: all build debug test bench bench-stream bench-perf clean

all: build

build:
	@scripts/build.sh

debug:
	@scripts/build_debug.sh

test: build
	@cd $(BUILD_REL) && ctest --output-on-failure

bench: build
	@SWIFTFIX_CORPUS=$(CORPUS_VALID) scripts/bench.sh $(BENCH_ARGS)

bench-stream: build
	@SWIFTFIX_CORPUS=$(CORPUS_BULK) scripts/bench.sh $(BENCH_ARGS)

bench-perf: build
	@SWIFTFIX_CORPUS=$(CORPUS_BULK) scripts/bench.sh \
		$(BENCH_ARGS) --benchmark_perf_counters=$(PERF_COUNTERS)

clean:
	@rm -rf $(BUILD_REL) $(BUILD_DBG)
# Thin wrapper around the CMake build for titan-engine.
# CMake does the real work (see CMakeLists.txt); these targets are convenience
# shortcuts so you can just run `make test`.

# cmake/ctest live in Homebrew's bin on this machine and may not be on PATH,
# so allow overriding but default to a sensible location.
CMAKE  ?= $(shell command -v cmake  2>/dev/null || echo /opt/homebrew/bin/cmake)
CTEST  ?= $(shell command -v ctest  2>/dev/null || echo /opt/homebrew/bin/ctest)

BUILD_DIR ?= build

.PHONY: all configure build test retest clean rebuild bench bench-baseline help

# Default target: build everything.
all: build

## configure: generate the CMake build system in $(BUILD_DIR) (idempotent).
configure:
	$(CMAKE) -S . -B $(BUILD_DIR)

## build: configure (if needed) and compile all targets, incl. the test binary.
build: configure
	$(CMAKE) --build $(BUILD_DIR)

## test: build then run every test in the tests/ folder via CTest.
test: build
	cd $(BUILD_DIR) && $(CTEST) --output-on-failure

## retest: run the tests without rebuilding (assumes `make build` already ran).
retest:
	cd $(BUILD_DIR) && $(CTEST) --output-on-failure

## clean: remove the entire build directory.
clean:
	rm -rf $(BUILD_DIR)

## rebuild: clean then build from scratch.
rebuild: clean build

## bench: build and run all benchmark binaries.
bench: build
	$(BUILD_DIR)/benchmarks/bench_match
	$(BUILD_DIR)/benchmarks/bench_cancel
	$(BUILD_DIR)/benchmarks/bench_multi_symbol
	$(BUILD_DIR)/benchmarks/bench_snapshot

## bench-baseline: run benches; update docs/benchmark-results/baseline.json by hand.
bench-baseline: bench
	@echo "Update docs/benchmark-results/baseline.json with the numbers above."

## help: list available targets.
help:
	@grep -E '^## ' $(MAKEFILE_LIST) | sed 's/^## //'

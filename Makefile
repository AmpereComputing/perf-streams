CMAKE:=cmake
NINJA:=$(shell command -v ninja 2> /dev/null)
PYTHON:=python3
CLANG_FORMAT:=clang-format
CLANG_TIDY:=run-clang-tidy
RUFF:=ruff
HATCH:=hatch
GCOVR:=gcovr

BUILD_ARGS:=-j 0
BUILD_DIRECTORY_PREFIX=build-
DEFAULT_BUILD_TYPE=release
BUILD_TYPE:=$(DEFAULT_BUILD_TYPE)
BUILD_DIRECTORY?=$(BUILD_DIRECTORY_PREFIX)$(BUILD_TYPE)
PROJECT_DIR=$(realpath $(dir $(realpath $(lastword $(MAKEFILE_LIST)))))

export CMAKE_GENERATOR=Ninja
CMAKE_ARGS:=
CMAKE_CONFIGURE_ARGS=-D CMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_ARGS)

# Benchmark options
BENCHMARK_BUILD_DIRECTORY?=build-benchmark
BENCHMARK_OUTPUT?=$(BENCHMARK_BUILD_DIRECTORY)/benchmark.json
BENCHMARK_ARGS?=
BASELINE?=
CANDIDATE?=$(BENCHMARK_OUTPUT)

# Testing options
CTEST_BIN:=ctest
CTEST_OPTIONS:=--output-on-failure
CTEST=$(CTEST_BIN) $(CTEST_OPTIONS)
COVERAGE_BUILD_TYPE:=coverage

# Linting options
FIND_SRCS=git ls-files --exclude-standard --deduplicate
CPP_FILES='*.c' '*.cc' '*.h' '*.hpp'
PY_FILES='*.py'

MAKEFLAGS += --quiet --no-print-directory

default: install

.PHONY: .force
.force:;

.PHONY: configure
configure:
	mkdir -p $(BUILD_DIRECTORY)
	cd $(BUILD_DIRECTORY) && ( [ -f CMakeCache.txt ] || $(CMAKE) $(CMAKE_CONFIGURE_ARGS) -S .. )

.PHONY: .build
.build:
	$(NINJA) -C $(BUILD_DIRECTORY) $(BUILD_ARGS) $(TARGET)

build-%: .force
	$(MAKE) configure BUILD_TYPE=$*
	$(MAKE) .build BUILD_TYPE=$*

.PHONY: build
build: build-$(DEFAULT_BUILD_TYPE)

.PHONY: debug
debug: build-debug

.PHONY: clang
clang: .force
	CC=clang CXX=clang++ $(MAKE) build-$(DEFAULT_BUILD_TYPE)-clang CMAKE_ARGS="-D CLANG_TIDY=OFF"

install-%: .force
	$(MAKE) build-$* TARGET=install

.PHONY: install
install: install-$(DEFAULT_BUILD_TYPE)

.PHONY: .test
.test:
	cd $(BUILD_DIRECTORY) && $(CTEST) $(CTEST_ARGS)

test-%: .force
	$(MAKE) install-$*
	$(MAKE) .test BUILD_TYPE=$*

.PHONY: test
test: test-$(DEFAULT_BUILD_TYPE)

.PHONY: coverage
coverage:
	$(MAKE) build-$(COVERAGE_BUILD_TYPE)

.PHONY: cpp-coverage-report
cpp-coverage-report:
	$(GCOVR) -j 0 --txt --sort uncovered-number 2>/dev/null

.PHONY: .python-coverage
.python-coverage:
	$(PYTHON) -m coverage combine -q --keep $$(echo $$(find $(BUILD_DIRECTORY_PREFIX)$(COVERAGE_BUILD_TYPE) -name '.coverage*' -type f))

.PHONY: python-coverage-report
python-coverage-report: .python-coverage
	$(PYTHON) -m coverage report

.PHONY: .consolidate-coverage
.consolidate-coverage:
	$(PYTHON) -m coverage xml -q -o python_coverage.xml
	$(GCOVR) -j 0 --xml cpp_coverage.xml 2>/dev/null
	$(GCOVR) -j 0 $(GCOVR_ARGS) --filter .*.py \
		--cobertura-add-tracefile cpp_coverage.xml \
		--cobertura-add-tracefile python_coverage.xml 2>/dev/null

.PHONY: coverage-report
coverage-report: .python-coverage
	$(MAKE) .consolidate-coverage GCOVR_ARGS="--txt --sort uncovered-number"

.PHONY: xml-coverage-report
xml-coverage-report: .python-coverage
	$(MAKE) .consolidate-coverage GCOVR_ARGS="-s --xml coverage.xml"

.require-clean:
	git diff HEAD --quiet || ( echo "Run in a clean tree" && exit 1 )

.PHONY: .clang-format
.clang-format:
	$(FIND_SRCS) $(CPP_FILES) | xargs $(CLANG_FORMAT) -i

.PHONY: clang-format
clang-format: .require-clean
	$(MAKE) .clang-format

.PHONY: clang-format-check
clang-format-check:
	$(FIND_SRCS) $(CPP_FILES) | xargs $(CLANG_FORMAT) -n -Werror

.PHONY: .clang-tidy
.clang-tidy: clang
	! $(CLANG_TIDY) -hide-progress -config-file .clang-tidy-required \
		-source-filter "$(PROJECT_DIR)/src/.*.cc" \
		-header-filter "$(PROJECT_DIR)/src/.*.h" \
		-p $(BUILD_DIRECTORY) \
		-j 0 $(CLANG_TIDY_ARGS) \
		| grep --line-buffered -v 'Enabled checks' \
		| grep --line-buffered -ve '^    [a-z-]\+$$' \
		| grep --line-buffered -v '^$$' \
		| grep --line-buffered -v "warnings generated." \
		| grep --line-buffered -v "Suppressed" \
		| grep --line-buffered -v "errors from all non-system headers"

.PHONY: clang-tidy
clang-tidy: .require-clean
	$(MAKE) .clang-tidy CLANG_TIDY_ARGS="-fix -format" BUILD_TYPE=$(DEFAULT_BUILD_TYPE)-clang

.PHONY: clang-tidy-check
clang-tidy-check:
	$(MAKE) .clang-tidy BUILD_TYPE=$(DEFAULT_BUILD_TYPE)-clang

.PHONY: .python-format
.python-format:
	$(FIND_SRCS) $(PY_FILES) | xargs -r $(RUFF) check --select I --fix
	$(FIND_SRCS) $(PY_FILES) | xargs -r $(RUFF) format

.PHONY: python-format
python-format: .require-clean
	$(MAKE) .python-format

.PHONY: python-format-check
python-format-check:
	$(FIND_SRCS) $(PY_FILES) | xargs -r $(RUFF) check --select I
	$(FIND_SRCS) $(PY_FILES) | xargs -r $(RUFF) format --check

.PHONY: .python-lint
.python-lint:
	$(FIND_SRCS) $(PY_FILES) | xargs -r $(RUFF) check --fix

.PHONY: python-lint
python-lint: .require-clean
	$(MAKE) .python-lint

.PHONY: python-lint-check
python-lint-check:
	$(FIND_SRCS) $(PY_FILES) | xargs -r $(RUFF) check

.PHONY: .lint
.lint:
	$(MAKE) .clang-format
	$(MAKE) .python-lint
	$(MAKE) .python-format

.PHONY: lint
lint: .require-clean
	$(MAKE) .lint

.PHONY: .lint-all
.lint-all:
	$(MAKE) .lint
	$(MAKE) clang-tidy

.PHONY: lint-all
lint-all: .require-clean
	$(MAKE) .lint-all

.PHONY: check
check:
	$(MAKE) clang-format-check
	$(MAKE) python-lint-check
	$(MAKE) python-format-check

.PHONY: check-all
check-all: check
	$(MAKE) clang-tidy-check

.PHONY: benchmark
benchmark:
	mkdir -p $(BENCHMARK_BUILD_DIRECTORY)
	cd $(BENCHMARK_BUILD_DIRECTORY) && $(CMAKE) -D CMAKE_BUILD_TYPE=$(BUILD_TYPE) -D PERF_STREAMS_BUILD_BENCHMARKS=ON $(CMAKE_ARGS) -S ..
	$(NINJA) -C $(BENCHMARK_BUILD_DIRECTORY) $(BUILD_ARGS) perf_streams_benchmarks
	$(BENCHMARK_BUILD_DIRECTORY)/perf_streams_benchmarks --benchmark_out=$(BENCHMARK_OUTPUT) --benchmark_out_format=json $(BENCHMARK_ARGS)

.PHONY: benchmark-compare
benchmark-compare:
	$(PYTHON) benchmarks/compare_benchmarks.py --baseline "$(BASELINE)" --candidate "$(CANDIDATE)"

.PHONY: package
package:
	$(HATCH) build

.PHONY: package-test
package-test:
	$(HATCH) test -vv

.PHONY: .clean
.clean:
	rm -rf $(BUILD_DIRECTORY)

clean-%: .force
	$(MAKE) .clean BUILD_TYPE=$*

.PHONY: clean
clean: clean-$(DEFAULT_BUILD_TYPE)

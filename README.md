# Performance Streams

Performance "Streams" are traces or collections of events or other messages,
used for CPU performance simulation and analysis.

Currently the following streams are supported, each with their own collection
of tools and interfaces:

* Events (`event_stream`): contains events which occur as output of a performance simulation
* Instructions: (`instruction_stream`): a trace of instructions and other metadata from a workload, used as input to a performance simulation
* Memory (`memory_stream`): memory accesses from a workload, used as input to a performance simulation
* Branches (`branch_stream`): branches from a workload, used as input to a performance simulation

## Building

    make

## Testing

    make test

To get coverage, you can run:

    make test-coverage
    make coverage-report

## Formatting & Linting

To check:
    
    make check

To fix all auto-fixable issues:

    make lint

Some checks are behind `make check-all`, which similarily can be fixed with
`make lint-all`.

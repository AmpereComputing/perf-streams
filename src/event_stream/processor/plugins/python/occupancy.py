# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

"""Occupancy helpers."""

from collections import Counter, defaultdict
from collections.abc import Callable
from contextlib import ExitStack
from pathlib import Path

import evp

from perf_streams.event_stream import Event, EventScalarValue
from perf_streams.processor.histogram import Histogram


class OccupancyTracker:
    """Occupancy tracking helper."""

    def __init__(self, alloc_event: str, dealloc_event: str, *, collect_average: bool = False) -> None:
        """Track occupancy based on matching allocation and deallocation events."""
        self.entries: Counter[str] = Counter()
        self.occupancy_time: Counter[str] = Counter()
        self.last_time: Counter[str] = Counter()
        self.earliest_time = 0
        self.latest_time = 0
        self.collect_average = collect_average

        evp.on(alloc_event, self._allocate)
        evp.on(dealloc_event, self._deallocate)

    def _allocate(self, event: Event) -> None:
        self._update_occupancy(event, 1)

    def _deallocate(self, event: Event) -> None:
        self._update_occupancy(event, -1)

    def _update_time(self, name: str, time: int) -> int:
        duration = time - self.last_time.get(name, 0)
        self.last_time[name] = time
        return duration

    def _update_occupancy(self, event: Event, inc: int, name: str | None = None) -> tuple[int, int, int | float]:
        name = name or event.name
        average = None
        duration = self._update_time(name, event.time)
        if self.collect_average:
            self.occupancy_time[name] += self.entries[name] * duration
            if self.earliest_time == 0 or event.time < self.earliest_time:
                self.earliest_time = event.time
            self.latest_time = max(self.latest_time, event.time)
            if self.latest_time != self.earliest_time:
                average = self.occupancy_time[name] / (self.latest_time - self.earliest_time)

        self.entries[name] = max(self.entries[name] + inc, 0)
        occupancy = self.entries[name]
        return occupancy, duration, average or occupancy


class Occupancy(OccupancyTracker):
    """Occupancy on each transition as CSV."""

    def __init__(  # noqa: PLR0913
        self,
        alloc_event: str,
        dealloc_event: str,
        output: str,
        name_col: str,
        entries_col: str,
        *,
        rename: Callable[[str], str] | None = None,
        group_on: str | None = None,
        report_average: bool = False,
    ) -> None:
        """Write occupancy transitions to CSV as matching events are processed."""
        self.report_average = report_average
        self.group_on = group_on
        self.active_events: set[EventScalarValue] = set()
        self._output_context = ExitStack()
        self.output = self._output_context.enter_context(Path(output).open("w"))  # noqa: SIM115
        self.rename = rename
        if self.report_average:
            self.output.write(f"time,{name_col},{entries_col},{entries_col} Running Average\n")
        else:
            self.output.write(f"time,{name_col},{entries_col}\n")

        super().__init__(alloc_event, dealloc_event, collect_average=report_average)
        evp.end_simulation(self._end_simulation)

    def _allocate(self, event: Event) -> None:
        if self.group_on is not None and self.group_on in event.data:
            assert event.data[self.group_on] not in self.active_events, (
                f"Error: duplicate allocate event for id: {event.data[self.group_on]}"
            )
            self.active_events.add(event.data[self.group_on])
        super()._allocate(event)

    def _deallocate(self, event: Event) -> None:
        if self.group_on is not None and self.group_on in event.data:
            if event.data[self.group_on] in self.active_events:
                super()._deallocate(event)
                self.active_events.discard(event.data[self.group_on])
        else:
            super()._deallocate(event)

    def _update_occupancy(self, event: Event, inc: int, name: str | None = None) -> None:
        name = self._rename(name or event.name)
        occupancy, _duration, average = super()._update_occupancy(event, inc, name=name)
        self._record(event.time, name, occupancy, average)

    def _rename(self, event_name: str) -> str:
        return self.rename(event_name) if self.rename is not None else event_name

    def _record(self, time: int, name: str, occupancy: int, average: float) -> None:
        if self.report_average:
            self.output.write(f"{time},{name},{occupancy},{average}\n")
        else:
            self.output.write(f"{time},{name},{occupancy}\n")

    def _end_simulation(self) -> None:
        self._output_context.close()


class OccupancyHistogram(OccupancyTracker):
    """Occupancy histogram, emits histogram of cycles within each occupancy."""

    def __init__(
        self,
        alloc_event: str,
        dealloc_event: str,
        metric_name: str,
        factor_name: str | None = None,
        histogram_settings: Histogram | None = None,
    ) -> None:
        """Collect occupancy duration histograms keyed by occupancy level."""
        self._name = metric_name
        self._factor_name = factor_name
        self._histogram: defaultdict[str, Counter[int]] = defaultdict(Counter)
        self._bucketer = histogram_settings

        super().__init__(alloc_event, dealloc_event)
        evp.collect(self.metrics)

    def _update_occupancy(self, event: Event, inc: int, name: str | None = None) -> None:
        name = name or ".".join(event.name.split(".")[:-1])
        prev_occupancy = self.entries[name]
        _occupancy, duration, _average = super()._update_occupancy(event, inc, name=name)
        self._histogram[name][prev_occupancy] += duration

    def adjust_buckets(self, values: dict[int, int]) -> dict[int, int]:
        """Apply optional histogram bucketing to occupancy counts."""
        if not self._bucketer:
            return values
        return self._bucketer(values)

    def bucket_name(self, name: str, occurences: int) -> str:
        """Build the metric name for a histogram bucket."""
        if self._factor_name:
            return f"{name}.{self._name}/{self._factor_name}:{occurences}"
        return f"{name}.{self._name}.{occurences}"

    def metrics(self, time: int) -> dict[str, int]:
        """Return the occupancy histogram metrics at the given time."""
        all_metrics: dict[str, int] = {}
        for event, histogram in self._histogram.items():
            histogram[self.entries[event]] += self._update_time(event, time)
            all_metrics.update(
                {
                    self.bucket_name(event, occurences): count
                    for occurences, count in self.adjust_buckets(histogram).items()
                }
            )

        return all_metrics

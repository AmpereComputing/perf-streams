# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

"""Histogram based upon event data as key."""

import evp
from perf_streams.event_stream import Event, EventScalarValue
from perf_streams.processor.histogram import Histogram


class EventHistogramPerKey:
    """Track per-key occurrence histograms for selected events."""

    class Occurences:
        """Histogram of occurrences for a single event."""

        def __init__(self) -> None:
            """Initialize empty histograms for active and completed keys."""
            self._histogram: dict[int, int] = {}
            self._total_histogram: dict[int, int] = {}

        @classmethod
        def _increment_histogram(cls, key: int, values: dict[int, int]) -> None:
            if key not in values:
                values[key] = 1
            else:
                values[key] += 1

        @property
        def totals(self) -> dict[int, int]:
            """Return the histogram of completed occurrence counts."""
            return self._total_histogram

        def begin(self, key: int) -> None:
            """Start tracking occurrences for a key."""
            self.stop(key)
            self._histogram[key] = 0

        def event(self, key: int) -> None:
            """Record one occurrence for an active key."""
            self._increment_histogram(key, self._histogram)

        def stop(self, key: int) -> None:
            """Stop tracking a key and store its completed count."""
            occurences = self._histogram.pop(key, None)
            if occurences is not None:
                self._increment_histogram(occurences, self._total_histogram)

        def stop_all(self) -> None:
            """Flush all active keys into the completed histogram."""
            for occurences in self._histogram.values():
                self._increment_histogram(occurences, self._total_histogram)
            self._histogram.clear()

    def __init__(  # noqa: PLR0913
        self,
        metric_name: str,
        events: list[str] | str,
        key_data_types: list[str] | str | None = None,
        start_event: str | None = None,
        stop_event: str | None = None,
        histogram_settings: Histogram | None = None,
    ) -> None:
        """Counts occurrences of events by some 'key' with optional start/stop events.

        Args:
            metric_name: Name for metric(s)
            events: Events to count occurrences (for each key)
            key_data_types: Data types to use as "key" on each event
            start_event: Start event (start looking for occurences for key)
            stop_event: Stop event (stop looking for occurences for key)
            histogram_settings: Optional histogram bucketing configuration
        """

        self._name = metric_name
        self._events: dict[str, EventHistogramPerKey.Occurences] = {}
        self._key_types = [key_data_types] if isinstance(key_data_types, str) else key_data_types
        self._key_names: list[str] | None = None if self._key_types else []
        self._bucketer = histogram_settings

        events = [events] if isinstance(events, str) else events
        for event in events:
            evp.on(event, self._event)

        if start_event:
            evp.on(start_event, self._start)

        if stop_event:
            evp.on(stop_event, self._stop)

        evp.collect(self.metrics)
        evp.end_simulation(self.end)

    def hash_key(self, data: dict[str, EventScalarValue]) -> int:
        """Build a stable hash key from the configured event data fields."""
        if self._key_names is None:
            self._key_names = [name for name in data if any(key_type in name for key_type in self._key_types)]
        return hash(tuple(data[name] for name in self._key_names))

    def _occurences(self, event: Event) -> "EventHistogramPerKey.Occurences":
        if event.name not in self._events:
            self._events[event.name] = self.Occurences()
        return self._events[event.name]

    def _event(self, event: Event) -> None:
        key = self.hash_key(event.data)
        self._occurences(event).event(key)

    def _start(self, event: Event) -> None:
        key = self.hash_key(event.data)
        for occurences in self._events.values():
            occurences.begin(key)

    def _stop(self, event: Event) -> None:
        key = self.hash_key(event.data)
        for occurences in self._events.values():
            occurences.stop(key)

    def adjust_buckets(self, values: dict[int, int]) -> dict[int, int]:
        """Apply optional histogram bucketing to occurrence counts."""
        if not self._bucketer:
            return values
        return self._bucketer(values)

    def metrics(self, time: int) -> dict[str, int]:
        """Return the collected metrics for the current histograms."""
        all_metrics: dict[str, int] = {}
        for event, histogram in self._events.items():
            all_metrics.update(
                {
                    f"{event}.{self._name}.{occurences}": count
                    for occurences, count in self.adjust_buckets(histogram.totals).items()
                }
            )

        return all_metrics

    def end(self) -> None:
        """Flush all in-progress keys at end of simulation."""
        for occurences in self._events.values():
            occurences.stop_all()

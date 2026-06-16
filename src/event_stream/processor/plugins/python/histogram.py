# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

"""Histogram bucketer."""

import enum
import math
from collections import Counter

SPEC_PART_COUNT = 4


class HistogramSequence(enum.StrEnum):
    """Bucket distribution modes for histograms."""

    LINEAR = enum.auto()
    EXPONENTIAL = enum.auto()


class Histogram:
    """Bucketer for grouping integer counts into coarser histogram buckets."""

    def __init__(
        self,
        minimum: int | None = None,
        maximum: int | None = None,
        granularity: int | None = None,
        sequence: HistogramSequence | None = None,
    ) -> None:
        """Histogram bucketer and associated settings.

        Args:
            minimum: Minimum bucket value
            maximum: Maximum bucket value
            granularity: Size of buckets (values rounded down via ceiling)
            sequence: How granularity is used, i.e. linear or exponential
        """

        self._minimum = minimum
        self._maximum = maximum
        self._granularity = granularity
        self._sequence = sequence or HistogramSequence.LINEAR

    @classmethod
    def from_spec(cls, spec_string: str) -> "Histogram":
        """Create from spec similar to +count (i.e. min:max:granularity:sequence)."""

        def convert_part(part: str) -> int | HistogramSequence | str | None:
            try:
                return int(part)
            except ValueError:
                for seq in HistogramSequence:
                    if part.lower() in seq.lower():
                        return seq
                return part or None

        parts = [convert_part(part) for part in spec_string.split(":")]
        if len(parts) < SPEC_PART_COUNT:
            parts += [None] * (SPEC_PART_COUNT - len(parts))

        return cls(*parts)

    def _adjust(self, value: int) -> int:
        if self._granularity is not None:
            if self._sequence == HistogramSequence.LINEAR:
                value = value // self._granularity * self._granularity
            elif self._sequence == HistogramSequence.EXPONENTIAL:
                value = 0 if value <= 0 else (self._granularity ** int(math.log(value, self._granularity)))

        if self._minimum is not None:
            value = max(value, self._minimum)
        if self._maximum is not None:
            value = min(value, self._maximum)

        return value

    def __call__(self, values: dict[int, int]) -> Counter[int]:
        """Bucket the provided histogram values."""
        adjusted_values = Counter()
        for bucket, count in values.items():
            adjusted = self._adjust(bucket)
            adjusted_values[adjusted] += count

        return adjusted_values

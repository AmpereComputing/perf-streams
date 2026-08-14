# Copyright (c) 2026, Ampere Computing LLC
# SPDX-License-Identifier: BSD-3-Clause

"""Event Stream reader utilities."""

import json
from collections.abc import Callable, Iterable, Iterator
from dataclasses import dataclass
from enum import Enum
from typing import TYPE_CHECKING, cast

import perf_streams.event_stream_pb2 as es_proto
from perf_streams.extensions.transactions import Transactions

if TYPE_CHECKING:
    from src.protobuf_utils.protobuf_stream import ProtobufStreamReader, ProtobufStreamWriter
else:
    from perf_streams.protobuf_stream import ProtobufStreamReader, ProtobufStreamWriter

protobuf_es_magic = 0x53454250  # 0x50(P) 0x42(B) 0x45(E) 0x53(S)
protobuf_es_version = 4  # synchronize with event_stream_proto.h
MIN_VERSION_WITH_PRE_EVENT_DEFINITIONS = 2

type JsonScalar = bool | int | float | str | None
type JsonValue = JsonScalar | list[JsonValue] | tuple[JsonValue, ...] | dict[str, JsonValue]
type EventConstructor = Callable[[es_proto.Event, "EventStreamReader"], "Event"]


@dataclass(frozen=True)
class EventType:
    """Metadata describing an event definition."""

    id: int
    name: str
    description: str = ""


@dataclass(frozen=True)
class ValueType:
    """Metadata describing an event value definition."""

    id: int
    name: str
    description: str = ""
    enumeration_id: int | None = None


EventScalarValue = Enum | int | str
EventValues = dict[ValueType, EventScalarValue]
EnumerationValues = dict[int, str]
EnumerationSource = EnumerationValues | type[Enum]


class Event:
    """Python-only definition of Event (either created from values or
    converted from protobuf objects via from_event).
    """

    def __init__(
        self,
        *,
        definition_id: int,
        event_id: int,
        time: int,
        name: str,
        data: dict[str, EventScalarValue] | None = None,
    ) -> None:
        """Create an event wrapper from explicit event fields."""
        self.definition_id = definition_id
        self.id = event_id
        self.time = time
        self.name = name
        self.data = data or {}

    @classmethod
    def from_event(cls, event: es_proto.Event, reader: "EventStreamReader") -> "Event":
        """Build an `Event` from a protobuf event message."""

        def as_data_value(value: es_proto.Value) -> EventScalarValue:
            data_value = getattr(value, value.WhichOneof("values"))
            return reader.convert_enumeration_value(value.definition_id, data_value)

        return cls(
            definition_id=event.definition_id,
            event_id=event.id,
            time=event.time,
            name=reader.definition_names[event.definition_id],
            data={reader.definition_names[value.definition_id]: as_data_value(value) for value in event.values},
        )

    def __repr__(self) -> str:
        """String representation of event."""
        s = f"id={self.id} time={self.time} name={self.name}"
        for k, v in self.data.items():
            s += f" {k}={v}"
        return s


class Transaction:
    """Notion of a transaction (start/end with some hierarchy and associated events) built off of events."""

    DEFAULT_TXID_NAME = "txid"  # Field which indicates what transaction event is associated with
    DEFAULT_START_NAME = "start_transaction"  # Event which "starts" a transaction
    DEFAULT_END_NAME = "end_transaction"  # Event which "ends" a transaction
    DEFAULT_PARENT_NAME = "parent"  # Field which indicates what the parent is (on start event)

    def __init__(self, txid: int) -> None:
        """Create a Transaction with the specified txid."""
        self.events: list[es_proto.Event] = []
        self.values: dict[str, int | str] = {}
        self.children: list[Transaction] = []
        self.txid = txid
        self.parent: Transaction | None = None
        self.closed = False

    def add_child(self, transaction: "Transaction") -> None:
        """Add a transaction as a child."""
        self.children.append(transaction)
        transaction.parent = self

    def add_event(self, event: es_proto.Event, value_fields: list[str] | None = None) -> None:
        """Add an event to this transaction."""
        self.events.append(event)

        if value_fields:
            for field in value_fields:
                if field in event.data:
                    self.values[field] = event.data[field]

    def all_events(self, *, include_children: bool = False) -> Iterator[es_proto.Event]:
        """Generate events from this and all child transactions."""
        yield from self.events

        if include_children:
            for transaction in self.children:
                yield from transaction.all_events(include_children=include_children)


class Enumeration(dict[int, str]):
    """Python-only definition of Enumeration (for writer-facing APIs)."""

    def __init__(self, enumeration_id: int, values: dict[int, str]) -> None:
        """Store enumeration values together with the assigned enumeration id."""
        super().__init__(values)
        self.enumeration_id = enumeration_id


class _EventStreamWriterBase(ProtobufStreamWriter):
    """Core protobuf-based EventStream writer implementation."""

    def __init__(self, filename: str) -> None:
        """Create EventStream writer.

        Args:
            filename: filename of resulting event stream
        """
        super().__init__(filename, protobuf_es_magic, protobuf_es_version)

        self.definitions: dict[int, es_proto.Definition] = {}  # keyed by definition id
        self.definition_names: dict[int, str] = {}  # keyed by definition id
        self.definition_name_to_id: dict[tuple[int, str], int] = {}  # keyed by (kind, name)
        self.enumerations: dict[int, EnumerationValues] = {}  # keyed by enumeration id
        self.parameters: dict[str, es_proto.Parameter] = {}  # keyed by parameter name
        self.simulation_started = False

        self._next_definition_id = 0
        self._next_event_id = 0
        self._next_enumeration_id = 0

    def _assert_preamble(self, operation: str) -> None:
        if self.simulation_started:
            raise RuntimeError(f"{operation} is not allowed after START_SIMULATION")

    def _assert_event_phase(self, operation: str) -> None:
        if not self.simulation_started:
            raise RuntimeError(f"{operation} is only allowed after START_SIMULATION")

    def _resolve_event_type(self, event_type: EventType) -> EventType:
        if not isinstance(event_type, EventType):
            raise TypeError(f"event_type must be EventType, but was {type(event_type).__name__}")

        definition = self.definitions.get(event_type.id)
        if definition and definition.kind == es_proto.Kind.Value("EVENT"):
            return EventType(id=definition.id, name=definition.name, description=definition.description)
        raise KeyError(f"Unknown event definition id: {event_type.id}")

    def _resolve_value_type(self, value_type: ValueType) -> ValueType:
        if not isinstance(value_type, ValueType):
            raise TypeError(f"value_type must be ValueType, but was {type(value_type).__name__}")

        definition = self.definitions.get(value_type.id)
        if definition and definition.kind == es_proto.Kind.Value("VALUE"):
            enumeration_id = definition.enumeration_id if definition.HasField("enumeration_id") else None
            return ValueType(
                id=definition.id,
                name=definition.name,
                description=definition.description,
                enumeration_id=enumeration_id,
            )
        raise KeyError(f"Unknown value definition id: {value_type.id}")

    def _definition_matches(
        self, definition: es_proto.Definition, description: str, enumeration_id: int | None = None
    ) -> bool:
        if definition.description != description:
            return False
        if enumeration_id is None:
            return not definition.HasField("enumeration_id")
        return cast("bool", definition.HasField("enumeration_id")) and definition.enumeration_id == enumeration_id

    @staticmethod
    def _encode_value(value: EventScalarValue) -> tuple[str, int | str]:
        if isinstance(value, Enum):
            value = value.value

        if isinstance(value, bool):
            raise TypeError("Boolean event values are not supported; use integer 0 or 1")

        if isinstance(value, str):
            return ("string_value", value)

        if isinstance(value, int):
            if value < -(1 << 63):
                raise ValueError(f"Integer value below int64 range: {value}")
            if value > ((1 << 64) - 1):
                raise ValueError(f"Integer value above uint64 range: {value}")
            if value < 0:
                return ("int_value", value)
            return ("uint_value", value)

        raise TypeError(f"Unsupported Value type: {type(value).__name__}")

    def write(
        self, msg: es_proto.Definition | es_proto.Event | es_proto.Control | es_proto.Parameter | es_proto.Enumeration
    ) -> None:
        """Write protobuf message to stream."""
        record = es_proto.Record()
        if isinstance(msg, es_proto.Definition):
            record.definition.CopyFrom(msg)
        elif isinstance(msg, es_proto.Event):
            record.event.CopyFrom(msg)
        elif isinstance(msg, es_proto.Control):
            record.control.CopyFrom(msg)
        elif isinstance(msg, es_proto.Parameter):
            record.parameter.CopyFrom(msg)
        elif isinstance(msg, es_proto.Enumeration):
            record.enumeration.CopyFrom(msg)
        else:
            raise Exception(f"EventStreamWriter.write called with unexpected message type: {type(msg).__name__}")
        super().write(record)

    def define_event(self, name: str, description: str) -> EventType:
        """Define event.

        Args:
            name: event name
            description: event description
        """
        self._assert_preamble("define_event")

        if not isinstance(name, str) or not name:
            raise ValueError("Definition name must be a non-empty string")

        kind = es_proto.Kind.Value("EVENT")
        key = (kind, name)
        existing_id = self.definition_name_to_id.get(key)
        if existing_id is not None:
            definition = self.definitions[existing_id]
            if not self._definition_matches(definition, description):
                raise ValueError(f"Definition {name} already exists with different attributes")
            return EventType(id=definition.id, name=definition.name, description=definition.description)

        definition_id = self._next_definition_id
        self._next_definition_id += 1

        definition = es_proto.Definition()
        definition.kind = kind
        definition.id = definition_id
        definition.name = name
        definition.description = description

        self.write(definition)
        self.definitions[definition_id] = definition
        self.definition_names[definition_id] = name
        self.definition_name_to_id[key] = definition_id
        return EventType(id=definition_id, name=name, description=description)

    def define_enumeration(self, values: EnumerationSource) -> Enumeration:
        """Define enumeration.

        Args:
            values: enumeration value-to-string mapping
        """
        self._assert_preamble("define_enumeration")

        if isinstance(values, type) and issubclass(values, Enum):
            candidate_values = {member.value: member.name for member in values}
        elif isinstance(values, dict):
            candidate_values = values
        else:
            raise TypeError(f"values must be dict[int, str] or Enum subclass, but was {type(values).__name__}")

        validated_values: EnumerationValues = {}
        for key, value in candidate_values.items():
            if not isinstance(key, int):
                raise TypeError(f"Enumeration key must be int, but was {type(key).__name__}")
            if not isinstance(value, str):
                raise TypeError(f"Enumeration value must be str, but was {type(value).__name__}")
            validated_values[key] = value

        if not validated_values:
            raise ValueError("Enumeration values must be non-empty")

        enumeration_id = self._next_enumeration_id
        self._next_enumeration_id += 1

        enumeration = es_proto.Enumeration()
        enumeration.id = enumeration_id
        enumeration.values.update(validated_values)
        self.write(enumeration)
        self.enumerations[enumeration_id] = validated_values
        return Enumeration(enumeration_id, validated_values)

    def define_data(
        self,
        name: str,
        description: str,
        enumeration: Enumeration | None = None,
    ) -> ValueType:
        """Define data.

        Args:
            name: name of data
            description: description of data
            enumeration: optional definition of enumeration (if data uses an enumeration)
        """
        self._assert_preamble("define_data")

        if not isinstance(name, str) or not name:
            raise ValueError("Definition name must be a non-empty string")

        enumeration_id = None
        if enumeration is not None:
            if not isinstance(enumeration, Enumeration):
                raise TypeError(f"enumeration must be Enumeration, but was {type(enumeration).__name__}")

            enumeration_id = enumeration.enumeration_id
            known_values = self.enumerations.get(enumeration_id)
            if known_values is None:
                raise KeyError(f"Unknown enumeration id: {enumeration_id}")
            if known_values != dict(enumeration):
                raise ValueError(f"Enumeration values mismatch for id: {enumeration_id}")

        kind = es_proto.Kind.Value("VALUE")
        key = (kind, name)
        existing_id = self.definition_name_to_id.get(key)
        if existing_id is not None:
            definition = self.definitions[existing_id]
            if not self._definition_matches(definition, description, enumeration_id):
                raise ValueError(f"Definition {name} already exists with different attributes")
            existing_enum = definition.enumeration_id if definition.HasField("enumeration_id") else None
            return ValueType(
                id=definition.id,
                name=definition.name,
                description=definition.description,
                enumeration_id=existing_enum,
            )

        definition_id = self._next_definition_id
        self._next_definition_id += 1

        definition = es_proto.Definition()
        definition.kind = kind
        definition.id = definition_id
        definition.name = name
        definition.description = description
        if enumeration_id is not None:
            definition.enumeration_id = enumeration_id

        self.write(definition)
        self.definitions[definition_id] = definition
        self.definition_names[definition_id] = name
        self.definition_name_to_id[key] = definition_id
        return ValueType(id=definition_id, name=name, description=description, enumeration_id=enumeration_id)

    def set_parameter(self, name: str, value: JsonValue, description: str = "") -> None:
        """Set parameter.

        Args:
            name: name of parameter
            value: value of parameter
            description: description of parameter
        """
        self._assert_preamble("set_parameter")

        if name in self.parameters:
            raise ValueError(f"Parameter already exists: {name}")

        parameter = es_proto.Parameter()
        parameter.name = name
        parameter.description = description

        if isinstance(value, bool):
            parameter.bool_value = value
        elif isinstance(value, int):
            if value < 0:
                parameter.int_value = value
            else:
                parameter.uint_value = value
        elif isinstance(value, float):
            parameter.double_value = value
        elif isinstance(value, str):
            parameter.string_value = value
        else:
            parameter.json_value = json.dumps(value, sort_keys=True)

        self.write(parameter)
        self.parameters[name] = parameter

    def _start_simulation_extensions(self) -> None:
        """Allow writer extensions to define pre-simulation records."""

    def start_simulation(self) -> None:
        """Start simulation (events can occur after this)."""
        self._assert_preamble("start_simulation")

        self._start_simulation_extensions()

        control = es_proto.Control()
        control.type = es_proto.CtrlType.Value("START_SIMULATION")
        self.write(control)
        self.simulation_started = True

    def post_event(
        self,
        event_type: EventType,
        time: int,
        values: EventValues | None = None,
    ) -> Event:
        """Post event.

        Args:
            event_type: event definition type
            time: time of event
            values: values associated with event (dict of data definition type with value)
        """
        self._assert_event_phase("post_event")

        if values is not None and not isinstance(values, dict):
            raise TypeError(f"values must be dict[ValueType, EventScalarValue], but was {type(values).__name__}")

        resolved_event_type = self._resolve_event_type(event_type)
        values = values or {}
        event_values: dict[int, tuple[ValueType, EventScalarValue]] = {}
        for value_type, raw_value in values.items():
            resolved_type = self._resolve_value_type(value_type)
            event_values[resolved_type.id] = (resolved_type, raw_value)

        event_proto = es_proto.Event()
        event_proto.definition_id = resolved_event_type.id
        event_proto.time = time
        event_proto.id = self._next_event_id
        self._next_event_id += 1

        event_data: dict[str, EventScalarValue] = {}
        for value_type, raw_value in event_values.values():
            value_proto = event_proto.values.add()
            value_proto.definition_id = value_type.id
            value_kind, normalized_value = self._encode_value(raw_value)
            setattr(value_proto, value_kind, normalized_value)
            event_data[value_type.name] = normalized_value

        self.write(event_proto)
        return Event(
            definition_id=resolved_event_type.id,
            event_id=event_proto.id,
            time=event_proto.time,
            name=resolved_event_type.name,
            data=event_data,
        )


def _event_stream_writer_extension_error(*extensions: type) -> str | None:
    """Return an extension validation error, or None when extensions are valid."""
    seen: set[type] = set()
    for extension in extensions:
        if not isinstance(extension, type):
            return f"EventStreamWriter extension must be a type, but was {type(extension).__name__}"

        requirements = getattr(extension, "requirements", ())
        if not isinstance(requirements, tuple):
            return f"{extension.__name__}.requirements must be a tuple[type, ...]"

        for requirement in requirements:
            if not isinstance(requirement, type):
                return f"{extension.__name__}.requirements must contain only types"
            if requirement not in seen:
                return (
                    f"{extension.__name__} requires {requirement.__name__}; "
                    "list required extensions before dependent extensions"
                )

        seen.add(extension)
    return None


def event_stream_writer_extensions_valid(*extensions: type) -> bool:
    """Return whether writer extensions have their requirements in listed order."""
    return _event_stream_writer_extension_error(*extensions) is None


def EventStreamWriterWith(*extensions: type) -> type[_EventStreamWriterBase]:  # noqa: N802
    """Create an EventStreamWriter class with the requested extensions."""
    error = _event_stream_writer_extension_error(*extensions)
    if error is not None:
        raise ValueError(error)

    if extensions:
        extension_names = "".join(extension.__name__ for extension in extensions)
        name = f"EventStreamWriterWith{extension_names}"
    else:
        name = "EventStreamWriterWith"

    return type(
        name,
        (*reversed(extensions), _EventStreamWriterBase),
        {"__module__": __name__, "__doc__": "Protobuf-based EventStream writer."},
    )


EventStreamWriter = EventStreamWriterWith(Transactions)
EventStreamWriter.__name__ = "EventStreamWriter"
EventStreamWriter.__qualname__ = "EventStreamWriter"


class EventStreamReader(ProtobufStreamReader):
    """Protobuf-based EventStream reader."""

    def __init__(self, filename: str, *, all_events: bool = False, convert_enumerations: bool = False):
        """Initialize an event stream for reading.

        Args:
            filename: event stream file to read
            all_events: read all events
            convert_enumerations: convert enumeration integer values to string
        """
        super().__init__(filename, protobuf_es_magic, protobuf_es_version)
        self.simulation_started = all_events
        self.convert_enumerations = convert_enumerations
        self.definitions: dict[int, es_proto.Definition] = {}  # keyed by definition id
        self.definition_names: dict[int, str] = {}  # keyed by definition id
        self.enumerations: dict[int, EnumerationValues] = {}  # keyed by enumeration id
        self.parameters: dict[str, es_proto.Parameter] = {}  # keyed by parameter name

    def _add_definition(self, definition: es_proto.Definition) -> None:
        self.definitions[definition.id] = definition
        self.definition_names[definition.id] = definition.name

    def _add_enumeration(self, enumeration: es_proto.Enumeration) -> None:
        self.enumerations[enumeration.id] = enumeration.values

    def _add_parameter(self, parameter: es_proto.Parameter) -> None:
        assert parameter.name not in self.parameters
        self.parameters[parameter.name] = parameter

    def find_definition_by_id(self, _id: int) -> es_proto.Definition:
        """Find definition by definition id."""
        return self.definitions[_id]

    def find_definition_by_name(self, name: str) -> es_proto.Definition | None:
        """Find definition by definition name."""
        for definition in self.definitions.values():
            if definition.name == name:
                return definition
        return None

    def find_parameter_by_name(self, name: str) -> es_proto.Parameter | None:
        """Find parameter by parameter name."""
        return self.parameters.get(name)

    def convert_enumeration_value(self, definition_id: int, value: EventScalarValue) -> EventScalarValue:
        """Convert (possible) enumeration value to string."""
        if isinstance(value, int) and self.convert_enumerations:
            definition = self.find_definition_by_id(definition_id)
            if definition is not None and definition.HasField("enumeration_id"):
                enumerations = self.enumerations.get(definition.enumeration_id)
                if enumerations is not None:
                    return enumerations.get(value, value)

        return value

    def read_event(self, event: es_proto.Event) -> bool:
        """Return the first unread event.

        Args:
            event: event to override (if present)

        Returns:
            status: read status
        """
        record = es_proto.Record()
        status = super().read(record)
        while status:
            if record.HasField("event") and self.simulation_started:
                break
            if record.HasField("definition"):
                self._add_definition(record.definition)
            elif record.HasField("parameter"):
                self._add_parameter(record.parameter)
            elif record.HasField("control"):
                if record.control.type == es_proto.CtrlType.Value("START_SIMULATION"):
                    self.simulation_started = True
            elif record.HasField("enumeration"):
                self._add_enumeration(record.enumeration)
            status = super().read(record)

        if status and self.simulation_started:
            event.CopyFrom(record.event)
        return status

    def _read_until_start_simulation(self) -> bool:
        record = es_proto.Record()
        status = super().read(record)
        while status:
            if record.HasField("definition"):
                self._add_definition(record.definition)
            elif record.HasField("parameter"):
                self._add_parameter(record.parameter)
            elif record.HasField("control"):
                if record.control.type == es_proto.CtrlType.Value("START_SIMULATION"):
                    self.simulation_started = True
                    break
            elif record.HasField("enumeration"):
                self._add_enumeration(record.enumeration)
            else:
                assert not record.HasField("event")

            status = super().read(record)

        return status

    def read_events(
        self,
        events: Iterable[str | Callable[[str], bool]],
        constructor: EventConstructor = Event.from_event,
    ) -> Iterable[Event]:
        """Read all matching events.

        Args:
            events (iterable of str or callable): event matchers or strings to read
            constructor (callable): Optional constructor for events

        Yields:
            Event: read event
        """
        assert self.version >= MIN_VERSION_WITH_PRE_EVENT_DEFINITIONS
        event_names = {event for event in events if isinstance(event, str)}
        event_matchers = [event for event in events if not isinstance(event, str)]

        # Read until START_SIMULATION and stop
        self._read_until_start_simulation()

        # We have seen all the definitions at this point, because we've read
        # the first event and we're using a version 2+ stream, so let's build a
        # set of the definitions
        matching_definitions = set()
        for def_id, definition in self.definitions.items():
            if definition.name in event_names or any(matcher(definition.name) for matcher in event_matchers):
                matching_definitions.add(def_id)

        record = es_proto.Record()
        status = super().read(record)
        while status:
            if record.event.definition_id in matching_definitions:
                yield constructor(record.event, self)
            status = super().read(record)

    def read_transactions(
        self,
        events: Iterable[str | Callable[[str], bool]],
        txid_field: str = Transaction.DEFAULT_TXID_NAME,
        parent_field: str = Transaction.DEFAULT_PARENT_NAME,
        value_fields: Iterable[str] | None = None,
    ) -> Iterable[Transaction]:
        """Yield transactions which are events grouped by txid_field.

        Args:
            events (iterable of str or callable): event matchers or strings to read
            txid_field (str, optional): TXID field name
            parent_field (str, optional): parent field name
            value_fields (iterable, optional): value fields to include in events

        Yields:
            Transaction: transaction with attached events
        """
        events = set(events)
        value_fields = list(value_fields or [])
        transactions_by_id: dict[int, Transaction] = {}
        parent_transactions: dict[int, Transaction] = {}

        for event in self.read_events(events):
            txid = event.data.get(txid_field)
            if txid is None:
                continue

            transaction_id = cast("int", txid)
            if transaction_id not in transactions_by_id:
                transactions_by_id[transaction_id] = Transaction(transaction_id)

            transaction = transactions_by_id[transaction_id]
            transaction.add_event(event, value_fields=value_fields)

            if parent_field in event.data:
                parent_id = cast("int", event.data[parent_field])
                if parent_id in transactions_by_id:
                    parent = transactions_by_id[parent_id]
                    parent_transactions[transaction_id] = parent
                    parent.add_child(transaction)

        for transaction in transactions_by_id.values():
            yield transaction

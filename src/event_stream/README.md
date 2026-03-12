# Event Stream

## Specification

### File Framing

An event stream file is a sequence of bytes with this layout:

1. 32-bit little-endian magic number (`0x53454250`, or "PBES" in little-endian
   byte order)
2. 32-bit little-endian stream version (currently `4`)
3. Zero or more length-delimited protobuf `Record` messages (this is the
   "interesting" part of the file)

Each delimited record is encoded as:

1. 32-bit little-endian message size in bytes
2. Serialized protobuf bytes for one `Record`

### Top-Level Record Model

Each stream element is a protobuf `Record` oneof with exactly one payload:

- `definition`: A `Definition`
- `event`: An `Event`
- `control`: A `Control`
- `parameter`: A `Parameter`
- `enumeration`: An `Enumeration`

### Core Entities and Relationships

#### Definitions

A `Definition` establishes a named ID. Each `Event` and `Value` refers to its
own `Definition` by `definition_id`. `Definition`s link `Events` and `Values`
of the same type, and provide a human-readable name and description for them.

**Fields:**

- `kind`: `EVENT` or `VALUE`
- `id`: integer ID (in one shared ID space for both event and value
  definitions)
- `name`: symbolic name
- `description`: text
- optional `enumeration_id`: points to an `Enumeration` when this definition is
  enum-valued

**Relationships:**

- `Event.definition_id` must refer to a `Definition` whose `kind == EVENT`
- `Value.definition_id` must refer to a `Definition` whose `kind == VALUE`
- If a `Definition` has `enumeration_id`, that ID refers to `Enumeration.id`

#### Enumerations

`Enumeration` maps integer values to display strings.

**Fields:**

- `id`: enumeration ID
- `values`: `map<int64, string>`

**Relationship:**

- Referenced by `Definition.enumeration_id`

**Interpretation behavior:**

- Event payload values remain numeric on disk.
- Readers may use an `Enumeration` relationship to map numeric values to
  strings when the associated `Value` definition contains an `enumeration_id`
  and the `Enumeration` with the matching `id` field is found.

#### Parameters

`Parameter` stores run/model metadata. This can be used to record the
configuration and state of the model's execution (including config files,
compilation settings, command-line arguments, etc.). This configuration can be
read both by humans and programs when it is helpful to understand the context
for the event stream.

**Fields:**

- `name`
- `description`
- `value` oneof: `bool_value`, `int_value`, `uint_value`, `double_value`,
  `string_value`, or `json_value`

**Relationship:**

- Parameters are independent metadata records. They are not linked by ID to
  events or definitions.

#### Controls

`Control` carries stream control markers.

For ordering semantics, the relevant control is:

- `START_SIMULATION`

**Relationship:**

The `START_SIMULATION` `Control` acts as a boundary marker between stream
preamble metadata and event records. All `Definition`, `Parameter` and
`Enumeration` records must be before `START_SIMULATION`. All `Event` records
must be after `START_SIMULATION`.

#### Events and Values

`Event` is a timestamped occurrence of an event.

**Fields:**

- `definition_id`: event type ID (`Definition.kind == EVENT`)
- `id`: event instance ID (monotonic counter in C++ writer)
- `time`: event timestamp (frequently assumed to be measured in picoseconds)
- `values[]`: repeated `Value`

**`Value` fields:**

- `definition_id`: value/data field ID (`Definition.kind == VALUE`)
- `values` oneof: `int_value`, `uint_value`, `string_value`

### Required Record Ordering

1. Preamble records before `START_SIMULATION`: any mix of
   - `Definition`
   - `Enumeration`
   - `Parameter`
2. A `Control(START_SIMULATION)` record
3. Event phase records (only `Event` records)

> **NOTE**: `Event` records are not allowed before the `START_SIMULATION`
> `Control` record - and no definitions are allowed after it!

### ID and Name Semantics

- `Definition.id` values are assigned by definition order in the C++ framework
  (1-based sequence).
- `Enumeration.id` values are assigned from the enumeration registry (commonly
  starting at 0).
- Event names and value names are reconstructed by looking up `definition_id`
  in `Definition` records.
- Value name collisions are not prevented by schema; readers generally key by
  definition ID, then map to names.

### Transaction Convention (not a Protobuf primitive)

Transactions are a convention established using other event stream protobuf
primitives, and not a dedicated record type.

#### Transaction fields

By convention, Readers and Writers of event streams use the following fields to
represent transactions:

- `txid` data field name (`txid_field="txid"`)
- `parent` data field name (`parent_field="parent"`)

These names are conventions in event payload data (i.e., event `Value`
entries), not schema-level reserved fields.

#### How to define transaction-capable streams

To encode transactions, first define value fields (as normal
`Definition(kind=VALUE)`) whose names include:

- `txid`: transaction identifier per event (required on every event in the
  transaction)
- `parent`: parent transaction identifier (optional, and only specified on the
  transaction's `start_transaction` event)

The naming convention for transaction event is as follows:
- An `start_transaction` `Event` specifies is the first event in a transaction.
  If this transaction has a parent, it contains a `parent` value matching the
  `txid` of its  parent transaction.
- The `end_transaction` `Event` specifies that no further events will occur
  with this `txid`.

#### Potential algorithm for linking related events using transactions

For each selected event:

1. Read event record.
2. If no `txid` value exists on the event, ignore event (for purposes of
transaction grouping).
3. Group events by `txid` (begin tracking a transaction the first time a new
txid is seen).
4. If event has `parent` and that parent transaction exists, link child to that
parent.

Link fields between transactions using a parent/child relationship:

- Child transaction key: `txid`
- Parent reference key: `parent` (value equals another transaction's `txid`)

#### Important caveat

Parent-child links are made only when the parent transaction has already been
seen in stream order. There is no deferred backpatch pass for
child-before-parent records.

#### Minimal Valid Example Sequence

    Record(definition=EVENT id=1 name="start_transaction")
    Record(definition=EVENT id=2 name="end_transaction")
    Record(definition=VALUE id=3 name="txid")
    Record(definition=VALUE id=4 name="parent")
    # (optional) Record(enumeration=...)
    # (optional) Record(parameter=...)
    Record(control=START_SIMULATION)
    Record(event definition_id=1 values: txid=100)
    Record(event definition_id=1 values: txid=101,parent=100)
    Record(event definition_id=2 values: txid=101)
    Record(event definition_id=2 values: txid=100)

This represents a transaction with transaction ID `100` and child transaction
`101`.

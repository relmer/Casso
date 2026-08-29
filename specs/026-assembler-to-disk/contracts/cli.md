# Contract: The Command Line

**Feature**: `026-assembler-to-disk` | **Date**: 2026-08-29

What the assembler grammars gain, how it is expressed, and what every refusal
says. The spellings below are the contract; the wording of a diagnostic is the
implementation's, but *which* condition earns one is fixed here.

## Where these flags live

Rows in `s_kAs65Flags` and `s_kMerlinFlags` (`CassoCore/CommandLineParser.cpp`),
which the parser walks and from which the help is generated. Adding a flag any
other way breaks the property those tables exist for: "the tool cannot document
a flag it does not take or take one it does not document."

**Each long flag also needs a row in the long-option list** — `--disk` under the
Windows prefix is `/disk`, and without a long-option entry the single-character
normalization reads it as the concatenated `-d -i -s -k`. The existing table
records exactly this hazard for `/flat` and it applies verbatim here.

All four are `FlagCategory::AssembledCode`. They shape what is assembled and
where it lands, which is that category, and they are not an output *format* —
the object's shape inside the file is still `--flat` / `--dos-bin`'s business.

## The flags

| Flag | Value | Meaning | FR |
|---|---|---|---|
| `--disk <image>` | filename, attached or separate | Write the object into this disk image instead of a host file. Its absence is what turns every behavior below off. | FR-001 |
| `--as <name>` | string | What the object is called on the volume. Overrides `DSK` and `SAV`. | FR-002, FR-007 |
| `--type <t>` | string | The filesystem type. Overrides `TYP`. Same accepted spellings as `disk put --type`. | FR-006, FR-007 |
| `--startup` | none | Make the object the volume's startup program. | FR-021 |

`--as` and `--type` are deliberately the same words `disk put` uses for the same
two ideas. The tree has been burned by the opposite: `--load`/`--exec`'s own
comment records "three names for two ideas, and one name for two of them" across
`run`, `disk create` and `disk put`. This does not add a fourth name.

**There is no `--load`.** That is the point of the feature. The load address
comes from the source's origin (FR-005), and a flag that could disagree with it
is the defect being fixed, not a convenience being withheld.

**There is no encoding flag.** `disk put` takes `--text` and `--basic`; an
assembler emits bytes, and a character conversion applied to an object would
corrupt it. Verbatim always.

## Precedence

Command line beats source directive, always. The rule is settled *by the
assembler*, which sees both, rather than by the parser, which sees one — this is
already the tree's stated position for `-o` versus `DSK`, recorded in
`ApplyMerlinDefaults`, and these follow it unchanged.

| Both given | Wins |
|---|---|
| `--as` and `DSK` / `SAV` | `--as` |
| `--type` and `TYP` | `--type` |
| Neither | Filesystem default: binary |

A directive that lost is not an error and earns no warning. A build script
pointing one source at several outputs without editing it is the reason the rule
exists.

**The name rule does not scale past one output, and the type rule does.**
`--as` and `-o` supply a single name; an assembly producing several outputs
while one is in force is refused, naming the flag and the count (FR-026).
Applying it to each output in turn would have each replace the last, reporting
success having written one file where the source asked for three. `--type` has
no such limit: one type applies to every output unambiguously.

This refusal is only knowable after the assembly, since nothing before pass 2
knows how many outputs there are. It is still a refusal: image untouched,
status 2.

## What is per-output, and what is per-assembly

Only the object is per-output (FR-028).

| Artifact | How many | Why |
|---|---|---|
| Object | one per save point | It is what a save point *is*. |
| Listing (`-l`) | one per assembly | It renders the whole source, including the lines after a save. |
| Symbol table (`-t`) | one per assembly | Symbols are global to the assembly. |
| Debug info (`-g`) | one per assembly | Addresses by name, likewise global. |

The flags settle this on their own: `-l<file>` and `-g` each name one file, and
there is no spelling for "one listing per save". Splitting them would also break
FR-004's reason for keeping them on the host — a debugger reads one listing for
the assembly it is stepping through.

## Refusals

Every one leaves the image byte-for-byte unchanged (FR-014, FR-015) and names
the condition it hit. All of them are **edge-layer verdicts**, never
`E_INVALIDARG`: these are user inputs, and `E_INVALIDARG` marks a coding error
and always asserts.

| Condition | FR |
|---|---|
| `--disk` names an image that does not exist. Says so, and names the command that creates one. | FR-018 |
| The image holds no recognized filesystem. | — |
| The image is held by another program. Already worded by `DiskImageSession::kInUseRefusalText`. | — |
| The volume has no room, or no free directory entry. | — |
| The name is illegal on the target filesystem — too long, or forbidden characters. | — |
| `--type`, or `TYP`, names a type with no counterpart on the target filesystem. Names both the type and the filesystem. | FR-010 |
| `--type`, or `TYP`, names a value outside the recognized set. Names the value. | FR-011 |
| `--startup` given with no `--disk`. | FR-023 |
| `--startup` on a volume whose operating system would not run the file. Judged by the same rules `disk boot` applies. | FR-022 |
| `--as` or `--type` given with no `--disk`. | — |

The last row is a judgment worth stating: a name or a type with nothing to apply
them to is a command line whose author believed something false about what was
about to happen. The tree's own precedent is `ParseRunOptions` — "EVERY
DIAGNOSTIC HERE IS ALSO A REFUSAL … Running anyway and reporting success told a
build script that a command line it got wrong had worked."

## Exit statuses

Unchanged, and deliberately so. `As65ExitStatus` maps a finished assembly onto
the three the tool speaks, and `AssemblerMode.h` records that "THE EXIT CODES
ARE NOT A PER-DIALECT DECISION". A failed write earns the same **2** as any
other failure to produce output, because the question a script asks is whether
it got a file.

| Status | Meaning here |
|---|---|
| 0 | Assembled cleanly and everything asked for was written. |
| 1 | Assembled with warnings; output written. |
| 2 | No output: a refused command line, a failed assembly, or a refused write. |

## The build loop, before and after

Before — three commands, and the third restates what the source already said:

```
CassoCli disk create mydisk.dsk --bootable
CassoCli as65 prog.a65 -oprog.bin
CassoCli disk put mydisk.dsk prog.bin --as PROG --type B --load $6000
```

After — two, and nothing is restated (SC-001):

```
CassoCli disk create mydisk.dsk --bootable
CassoCli as65 prog.a65 --disk mydisk.dsk --as PROG --startup
```

`--type` is absent from the second because binary is the default, and `--load`
is absent because it no longer exists. `--startup` is what folds the former
fourth command, `disk boot mydisk.dsk PROG`, into the assembly.

## What does not change

- Assembling with no `--disk` produces byte-for-byte the same host files as
  before (FR-016, SC-006).
- `disk put` stays, and remains the right tool for placing files the assembler
  did not produce.
- Listing, symbol table and debug info stay on the host, requested by their own
  flags, whether or not `--disk` is given (FR-004).

# Contract: The Command Line

**Feature**: `026-assembler-to-disk` | **Date**: 2026-08-29

What the assembler grammars gain, how it is expressed, and what every refusal
says. The forms below are the contract; the wording of a diagnostic is the
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
| `--type <t>` | string | The filesystem type. Overrides `TYP`. Same accepted forms as `disk put --type`. | FR-006, FR-007 |
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

**Every artifact splits per output, and each set is named from its output**
(FR-031, FR-032). One file holding several sections would make a reader hunting
for one program's code walk past the others, and there is no need for it: each
output already carries a name, so each set of artifacts has a stem without
anything being invented.

**This is the debug flag's existing behavior generalized.** `-g` takes no
filename today and derives `<source>.dbg`. Only the stem changes.

| Artifact | One output | Several outputs |
|---|---|---|
| Object | as today | one per save point, named by `SAV` / `DSK` |
| Listing (`-l`) | see the dialect table below | `<output>.lst` each |
| Symbol file | as today | `<output>` each |
| Debug info (`-g`) | `<source>.dbg` | `<output>.dbg` each |

The listing is the one row that also depends on which dialect is assembling,
because as65's `-l` carries a compatibility obligation Merlin's does not.

### Nothing here can regress, and that is structural

Multi-output assembly **cannot happen today** — `SAV` is refused by the boundary
table and `DSK` keeps only its last name. So every rule in the right-hand column
describes behavior that does not yet exist, and the left-hand column is
unchanged (FR-033), apart from Merlin's listing flag below.

That is also why the single-output case keeps deriving from the **source** name
rather than the output name: changing it would be a real regression for the
common case, and with one output there is nothing to disambiguate.

**as65 is untouched entirely.** It has no `DSK`, `SAV` or `TYP` directive at
all, so an as65 assembly always produces exactly one output and never reaches
the right-hand column. All of this is Merlin's.

### `-l` diverges by dialect, deliberately

`-l` is already shipped for both dialects, taking an optional filename and
defaulting to standard output. That stays for as65 and changes for Merlin.

| Dialect | `-l` | `-l<file>` |
|---|---|---|
| as65 | stdout, as today | `<file>`, as today |
| Merlin | `<output>.lst`, one per output | **not accepted** (FR-034) |

**Why Merlin drops the filename.** A listing is named after the output it
describes, so a filename can express the single-output case and nothing else. A
flag that works only when the source happens to have one output is worse than a
flag that does not take a value: withdrawing it removes the mismatch instead of
adding a refusal for it. A filename given anyway earns a diagnostic saying
listings are named after each output — not a generic unknown-flag message,
which would leave the reader guessing.

**Why Merlin drops standard output.** A listing is read later to find something,
which is what a file is for. It also cannot be split per output, so it would
reintroduce the one-stream-holding-everything shape the split exists to remove.

**Why as65 keeps both.** Its `-l` is an as65 compatibility obligation rather
than a choice. And an as65 assembly always produces exactly one output — the
dialect has no directive that could produce a second — so none of the
multi-output reasoning reaches it (FR-038).

**The divergence is the intended shape, not a fault.** The two dialects hold
separate flag tables and answer separate help pages by design;
`AssemblerMode.h` states that a dialect's "flags, its examples and where its
supported subset ends are all its own, and the dialect that answers `merlin
--help` is not the one that answers `as65 --help`". as65's `-l` is owed to
as65; Merlin's is Casso's own invention, since Merlin wrote no listing file at
all.

Merlin's `-l` took a filename and defaulted to standard output from 1.18.0;
both go away. CHANGELOG entry, same as any other user-visible change.

### Shared symbols and shared source lines are repeated, not factored out

The equates above the first output belong to no output, and they go into
**every** per-output artifact (FR-035, FR-036).

This reverses what a single combined file allowed. Factoring them into one
shared section saves space only while the sections live together; once the files
are split, a file missing them does not stand alone, and a debugger holding only
`MAIN.dbg` cannot resolve the hardware address it was opened to look up.

### None of this is period behavior, and that is fine

Merlin wrote **no listing file at all** — screen or printer only, with `LST`
controlling whether the listing is emitted rather than where it goes. And it
printed **one flat symbol table per assembly**, unsegmented, even for
multi-output sources.

So `-l`, `-t` and `-g` are as65's and Casso's, not Merlin's, and SC-003 does not
reach them: a period assembler produced no file to compare against. The
decisions above rest on the ambiguity argument below, which is enough on its
own. Recorded here so a later reader does not "restore fidelity" by flattening
them back.

### Why symbols and debug info have to be scoped

Independent outputs may occupy **overlapping addresses** and are never in memory
at the same time. A flat index across all of them cannot answer the question
`FormatDebugInfo`'s own comment says a debug file exists to answer — "what is at
$0310" — because two programs may both have something there, and at most one of
them is loaded. Answering with both is worse than not answering: it hands a
debugger a symbol from a program that is not running.

This is not hypothetical for this feature. Two outputs from one source is the
*ordinary* multi-save shape, and a loader at `$0300` followed by a main program
that also starts low is the textbook case.

### How a symbol is assigned to an output

By **where it is defined in the source**, using the same cuts that divide the
object (FR-030). Scoping by address would be ambiguous exactly where the outputs
overlap, which is the case that motivated the requirement in the first place.

Symbols defined above the first output — the equates naming hardware addresses,
zero-page locations and the like — belong to no output, and so are repeated into
every one of them (FR-035). See "Shared symbols and shared source lines" above
for why the duplication is the right trade once the artifacts are separate
files.

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
| The name is illegal on the target filesystem — too long, or forbidden characters. A derived default is subject to this too. | — |
| `--type`, or `TYP`, names a type with no counterpart on the target filesystem. Names both the type and the filesystem. | FR-010 |
| `--type`, or `TYP`, names a value outside the recognized set. Names the value. | FR-011 |
| `--startup` given with no `--disk`. | FR-023 |
| `--startup` on a volume whose operating system would not run the file. Judged by the same rules `disk boot` applies. | FR-022 |
| `--as` or `--type` given with no `--disk`. Both describe a placement on a volume. | FR-040 |
| A file of that name is on the volume and the filesystem protects it — ProDOS destroy-disabled, or DOS 3.3 locked. | FR-039 |
| The source declares no origin, so the object has no load address to record. | — |
| The assembly produced zero bytes, so there is nothing to place. | — |

The `--as`/`--type` row is a judgment worth stating: a name or a type with
nothing to apply them to is a command line whose author believed something false
about what was about to happen. The tree's own precedent is `ParseRunOptions` —
"EVERY DIAGNOSTIC HERE IS ALSO A REFUSAL … Running anyway and reporting success
told a build script that a command line it got wrong had worked."

**The protection row qualifies FR-019 rather than contradicting it.**
Replacement is the rule, and these are the two cases the filesystem itself
forbids. ProDOS gates destroy and write on different bits and a replacement here
releases the old file's blocks, so it needs destroy permission — a file marked
writable but not destroyable is refused. DOS 3.3 refuses placement over a locked
file, matching the guest. Both are existing volume-layer behavior; the point of
listing them is that FR-019's replace-on-collision is the *common* path in a
build loop, so its exceptions must be stated rather than discovered.

**The no-origin row is existing behavior and the right one.** The volume layer
already refuses a binary with no load address rather than defaulting one,
"because `$0000` is a legal load address and a default would be
indistinguishable from an answer" — which is precisely the silent disagreement
this feature exists to remove.

**Source directives without an image target do not all answer the same way.**
`DSK` names a host file and `SAV` writes one (FR-020), because each has a host
meaning to degrade to. `TYP` has none — a host file has no filesystem type — so
it is refused (FR-041), exactly as the `--type` flag is. What decides is whether
the construct has anything to mean without a volume, not whether it arrived as a
flag or as a directive.

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

- The assembled bytes are byte-for-byte identical to before, with no exception
  (FR-016, SC-006). Listing text is identical line for line; only the file
  division and, under Merlin, the destination change.
- `disk put` stays, and remains the right tool for placing files the assembler
  did not produce.
- Listing, symbol table and debug info stay on the host, requested by their own
  flags, whether or not `--disk` is given (FR-004).

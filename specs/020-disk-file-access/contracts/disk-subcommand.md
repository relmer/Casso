# Contract: `disk` Subcommand (CLI)

Additive by construction. **One row** in `s_kSubcommands`, **one arm** in
`CommandLineParser::Parse`, and new fields on `CommandLineOptions`. The
dispatcher is not reshaped, spec 019 is being developed concurrently against
these same files, and `UnitTest/CommandLineTests.cpp` pins the current behavior.

## Parser change, in full

```cpp
//  CommandLineParser.cpp -- one row added to the existing table.
static constexpr CommandLineParser::SubcommandName  s_kSubcommands[] =
{
    { "run",  CommandLineOptions::Subcommand::Run  },
    { "disk", CommandLineOptions::Subcommand::Disk },
};

//  Parse() -- one arm added alongside the existing Run arm.
if (named == CommandLineOptions::Subcommand::Disk)
{
    ParseDiskOptions (argc, argv, 2, options);
}
```

`CommandLineOptions` gains a `Disk` enum value, a `DiskVerb` enum, and the
verb's operands. Nothing existing changes shape, so both features extend the
struct without a blind merge.

## Grammar

```text
casso disk list   <image>
casso disk get    <image> <path> [--out <file>] [--text | --basic]
casso disk put    <image> <file> [--as <path>] [--type <t>] [--addr $XXXX]
                                 [--text | --basic]
casso disk delete <image> <path>
casso disk boot   <image> <path>
```

Aliases: `ls` → `list`, `rm` → `delete`. Help displays the descriptive form.

An argument beginning with `-` that is none of these options is **refused**, and
the command runs nothing. It used to be counted as an operand, and this grammar
has none past the second, so `disk get img FILE -o out.bin` discarded both the
flag and its value: the file went to standard output, nothing was written where
the caller asked, and the exit status said the command had worked. Only a dash
is treated as a mistake; a ProDOS path is `/VOLUME/FILE` and stays an operand.

`put` and `get` are named from the **disk's** perspective, which is what makes
their direction unambiguous, and it matches the mnemonics of the tool most
migrating developers are coming from. The help text says so, so the mental model
transfers without adopting that tool's flag syntax.

`cat` is deliberately **not** a spelling of `list`: it collides with the
established meaning of printing a file's contents, which this tool does under
`get`.

**There is no flag for the no-conversion path.** It was `--verbatim`, spelled
that way rather than `--raw` or `--binary` because both of those already named
assembler output shapes inside this same parser. The owner retired it: verbatim
is the default, so the flag's only surviving effect was cancelling a `--text` or
`--basic` earlier on the same line, which nothing needs and no caller writes.
Naming neither conversion is how the unconverted path is reached.

`--long` is retired for a related reason. It withheld the ProDOS `eof=` and
`aux=` columns, which `ProDosVolume::Enumerate` fills whether or not anybody
asks and which are the two fields a build loop most wants, the exact length of
a file and the address a binary loads at. A listing prints them always; the
widest measured row is 51 characters, well inside 80.

`--out` stays the disk grammar's flag and `-o` the assembler's. They are
deliberately **not** unified: as65 argument compatibility is what the assembly
grammar exists for, and it outranks uniformity here. Each grammar refuses the
other's spelling with a message naming it, so the collision is diagnosed rather
than absorbed in either direction.

## Options nest rather than flatten

`disk` is the first subcommand to break `CommandLineOptions`' stated rationale
for a flat struct, and the resolution was to nest, `options.disk.verb`,
`options.disk.imagePath`, and so on, inside a `DiskOptions` member.

The flat shape was chosen when every option belonged to the assembler and the
question of which subcommand owned a field did not arise. `disk` brings eleven
fields that mean nothing to `as65` or `run`, three of which (`path`, `hostFile`,
`loadAddress`) would need disambiguating prefixes at the top level to avoid
reading as assembler options. Nesting says which subcommand owns a field by
where it sits, rather than by a naming convention a later field can forget to
follow. The comment on the flat section was amended rather than deleted, so the
original rationale and the reason it stopped holding are both readable.

## Encoding is applied to the payload, not to the destination

`get --text` must deliver the same bytes whether they are piped or written to
`--out`. The conversion therefore happens once, on the payload, before the
destination is chosen, not at each write site.

**An encoding this build cannot perform is refused, never ignored.** A flag that
is parsed and then silently dropped is worse than one that does not exist: the
caller reads "converted to a listing" in the help, receives tokenized bytes, and
has nothing in the output distinguishing that from a file needing no conversion.
`--basic` therefore exits `2` with a message naming the flag until the Applesoft
tokenizer lands.

On the read path the high-bit convention is inert (once bit 7 is ignored, high
ASCII and plain ASCII differ in nothing) so the choice becomes load-bearing
only when writing. See research R-011: the `TXT` type does not imply a
convention, the producer does.

## Listing shape

- **Zero-sector catalog entries are rendered, not filtered.** Real disks use
  them to draw section headings, twenty of the sixty-three on Merlin Pro's own
  disk. DOS renders them and so does the vendor's printed catalog; hiding them
  makes this listing disagree with the machine's, and the disagreement reads as
  an enumeration bug. Anyone wanting them gone is asking for a filter, which is
  a different request.
- **A damage message must say the LISTING is incomplete**, not merely that the
  disk is damaged. The exit status carries the distinction for a script; the
  wording has to carry it for whoever reads the log hours later. "The disk was
  damaged" does not tell them they are holding less than they asked for.

## Exit statuses

`0`, `1`, and `2` are **reserved and universal** across every subcommand of this
tool, matching what `as65` and `run` already return, so a script needs no
per-subcommand knowledge to branch on them (FR-031):

| Code | Meaning | Example |
|---|---|---|
| `0` | Completed cleanly | Listing a healthy volume |
| `1` | Succeeded, with complaints: usable result on stdout, damage on stderr | Listing a volume with a damaged catalog or an undecodable track |
| `2` | Produced no output | Image unreadable, volume full, file locked, track not writable |

Values of **3 and above are subcommand-scoped** and documented in that
subcommand's own help (FR-032). They carry no cross-subcommand meaning and are
not coordinated against other subcommands' values; a caller already knows which
subcommand it invoked, and requiring global uniqueness above 2 would couple
subcommands that are otherwise independent.

## Streams

- Listings and extracted bytes go to **stdout**, so they pipe.
- Every diagnostic: including the damage description that accompanies exit `1`,
  goes to **stderr**, so it never contaminates piped output (FR-033).
- Failure messages name **the image, the file, and the reason** (FR-033).
  Write-protect refusals are reported in intelligible terms, not as a raw
  platform error code, including when protection comes from the host file's
  read-only attribute and surfaces as an access denial at commit time (FR-014).

## Shell responsibilities

The commit is these five steps, in this order:

1. Read the image file; record its size and modification time (FR-036).
2. Call core to compute a complete new image, or fail.
3. Re-verify size and modification time; refuse if either changed.
4. Write to a **uniquely named** temporary file beside the target; atomically
   replace; remove the temporary on any failure (FR-013).
5. Print what core returned.

**This list originally said the CLI shell OWNS all five, and that reading was
wrong**; it would have put the entire commit policy in the one project the test
assembly does not link, which is Principle VI's litmus failing in the place
where a bug destroys a user's disk image. What is irreducibly the shell's is the
*syscalls*: read bytes, write bytes, stat, exists, remove, atomic replace,
exclusive-open probe. Every **decision** in the sequence, what the temporary is
called, whether the stamps agree, whether a temporary may still be sitting there
once the sequence stopped, and what to say when it is refused, is core's, in
`CommitPlan` and `DiskCommandRunner::CommitImage`, above `IDiskFileIo`. The
sequence above is what the shell *causes to happen* by calling one method; it is
not a list of things the shell implements.

Best-effort exclusive-open probe refuses when *another* holder has the file
open. It cannot detect Casso, which holds no handle on a mounted image, and the
help text does not imply otherwise (FR-035). The wording that carries FR-035
lives on `DiskCommandRunner::kInUseHelpText`, beside the code that performs the
probe, so a test can read it, a claim written only into the executable's help
block is a claim nothing can check.

## Help output

Every capability appears in help (FR-034). SC-002 requires a newcomer to complete
the loop from help alone, so the help text carries a worked example of the whole
loop (assemble, put, boot) not just a flag list.

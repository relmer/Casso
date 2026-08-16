# Contract: `disk` Subcommand (CLI)

Additive by construction. **One row** in `s_kSubcommands`, **one arm** in
`CommandLineParser::Parse`, and new fields on `CommandLineOptions`. The
dispatcher is not reshaped — spec 019 is being developed concurrently against
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
casso disk list   <image> [--long]
casso disk get    <image> <path> [--out <file>] [--text | --basic | --binary]
casso disk put    <image> <file> [--as <path>] [--type <t>] [--addr $XXXX]
                                 [--text | --basic | --binary]
casso disk delete <image> <path>
casso disk boot   <image> <path>
```

Aliases: `ls` → `list`, `rm` → `delete`. Help displays the descriptive form.

`put` and `get` are named from the **disk's** perspective, which is what makes
their direction unambiguous — and it matches the mnemonics of the tool most
migrating developers are coming from. The help text says so, so the mental model
transfers without adopting that tool's flag syntax.

`cat` is deliberately **not** a spelling of `list`: it collides with the
established meaning of printing a file's contents, which this tool does under
`get`.

The encoding selector is `--binary`, **not** `--raw`, for the same reason:
`--raw` already exists on the assembler as an *output shape*
(`CommandLineOptions::OutputFormat::Raw`). Reusing the spelling for an encoding
selector would give one flag two meanings inside one parser — and spec 019 is
editing that parser concurrently, which is the worst possible time to introduce a
collision that only shows up as a confusing help page.

## Exit statuses

`0`, `1`, and `2` are **reserved and universal** across every subcommand of this
tool, matching what `as65` and `run` already return, so a script needs no
per-subcommand knowledge to branch on them (FR-031):

| Code | Meaning | Example |
|---|---|---|
| `0` | Completed cleanly | Listing a healthy volume |
| `1` | Succeeded, with complaints — usable result on stdout, damage on stderr | Listing a volume with a damaged catalog or an undecodable track |
| `2` | Produced no output | Image unreadable, volume full, file locked, track not writable |

Values of **3 and above are subcommand-scoped** and documented in that
subcommand's own help (FR-032). They carry no cross-subcommand meaning and are
not coordinated against other subcommands' values — a caller already knows which
subcommand it invoked, and requiring global uniqueness above 2 would couple
subcommands that are otherwise independent.

## Streams

- Listings and extracted bytes go to **stdout**, so they pipe.
- Every diagnostic — including the damage description that accompanies exit `1` —
  goes to **stderr**, so it never contaminates piped output (FR-033).
- Failure messages name **the image, the file, and the reason** (FR-033).
  Write-protect refusals are reported in intelligible terms, not as a raw
  platform error code, including when protection comes from the host file's
  read-only attribute and surfaces as an access denial at commit time (FR-014).

## Shell responsibilities

The CLI shell owns only the irreducible platform edge:

1. Read the image file; record its size and modification time (FR-036).
2. Call core to compute a complete new image, or fail.
3. Re-verify size and modification time; refuse if either changed.
4. Write to a **uniquely named** temporary file beside the target; atomically
   replace; remove the temporary on any failure (FR-013).
5. Print what core returned.

Best-effort exclusive-open probe refuses when *another* holder has the file
open. It cannot detect Casso, which holds no handle on a mounted image, and the
help text does not imply otherwise (FR-035).

## Help output

Every capability appears in help (FR-034). SC-002 requires a newcomer to complete
the loop from help alone, so the help text carries a worked example of the whole
loop — assemble, put, boot — not just a flag list.

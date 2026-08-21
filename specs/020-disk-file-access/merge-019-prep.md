# Merging 019 into 020: what the command line will do

Written from 020 by surveying `019-assembler-dialects` before it landed, so
whoever resolves the merge is not discovering the shape of it inside a conflict.
Nothing here is speculative: every claim was read off 019's tree or measured on
020's binary.

**Landing order is 019 to master first, then master into here, then here to
master.** So this is a one-way problem: 019 arrives as `origin/master` and 020
absorbs it.

## The headline

**019 dismantled `CommandLine.cpp`; 020 grew it.** 019 took it from ~1,200 lines
to **475** by splitting the modes out; 020 has it at **1,481**. Almost everything
020 added this cycle lives in the file 019 emptied.

019's new units, none of which exist on 020:

| unit | what moved into it |
|---|---|
| `CassoCli/AssemblerMode.{h,cpp}` | the assemble path shared by both dialects |
| `CassoCli/As65Mode.{h,cpp}` | as65 specifics |
| `CassoCli/MerlinMode.{h,cpp}` | Merlin specifics |
| `CassoCli/RunMode.{h,cpp}` | everything `DoRun` did |
| `CassoCli/SourceAssembler.{h,cpp}` | reading and assembling one source |
| `CassoCli/ArtifactWriter.{h,cpp}` | writing .bin/.lst/.dbg and friends |
| `CassoCli/HostFile.{h,cpp}` | the filesystem edge |

020's `DoAs65`, `DoRun`, `PrintUsageAssembly`, `PrintUsageRun` and `BuildBanner`
are all still in `CommandLine.cpp`. **Expect git to report the whole file as a
conflict and be unable to help.** Resolve it by moving 020's work into 019's
modules by hand, not by picking a side.

## Where the two agree more than it looks

**Both moved usage text into core, independently, and they are not rivals:**

- 020: `CassoCore/CommandLineHelp.{h,cpp}` — help **content** (the general page,
  the usage line per mode, the worked example).
- 019: `CassoCore/UsageText.{h,cpp}` — a pure **mechanism**: `Wrap(line, width)`
  and `ContinuationIndent(line)`. It holds no content at all.
- 019: `CassoCore/DialectHelp.{h,cpp}` — content, but only the dialect flag lines.

`UsageText` and `CommandLineHelp` compose. The good end state is 020's content fed
through 019's folding.

**019 folds help to the reader's terminal; 020 hand-wraps it.** 019 authors one
logical line per item and folds at print time, finding the continuation column
rather than authoring it. Every line of 020's help is hand-wrapped to about 79
columns inside string literals. 019's approach is better and 020's tables are the
hard case: the option tables have a description column that folding has to
preserve, which is exactly what `ContinuationIndent` is for. **Do not hand-wrap
new help text during the merge** — it will have to be unwrapped again.

**019 keeps the bare-filename fallback.** `BareFilename_FallsBackToAs65` is one of
its tests. 020's whole help structure rests on the assembler being the fallback
rather than a named mode, and on `?` opening the assembler's page; that assumption
survives. 019 *adds* `as65` and `merlin` as named subcommands beside it.

## Real collisions

### 1. Separated values: 019 accepts more than 020 documents

019's rule is general — "a flag's argument may be glued to it or separated" — and
its help says `-o` **and `-l`** both take a separated value. 020 accepts it for
`-o` alone; `-l prog.lst` is refused here, measured.

These are complementary rather than contradictory. Accepting both is a superset of
as65 and breaks no compatibility claim. But **020's help currently states "`-o` is
the one switch where the space before its value is optional", and that sentence
becomes false the moment 019 lands.**

Note the interaction: 020 built the PowerShell rejoin partly *because* `-l` had no
separated form to fall back on. With 019's `-l` accepting one, the rejoin is still
wanted — it repairs `-d` and `-s` too, and it fixes the attached form the user
actually typed rather than asking them to retype it.

### 2. `--cpu` versus `-x`

020 withdrew `--cpu` and answers it by name, pointing at `-x`. 019 **pins `--cpu`
in as65 mode with its own tests**, and its Merlin path refuses `--cpu` by name to
point the user at Merlin's `XC` directive. Already recorded in `tasks.md` Phase 13
and in CLAUDE.md; repeated here because it is the one collision that silently
degrades into a bare "unknown option" if the refusal is dropped rather than
reworded.

### 3. Help structure: tiered pages versus one page

020 tiered the help — a general page naming three modes, and a page per mode
reached by `?`, `run --help`, `disk --help`, each with its own banner and exit
codes. 019 prints **one page**: `Usage: CassoCli {subcommands} [options] | -? |
--version`, with an as65-habits paragraph that is 020's as65-compatibility section
under another name.

Both were written for the same complaint. 020's tiering is the more finished
answer and 019's dialect content has to find a home inside it: a `merlin` page
beside the as65 one, most likely, with `DialectHelp` feeding it.

### 4. 019 has never heard of `disk`

Its subcommand table is `as65`, `merlin`, `run` — zero references to
`Subcommand::Disk`. Every disk row, verb and help page is 020's alone and should
survive the merge untouched. The risk is the opposite one: taking 019's version of
a shared table and silently dropping the disk rows.

### 5. 019's help text predates 020's wording rules

019's usage text uses `" -- "` as a dash and reaches for "spelling" the way 020's
did before the sweep. Sweep 019's incoming help text for the retired words: `" -- "`,
`shape`, `spelling`, `Exit status`, `65SC02`.

## Suggested order for the merge session

1. Take 019's module decomposition as the destination. It is the better shape and
   it is the thing that cannot be re-derived from 020.
2. Move 020's `DoAs65`/`DoRun` bodies into `AssemblerMode`/`RunMode`, and its
   artifact writing into `ArtifactWriter`, rather than merging `CommandLine.cpp`
   textually.
3. Keep `CommandLineHelp` as the content store; make it emit logical lines and
   print them through `UsageText::Wrap`. Unwrap 020's hand-wrapped literals as you
   go.
4. Add the `merlin` page to the tier, fed by `DialectHelp`.
5. Reconcile the two grammars: accept a separated value wherever either branch
   did, keep the rejoin, and correct the `-o` sentence.
6. Reword 019's `--cpu` refusal at `-x`, and retarget its three `--cpu` tests.
7. Sweep the incoming help text for the retired words.
8. Both test suites are the gate. 019 brings `UnitTest/MerlinCommandLineTests.cpp`
   (971 lines) and 020 has `CommandLineTests` and `DiskHelpTextTests`; a merge that
   satisfies one and not the other has picked a side.

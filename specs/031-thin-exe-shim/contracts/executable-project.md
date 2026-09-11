# Contract: What an Executable Project Contains

**Feature**: 031-thin-exe-shim | **Requirements**: FR-003, FR-003a, FR-003b, SC-002

This contract governs `Casso.vcxproj` and `CassoCli.vcxproj` after User Story 8.
`MeshCreator` is out of scope: it is a build-time tool, not a shipped
executable.

The reference implementation is `TCDir`, in this developer's tree. Where this
contract and a future argument disagree, `TCDir` is the tiebreaker, because it
is a running program rather than a position.

## Permitted contents

An executable project contains exactly three kinds of item:

| Item | `Casso` | `CassoCli` |
|---|---|---|
| Resource script | `Casso.rc` | — if it has none, none is added |
| Generated resource header | `resource.h` | — |
| One comment-only translation unit | `Main.cpp` | `CassoCli.cpp` |

Nothing else. No headers of its own, no additional sources, no shader items.

## Prohibited contents

- Any function definition, the entry point included.
- Any executable statement.
- Any `ClInclude` other than the generated resource header.

**The check**: count the functions defined in the project's translation units.
The expected answer is zero, and it is a check that can fail.

## The linker mechanism

A static library member that nothing references is discarded. The project must
therefore give the linker an undefined symbol to resolve, which it does by
naming the CRT startup symbol. Startup references the entry point; the entry
point drags the program out of the library.

Set in **every** configuration:

| Project | Subsystem | `EntryPointSymbol` | Entry point lives in |
|---|---|---|---|
| `Casso` | `Windows` | `wWinMainCRTStartup` | `CassoEmuCore/Gui/` |
| `CassoCli` | `Console` | `mainCRTStartup` | `CassoEmuCore/Cli/CliMain.cpp` |

`TCDir` sets `<EntryPointSymbol>wmainCRTStartup</EntryPointSymbol>` in all five
of its configurations alongside `<SubSystem>Console</SubSystem>`; its `wmain` is
at `TCDirCore/TCDir.cpp:226`.

`/WHOLEARCHIVE` and `/INCLUDE:` reach the same result and are not used. The
first defeats the dead-code elimination that keeps `CassoCli.exe` from carrying
the desk scene, and a second mechanism in one tree is something a reader has to
learn for no benefit.

## The comment-only translation unit

Its entire job is to be found by someone looking for code that is not there and
to tell them where the code went. It carries a line saying the project only
produces the executable from the library, then a haiku.

`TCDir/Main.cpp`, the model:

```
// This project only produces the .exe from the TCDirCore.lib
//
// There is no code here
// Cheer up, everything is fine
// Seek TCDirCore
```

`Casso/Main.cpp` closes "CassoEmuCore" — five syllables exactly, where "Seek
CassoEmuCore" would be six. The verb goes rather than the form.

## Verification

| Check | Passes when |
|---|---|
| Function count | Zero in both projects |
| Project items | Resource script, resource header, one comment-only unit |
| `EntryPointSymbol` | Set in every configuration of both projects |
| Build | Debug and Release link and both executables start |
| Discoverability | The comment names the library holding the entry point |
| `UnitTest` coupling | No `ProjectReference` to `Casso.vcxproj`, no `..\Casso` include path, no `..\Casso\*` `ClCompile` entries |

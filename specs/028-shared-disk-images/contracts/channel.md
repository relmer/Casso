# Contract: the intent channel

**Feature**: `028-shared-disk-images`

How a writing tool tells a running emulator what it meant. **This carries only
the intent.** The change itself is found by watching the file, because a change
can come from anything — a text editor, another emulator, a script — and only
`CassoCli` can speak here.

## The core seam

```cpp
class IIntentChannel
{
public:
    virtual ~IIntentChannel () = default;

    //  Say what a write to `imagePath` should do to any emulator holding it.
    //  Best-effort: no delivery guarantee, no acknowledgement, no failure.
    virtual void  StateIntent (const std::string & imagePath,
                               PickUpIntent        intent) = 0;
};
```

**`StateIntent` RETURNS NOTHING, and that is the contract rather than an
oversight.** A failure to deliver degrades to the emulator's fallback answer,
which is correct behavior; there is no caller who could do anything useful with
an error, and one that failed a build over it would be worse than the bug this
feature fixes.

**Core owns the interface AND the Win32 implementation**, beside
`CassoEmuCore/Cli/Win32DiskFileIo.cpp`. Only the window handle and the message
pump belong to an executable.

Two reasons, and the second is not negotiable by taste. The constitution says
calling Win32 is not a reason to live in an exe, and names "does this call a
platform API?" as the wrong question. And this is the SENDER: its callers run
inside `CassoCli.exe`, which cannot link `Casso.exe`, so a shim in the shell
would not link at all.

`UnitTest` drives the whole decision path with a fake that records what was
stated.

## The Win32 shim

Broadcast `WM_COPYDATA` to every top-level window of class `CassoWindow`.

| Part | Value |
|---|---|
| `dwData` | A registered message id, so an unrelated `WM_COPYDATA` is ignored |
| `cbData` | Size of the payload below |
| `lpData` | The intent, then the image path as UTF-8 |

**The path is sent as the writer resolved it, absolute.** The receiver compares
against its own mounted paths after the same normalization the mount used, since
two spellings of one path must match.

**Every emulator receives it; each ignores paths it has not mounted.** That is
what makes multi-instance need no discovery protocol.

### Two Win32 facts this depends on

- **`WM_COPYDATA` is already allowed through the integrity filter.**
  `EmulatorShell::InstallDragDropTarget` calls `ChangeWindowMessageFilterEx` for
  it, because an elevated Casso would otherwise see messages from a
  normal-integrity sender silently dropped. That is precisely the hazard here —
  an elevated Casso and an ordinary `CassoCli` — and the mitigation is already
  installed on the receiving window.
- **`SendMessage`, not `PostMessage`.** `WM_COPYDATA` requires the sender's
  buffer to stay alive for the duration, which posting cannot promise. The
  sender must use a timeout so a hung emulator cannot hang a build.

## Ordering

**State the intent AFTER the commit, never before.** The receiver reads the
image when it acts, so an intent arriving first would describe contents not yet
on disk. `ImageArtifactSink` and `DiskCommandRunner` both already have a single
commit point to hang this on.

## Receiving

The shell turns a received message into a `PendingChange` on the matching bay
and returns immediately. It does NOT act on it: acting happens on the
motor-spindown callback, on the CPU thread that owns disk writes (FR-014).

**A message never bypasses the watcher.** If the file did not actually change,
the identity comparison finds nothing and the intent is discarded. The channel
is a hint about a change, not a substitute for observing one.

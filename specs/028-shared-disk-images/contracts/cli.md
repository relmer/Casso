# Contract: stating an intent from the command line

**Feature**: `028-shared-disk-images`

## The flag

```text
--on-change <what>
```

| Value | Meaning |
|---|---|
| `reload` | Take the new contents up in place; leave the machine running |
| `restart` | Take the new contents up and restart the machine |

Omitted means nothing is stated, and a running emulator falls back to the answer
the user declared (FR-007).

## Where it is accepted

Everywhere an image is written, because every one of those writes can land under
a running emulator:

```bash
CassoCli as65 prog.a65 --disk game.dsk --as PROG --on-change reload
```
```bash
CassoCli merlin PROG.S --disk game.dsk --on-change restart
```
```bash
CassoCli disk put game.dsk levels.dat --as LEVELS --on-change reload
```

It joins the image-target group `--disk`, `--as`, `--type` and `--startup`
already occupy, and appears in generated help under **Assembled code** for the
assemblers and under the `disk` grammar for the subcommand. Both flag prefixes
work, as everywhere else: `--on-change` and `/on-change`.

## Rules

- **Refused without `--disk`**, exactly as `--as`, `--type` and `--startup` are.
  That rule is spec 026's, not this one's — see its requirement on image options
  given with no image target — and the refusal here should share its wording
  rather than invent a second phrasing for the same mistake. An intent describes
  what a change does to a mounted image, and a host-file write changes no image.
- **Stating it when nothing is running is NOT an error** (FR-015). The writer
  cannot know whether an emulator is up, and a build script must behave
  identically either way. The flag succeeds and has no effect.
- **An unrecognized value is refused naming the value**, listing the two that
  are accepted. This follows the tree's rule that a name outside a known set is
  refused rather than approximated.
- **It does not change what is written.** Byte for byte, an assembly with
  `--on-change` produces the same image as one without.

## Exit codes

Unchanged. The flag cannot fail on its own: it is either refused at parse time
with the existing bad-command-line status, or it is carried and announced after
a successful commit. Announcing it never fails the run — a channel that could
fail the build would be worse than the problem it solves.

## What is deliberately absent

- **No `--on-change ask`.** Asking is what an unstated intent already produces
  through the fallback, so a third value would be a second spelling of omitting
  the flag.
- **No way to address a particular emulator.** The intent attaches to the IMAGE
  (FR-006); every emulator holding that image acts on it, and none holding a
  different one does.
- **No report of whether anything received it.** The channel is best-effort by
  design, and a script that branched on delivery would be branching on whether
  the developer happened to have the emulator open.

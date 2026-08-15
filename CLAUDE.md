# Casso — Claude Code Instructions

Project guidelines, code style, EHM patterns, build rules, and current feature context are in:

**`.github/copilot-instructions.md`**

Read that file at the start of every session.

<!-- SPECKIT START -->
**Active spec: `specs/020-disk-file-access`** (Draft) — disk file access for the
build loop: assembler binary output, DOS 3.3 / ProDOS file read+write, a
`disk` subcommand, and boot configuration. Next step is `/speckit-clarify` or
`/speckit-plan`.

Three further specs are drafted but NOT started, each to be picked up in its own
session:

- `specs/019-assembler-dialects` — Merlin (and the ca65 absolute subset). Seeded
  by GH #92.
- `specs/021-disk-manager` — graphical disk manager, live editing of the mounted
  disk, inspection tools, and CLI parity. Depends on 020.
- `specs/022-disk-image-formats` — 2MG, nibble, compressed/archived images, extra
  filesystems, larger media. Depends on 020/021; large-media parts gated on
  GH #101 / #93.

Recent specs live under `specs/` (015 printer support, 016 Apple //c, and 017
blank-disk creation are all complete and shipped).
<!-- SPECKIT END -->

# DebugFiles fixtures

`hello.s`, `hello.inc` and `hello.cfg` were written for Casso's test suite and are under the repository's own license.

`hello.dbg` and `hello.bin` were produced from them by cc65's own tools, built from https://github.com/cc65/cc65 on 2026-09-18:

```text
ca65 -g hello.s -o hello.o
ld65 -C hello.cfg -o hello.bin --dbgfile hello.dbg hello.o
```

They are tool output, not cc65 source; cc65 itself is under the zlib license. Regenerate them with the commands above if the sources change.

# Contract: Debug File Format

The file `CassoCli as65 -g` and `CassoCli merlin -g` write, and the file the
debugger's symbol and source commands read (FR-033, R-021). It is cc65's
debug-info format, version 2, as written by cc65's linker, plus one key.

## Records Casso writes

One record per line, `keyword<TAB>key=value,key=value,...`, in this order:

```text
version	major=2,minor=0
info	csym=0,file=N,lib=0,line=N,mod=N,scope=N,seg=N,span=N,sym=N,type=0
file	id=0,name="main.a65",size=1234,mtime=0x66E9A1B2,mod=0,sha1="2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"
file	id=1,name="macros.inc",size=321,mtime=0x66E9A1B2,mod=0,sha1="..."
mod	id=0,name="main",file=0
seg	id=0,name="CODE",start=0x0300,size=0x00F2,addrsize=absolute,type=rw
span	id=0,seg=0,start=0,size=2
span	id=1,seg=0,start=2,size=3
line	id=0,file=0,line=12,span=0
line	id=1,file=0,line=13,span=1
line	id=2,file=1,line=4,type=2,count=1,span=1
sym	id=0,name="start",addrsize=absolute,scope=0,def=0,val=0x0300,seg=0,type=lab
scope	id=0,name="",mod=0,size=0x00F2
```

- `file.name` is the path relative to the debug file, with `/` separators.
- `file.size` is the byte count of the file as read. `file.mtime` is the
  file's last-write time as Unix seconds, written for cc65 compatibility and
  never used by Casso to decide a match.
- `file.sha1` is the SHA-1, as 40 lowercase hex digits, of the file's text
  with `\r\n` and lone `\r` replaced by `\n`, over UTF-8 bytes. It is the key
  cc65 does not write; cc65's reader skips it with a warning.
- `span.start` is relative to `seg.start`. `seg.start` is where the segment
  was placed. A reader computes an address as `seg.start + span.start`.
- `line.type` is omitted for assembler source lines (type 0), `2` for a line
  inside a macro expansion, `3` for a macro parameter expansion; `line.count`
  is the nesting depth and is omitted when the type is omitted. The
  invocation line and each macro body line get their own `line` records
  listing the same spans, so an address maps to both (FR-033a).
- `sym.val` is the symbol's value; `sym.seg` is present when the value is an
  address in a segment. Local labels and macro-generated labels are written
  with a `scope` other than the module's, so the source view can tell them
  from top-level symbols; the earlier symbol table omitted them, and the
  `SYM` commands keep omitting them from listings by default.
- Merlin: one `mod` and one `seg` per `SAV` output, in one file per output as
  before.

## What Casso reads

- Any cc65 version-2 file, from any assembler or linker, using `file`,
  `line`, `span`, `seg`, `sym`, `mod` and `scope`; `csym` and `type` records
  are skipped. A `file` record without `sha1` is matched by `size` only, and
  the source opens with the mismatch warning off but a "no hash" note.
- A major version other than 2 is an error that says which version was found.
- The earlier Casso symbol file (`NAME=$ADDR`), a Merlin listing's symbol
  table, AppleWin `.SYM` and VICE labels, as before.
- A Merlin 8/16 listing as a debug file whose single source file is the
  listing itself (FR-033b): every listing line with an address is a `line`
  record for that address, and the trailing symbol table gives the symbols.

## Detection

By contents, not extension: a first line of `version` with `major=` is cc65;
`SYMBOL TABLE` anywhere is a Merlin listing; otherwise the existing rules.

## Errors

`SYM LOAD` and the source commands report, on one line each: the file that
could not be read; the version that is not 2; a `line` record whose `file` or
`span` id does not exist; a span whose `seg` does not exist. A file with such
an error loads nothing.

# UnitTest/Fixtures/Cassque: scratch volumes and host files for the disk browser

Everything in this directory is repo-original and MIT licensed like the rest
of Casso. It is deliberately separate from `../Disks`, whose license note
declares every file there third-party material.

`MakeFixtures.ps1` regenerates all of it from a built `CassoCli.exe`, so the
provenance of every byte is the script itself. Run it from anywhere:

```powershell
UnitTest\Fixtures\Cassque\MakeFixtures.ps1
```

## Images

| File | Contents |
|---|---|
| `dos33.dsk` | DOS 3.3 volume 77: `HELLO` (A), `NOTES` (T), `PICTURE` (B, $2000, 8192 bytes), `LORES` (B, $400, 1024 bytes), `DHIRES` (B, $2000, 16384 bytes), `ODD` (B, $803, 777 bytes), `INTPROG` (I) |
| `prodos.po` | ProDOS volume `/CASSQUE`: the same files as BAS, TXT and BIN, plus an empty subdirectory `SUBDIR`; every entry carries a creation and modification stamp of 1984-08-17 12:34 |

The files were put with these commands (paths shortened):

```
CassoCli disk create dos33.dsk --format dos33 --volume 77
CassoCli disk create prodos.po --format prodos --volume CASSQUE
CassoCli disk put <image> Host\applesoft.txt --as HELLO --basic
CassoCli disk put <image> Host\notes.txt --as NOTES --text
CassoCli disk put <image> Host\hires.bin --as PICTURE --type B --load $2000
CassoCli disk put <image> Host\lores.bin --as LORES --type B --load $400
CassoCli disk put <image> Host\dhires.bin --as DHIRES --type B --load $2000
CassoCli disk put <image> Host\odd.bin --as ODD --type B --load $803
CassoCli disk put dos33.dsk Host\integer.tok --as INTPROG --type I
```

The ProDOS subdirectory and the date stamps are written by the script
directly into the image, because the volume layer has no directory-creation
call and its writer clears the date fields. The layout follows the ProDOS
Technical Reference Manual.

## Host

| File | Purpose |
|---|---|
| `applesoft.txt` | An Applesoft listing; the source of `HELLO` |
| `implicit-let.txt` | Lines whose first token is an identifier followed by `=` |
| `numbered-readme.txt` | Numbered lines that are not BASIC |
| `integer-listing.txt` | An Integer BASIC listing, with `DSP`, which Applesoft never had |
| `notes.txt` | Printable text; the source of `NOTES` |
| `integer.tok` | The hand-tokenized Integer BASIC program; the source of `INTPROG` |
| `hires.bin`, `lores.bin`, `dhires.bin`, `odd.bin` | Deterministic binary patterns of 8192, 1024, 16384 and 777 bytes |

## Expected

Command-line outputs the tests compare against, recorded with:

```
CassoCli disk get dos33.dsk HELLO --basic --out Expected\dos33-HELLO-basic.txt
CassoCli disk get dos33.dsk NOTES --text --out Expected\dos33-NOTES-text.txt
CassoCli disk get prodos.po HELLO --basic --out Expected\prodos-HELLO-basic.txt
CassoCli disk get prodos.po NOTES --text --out Expected\prodos-NOTES-text.txt
CassoCli disk list dos33.dsk > Expected\dos33-list.txt
CassoCli disk list prodos.po > Expected\prodos-list.txt
```

Every file here is marked binary in `.gitattributes` so a checkout never
rewrites its line endings; the comparisons are byte for byte.

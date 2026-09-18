# Contract: Host file names and content sniffing

**Feature**: 033-cassque | **Date**: 2026-09-10

Pure classes in `CassoEmuCore/Cassque/Model/`, no I/O.

## HostFileNaming

```cpp
class HostFileNaming
{
public:
    enum class Style { Descriptive, CiderPress };

    static std::wstring  ForConverted (const std::string & catalogName, PayloadEncoding encoding);
    static std::wstring  ForRaw       (const std::string & catalogName, VolumeKind kind, const FileEntry & entry, Style style);
    static bool          Parse        (const std::wstring & hostName, ParsedHostName & out);
    static std::wstring  MakeHostLegal (const std::string & catalogName, bool & substituted);
};

struct ParsedHostName
{
    std::string  catalogName;
    bool         hasType;        Byte  type;
    bool         hasAux;         Word  aux;
    bool         converted;      PayloadEncoding encoding;   // when a converted suffix was found
};
```

Rules, out:

| Case | Descriptive | CiderPress |
|---|---|---|
| Applesoft listing | `NAME.Applesoft BASIC.txt` | same |
| Integer listing | `NAME.Integer BASIC.txt` | same |
| Text | `NAME.Text.txt` | same |
| Binary, ProDOS `$06` or DOS `B` | `NAME.Binary.$AAAA.bin` | `NAME#06AAAA` |
| Other ProDOS type | `NAME.ProDOS.$TT.bin`, `.$AAAA` before `.bin` when aux nonzero | `NAME#TTAAAA` |
| Other DOS 3.3 type | `NAME.DOS.X.bin` | `NAME#TT0000` with the DOS letter mapped to its ProDOS type |

Hex is uppercase, addresses four digits, types two. Host-illegal
characters in `NAME` are replaced by `_` and the caller is told.

Rules, in: `Parse` recognizes every descriptive form, the CiderPress
`#TTAAAA` form, and otherwise returns false with `catalogName` set to the
name minus any trailing extension. Matching is case-insensitive on the
suffix words.

## ContentSniffer

```cpp
class ContentSniffer
{
public:
    enum class Verdict { Applesoft, Text, Binary };
    static Verdict  Classify (std::span<const Byte> bytes, Word & suggestedAddress);
};
```

Rules, in order:

1. Applesoft when every non-blank line starts with an integer at most
   63999, the integers strictly ascend, and each line's first token after
   the number is an Applesoft keyword or an identifier followed by `=`.
   Lines end in CR, LF or CRLF.
2. Text when every byte is printable ASCII, tab, CR or LF, and there is
   at least one line ending or the file is short.
3. Binary otherwise. `suggestedAddress` is `$2000` for 8192 bytes, `$803`
   for anything else.

No grammar validation beyond rule 1. Integer BASIC listings fall to Text
by design.

## Conversion table on put

| Verdict or parsed suffix | Put as |
|---|---|
| Applesoft | tokenized, type A or BAS, address $801 |
| Text | text conversion, type T or TXT |
| Binary with address | raw, type B or BIN, that address |
| Parsed type and aux | raw, that type and aux, no dialog |

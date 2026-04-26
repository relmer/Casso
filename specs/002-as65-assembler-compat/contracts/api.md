# Public API Contract: Assembler (Extended)

**Feature**: 002-as65-assembler-compat  
**Date**: 2026-04-25

## Library API (Casso65Core)

### Assembler Class

The existing `Assembler` class is extended with new capabilities. The public interface remains backward-compatible.

```
Assembler (instructionSet[256], options)
    → Constructor. Options extended with new fields.

Assemble (sourceText) → AssemblyResult
    → Two-pass assembly. Now supports expressions, constants, macros,
      conditionals, includes, structs, cmap, all AS65 directives.
    → Backward-compatible: existing callers get identical behavior.

Assemble (sourceText, fileReader) → AssemblyResult
    → Overload accepting a file reader for include resolution.
    → File reader is injectable for test isolation.

FormatListingLine (line) → string
    → Extended with macro expansion prefix ('>'), cycle counts.
```

### AssemblerOptions (extended)

```
fillByte          : Byte     (default 0xFF; 0x00 when -z)
generateListing   : bool     (default false)
warningMode       : WarningMode
showCycleCounts   : bool     (default false; -c flag)
showMacroExpansion: bool     (default false; -m flag)
pageHeight        : int      (default 66; 0 = infinite; -h flag)
pageWidth         : int      (default 79; -w flag)
predefinedSymbols : map<string, int32_t>  (-d flag)
caseSensitive     : bool     (default false; true = AS65 default)
```

### AssemblyResult (extended)

```
success      : bool
bytes        : vector<Byte>            (flat binary image)
startAddress : Word
endAddress   : Word
symbols      : map<string, Word>       (all labels + constants)
errors       : vector<AssemblyError>
warnings     : vector<AssemblyError>
listing      : vector<AssemblyLine>
entryPoint   : Word                    (from 'end EXPR', or startAddress)
```

### ExpressionEvaluator (new)

```
Evaluate (expression, context) → ExprResult
    → Static method. Evaluates an AS65 expression string.
    → Context provides symbol table and current PC.
    → Returns value, success flag, error message.
```

### FileReader Interface (new)

```
ReadFile (filename, baseDir) → {success, contents, error}
    → Abstract interface for include file resolution.
    → Default implementation reads from filesystem.
    → Test implementation returns in-memory strings.
```

### OutputFormats (new)

```
WriteBinary   (output, stream, fillByte)       → void
WriteSRecord  (output, stream, entryPoint)     → void
WriteIntelHex (output, stream, entryPoint)     → void
    → Each writes the assembled output to the given stream.
    → output contains byte ranges with start/end addresses.
```

## CLI Contract (Casso65 executable)

### AS65-Compatible Flags

```
casso65 [-cdghilmnopqstvwxz] file

  -c           Show cycle counts in listing
  -d<name>     Define symbol (default: DEBUG=1)
  -g           Generate debug information file
  -h<lines>    Page height (0=infinite)
  -i           Case-insensitive opcodes (default in Casso65)
  -l           Generate pass 2 listing
  -l<file>     Listing file name
  -m           Show macro expansions in listing
  -n           Disable optimizations globally
  -o<file>     Output file name
  -p           Generate pass 1 listing
  -q           Quiet mode
  -s           S-record output
  -s2          Intel HEX output
  -t           Symbol table between passes
  -v           Verbose mode
  -w<width>    Column width (default 79, -w alone = 133)
  -z           Fill unused with $00 (default $FF)
```

### Existing Casso65 CLI (preserved)

```
casso65 assemble <file> [-o <outfile>] [-l <labels>] [-a] [--fill N]
casso65 run <file> [--stop ADDR] [--max-cycles N] [--entry ADDR] [--reset-vector]
casso65 --help
casso65 --version
```

### Compatibility Mode

When invoked with AS65-style flags (single-dash concatenated), Casso65 operates in AS65 compatibility mode. When invoked with spec-001-style subcommands (`assemble`, `run`), it operates in Casso65 native mode. Both modes are supported simultaneously.

## Return Codes (matching AS65)

```
0 - Assembled without errors
1 - Incorrect command-line parameter
2 - Unable to open input or output file
3 - Assembly errors
4 - Memory allocation failure
```

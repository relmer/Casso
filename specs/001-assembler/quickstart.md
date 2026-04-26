# Quickstart: 6502 Assembler

**Feature**: 001-assembler | **Date**: 2026-04-23

## Build

```powershell
# From the Casso65 workspace root
# Use VS Code build tasks (Ctrl+Shift+B) — preferred
# Or from terminal:
pwsh -NoProfile -File scripts/Build.ps1 -Configuration Debug -Platform x64
```

## Run Tests

```powershell
# Via VS Code task (preferred):
#   Run "Run Tests (current arch)" task

# Or from terminal:
pwsh -NoProfile -File scripts/RunTests.ps1 -Configuration Debug -Platform x64
```

## CLI Usage

```powershell
# Assemble a file to binary
.\x64\Debug\Casso65.exe assemble input.asm -o output.bin

# Assemble with listing and symbol table
.\x64\Debug\Casso65.exe assemble input.asm -o output.bin -l symbols.txt -a

# Assemble and run immediately
.\x64\Debug\Casso65.exe run input.asm

# Load and run a binary
.\x64\Debug\Casso65.exe run program.bin --load $8000

# Run with stop address and cycle limit
.\x64\Debug\Casso65.exe run input.asm --stop $FFFC --max-cycles 50000
```

## Programmatic Usage (in tests)

```cpp
#include "TestHelpers.h"

TEST_METHOD (AssembleAndRun_LDA_Immediate)
{
    TestCpu cpu;
    cpu.InitForTest ();

    auto result = cpu.Assemble (
        "    LDA #$42\n"
        "    STA $10\n"
        "done:\n"
        "    BRK\n"
    );

    Assert::IsTrue (result.success);

    cpu.RunUntil (cpu.LabelAddress (result, "done"));

    Assert::AreEqual ((Byte) 0x42, cpu.RegA ());
    Assert::AreEqual ((Byte) 0x42, cpu.Peek (0x10));
}
```

## Assembly Source Example

```asm
; Multiply A by 2 using shifts
    .org $8000

    LDA #$15        ; Load 21 into accumulator
    ASL A           ; Arithmetic shift left (multiply by 2)
    STA $10         ; Store result at zero-page $10
done:
    BRK             ; Stop

; Data section
    .org $9000
table:
    .byte $01, $02, $03, $04
message:
    .text "Hello"
```

## Key Files

| File | Purpose |
|---|---|
| `Casso65Core/Assembler.h` | Assembler class declaration |
| `Casso65Core/Assembler.cpp` | Two-pass assembler implementation |
| `Casso65Core/AssemblerTypes.h` | Result structs (AssemblyResult, AssemblyError, etc.) |
| `Casso65Core/Parser.h/cpp` | Line parser and operand classifier |
| `Casso65Core/OpcodeTable.h/cpp` | Reverse opcode lookup (mnemonic+mode → byte) |
| `Casso65/CommandLine.h/cpp` | CLI argument parsing |
| `Casso65/Casso65.cpp` | Main entry point with subcommand dispatch |
| `UnitTest/TestHelpers.h` | TestCpu extensions (Assemble, RunUntil) |
| `UnitTest/AssemblerTests.cpp` | Assembler unit tests |
| `UnitTest/ParserTests.cpp` | Parser unit tests |
| `UnitTest/IntegrationTests.cpp` | Assemble-and-run tests |

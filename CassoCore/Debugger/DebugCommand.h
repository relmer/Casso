#pragma once

#include "Debugger/DebugExpressionEvaluator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandMode
//
////////////////////////////////////////////////////////////////////////////////

enum class CommandMode
{
    AppleWin,
    Monitor,
    GSSquared,
};





////////////////////////////////////////////////////////////////////////////////
//
//  OutputFormat
//
//  How replies are written, separate from the mode lines are read in.
//  Changing the mode sets the format to that mode's own; OUTPUT changes the
//  format alone.
//
////////////////////////////////////////////////////////////////////////////////

enum class OutputFormat
{
    AppleWin,
    Monitor,
    GSSquared,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugVerb
//
//  Every operation the engine performs. Both command modes parse to these, so
//  an AppleWin name and a Monitor form with the same effect share one verb.
//  Count closes the list so sweeps can cover it.
//
////////////////////////////////////////////////////////////////////////////////

enum class DebugVerb
{
    None,

    // Execution
    Go,
    GoFullSpeed,
    StepInto,
    StepOver,
    StepOut,
    Trace,
    TraceToFile,
    SetProgramCounter,
    CallSubroutine,
    WriteNop,
    InjectKey,
    ShowBranchRecord,
    Profile,
    Benchmark,
    ExitBenchmark,
    ShowCycles,
    ResetCycles,
    BreakOnVideoLine,
    ShowVideoInfo,
    Pause,
    SetBudget,

    // Breakpoints and watchpoints
    SetBreakpoint,
    SetConditionalBreakpoint,
    SetRegisterBreakpoint,
    SetBreakpointAndWatchpoint,
    SetMemoryWatchpoint,
    SetReadWatchpoint,
    SetWriteWatchpoint,
    SetValueBreakpoint,
    BreakOnBrk,
    BreakOnOpcode,
    BreakOnInterrupt,
    ClearBreakpoint,
    DisableBreakpoint,
    EnableBreakpoint,
    ListBreakpoints,
    EditBreakpoint,
    ChangeBreakpoint,
    SaveBreakpoints,

    // Registers and stack
    ShowRegisters,
    SetRegister,
    ClearFlag,
    SetFlag,
    PopStack,
    PopStackWord,
    PushStack,
    ShowStack,

    // Memory
    DumpMemory,
    EnterBytes,
    EnterWords,
    PatchBytes,
    MoveMemory,
    CompareMemory,
    FillMemory,
    SearchMemory,
    SearchHex,
    ShowSearchResults,
    LoadBinary,
    SaveBinary,
    SaveText,
    ReadIo,
    WriteIo,
    ShowSwitches,

    // Disassembly and data directives
    Disassemble,
    DefineBytes,
    DefineWords,
    DefineAddress,
    DefineText,
    DefineFloat,
    RemoveData,
    ListData,
    EnterAssembler,

    // Watches, zero-page pointers and bookmarks
    AddWatch,
    ClearWatch,
    DisableWatch,
    EnableWatch,
    ListWatches,
    SaveWatches,
    AddZeroPagePointer,
    ClearZeroPagePointer,
    DisableZeroPagePointer,
    EnableZeroPagePointer,
    ListZeroPagePointers,
    SaveZeroPagePointers,
    AddBookmark,
    ClearBookmark,
    ListBookmarks,
    GoToBookmark,
    SaveBookmarks,

    // Symbols
    LoadSymbols,
    SaveSymbols,
    ClearSymbols,
    EnableSymbols,
    LookupSymbol,
    AddSymbol,
    RemoveSymbol,
    ShowSymbolInfo,
    ListSymbols,

    // Configuration and output
    PrintDirectory,
    ChangeDirectory,
    LoadConfig,
    SaveConfig,
    ConfigureDisassembly,
    RunStartup,
    RunScript,
    DiskCommand,
    Log,
    Echo,
    Print,
    PrintFormatted,
    Calculate,
    Help,
    ShowVersion,
    ShowMessageOfTheDay,

    // Monitor
    Examine,
    Deposit,
    List,
    Verify,
    Arithmetic,
    SetInverse,
    SetNormal,
    SetInputSlot,
    SetOutputSlot,
    BasicColdStart,
    BasicWarmStart,
    UserVector,
    ShowRegistersForEdit,
    EditRegisters,
    ReadFile,
    WriteFile,

    // Casso engine
    SetMode,
    ShowMode,
    ShowSource,
    SetSourceStepping,
    SetSourceBreakpoint,
    ListStepFilter,
    AddStepFilter,
    RemoveStepFilter,
    ClearStepFilter,
    ShowOutputFormat,
    SetOutputFormat,

    // Window-only views, carried by name
    View,

    Count,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugCommand
//
//  One parsed command. sourceName is the name as typed, for replies and
//  errors. A budget, when present, applies to the run this command starts.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugCommand
{
    DebugVerb                verb     = DebugVerb::None;
    std::string              sourceName;

    Word                     a1       = 0;
    Word                     a2       = 0;
    Word                     a3       = 0;
    bool                     hasA1    = false;
    bool                     hasA2    = false;
    bool                     hasA3    = false;

    std::vector<Byte>        values;
    std::vector<Byte>        mask;         // per byte of values: bits a search must match
    Expression               expression;
    std::string              text;
    uint32_t                 count    = 0;
    std::optional<uint64_t>  budget;
    CommandMode              mode     = CommandMode::AppleWin;
    OutputFormat             output   = OutputFormat::AppleWin;
};

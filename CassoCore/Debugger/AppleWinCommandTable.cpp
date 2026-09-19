#include "Pch.h"

#include "Debugger/AppleWinCommandTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kAppleWinCommands
//
//  Every name AppleWin's debugger accepts, with the engine operation it
//  performs here. Names and behavior only are taken from AppleWin's command
//  table and help pages.
//
////////////////////////////////////////////////////////////////////////////////

using V = DebugVerb;
using F = AppleWinCommandFamily;
using A = CommandAvailability;

static constexpr AppleWinCommand s_kAppleWinCommands[] =
{
    // Assembler
    { "A",           V::EnterAssembler,           F::Assembler,   A::Headless,     nullptr,     nullptr },

    // CPU
    { "=",           V::SetProgramCounter,        F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "G",           V::Go,                       F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "GG",          V::GoFullSpeed,              F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "IN",          V::ReadIo,                   F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "KEY",         V::InjectKey,                F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "JSR",         V::CallSubroutine,           F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "NOP",         V::WriteNop,                 F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "OUT",         V::WriteIo,                  F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "LBR",         V::ShowBranchRecord,         F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "PROFILE",     V::Profile,                  F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "R",           V::ShowRegisters,            F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "POP",         V::PopStack,                 F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "PPOP",        V::PopStackWord,             F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "PUSH",        V::PushStack,                F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "P",           V::StepOver,                 F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "RTS",         V::StepOut,                  F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "T",           V::StepInto,                 F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "TF",          V::TraceToFile,              F::Cpu,         A::Headless,     nullptr,     nullptr },
    { "TL",          V::StepInto,                 F::Cpu,         A::Headless,     "T",         nullptr },
    { "U",           V::Disassemble,              F::Cpu,         A::Headless,     nullptr,     nullptr },

    // Bookmarks
    { "BM",          V::AddBookmark,              F::Bookmarks,   A::Headless,     nullptr,     nullptr },
    { "BMA",         V::AddBookmark,              F::Bookmarks,   A::Headless,     nullptr,     nullptr },
    { "BMC",         V::ClearBookmark,            F::Bookmarks,   A::Headless,     nullptr,     nullptr },
    { "BML",         V::ListBookmarks,            F::Bookmarks,   A::Headless,     nullptr,     nullptr },
    { "BMG",         V::GoToBookmark,             F::Bookmarks,   A::Headless,     nullptr,     nullptr },
    { "BMSAVE",      V::SaveBookmarks,            F::Bookmarks,   A::Headless,     nullptr,     nullptr },

    // Breakpoints
    { "BRK",         V::BreakOnBrk,               F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BRKOP",       V::BreakOnOpcode,            F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BRKINT",      V::BreakOnInterrupt,         F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BP",          V::SetBreakpoint,            F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPA",         V::SetBreakpointAndWatchpoint, F::Breakpoints, A::Headless,   nullptr,     nullptr },
    { "BPR",         V::SetRegisterBreakpoint,    F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPX",         V::SetBreakpoint,            F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPIO",        V::SetMemoryWatchpoint,      F::Breakpoints, A::Headless,     "BPM",       nullptr },
    { "BPM",         V::SetMemoryWatchpoint,      F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPMR",        V::SetReadWatchpoint,        F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPMW",        V::SetWriteWatchpoint,       F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPMV",        V::SetValueBreakpoint,       F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPC",         V::ClearBreakpoint,          F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPD",         V::DisableBreakpoint,        F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPEDIT",      V::EditBreakpoint,           F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPE",         V::EnableBreakpoint,         F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPL",         V::ListBreakpoints,          F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPSAVE",      V::SaveBreakpoints,          F::Breakpoints, A::Headless,     nullptr,     nullptr },
    { "BPCHANGE",    V::ChangeBreakpoint,         F::Breakpoints, A::Headless,     nullptr,     nullptr },

    // Video timing
    { "BPV",         V::BreakOnVideoLine,         F::Video,       A::Headless,     nullptr,     nullptr },
    { "VIDEOINFO",   V::ShowVideoInfo,            F::Video,       A::Headless,     nullptr,     nullptr },

    // Config
    { "BENCHMARK",   V::Benchmark,                F::Config,      A::Headless,     nullptr,     nullptr },
    { "DISASM",      V::ConfigureDisassembly,     F::Config,      A::Headless,     nullptr,     nullptr },
    { "LOAD",        V::LoadConfig,               F::Config,      A::Headless,     nullptr,     nullptr },
    { "SAVE",        V::SaveConfig,               F::Config,      A::Headless,     nullptr,     nullptr },
    { "PWD",         V::PrintDirectory,           F::Config,      A::Headless,     nullptr,     nullptr },
    { "CD",          V::ChangeDirectory,          F::Config,      A::Headless,     nullptr,     nullptr },

    // Cycles
    { "CYCLES",      V::ShowCycles,               F::Cycles,      A::Headless,     nullptr,     nullptr },
    { "RCC",         V::ResetCycles,              F::Cycles,      A::Headless,     nullptr,     nullptr },

    // Disassembler data
    { "Z",           V::DefineBytes,              F::Data,        A::Headless,     "DB",        nullptr },
    { "X",           V::RemoveData,               F::Data,        A::Headless,     nullptr,     nullptr },
    { "B",           V::ListData,                 F::Data,        A::Headless,     nullptr,     nullptr },
    { "DB",          V::DefineBytes,              F::Data,        A::Headless,     nullptr,     nullptr },
    { "DB2",         V::DefineBytes,              F::Data,        A::Headless,     nullptr,     nullptr },
    { "DB4",         V::DefineBytes,              F::Data,        A::Headless,     nullptr,     nullptr },
    { "DB8",         V::DefineBytes,              F::Data,        A::Headless,     nullptr,     nullptr },
    { "DW",          V::DefineWords,              F::Data,        A::Headless,     nullptr,     nullptr },
    { "DW2",         V::DefineWords,              F::Data,        A::Headless,     nullptr,     nullptr },
    { "DW4",         V::DefineWords,              F::Data,        A::Headless,     nullptr,     nullptr },
    { "ASC",         V::DefineText,               F::Data,        A::Headless,     nullptr,     nullptr },
    { "DF",          V::DefineFloat,              F::Data,        A::Headless,     nullptr,     nullptr },
    { "DA",          V::DefineAddress,            F::Data,        A::Headless,     nullptr,     nullptr },

    // Disk
    { "DISK",        V::DiskCommand,              F::Disk,        A::Headless,     nullptr,     nullptr },

    // Flags
    { "CL",          V::ClearFlag,                F::Flags,       A::Headless,     nullptr,     nullptr },
    { "CLC",         V::ClearFlag,                F::Flags,       A::Headless,     nullptr,     nullptr },
    { "CLZ",         V::ClearFlag,                F::Flags,       A::Headless,     nullptr,     nullptr },
    { "CLI",         V::ClearFlag,                F::Flags,       A::Headless,     nullptr,     nullptr },
    { "CLD",         V::ClearFlag,                F::Flags,       A::Headless,     nullptr,     nullptr },
    { "CLB",         V::ClearFlag,                F::Flags,       A::Headless,     nullptr,     nullptr },
    { "CLR",         V::ClearFlag,                F::Flags,       A::Headless,     nullptr,     nullptr },
    { "CLV",         V::ClearFlag,                F::Flags,       A::Headless,     nullptr,     nullptr },
    { "CLN",         V::ClearFlag,                F::Flags,       A::Headless,     nullptr,     nullptr },
    { "SE",          V::SetFlag,                  F::Flags,       A::Headless,     nullptr,     nullptr },
    { "SEC",         V::SetFlag,                  F::Flags,       A::Headless,     nullptr,     nullptr },
    { "SEZ",         V::SetFlag,                  F::Flags,       A::Headless,     nullptr,     nullptr },
    { "SEI",         V::SetFlag,                  F::Flags,       A::Headless,     nullptr,     nullptr },
    { "SED",         V::SetFlag,                  F::Flags,       A::Headless,     nullptr,     nullptr },
    { "SEB",         V::SetFlag,                  F::Flags,       A::Headless,     nullptr,     nullptr },
    { "SER",         V::SetFlag,                  F::Flags,       A::Headless,     nullptr,     nullptr },
    { "SEV",         V::SetFlag,                  F::Flags,       A::Headless,     nullptr,     nullptr },
    { "SEN",         V::SetFlag,                  F::Flags,       A::Headless,     nullptr,     nullptr },

    // Help
    { "?",           V::Help,                     F::Help,        A::Headless,     nullptr,     nullptr },
    { "HELP",        V::Help,                     F::Help,        A::Headless,     nullptr,     nullptr },
    { "VERSION",     V::ShowVersion,              F::Help,        A::Headless,     nullptr,     nullptr },
    { "MOTD",        V::ShowMessageOfTheDay,      F::Help,        A::Headless,     nullptr,     nullptr },

    // Memory
    { "MC",          V::CompareMemory,            F::Memory,      A::Headless,     nullptr,     nullptr },
    { "ME",          V::EnterBytes,               F::Memory,      A::Headless,     nullptr,     nullptr },
    { "MEB",         V::EnterBytes,               F::Memory,      A::Headless,     nullptr,     nullptr },
    { "MEW",         V::EnterWords,               F::Memory,      A::Headless,     nullptr,     nullptr },
    { "BLOAD",       V::LoadBinary,               F::Memory,      A::Headless,     nullptr,     nullptr },
    { "M",           V::MoveMemory,               F::Memory,      A::Headless,     nullptr,     nullptr },
    { "BSAVE",       V::SaveBinary,               F::Memory,      A::Headless,     nullptr,     nullptr },
    { "S",           V::SearchMemory,             F::Memory,      A::Headless,     nullptr,     nullptr },
    { "@",           V::ShowSearchResults,        F::Memory,      A::Headless,     nullptr,     nullptr },
    { "SH",          V::SearchHex,                F::Memory,      A::Headless,     nullptr,     nullptr },
    { "F",           V::FillMemory,               F::Memory,      A::Headless,     nullptr,     nullptr },
    { "TSAVE",       V::SaveText,                 F::Memory,      A::Headless,     nullptr,     nullptr },
    { "D",           V::DumpMemory,               F::Memory,      A::Headless,     nullptr,     nullptr },

    // Output and scripts
    { "CALC",        V::Calculate,                F::Output,      A::Headless,     nullptr,     nullptr },
    { "ECHO",        V::Echo,                     F::Output,      A::Headless,     nullptr,     nullptr },
    { "LOG",         V::Log,                      F::Output,      A::Headless,     nullptr,     nullptr },
    { "PRINT",       V::Print,                    F::Output,      A::Headless,     nullptr,     nullptr },
    { "PRINTF",      V::PrintFormatted,           F::Output,      A::Headless,     nullptr,     nullptr },
    { "RUN",         V::RunScript,                F::Output,      A::Headless,     nullptr,     nullptr },

    // Symbols
    { "SYM",         V::LookupSymbol,             F::Symbols,     A::Headless,     nullptr,     nullptr },
    { "SYMMAIN",     V::LookupSymbol,             F::Symbols,     A::Headless,     nullptr,     nullptr },
    { "SYMBASIC",    V::LookupSymbol,             F::Symbols,     A::Headless,     nullptr,     nullptr },
    { "SYMASM",      V::LookupSymbol,             F::Symbols,     A::Headless,     nullptr,     nullptr },
    { "SYMUSER",     V::LookupSymbol,             F::Symbols,     A::Headless,     nullptr,     nullptr },
    { "SYMUSER2",    V::LookupSymbol,             F::Symbols,     A::Headless,     nullptr,     nullptr },
    { "SYMSRC",      V::LookupSymbol,             F::Symbols,     A::Headless,     nullptr,     nullptr },
    { "SYMSRC2",     V::LookupSymbol,             F::Symbols,     A::Headless,     nullptr,     nullptr },
    { "SYMDOS33",    V::LookupSymbol,             F::Symbols,     A::Headless,     nullptr,     nullptr },
    { "SYMPRODOS",   V::LookupSymbol,             F::Symbols,     A::Headless,     nullptr,     nullptr },
    { "SYMINFO",     V::ShowSymbolInfo,           F::Symbols,     A::Headless,     nullptr,     nullptr },
    { "SYMLIST",     V::ListSymbols,              F::Symbols,     A::Headless,     nullptr,     nullptr },

    // Watches
    { "W",           V::AddWatch,                 F::Watch,       A::Headless,     nullptr,     nullptr },
    { "WA",          V::AddWatch,                 F::Watch,       A::Headless,     nullptr,     nullptr },
    { "WC",          V::ClearWatch,               F::Watch,       A::Headless,     nullptr,     nullptr },
    { "WD",          V::DisableWatch,             F::Watch,       A::Headless,     nullptr,     nullptr },
    { "WE",          V::EnableWatch,              F::Watch,       A::Headless,     nullptr,     nullptr },
    { "WL",          V::ListWatches,              F::Watch,       A::Headless,     nullptr,     nullptr },
    { "WSAVE",       V::SaveWatches,              F::Watch,       A::Headless,     nullptr,     nullptr },

    // Zero-page pointers
    { "ZP",          V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZP0",         V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZP1",         V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZP2",         V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZP3",         V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZP4",         V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZP5",         V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZP6",         V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZP7",         V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZPA",         V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZPC",         V::ClearZeroPagePointer,     F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZPD",         V::DisableZeroPagePointer,   F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZPE",         V::EnableZeroPagePointer,    F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZPL",         V::ListZeroPagePointers,     F::ZeroPage,    A::Headless,     nullptr,     nullptr },
    { "ZPSAVE",      V::SaveZeroPagePointers,     F::ZeroPage,    A::Headless,     nullptr,     nullptr },

    // Startup
    { "STARTUP",     V::RunStartup,               F::Startup,     A::Headless,     nullptr,     nullptr },

    // Aliases
    { "INPUT",       V::ReadIo,                   F::Cpu,         A::Headless,     "IN",        nullptr },
    { "RC",          V::ClearFlag,                F::Flags,       A::Headless,     "CLC",       nullptr },
    { "RZ",          V::ClearFlag,                F::Flags,       A::Headless,     "CLZ",       nullptr },
    { "RI",          V::ClearFlag,                F::Flags,       A::Headless,     "CLI",       nullptr },
    { "RD",          V::ClearFlag,                F::Flags,       A::Headless,     "CLD",       nullptr },
    { "RB",          V::ClearFlag,                F::Flags,       A::Headless,     "CLB",       nullptr },
    { "RR",          V::ClearFlag,                F::Flags,       A::Headless,     "CLR",       nullptr },
    { "RV",          V::ClearFlag,                F::Flags,       A::Headless,     "CLV",       nullptr },
    { "RN",          V::ClearFlag,                F::Flags,       A::Headless,     "CLN",       nullptr },
    { "SC",          V::SetFlag,                  F::Flags,       A::Headless,     "SEC",       nullptr },
    { "SZ",          V::SetFlag,                  F::Flags,       A::Headless,     "SEZ",       nullptr },
    { "SI",          V::SetFlag,                  F::Flags,       A::Headless,     "SEI",       nullptr },
    { "SD",          V::SetFlag,                  F::Flags,       A::Headless,     "SED",       nullptr },
    { "SB",          V::SetFlag,                  F::Flags,       A::Headless,     "SEB",       nullptr },
    { "SR",          V::SetFlag,                  F::Flags,       A::Headless,     "SER",       nullptr },
    { "SV",          V::SetFlag,                  F::Flags,       A::Headless,     "SEV",       nullptr },
    { "SN",          V::SetFlag,                  F::Flags,       A::Headless,     "SEN",       nullptr },
    { "ME8",         V::EnterBytes,               F::Memory,      A::Headless,     "MEB",       nullptr },
    { "ME16",        V::EnterWords,               F::Memory,      A::Headless,     "MEW",       nullptr },
    { "MM",          V::MoveMemory,               F::Memory,      A::Headless,     "M",         nullptr },
    { "MS",          V::SearchMemory,             F::Memory,      A::Headless,     "S",         nullptr },
    { "P0",          V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     "ZP0",       nullptr },
    { "P1",          V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     "ZP1",       nullptr },
    { "P2",          V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     "ZP2",       nullptr },
    { "P3",          V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     "ZP3",       nullptr },
    { "P4",          V::AddZeroPagePointer,       F::ZeroPage,    A::Headless,     "ZP4",       nullptr },
    { "REGISTER",    V::ShowRegisters,            F::Cpu,         A::Headless,     "R",         nullptr },
    { "TRACE",       V::StepInto,                 F::Cpu,         A::Headless,     "T",         nullptr },
    { "SYMDOS",      V::LookupSymbol,             F::Symbols,     A::Headless,     "SYMDOS33",  nullptr },
    { "SYMPRO",      V::LookupSymbol,             F::Symbols,     A::Headless,     "SYMPRODOS", nullptr },
    { "ZAP",         V::WriteNop,                 F::Cpu,         A::Headless,     "NOP",       nullptr },

    // Deprecated
    { "BENCH",       V::Benchmark,                F::Config,      A::Headless,     "BENCHMARK", nullptr },
    { "EXITBENCH",   V::ExitBenchmark,            F::Config,      A::Headless,     nullptr,     nullptr },
    { "MDB",         V::DumpMemory,               F::Memory,      A::Headless,     "D",         nullptr },

    // Casso engine commands
    { "MODE",        V::ShowMode,                 F::Engine,      A::Headless,     nullptr,     nullptr },
    { "PAUSE",       V::Pause,                    F::Engine,      A::Headless,     nullptr,     nullptr },
    { "BUDGET",      V::SetBudget,                F::Engine,      A::Headless,     nullptr,     nullptr },
    { "SWITCHES",    V::ShowSwitches,             F::Engine,      A::Headless,     nullptr,     nullptr },
    { "STACK",       V::ShowStack,                F::Engine,      A::Headless,     nullptr,     nullptr },
    { "PATCH",       V::PatchBytes,               F::Memory,      A::Headless,     nullptr,     nullptr },
    { "SRC",         V::ShowSource,               F::Engine,      A::Headless,     nullptr,     nullptr },
    { "SKIP",        V::ListStepFilter,           F::Engine,      A::Headless,     nullptr,     nullptr },
    { "CALLS",       V::ShowCallStack,            F::Engine,      A::Headless,     nullptr,     nullptr },
    { "HISTORY",     V::ShowHistory,              F::Engine,      A::Headless,     nullptr,     nullptr },
    { "PANEL",       V::ListPanels,               F::Engine,      A::Headless,     nullptr,     nullptr },

    // Window only: cursor
    { ".",           V::View,                     F::Cursor,      A::WindowOnly,   nullptr,     nullptr },
    { "RET",         V::View,                     F::Cursor,      A::WindowOnly,   nullptr,     nullptr },
    { "^",           V::View,                     F::Cursor,      A::WindowOnly,   nullptr,     nullptr },
    { "V",           V::View,                     F::Cursor,      A::WindowOnly,   nullptr,     nullptr },
    { "->",          V::View,                     F::Cursor,      A::WindowOnly,   nullptr,     nullptr },
    { "PAGEUP",      V::View,                     F::Cursor,      A::WindowOnly,   nullptr,     nullptr },
    { "PAGEUP256",   V::View,                     F::Cursor,      A::WindowOnly,   nullptr,     nullptr },
    { "PAGEUP4K",    V::View,                     F::Cursor,      A::WindowOnly,   nullptr,     nullptr },
    { "PAGEDN",      V::View,                     F::Cursor,      A::WindowOnly,   nullptr,     nullptr },
    { "PAGEDOWN256", V::View,                     F::Cursor,      A::WindowOnly,   nullptr,     nullptr },
    { "PAGEDOWN4K",  V::View,                     F::Cursor,      A::WindowOnly,   nullptr,     nullptr },

    // Window only: panes
    { "WIN",         V::View,                     F::Window,      A::WindowOnly,   nullptr,     nullptr },
    { "WINDOW",      V::View,                     F::Window,      A::WindowOnly,   nullptr,     nullptr },
    { "CODE",        V::View,                     F::Window,      A::WindowOnly,   nullptr,     nullptr },
    { "CODE1",       V::View,                     F::Window,      A::WindowOnly,   nullptr,     nullptr },
    { "CODE2",       V::View,                     F::Window,      A::WindowOnly,   nullptr,     nullptr },
    { "CONSOLE",     V::View,                     F::Window,      A::WindowOnly,   nullptr,     nullptr },
    { "DATA",        V::View,                     F::Window,      A::WindowOnly,   nullptr,     nullptr },
    { "DATA1",       V::View,                     F::Window,      A::WindowOnly,   nullptr,     nullptr },
    { "DATA2",       V::View,                     F::Window,      A::WindowOnly,   nullptr,     nullptr },
    { "SOURCE1",     V::View,                     F::Window,      A::WindowOnly,   nullptr,     nullptr },
    { "SOURCE2",     V::View,                     F::Window,      A::WindowOnly,   nullptr,     nullptr },
    { "\\",          V::View,                     F::Window,      A::WindowOnly,   nullptr,     nullptr },

    // Window only: mini memory panes
    { "MD1",         V::View,                     F::MiniMemory,  A::WindowOnly,   nullptr,     nullptr },
    { "MD2",         V::View,                     F::MiniMemory,  A::WindowOnly,   nullptr,     nullptr },
    { "MA1",         V::View,                     F::MiniMemory,  A::WindowOnly,   nullptr,     nullptr },
    { "MA2",         V::View,                     F::MiniMemory,  A::WindowOnly,   nullptr,     nullptr },
    { "MT1",         V::View,                     F::MiniMemory,  A::WindowOnly,   nullptr,     nullptr },
    { "MT2",         V::View,                     F::MiniMemory,  A::WindowOnly,   nullptr,     nullptr },
    { "M1",          V::View,                     F::MiniMemory,  A::WindowOnly,   nullptr,     nullptr },
    { "M2",          V::View,                     F::MiniMemory,  A::WindowOnly,   nullptr,     nullptr },

    // Window only: screen views
    { "TEXT",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "TEXT1",       V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "TEXT2",       V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "TEXT80",      V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "TEXT81",      V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "TEXT82",      V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "TEXT40",      V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "TEXT41",      V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "TEXT42",      V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "GR",          V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "GR1",         V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "GR2",         V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "DGR",         V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "DGR1",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "DGR2",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "HGR",         V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "HGR0",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "HGR1",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "HGR2",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "HGR3",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "HGR4",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "HGR5",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "HGR6",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "HGR7",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "HGR8",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "DHGR",        V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "DHGR1",       V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },
    { "DHGR2",       V::View,                     F::Views,       A::WindowOnly,   nullptr,     nullptr },

    // Window only: appearance
    { "BW",          V::View,                     F::Appearance,  A::WindowOnly,   nullptr,     nullptr },
    { "COLOR",       V::View,                     F::Appearance,  A::WindowOnly,   nullptr,     nullptr },
    { "FONT",        V::View,                     F::Appearance,  A::WindowOnly,   nullptr,     nullptr },
    { "HCOLOR",      V::View,                     F::Appearance,  A::WindowOnly,   nullptr,     nullptr },
    { "MONO",        V::View,                     F::Appearance,  A::WindowOnly,   nullptr,     nullptr },

    // Not available
    { "SHR",         V::None,                     F::Unsupported, A::NotAvailable, nullptr,     "SHR needs a machine with Super Hi-Res." },
    { "SOURCE",      V::None,                     F::Unsupported, A::NotAvailable, nullptr,     "SOURCE needs a link to an assembler listing." },
    { "SYNC",        V::None,                     F::Unsupported, A::NotAvailable, nullptr,     "SYNC needs a link to an assembler listing." },
    { "NTSC",        V::None,                     F::Unsupported, A::NotAvailable, nullptr,     "NTSC reads an AppleWin palette file, which Casso does not use." },
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinCommandTable::Find
//
////////////////////////////////////////////////////////////////////////////////

const AppleWinCommand * AppleWinCommandTable::Find (const std::string & name)
{
    std::string  upper (name);



    for (char & ch : upper)
    {
        ch = (char) toupper ((unsigned char) ch);
    }

    for (const AppleWinCommand & command : s_kAppleWinCommands)
    {
        if (upper == command.name)
        {
            return &command;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppleWinCommandTable::GetAll
//
////////////////////////////////////////////////////////////////////////////////

std::span<const AppleWinCommand> AppleWinCommandTable::GetAll()
{
    return s_kAppleWinCommands;
}

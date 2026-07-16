#include "Pch.h"

#include "HeadlessHost.h"
#include "Core/GamePatcher.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


////////////////////////////////////////////////////////////////////////////////
//
//  GamePatcherTests
//
//  Unit coverage for the pattern->fixup table. The built-in rule is the Apple
//  //c VBL-spin defuser: `LDA $C019 / BMI *-3` (AD 19 C0 30 FB), NOP the BMI.
//  Karateka ships five copies of that idiom and hangs on the //c (sticky
//  VBL-interrupt latch at $C019); the patcher NOPs them so the wait falls
//  through. These tests drive the scanner against a real MemoryBus (a headless
//  //e whose $0000-$BFFF is plain main RAM) rather than the game end to end.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    static constexpr Word   kVblLo = 0x0400;   // scan window used by the tests
    static constexpr Word   kVblHi = 0xBFF9;

    void WriteRam (EmulatorCore & core, Word at, std::initializer_list<Byte> bytes)
    {
        Word   a = at;

        for (Byte b : bytes)
        {
            core.bus->WriteByte (a++, b);
        }
    }
}


TEST_CLASS (GamePatcherTests)
{
public:

    // The BMI branch (bytes 3-4 of the match) becomes two NOPs; the
    // LDA $C019 that precedes it is left intact so the read still happens.
    TEST_METHOD (VblSpin_NopsTheBranchOnly)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        Assert::IsTrue (SUCCEEDED (host.BuildApple2e (core)), L"BuildApple2e");
        WriteRam (core, 0x2000, { 0xAD, 0x19, 0xC0, 0x30, 0xFB });

        GamePatcher   patcher;
        patcher.AddApple2cVblSpin ();

        Assert::AreEqual (1, patcher.Scan (*core.bus, kVblLo, kVblHi), L"one site");
        Assert::AreEqual<Byte> (0xAD, core.bus->ReadByte (0x2000), L"LDA kept");
        Assert::AreEqual<Byte> (0x19, core.bus->ReadByte (0x2001), L"operand kept");
        Assert::AreEqual<Byte> (0xC0, core.bus->ReadByte (0x2002), L"operand kept");
        Assert::AreEqual<Byte> (0xEA, core.bus->ReadByte (0x2003), L"BMI -> NOP");
        Assert::AreEqual<Byte> (0xEA, core.bus->ReadByte (0x2004), L"offset -> NOP");
    }


    // Re-scanning a patched site is a no-op: the NOPs no longer match the
    // signature, so periodic per-frame scans stay cheap and stable.
    TEST_METHOD (Scan_IsIdempotent)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        Assert::IsTrue (SUCCEEDED (host.BuildApple2e (core)), L"BuildApple2e");
        WriteRam (core, 0x3000, { 0xAD, 0x19, 0xC0, 0x30, 0xFB });

        GamePatcher   patcher;
        patcher.AddApple2cVblSpin ();

        Assert::AreEqual (1, patcher.Scan (*core.bus, kVblLo, kVblHi), L"first patches");
        Assert::AreEqual (0, patcher.Scan (*core.bus, kVblLo, kVblHi), L"second no-op");
        Assert::AreEqual (1, patcher.TotalApplied (), L"running total holds at 1");
    }


    // Every occurrence in the window is patched in a single pass -- Karateka
    // has five, so the scanner must not stop at the first hit.
    TEST_METHOD (Scan_PatchesEverySite)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        Assert::IsTrue (SUCCEEDED (host.BuildApple2e (core)), L"BuildApple2e");
        WriteRam (core, 0x0700, { 0xAD, 0x19, 0xC0, 0x30, 0xFB });
        WriteRam (core, 0x7A00, { 0xAD, 0x19, 0xC0, 0x30, 0xFB });

        GamePatcher   patcher;
        patcher.AddApple2cVblSpin ();

        Assert::AreEqual (2, patcher.Scan (*core.bus, kVblLo, kVblHi), L"both sites");
        Assert::AreEqual<Byte> (0xEA, core.bus->ReadByte (0x0703), L"site 1 NOP'd");
        Assert::AreEqual<Byte> (0xEA, core.bus->ReadByte (0x7A03), L"site 2 NOP'd");
    }


    // A near-miss must be left alone: `LDA $C019 / BPL` (wait-while-clear, the
    // non-hanging polarity) and a bare `LDA $C019` are legitimate and untouched.
    TEST_METHOD (Scan_LeavesNonMatchesAlone)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        Assert::IsTrue (SUCCEEDED (host.BuildApple2e (core)), L"BuildApple2e");
        WriteRam (core, 0x2000, { 0xAD, 0x19, 0xC0, 0x10, 0xFB });         // BPL, not BMI
        WriteRam (core, 0x2100, { 0xAD, 0x19, 0xC0, 0x8D, 0x00, 0x04 });   // LDA then STA

        GamePatcher   patcher;
        patcher.AddApple2cVblSpin ();

        Assert::AreEqual (0, patcher.Scan (*core.bus, kVblLo, kVblHi), L"no false hits");
        Assert::AreEqual<Byte> (0x10, core.bus->ReadByte (0x2003), L"BPL untouched");
        Assert::AreEqual<Byte> (0x8D, core.bus->ReadByte (0x2103), L"STA untouched");
    }


    // A patcher with no rules never touches memory (the state every non-//c
    // machine runs in, so the per-frame ScanRam stays a cheap no-op).
    TEST_METHOD (NoRules_LeavesMemoryUntouched)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        Assert::IsTrue (SUCCEEDED (host.BuildApple2e (core)), L"BuildApple2e");
        WriteRam (core, 0x2000, { 0xAD, 0x19, 0xC0, 0x30, 0xFB });

        GamePatcher   patcher;   // no rules installed

        Assert::AreEqual (0, patcher.ScanRam (*core.bus), L"no rules -> no patches");
        Assert::AreEqual<Byte> (0x30, core.bus->ReadByte (0x2003), L"byte unchanged");
    }


    // Clear() drops the rules (machine switch away from the //c) so a later
    // scan is inert even though a signature is present in RAM.
    TEST_METHOD (Clear_DisablesPatching)
    {
        HeadlessHost   host;
        EmulatorCore   core;

        Assert::IsTrue (SUCCEEDED (host.BuildApple2e (core)), L"BuildApple2e");
        WriteRam (core, 0x2000, { 0xAD, 0x19, 0xC0, 0x30, 0xFB });

        GamePatcher   patcher;
        patcher.AddApple2cVblSpin ();
        patcher.Clear ();

        Assert::AreEqual<size_t> (0, patcher.RuleCount (), L"rules cleared");
        Assert::AreEqual (0, patcher.ScanRam (*core.bus), L"cleared patcher is inert");
        Assert::AreEqual<Byte> (0x30, core.bus->ReadByte (0x2003), L"byte unchanged");
    }
};

#include "Pch.h"

#include "WatchMonitorRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace WatchMonitorTests;





////////////////////////////////////////////////////////////////////////////////
//
//  ZeroPagePointerHandlersTests
//
//  The zero-page pointer commands -- ZP, ZPA, ZP0-ZP7 and P0-P4, ZPL, ZPC,
//  ZPD and ZPE, and ZPSAVE -- over a real machine, grouped by command, each in
//  every dialect that reaches it.
//
//  A pointer shows the word its two zero-page bytes hold, which is the
//  address a `(zp),Y` instruction would use.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ZeroPagePointerHandlersTests)
    {
    public:

        //  $36 holds $FDF0, $3C holds $0300, $50 holds $1234.
        static void SetPointers (SessionRig & rig)
        {
            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.Poke (0x003C, { 0x00, 0x03 });
            rig.Poke (0x0050, { 0x34, 0x12 });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  AddZeroPagePointer
        //
        ////////////////////////////////////////////////////////////////////////

        //  ZP and ZPA take the lowest free slot and reply with that entry. An
        //  address already pointed at is a second entry.
        TEST_METHOD (Add_TakesTheLowestFreeSlot)
        {
            SessionRig  rig;



            SetPointers (rig);

            rig.RunIs ("ZP 36",   { "#0 $0036 -> $FDF0" });
            rig.RunIs ("ZPA $3C", { "#1 $003C -> $0300" });
            rig.RunIs ("zpa 50",  { "#2 $0050 -> $1234" });
            rig.RunIs ("ZP 36",   { "#3 $0036 -> $FDF0" });

            rig.RunOk ("ZPC 1");
            rig.RunIs ("ZPA 50",  { "#1 $0050 -> $1234" });
        }



        //  ZP0-ZP7 and P0-P4 fill the slot the name carries, in either case,
        //  and the list stays in slot order.
        TEST_METHOD (Add_SlotFormsFillTheirSlot)
        {
            SessionRig  rig;



            SetPointers (rig);

            rig.RunIs ("ZP3 3C", { "#3 $003C -> $0300" });
            rig.RunIs ("P2 36",  { "#2 $0036 -> $FDF0" });
            rig.RunIs ("ZP7 50", { "#7 $0050 -> $1234" });
            rig.RunIs ("ZP0 36", { "#0 $0036 -> $FDF0" });
            rig.RunIs ("P4 3C",  { "#4 $003C -> $0300" });
            rig.RunIs ("zp5 50", { "#5 $0050 -> $1234" });
            rig.RunIs ("p1 36",  { "#1 $0036 -> $FDF0" });

            rig.RunIs ("ZPL", { "#0 $0036 -> $FDF0",
                                "#1 $0036 -> $FDF0",
                                "#2 $0036 -> $FDF0",
                                "#3 $003C -> $0300",
                                "#4 $003C -> $0300",
                                "#5 $0050 -> $1234",
                                "#7 $0050 -> $1234" });

            //  P0 is ZP0 and replaces it.
            rig.RunIs ("P0 50",  { "#0 $0050 -> $1234" });
            rig.RunIs ("ZPA 3C", { "#6 $003C -> $0300" });
        }



        //  Eight slots, and no ninth: with all eight set, only a slot form
        //  changes anything.
        TEST_METHOD (Add_WithEverySlotSet_IsAnError)
        {
            SessionRig  rig;



            SetPointers (rig);

            for (int i = 0; i < 8; ++i)
            {
                rig.RunIs ("ZP 36", { std::format ("#{} $0036 -> $FDF0", i) });
            }

            rig.RunFails ("ZP 3C",  "zero-page pointers full", "All eight zero-page pointers are set. Clear one with ZPC, or replace one with ZP0 to ZP7.");
            rig.RunFails ("ZPA 3C", "zero-page pointers full", "All eight zero-page pointers are set. Clear one with ZPC, or replace one with ZP0 to ZP7.");

            rig.RunIs ("ZP6 3C", { "#6 $003C -> $0300" });
            Assert::AreEqual ((size_t) 8, rig.RunOk ("ZPL").text.size());
        }



        //  Setting a slot again replaces its address and enables it.
        TEST_METHOD (Add_ReplacingASlot_EnablesIt)
        {
            SessionRig  rig;



            SetPointers (rig);

            rig.RunOk ("ZP3 36");
            rig.RunIs ("ZPD 3",  { "#3 $0036 -> $FDF0 (disabled)" });
            rig.RunIs ("ZP3 3C", { "#3 $003C -> $0300" });
            rig.RunIs ("ZPL",    { "#3 $003C -> $0300" });
        }



        //  A POINTER AT $FF TAKES ITS HIGH BYTE FROM $00, as the 6502's
        //  `($FF),Y` does, never from $0100.
        TEST_METHOD (Add_APointerAtFF_WrapsWithinZeroPage)
        {
            SessionRig  rig;



            rig.Poke (0x00FF, { 0x34 });
            rig.Poke (0x0000, { 0x12 });
            rig.Poke (0x0100, { 0x99 });

            rig.RunIs ("ZP FF", { "#0 $00FF -> $1234" });
        }



        //  A zero-page pointer is in zero page.
        TEST_METHOD (Add_AnAddressOutsideZeroPage_IsAnError)
        {
            SessionRig  rig;



            rig.RunFails ("ZP 100",    "invalid arguments", "$0100 is not in zero page. A zero-page pointer is at an address from $00 to $FF.");
            rig.RunFails ("ZP3 1234",  "invalid arguments", "$1234 is not in zero page. A zero-page pointer is at an address from $00 to $FF.");
            rig.RunFails ("ZPA FFFF",  "invalid arguments", "$FFFF is not in zero page. A zero-page pointer is at an address from $00 to $FF.");
            rig.RunFails ("ZP nosuch", "invalid arguments", "nosuch is not a symbol, register, or hex number.");

            rig.RunIs ("ZPL", { "No zero-page pointers." });
        }



        //  With no address, every form lists and none adds: ZP3 alone does not
        //  set slot 3.
        TEST_METHOD (Add_WithNoAddress_Lists)
        {
            SessionRig  rig;
            Reply       reply;



            SetPointers (rig);

            for (const char * line : { "ZP", "ZPA", "ZP3", "P2" })
            {
                rig.RunIs (line, { "No zero-page pointers." });
            }

            rig.RunOk ("ZP 36");

            for (const char * line : { "ZP", "ZPA", "ZP3", "P2", "ZP0", "P0" })
            {
                rig.RunIs (line, { "#0 $0036 -> $FDF0" });
            }

            reply = rig.RunOk ("ZP7");
            Assert::IsTrue (reply.verb == DebugVerb::ListZeroPagePointers, L"an add with nothing to add is a list");
        }



        TEST_METHOD (Add_EveryDialect)
        {
            SessionRig  rig;



            SetPointers (rig);

            rig.RunOk ("MODE CASSO");
            rig.RunIs ("ZP 36",      { "#0 $0036 -> $FDF0" });

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/ZP3 3C",    { "#3 $003C -> $0300" });
            rig.RunIs ("/ZPA 50",    { "#1 $0050 -> $1234" });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!P2 0x36",   { "#2 $0036 -> $FDF0" });
            rig.RunIs ("!ZP 3C",     { "#4 $003C -> $0300" });

            rig.RunOk ("!MODE GSSQUARED");
            rig.RunIs ("ZP5 50",     { "#5 $0050 -> $1234" });
            rig.RunIs ("ZPA 36",     { "#6 $0036 -> $FDF0" });

            rig.RunOk ("MODE APPLEWIN");
            rig.RunIs ("ZPL", { "#0 $0036 -> $FDF0",
                                "#1 $0050 -> $1234",
                                "#2 $0036 -> $FDF0",
                                "#3 $003C -> $0300",
                                "#4 $003C -> $0300",
                                "#5 $0050 -> $1234",
                                "#6 $0036 -> $FDF0" });
        }



        //  The console and a channel client share the list.
        TEST_METHOD (Add_FromTheConsoleAndThePipe)
        {
            WindowRig  rig;



            rig.rig.machine.GetMemoryBus().WriteByte (0x0036, 0xF0);
            rig.rig.machine.GetMemoryBus().WriteByte (0x0037, 0xFD);

            rig.TypeIs ("ZP3 36", { "#3 $0036 -> $FDF0" });

            Assert::AreEqual (std::string (R"json({"type":"reply","id":1,"status":"ok","command":"ZP 36","data":{"kind":"zeroPageList","entries":[{"id":0,"address":54,"enabled":true,"value":65008}]},"text":["#0 $0036 -> $FDF0"]})json"),
                              rig.Request ("ZP 36"));

            rig.TypeIs ("ZPL", { "#0 $0036 -> $FDF0", "#3 $0036 -> $FDF0" });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ListZeroPagePointers
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (List_MarksDisabled_AndReadsAgain)
        {
            SessionRig  rig;



            rig.RunIs ("ZPL", { "No zero-page pointers." });

            SetPointers (rig);
            rig.RunOk ("ZP 36");
            rig.RunOk ("ZP 3C");
            rig.RunOk ("ZPD 1");

            rig.RunIs ("ZPL", { "#0 $0036 -> $FDF0", "#1 $003C -> $0300 (disabled)" });

            rig.Poke (0x003C, { 0x00, 0x04 });
            rig.RunIs ("ZPL anything", { "#0 $0036 -> $FDF0", "#1 $003C -> $0400 (disabled)" });

            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"ZPL","data":{"kind":"zeroPageList","entries":[{"id":0,"address":54,"enabled":true,"value":65008},{"id":1,"address":60,"enabled":false,"value":1024}]},"text":["#0 $0036 -> $FDF0","#1 $003C -> $0400 (disabled)"]})json"),
                              rig.Json ("ZPL"));
        }



        //  No format but AppleWin's has a layout for pointers, so each writes
        //  AppleWin's.
        TEST_METHOD (List_InEachOutputFormat)
        {
            SessionRig  rig;



            SetPointers (rig);
            rig.RunOk ("ZP 36");

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/ZPL", { "#0 $0036 -> $FDF0" });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!ZPL", { "#0 $0036 -> $FDF0" });

            rig.RunOk ("!MODE GSSQUARED");
            rig.RunIs ("ZPL",  { "#0 $0036 -> $FDF0" });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ClearZeroPagePointer
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Clear_OneOrAll)
        {
            SessionRig  rig;



            SetPointers (rig);
            rig.RunOk ("ZP 36");
            rig.RunOk ("ZP3 3C");
            rig.RunOk ("ZP 50");

            rig.RunIs ("ZPC 3", { "Zero-page pointer #3 cleared." });
            rig.RunIs ("ZPL",   { "#0 $0036 -> $FDF0", "#1 $0050 -> $1234" });

            rig.RunIs ("ZPC *", { "No zero-page pointers." });
            rig.RunIs ("ZP 3C", { "#0 $003C -> $0300" });

            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"ZPC 0","data":{"kind":"message"},"text":["Zero-page pointer #0 cleared."]})json"),
                              rig.Json ("ZPC 0"));
        }



        TEST_METHOD (Clear_DisableEnable_Errors)
        {
            SessionRig  rig;



            rig.RunOk ("ZP 36");

            for (const char * verb : { "ZPC", "ZPD", "ZPE" })
            {
                rig.RunFails (std::format ("{} 9", verb), "no such zero-page pointer", "There is no zero-page pointer #9.");
                rig.RunFails (verb,                        "invalid arguments",         "Give an id, or * for all.");
                rig.RunFails (std::format ("{} x", verb), "invalid arguments",         "x is not an id.");
            }

            Assert::AreEqual ((size_t) 1, rig.RunOk ("ZPL").text.size(), L"no error cleared anything");
        }



        TEST_METHOD (Clear_EveryDialect)
        {
            SessionRig  rig;



            rig.RunOk ("ZP 36");
            rig.RunOk ("ZP 36");
            rig.RunOk ("ZP 36");

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/ZPC 0", { "Zero-page pointer #0 cleared." });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!ZPC 1", { "Zero-page pointer #1 cleared." });

            rig.RunOk ("!MODE GSSQUARED");
            rig.RunIs ("ZPC *",  { "No zero-page pointers." });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  DisableZeroPagePointer and EnableZeroPagePointer
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DisableAndEnable_OneOrAll)
        {
            SessionRig  rig;



            rig.RunIs ("ZPD *", { "No zero-page pointers." });
            rig.RunIs ("ZPE *", { "No zero-page pointers." });

            SetPointers (rig);
            rig.RunOk ("ZP 36");
            rig.RunOk ("ZP 3C");

            rig.RunIs ("ZPD 1", { "#1 $003C -> $0300 (disabled)" });
            rig.RunIs ("ZPE 1", { "#1 $003C -> $0300" });
            rig.RunIs ("ZPD *", { "#0 $0036 -> $FDF0 (disabled)", "#1 $003C -> $0300 (disabled)" });
            rig.RunIs ("ZPE *", { "#0 $0036 -> $FDF0", "#1 $003C -> $0300" });

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/ZPD 0", { "#0 $0036 -> $FDF0 (disabled)" });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!ZPE 0", { "#0 $0036 -> $FDF0" });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  SaveZeroPagePointers
        //
        ////////////////////////////////////////////////////////////////////////

        //  The script keeps each pointer's slot and disables by slot, so a
        //  replay puts every one back where it was, all eight included.
        TEST_METHOD (Save_KeepsSlots_AndRunReplaysIt)
        {
            SessionRig                rig;
            std::vector<std::string>  before;



            SetPointers (rig);

            for (int i = 0; i < 8; ++i)
            {
                rig.RunOk (std::format ("ZP{} {:02X}", i, 0x36 + i));
            }

            rig.RunOk ("ZPC 1");
            rig.RunOk ("ZPD 5");
            before = rig.RunOk ("ZPL").text;

            rig.RunIs ("ZPSAVE z.txt", { "Saved 7 zero-page pointers to z.txt." });
            Assert::AreEqual (std::string ("ZPC *\nZP0 0036\nZP2 0038\nZP3 0039\nZP4 003A\nZP5 003B\nZP6 003C\nZP7 003D\nZPD 5\n"),
                              rig.files.PeekContent (L"z.txt"));

            rig.RunOk ("ZPC *");
            rig.RunOk ("ZP 50");
            rig.RunOk ("RUN z.txt");

            Assert::AreEqual (Check::Join (before), Check::Join (rig.RunOk ("ZPL").text), L"the replay recreates the list, slots and flags");
        }



        TEST_METHOD (Save_CountsAndErrors)
        {
            SessionRig  rig;



            rig.RunIs ("ZPSAVE none.txt", { "Saved 0 zero-page pointers to none.txt." });
            Assert::AreEqual (std::string ("ZPC *\n"), rig.files.PeekContent (L"none.txt"));

            rig.RunOk ("ZP3 36");
            rig.RunIs ("ZPSAVE one.txt", { "Saved 1 zero-page pointer to one.txt." });
            Assert::AreEqual (std::string ("ZPC *\nZP3 0036\n"), rig.files.PeekContent (L"one.txt"));

            rig.RunFails ("ZPSAVE", "invalid arguments", "ZPSAVE takes a file name.");

            Assert::AreEqual (S_OK, rig.files.WriteAllText (L"ro.txt", "kept"));
            Assert::AreEqual (S_OK, rig.files.SetReadOnlyAttribute (L"ro.txt", true));
            rig.RunFails ("ZPSAVE ro.txt", "file not written", "ro.txt could not be written.");
        }



        TEST_METHOD (Save_EveryDialect)
        {
            SessionRig  rig;



            rig.RunOk ("ZP 36");

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/ZPSAVE m.txt", { "Saved 1 zero-page pointer to m.txt." });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!ZPSAVE w.txt", { "Saved 1 zero-page pointer to w.txt." });

            rig.RunOk ("!MODE GSSQUARED");
            rig.RunIs ("ZPSAVE g.txt",  { "Saved 1 zero-page pointer to g.txt." });

            for (const wchar_t * name : { L"m.txt", L"w.txt", L"g.txt" })
            {
                Assert::AreEqual (std::string ("ZPC *\nZP0 0036\n"), rig.files.PeekContent (name), name);
            }
        }
    };
}

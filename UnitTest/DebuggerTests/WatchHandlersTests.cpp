#include "Pch.h"

#include "Debugger/Handlers/WatchHandlers.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "WatchMonitorRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace WatchMonitorTests;





////////////////////////////////////////////////////////////////////////////////
//
//  WatchHandlersTests
//
//  The watch commands -- W and WA, WL, WC, WD and WE, WSAVE, and GSSquared's
//  watch and nowatch -- over a real machine, grouped by command. Each group
//  runs the command in every dialect that reaches it, then through the paths
//  a user takes: the batch session, the window's console, the pipe and a
//  batch script.
//
//  Memory is set before a watch is added, so each value asserted is one the
//  test put there rather than whatever the power-on pattern left.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (WatchHandlersTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////
        //
        //  AddWatch
        //
        ////////////////////////////////////////////////////////////////////////

        //  Every argument form AppleWin mode takes, each adding the next id
        //  and replying with that entry alone. A second watch on an address
        //  already watched is a second entry.
        TEST_METHOD (AddWatch_EveryArgumentForm_AddsTheNextId)
        {
            SessionRig  rig;
            Word        cout = 0;



            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.Poke (0x0300, { 0x34, 0x12 });
            cout = rig.PeekWord (0xFDED);

            rig.RunIs ("W 36",     { "#0 $0036 = $FDF0" });
            rig.RunIs ("WA $300",  { "#1 $0300 = $1234" });
            rig.RunIs ("wa 300",   { "#2 $0300 = $1234" });
            rig.RunIs ("W COUT",   { std::format ("#3 $FDED = ${:04X}", cout) });
            rig.RunIs ("WA 2FF+1", { "#4 $0300 = $1234" });

            rig.RunIs ("WL", { "#0 $0036 = $FDF0",
                               "#1 $0300 = $1234",
                               "#2 $0300 = $1234",
                               std::format ("#3 $FDED = ${:04X}", cout),
                               "#4 $0300 = $1234" });
        }



        //  Tokens after the address are not read.
        TEST_METHOD (AddWatch_TokensAfterTheAddress_AreIgnored)
        {
            SessionRig  rig;



            rig.Poke (0x0036, { 0xF0, 0xFD });

            rig.RunIs ("WA 36 37", { "#0 $0036 = $FDF0" });
            rig.RunIs ("WL",       { "#0 $0036 = $FDF0" });
        }



        //  An address that does not evaluate is the evaluator's own message,
        //  and nothing is added.
        TEST_METHOD (AddWatch_AnAddressThatDoesNotEvaluate_IsAnErrorAndAddsNothing)
        {
            SessionRig  rig;



            rig.RunFails ("WA ZZZZ", "invalid arguments", "ZZZZ is not a symbol, register, or hex number.");
            rig.RunFails ("W 1+",    "invalid arguments", "The expression is incomplete.");
            rig.RunFails ("WA (300", "invalid arguments", "The parentheses do not balance.");

            rig.RunIs ("WL", { "No watches." });
        }



        //  W or WA with no address lists, adds nothing, and replies under the
        //  list's own verb, whatever the list holds.
        TEST_METHOD (AddWatch_WithNoAddress_ListsTheWatches)
        {
            SessionRig  rig;
            Reply       reply;



            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.Poke (0x0300, { 0x34, 0x12 });

            rig.RunIs ("W",  { "No watches." });
            rig.RunIs ("WA", { "No watches." });

            rig.RunOk ("WA 36");
            rig.RunOk ("WA 300");

            reply = rig.RunIs ("WA", { "#0 $0036 = $FDF0", "#1 $0300 = $1234" });
            Assert::IsTrue (reply.verb == DebugVerb::ListWatches, L"an add with nothing to add is a list");

            rig.RunIs ("W",  { "#0 $0036 = $FDF0", "#1 $0300 = $1234" });
            rig.RunIs ("WL", { "#0 $0036 = $FDF0", "#1 $0300 = $1234" });
        }



        //  THE VALUE IS READ AS THE CPU SEES MEMORY, through the debugger's
        //  view: the word at $FFFF takes its high byte from $0000, a word with
        //  an I/O byte in it has no value, and a word in $D000-$FFFF comes from
        //  language-card RAM while that RAM is banked in.
        TEST_METHOD (AddWatch_TheValueIsReadThroughTheMemoryView)
        {
            SessionRig  rig;



            rig.Poke (0x0000, { 0x12 });

            rig.RunIs ("WA FFFF", { std::format ("#0 $FFFF = $12{:02X}", rig.Peek (0xFFFF)) });
            rig.RunIs ("WA C000", { "#1 $C000" });
            rig.RunIs ("WA BFFF", { "#2 $BFFF" });

            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"WA C0FF","data":{"kind":"watchList","entries":[{"id":3,"address":49407,"enabled":true,"value":null}]},"text":["#3 $C0FF"]})json"),
                              rig.Json ("WA C0FF"), L"an unreadable word is null, not zero");

            //  Language-card RAM, read and write, bank 2: the second of two
            //  reads of $C083 enables the write.
            rig.RunOk ("IN C083");
            rig.RunOk ("IN C083");
            rig.RunOk ("MEB D000 AA BB");
            rig.RunIs ("WA D000", { "#4 $D000 = $BBAA" });

            //  And the ROM again once it is banked back.
            rig.RunOk ("IN C082");
            Assert::AreEqual (std::format ("#4 $D000 = ${:04X}", rig.PeekWord (0xD000)), rig.RunOk ("WL").text.at (4));
            Assert::AreNotEqual ((Word) 0xBBAA, rig.PeekWord (0xD000), L"the ROM is what $D000 reads now");
        }



        //  Every dialect's form of the add, each against one session's list.
        TEST_METHOD (AddWatch_EveryDialect)
        {
            SessionRig  rig;



            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.Poke (0x0300, { 0x34, 0x12 });
            rig.Poke (0x0007, { 0x56, 0x78 });

            rig.RunOk ("MODE CASSO");
            rig.RunIs ("W 36",       { "#0 $0036 = $FDF0" });

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/WA 300",    { "#1 $0300 = $1234" });
            rig.RunIs ("/W 7",       { "#2 $0007 = $7856" });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!W 0x300",   { "#3 $0300 = $1234" });
            rig.RunIs ("!WA 36",     { "#4 $0036 = $FDF0" });

            rig.RunOk ("!MODE GSSQUARED");
            rig.RunIs ("W 36",       { "Watch id=5 set" });
            rig.RunIs ("WA 300",     { "Watch id=6 set" });
            rig.RunIs ("watch 7",    { "Watch id=7 set" });

            rig.RunOk ("MODE APPLEWIN");
            rig.RunIs ("WL", { "#0 $0036 = $FDF0",
                               "#1 $0300 = $1234",
                               "#2 $0007 = $7856",
                               "#3 $0300 = $1234",
                               "#4 $0036 = $FDF0",
                               "#5 $0036 = $FDF0",
                               "#6 $0300 = $1234",
                               "#7 $0007 = $7856" });
        }



        //  IN MONITOR MODE A BARE W IS THE MONITOR'S WRITE, not a watch. With
        //  no range it writes nothing and says what it needs.
        TEST_METHOD (AddWatch_BareWInMonitorMode_IsTheMonitorsWrite)
        {
            SessionRig  rig;



            rig.RunOk ("MODE MONITOR");

            rig.RunFails ("W 300", "invalid arguments", "W takes the range to write: first.lastW name.");
            Assert::AreEqual ((size_t) 0, rig.files.FileCount(), L"no file was written");

            rig.RunIs ("/WL", { "No watches." });
        }



        //  GSSquared's W and WA with no address list, as its `watch` does,
        //  rather than reporting every watch as just set.
        TEST_METHOD (AddWatch_GSSquaredWithNoAddress_Lists)
        {
            SessionRig  rig;



            rig.RunOk ("MODE GSSQUARED");
            rig.RunIs ("W 6", { "Watch id=0 set" });
            rig.RunIs ("W 7", { "Watch id=1 set" });

            rig.RunIs ("W",     { "Current memory watches:", "[0] 00/0006", "[1] 00/0007" });
            rig.RunIs ("WA",    { "Current memory watches:", "[0] 00/0006", "[1] 00/0007" });
            rig.RunIs ("watch", { "Current memory watches:", "[0] 00/0006", "[1] 00/0007" });
        }



        //  The window's console and a channel client reach the same list, and
        //  the Watches pane shows it.
        TEST_METHOD (AddWatch_FromTheConsoleAndThePipe_IsOneList)
        {
            WindowRig             rig;
            DebuggerViewSnapshot  snapshot;
            std::string           reply;



            rig.rig.machine.GetMemoryBus().WriteByte (0x0036, 0xF0);
            rig.rig.machine.GetMemoryBus().WriteByte (0x0037, 0xFD);

            //  $0300 holds the rig's LDA #$41.
            rig.TypeIs ("WA 300", { "#0 $0300 = $41A9" });

            reply = rig.Request ("W 36");
            Assert::AreEqual (std::string (R"json({"type":"reply","id":1,"status":"ok","command":"W 36","data":{"kind":"watchList","entries":[{"id":1,"address":54,"enabled":true,"value":65008}]},"text":["#1 $0036 = $FDF0"]})json"),
                              reply);

            reply = rig.Request ("/WA C000", "monitor");
            Assert::AreEqual (std::string (R"json({"type":"reply","id":2,"status":"ok","command":"/WA C000","data":{"kind":"watchList","entries":[{"id":2,"address":49152,"enabled":true,"value":null}]},"text":["#2 $C000"]})json"),
                              reply);

            rig.TypeIs ("WL", { "#0 $0300 = $41A9", "#1 $0036 = $FDF0", "#2 $C000" });

            snapshot = rig.rig.view.Build (rig.Session());

            Assert::AreEqual ((size_t) 3, snapshot.watches.size());
            Assert::AreEqual ((Word) 0x0300, snapshot.watches[0].address);
            Assert::AreEqual (std::string ("41A9"), snapshot.watches[0].value);
            Assert::AreEqual (std::string ("FDF0"), snapshot.watches[1].value);
            Assert::AreEqual (std::string ("--"),   snapshot.watches[2].value, L"a word the pane cannot read shows as --");
        }



        //  A batch script, as `CassoCli debug` prints it.
        TEST_METHOD (AddWatch_InABatchScript)
        {
            ScriptRig  rig;



            Assert::AreEqual (std::string (">MEB 300 34 12\n"
                                           "0300: 34 12                    4.\n"
                                           ">WA 300\n"
                                           "#0 $0300 = $1234\n"
                                           ">W\n"
                                           "#0 $0300 = $1234\n"),
                              rig.Run ("MEB 300 34 12\nWA 300\nW\n"));
            Assert::AreEqual (0, rig.result.exitStatus);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ListWatches
        //
        ////////////////////////////////////////////////////////////////////////

        //  In id order, a disabled entry marked, every value read again when
        //  the list is built, and anything after WL ignored.
        TEST_METHOD (ListWatches_InIdOrder_MarkedAndReadAgain)
        {
            SessionRig  rig;



            rig.RunIs ("WL", { "No watches." });

            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.Poke (0x0300, { 0x34, 0x12 });
            rig.RunOk ("WA 36");
            rig.RunOk ("WA 300");
            rig.RunOk ("WA 36");
            rig.RunOk ("WD 1");

            rig.RunIs ("WL", { "#0 $0036 = $FDF0", "#1 $0300 = $1234 (disabled)", "#2 $0036 = $FDF0" });

            rig.Poke (0x0300, { 0xCD, 0xAB });
            rig.RunIs ("WL anything", { "#0 $0036 = $FDF0", "#1 $0300 = $ABCD (disabled)", "#2 $0036 = $FDF0" });
        }



        TEST_METHOD (ListWatches_AsJson)
        {
            SessionRig  rig;



            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"WL","data":{"kind":"watchList","entries":[]},"text":["No watches."]})json"),
                              rig.Json ("WL"));

            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.RunOk ("WA 36");
            rig.RunOk ("WA C000");
            rig.RunOk ("WD 1");

            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"WL","data":{"kind":"watchList","entries":[{"id":0,"address":54,"enabled":true,"value":65008},{"id":1,"address":49152,"enabled":false,"value":null}]},"text":["#0 $0036 = $FDF0","#1 $C000 (disabled)"]})json"),
                              rig.Json ("WL"));
        }



        //  GSSquared has its own list; the Monitor and WinDbg formats have
        //  none and write AppleWin's.
        TEST_METHOD (ListWatches_InEachOutputFormat)
        {
            SessionRig  rig;



            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.RunOk ("WA 36");
            rig.RunOk ("WA 300");

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/WL", { "#0 $0036 = $FDF0", std::format ("#1 $0300 = ${:04X}", rig.PeekWord (0x0300)) });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!WL", { "#0 $0036 = $FDF0", std::format ("#1 $0300 = ${:04X}", rig.PeekWord (0x0300)) });

            rig.RunOk ("!MODE GSSQUARED");
            rig.RunIs ("WL",    { "Current memory watches:", "[0] 00/0036", "[1] 00/0300" });
            rig.RunIs ("watch", { "Current memory watches:", "[0] 00/0036", "[1] 00/0300" });
        }



        //  A value the program changed is the one the next list shows.
        TEST_METHOD (ListWatches_ShowsWhatARunWrote)
        {
            SessionRig  rig;



            rig.Poke (0x0400, { 0x00, 0x00 });
            rig.RunIs ("WA 400", { "#0 $0400 = $0000" });

            //  LDA #$41 / STA $0400, run to the instruction after.
            rig.RunOk ("MEB 300 A9 41 8D 00 04 EA");
            rig.RunOk ("R PC 300");
            rig.RunOk ("G 305");

            Assert::AreEqual ((Word) 0x0305, rig.target.GetRegisters().pc, L"the run stopped where it was sent");
            rig.RunIs ("WL", { "#0 $0400 = $0041" });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ClearWatch
        //
        ////////////////////////////////////////////////////////////////////////

        //  One entry goes and the rest stay; later adds keep counting.
        TEST_METHOD (ClearWatch_RemovesThatEntryOnly_AndIdsAreNotReused)
        {
            SessionRig  rig;



            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.Poke (0x0300, { 0x34, 0x12 });
            rig.RunOk ("WA 36");
            rig.RunOk ("WA 300");
            rig.RunOk ("WA 36");

            rig.RunIs ("WC 1", { "Watch #1 cleared." });
            rig.RunIs ("WL",   { "#0 $0036 = $FDF0", "#2 $0036 = $FDF0" });

            rig.RunIs ("WA 300", { "#3 $0300 = $1234" });
            rig.RunIs ("wc 0",   { "Watch #0 cleared." });
            rig.RunIs ("WL",     { "#2 $0036 = $FDF0", "#3 $0300 = $1234" });
        }



        //  `*` empties the list, and numbering starts over.
        TEST_METHOD (ClearWatch_Star_EmptiesTheListAndRestartsNumbering)
        {
            SessionRig  rig;



            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.RunOk ("WA 36");
            rig.RunOk ("WA 36");

            rig.RunIs ("WC *",  { "No watches." });
            rig.RunIs ("WL",    { "No watches." });
            rig.RunIs ("WA 36", { "#0 $0036 = $FDF0" });
        }



        //  Ids are decimal, only the first token is read, leading zeros are
        //  allowed, and an id too large for an int is reported as typed.
        TEST_METHOD (ClearWatch_IdForms)
        {
            SessionRig  rig;



            for (int i = 0; i < 8; ++i)
            {
                rig.RunOk ("WA 300");
            }

            rig.RunIs ("WC 007", { "Watch #7 cleared." });
            rig.RunIs ("WC 1 2", { "Watch #1 cleared." });
            Assert::AreEqual ((size_t) 6, rig.RunOk ("WL").text.size(), L"only #1 of `WC 1 2` was cleared");

            rig.RunFails ("WC 2147483648", "no such watch", "There is no watch #2147483648.");
            rig.RunFails ("WC 4294967295", "no such watch", "There is no watch #4294967295.");
        }



        TEST_METHOD (ClearWatch_Errors)
        {
            SessionRig  rig;



            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.RunOk ("WA 36");

            rig.RunFails ("WC 5",  "no such watch",     "There is no watch #5.");
            rig.RunFails ("WC",    "invalid arguments", "Give an id, or * for all.");
            rig.RunFails ("WC x",  "invalid arguments", "x is not an id.");
            rig.RunFails ("WC A",  "invalid arguments", "A is not an id.");
            rig.RunFails ("WC -1", "invalid arguments", "-1 is not an id.");

            rig.RunIs ("WL", { "#0 $0036 = $FDF0" });
        }



        TEST_METHOD (ClearWatch_EveryDialect)
        {
            SessionRig  rig;



            for (int i = 0; i < 5; ++i)
            {
                rig.RunOk ("WA 300");
            }

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/WC 0", { "Watch #0 cleared." });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!WC 1", { "Watch #1 cleared." });

            rig.RunOk ("!MODE GSSQUARED");
            rig.RunIs ("nowatch 2", { "Watch #2 cleared." });
            rig.RunIs ("WC 3",      { "Watch #3 cleared." });
            rig.RunIs ("watch",     { "Current memory watches:", "[4] 00/0300" });

            Check::Fails (rig.Run ("nowatch 9"), "nowatch 9", CommandStatus::Error, "no such watch", "There is no watch #9.");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  DisableWatch and EnableWatch
        //
        ////////////////////////////////////////////////////////////////////////

        //  The flag is all that changes: the value is still read, and the
        //  list and its JSON both show it.
        TEST_METHOD (DisableAndEnable_SetTheFlag)
        {
            SessionRig  rig;



            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.RunOk ("WA 36");
            rig.RunOk ("WA 36");

            rig.RunIs ("WD 0", { "#0 $0036 = $FDF0 (disabled)" });
            rig.RunIs ("WL",   { "#0 $0036 = $FDF0 (disabled)", "#1 $0036 = $FDF0" });

            rig.Poke (0x0036, { 0x11, 0x22 });
            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"WL","data":{"kind":"watchList","entries":[{"id":0,"address":54,"enabled":false,"value":8721},{"id":1,"address":54,"enabled":true,"value":8721}]},"text":["#0 $0036 = $2211 (disabled)","#1 $0036 = $2211"]})json"),
                              rig.Json ("WL"));

            rig.RunIs ("WE 0", { "#0 $0036 = $2211" });
            rig.RunIs ("WL",   { "#0 $0036 = $2211", "#1 $0036 = $2211" });
        }



        TEST_METHOD (DisableAndEnable_Star_ActOnEveryEntry)
        {
            SessionRig  rig;



            rig.RunIs ("WD *", { "No watches." });
            rig.RunIs ("WE *", { "No watches." });

            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.RunOk ("WA 36");
            rig.RunOk ("WA 36");

            rig.RunIs ("WD *", { "#0 $0036 = $FDF0 (disabled)", "#1 $0036 = $FDF0 (disabled)" });
            rig.RunIs ("WE *", { "#0 $0036 = $FDF0", "#1 $0036 = $FDF0" });
        }



        TEST_METHOD (DisableAndEnable_Errors)
        {
            SessionRig  rig;



            rig.RunOk ("WA 300");

            rig.RunFails ("WD 5", "no such watch",     "There is no watch #5.");
            rig.RunFails ("WE 5", "no such watch",     "There is no watch #5.");
            rig.RunFails ("WD",   "invalid arguments", "Give an id, or * for all.");
            rig.RunFails ("WE q", "invalid arguments", "q is not an id.");

            Assert::IsFalse (rig.RunOk ("WL").text.at (0).ends_with ("(disabled)"), L"no error changed the flag");
        }



        TEST_METHOD (DisableAndEnable_EveryDialect)
        {
            SessionRig  rig;



            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.RunOk ("WA 36");

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/WD 0", { "#0 $0036 = $FDF0 (disabled)" });
            rig.RunIs ("/WE 0", { "#0 $0036 = $FDF0" });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!WD *", { "#0 $0036 = $FDF0 (disabled)" });
            rig.RunIs ("!WE *", { "#0 $0036 = $FDF0" });

            rig.RunOk ("!MODE CASSO");
            rig.RunIs ("WD 0", { "#0 $0036 = $FDF0 (disabled)" });

            rig.RunOk ("MODE GSSQUARED");
            rig.RunOk ("WE 0");
            rig.RunOk ("MODE APPLEWIN");
            rig.RunIs ("WL", { "#0 $0036 = $FDF0" });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  SaveWatches
        //
        ////////////////////////////////////////////////////////////////////////

        //  The script clears, adds each entry in id order and disables by
        //  position, since the replay numbers from zero. RUN replays it.
        TEST_METHOD (SaveWatches_WritesAScriptThatRecreatesTheList)
        {
            SessionRig  rig;



            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.Poke (0x0300, { 0x34, 0x12 });
            rig.RunOk ("WA 36");
            rig.RunOk ("WA 7");
            rig.RunOk ("WA 300");
            rig.RunOk ("WC 1");
            rig.RunOk ("WD 2");

            rig.RunIs ("WSAVE w.txt", { "Saved 2 watches to w.txt." });
            Assert::AreEqual (std::string ("WC *\nWA 0036\nWA 0300\nWD 1\n"), rig.files.PeekContent (L"w.txt"));

            rig.RunOk ("WC *");
            rig.RunOk ("WA 1234");
            rig.RunOk ("RUN w.txt");

            rig.RunIs ("WL", { "#0 $0036 = $FDF0", "#1 $0300 = $1234 (disabled)" });
        }



        TEST_METHOD (SaveWatches_CountsInTheReply)
        {
            SessionRig  rig;



            rig.RunIs ("WSAVE none.txt", { "Saved 0 watches to none.txt." });
            Assert::AreEqual (std::string ("WC *\n"), rig.files.PeekContent (L"none.txt"));

            rig.RunOk ("WA 300");
            rig.RunIs ("WSAVE one.txt", { "Saved 1 watch to one.txt." });
            Assert::AreEqual (std::string ("WC *\nWA 0300\n"), rig.files.PeekContent (L"one.txt"));
        }



        //  Quotes keep a name with spaces together and are not part of it; a
        //  relative name is taken from the CD directory.
        TEST_METHOD (SaveWatches_PathForms)
        {
            SessionRig  rig;



            rig.RunOk ("WA 300");

            rig.RunIs ("WSAVE \"my watches.txt\"", { "Saved 1 watch to \"my watches.txt\"." });
            Assert::IsTrue (rig.files.Exists (L"my watches.txt"));

            rig.RunOk ("CD C:\\Work");
            rig.RunIs ("WSAVE w.txt", { "Saved 1 watch to w.txt." });
            Assert::AreEqual (std::string ("WC *\nWA 0300\n"), rig.files.PeekContent (L"C:\\Work\\w.txt"));

            rig.RunIs ("WSAVE D:\\elsewhere.txt", { "Saved 1 watch to D:\\elsewhere.txt." });
            Assert::IsTrue (rig.files.Exists (L"D:\\elsewhere.txt"), L"a rooted path is used as given");
        }



        TEST_METHOD (SaveWatches_Errors)
        {
            SessionRig  rig;



            rig.RunFails ("WSAVE", "invalid arguments", "WSAVE takes a file name.");

            Assert::AreEqual (S_OK, rig.files.WriteAllText (L"ro.txt", "kept"));
            Assert::AreEqual (S_OK, rig.files.SetReadOnlyAttribute (L"ro.txt", true));
            rig.RunFails ("WSAVE ro.txt", "file not written", "ro.txt could not be written.");
            Assert::AreEqual (std::string ("kept"), rig.files.PeekContent (L"ro.txt"));
        }



        //  NO PRODUCTION SESSION LACKS A FILE SYSTEM -- the batch runner and
        //  the controller both give one -- so this builds a bare session, the
        //  only way to reach the branch, to pin what it says.
        TEST_METHOD (SaveWatches_WithNoFileSystem_SaysSo)
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session (target, sink, RunState::Paused);
            DebugHandlerSet            handlers;
            Reply                      reply;



            handlers.Attach (session);

            reply = session.ExecuteLine ("WSAVE w.txt");
            Check::Fails (reply, "WSAVE w.txt", CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        }



        TEST_METHOD (SaveWatches_EveryDialect)
        {
            SessionRig  rig;



            rig.RunOk ("WA 300");

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/WSAVE m.txt", { "Saved 1 watch to m.txt." });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!WSAVE w.txt", { "Saved 1 watch to w.txt." });

            rig.RunOk ("!MODE GSSQUARED");
            rig.RunIs ("WSAVE g.txt", { "Saved 1 watch to g.txt." });

            for (const wchar_t * name : { L"m.txt", L"w.txt", L"g.txt" })
            {
                Assert::AreEqual (std::string ("WC *\nWA 0300\n"), rig.files.PeekContent (name), name);
            }
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  GSSquared's watch and nowatch
        //
        ////////////////////////////////////////////////////////////////////////

        //  A range is one watch per address, up to 256, and one reply line
        //  per watch set.
        TEST_METHOD (GSSquaredWatch_ARangeIsOneWatchPerAddress)
        {
            SessionRig  rig;
            Reply       reply;



            rig.RunOk ("MODE GSSQUARED");

            rig.RunIs ("watch 6.7", { "Watch id=0 set", "Watch id=1 set" });

            reply = rig.RunOk ("watch FF00.FFFF");
            Assert::AreEqual ((size_t) 256, reply.text.size(), L"a range of 256 is the most one command sets");
            Assert::AreEqual (std::string ("Watch id=2 set"),   reply.text.front());
            Assert::AreEqual (std::string ("Watch id=257 set"), reply.text.back());

            reply = rig.RunOk ("watch");
            Assert::AreEqual ((size_t) 259, reply.text.size());
            Assert::AreEqual (std::string ("[0] 00/0006"),   reply.text[1]);
            Assert::AreEqual (std::string ("[1] 00/0007"),   reply.text[2]);
            Assert::AreEqual (std::string ("[257] 00/FFFF"), reply.text.back());
        }



        TEST_METHOD (GSSquaredWatch_Errors)
        {
            SessionRig  rig;



            rig.RunOk ("MODE GSSQUARED");

            rig.RunFails ("watch 0.100",  "invalid arguments", "A watch range covers at most 256 addresses.");
            rig.RunFails ("watch 7.6",    "invalid arguments", "7.6 ends before it starts.");
            rig.RunFails ("watch 1 2",    "invalid arguments", "WATCH takes an address or a range.");
            rig.RunFails ("watch ZZ",     "invalid arguments", "ZZ is not an address. An address is one to four hex digits.");
            rig.RunFails ("nowatch",      "invalid arguments", "NOWATCH takes a watch id.");
            rig.RunFails ("nowatch x",    "invalid arguments", "NOWATCH takes a watch id.");
            rig.RunFails ("nowatch 1 2",  "invalid arguments", "NOWATCH takes a watch id.");

            Check::Fails (rig.Run ("watch E1/0006"), "watch E1/0006", CommandStatus::NotAvailable,
                          "command not available", "E1/0006 is in bank E1. Only bank 00 exists on this machine.");

            rig.RunIs ("watch", { "Current memory watches:" });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The Watches pane
        //
        ////////////////////////////////////////////////////////////////////////

        //  The pane's Delete key and its Remove item send `WC id`, and an edit
        //  of the expression sends `WC id` then `W typed`, each in the words
        //  of the session's mode. Run in every mode, each clears or moves the
        //  watch it was sent for.
        TEST_METHOD (WatchesPane_LinesRunInEveryMode)
        {
            static constexpr const char * kModes[] = { "APPLEWIN", "MONITOR", "GSSQUARED", "WINDBG", "CASSO" };



            for (const char * mode : kModes)
            {
                WindowRig                 rig;
                DebuggerViewSnapshot      snapshot;
                std::vector<std::string>  edit;
                std::string               why = std::string ("in ") + mode;



                rig.TypeOk ("WA 300");
                rig.TypeOk ("WA 301");
                rig.TypeOk (std::string ("MODE ") + mode);

                snapshot = rig.rig.view.Build (rig.Session());

                Check::IsOk (rig.Type (DebuggerViewState::GetModeLine ("WC 0", snapshot.mode)), why + ": delete");

                edit = DebuggerViewState::GetWatchEditLines (snapshot, 1, std::nullopt, 0, "400");
                Assert::AreEqual ((size_t) 2, edit.size());

                for (const std::string & line : edit)
                {
                    Check::IsOk (rig.Type (DebuggerViewState::GetModeLine (line, snapshot.mode)), why + ": " + line);
                }

                snapshot = rig.rig.view.Build (rig.Session());

                Assert::AreEqual ((size_t) 1, snapshot.watches.size(), Check::Widen (why).c_str());
                Assert::AreEqual (2,               snapshot.watches[0].id,      Check::Widen (why + ": the edit made a new id").c_str());
                Assert::AreEqual ((Word) 0x0400,   snapshot.watches[0].address, Check::Widen (why).c_str());
            }
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The three lists together
        //
        ////////////////////////////////////////////////////////////////////////

        //  Watches, zero-page pointers and bookmarks are labels on memory, so
        //  a mode switch, a run, a reset and a machine change all leave them;
        //  values are read again each time.
        //
        //  THE RESET AND THE MACHINE CHANGE ARE THE HOST'S NOTIFICATIONS. No
        //  command resets the machine or changes it, so the test makes the two
        //  calls the emulator makes when the user does.
        TEST_METHOD (TheListsOutliveModesRunsResetsAndMachineChanges)
        {
            SessionRig  rig;



            rig.Poke (0x0036, { 0xF0, 0xFD });
            rig.Poke (0x0400, { 0x00, 0x00 });
            rig.RunOk ("WA 400");
            rig.RunOk ("ZP 36");
            rig.RunOk ("BMA 300");

            rig.RunOk ("MODE MONITOR");
            rig.RunOk ("/MODE APPLEWIN");

            rig.RunOk ("MEB 300 A9 41 8D 00 04 EA");
            rig.RunOk ("R PC 300");
            rig.RunOk ("G 305");

            rig.session.OnReset (false);
            rig.session.OnMachineChanged ("Apple2e");

            rig.RunIs ("WL",  { "#0 $0400 = $0041" });
            rig.RunIs ("ZPL", { "#0 $0036 -> $FDF0" });
            rig.RunIs ("BML", { "#0 $0300" });
        }



        //  None of these commands writes the machine, so each runs while it
        //  runs, and a list shows the memory the running program wrote.
        TEST_METHOD (TheListCommandsRunWhileTheMachineRuns)
        {
            WindowRig  rig;
            uint64_t   ran = 0;



            rig.rig.machine.GetMemoryBus().WriteByte (0x0400, 0x00);
            rig.rig.machine.GetMemoryBus().WriteByte (0x0401, 0x00);
            rig.TypeOk ("WA 400");

            //  G with no stop set: the run is on foot until a slice reports it.
            rig.TypeOk ("G");
            Assert::IsTrue (rig.Session().GetRunState() == RunState::DebugRun, L"the machine is running");

            //  LDA #$41 / STA $0400 is six cycles.
            ran = rig.rig.machine.RunCycles (6);
            Assert::AreEqual ((uint64_t) 6, ran);
            Assert::IsFalse (rig.rig.controller.GetRunDriver().OnSliceExecuted ((uint32_t) ran), L"still running");

            rig.TypeIs ("WL",      { "#0 $0400 = $0041" });
            rig.TypeIs ("WA 36",   { std::format ("#1 $0036 = ${:02X}{:02X}", rig.Peek (0x0037), rig.Peek (0x0036)) });
            rig.TypeOk ("WD 0");
            rig.TypeOk ("ZP 36");
            rig.TypeOk ("BMA 300");
            rig.TypeOk ("BML");
            rig.TypeOk ("WC 1");
            Assert::IsTrue (rig.Session().GetRunState() == RunState::DebugRun, L"and none of that stopped it");

            Assert::IsTrue (rig.Request ("WL").find (R"json("text":["#0 $0400 = $0041 (disabled)"])json") != std::string::npos,
                            L"a channel client reads the same list during the run");

            rig.rig.controller.RequestPause();
            Assert::IsTrue (rig.rig.controller.GetRunDriver().OnSliceExecuted (0), L"the pause ends the run");
        }
    };
}

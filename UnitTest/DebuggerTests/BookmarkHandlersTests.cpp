#include "Pch.h"

#include "WatchMonitorRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace WatchMonitorTests;





////////////////////////////////////////////////////////////////////////////////
//
//  BookmarkHandlersTests
//
//  The bookmark commands -- BM and BMA, BML, BMC, BMG and BMSAVE -- over a
//  real machine, grouped by command, each in every dialect that reaches it.
//
//  BMG lists what U lists at the same address, in the session's output
//  format, so each listing here is checked against the one the dialect's own
//  disassembly command gives.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (BookmarkHandlersTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////
        //
        //  AddBookmark
        //
        ////////////////////////////////////////////////////////////////////////

        //  The next id, with no value; the same address twice is two entries.
        TEST_METHOD (Add_EveryArgumentForm)
        {
            SessionRig  rig;



            rig.RunIs ("BM 300",   { "#0 $0300" });
            rig.RunIs ("BMA $400", { "#1 $0400" });
            rig.RunIs ("bm 500",   { "#2 $0500" });
            rig.RunIs ("BMA COUT", { "#3 $FDED" });
            rig.RunIs ("BMA 2FF+1 junk", { "#4 $0300" });

            rig.RunIs ("BML", { "#0 $0300", "#1 $0400", "#2 $0500", "#3 $FDED", "#4 $0300" });
        }



        TEST_METHOD (Add_AnAddressThatDoesNotEvaluate_IsAnError)
        {
            SessionRig  rig;



            rig.RunFails ("BMA 1+",   "invalid arguments", "The expression is incomplete.");
            rig.RunFails ("BM ZZZZ",  "invalid arguments", "ZZZZ is not a symbol, register, or hex number.");

            rig.RunIs ("BML", { "No bookmarks." });
        }



        TEST_METHOD (Add_WithNoAddress_Lists)
        {
            SessionRig  rig;
            Reply       reply;



            rig.RunIs ("BM",  { "No bookmarks." });
            rig.RunIs ("BMA", { "No bookmarks." });

            rig.RunOk ("BMA 300");

            reply = rig.RunIs ("BM", { "#0 $0300" });
            Assert::IsTrue (reply.verb == DebugVerb::ListBookmarks, L"an add with nothing to add is a list");
            rig.RunIs ("BMA", { "#0 $0300" });
        }



        TEST_METHOD (Add_EveryDialect)
        {
            SessionRig  rig;



            rig.RunOk ("MODE CASSO");
            rig.RunIs ("BM 300",       { "#0 $0300" });

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/BMA 400",     { "#1 $0400" });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!BMA 0x500",   { "#2 $0500" });

            rig.RunOk ("!MODE GSSQUARED");
            rig.RunIs ("BMA 600",      { "#3 $0600" });

            rig.RunOk ("MODE APPLEWIN");
            rig.RunIs ("BML", { "#0 $0300", "#1 $0400", "#2 $0500", "#3 $0600" });
        }



        TEST_METHOD (Add_FromTheConsoleAndThePipe)
        {
            WindowRig  rig;



            rig.TypeIs ("BMA 300", { "#0 $0300" });

            Assert::AreEqual (std::string (R"json({"type":"reply","id":1,"status":"ok","command":"BM 400","data":{"kind":"bookmarkList","entries":[{"id":1,"address":1024,"enabled":true,"value":null}]},"text":["#1 $0400"]})json"),
                              rig.Request ("BM 400"));

            Assert::AreEqual (std::string (R"json({"type":"reply","id":2,"status":"ok","command":"BML","data":{"kind":"bookmarkList","entries":[{"id":0,"address":768,"enabled":true,"value":null},{"id":1,"address":1024,"enabled":true,"value":null}]},"text":["#0 $0300","#1 $0400"]})json"),
                              rig.Request ("BML"));
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ListBookmarks
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (List_InEveryDialect)
        {
            SessionRig  rig;



            rig.RunIs ("BML", { "No bookmarks." });

            rig.RunOk ("BMA 300");
            rig.RunOk ("BMA FFFF");
            rig.RunIs ("BML anything", { "#0 $0300", "#1 $FFFF" });

            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"BML","data":{"kind":"bookmarkList","entries":[{"id":0,"address":768,"enabled":true,"value":null},{"id":1,"address":65535,"enabled":true,"value":null}]},"text":["#0 $0300","#1 $FFFF"]})json"),
                              rig.Json ("BML"));

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/BML", { "#0 $0300", "#1 $FFFF" });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!BML", { "#0 $0300", "#1 $FFFF" });

            rig.RunOk ("!MODE GSSQUARED");
            rig.RunIs ("BML",  { "#0 $0300", "#1 $FFFF" });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ClearBookmark
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Clear_OneOrAll)
        {
            SessionRig  rig;



            rig.RunOk ("BMA 300");
            rig.RunOk ("BMA 400");
            rig.RunOk ("BMA 500");

            rig.RunIs ("BMC 1", { "Bookmark #1 cleared." });
            rig.RunIs ("BML",   { "#0 $0300", "#2 $0500" });
            rig.RunIs ("BMA 600", { "#3 $0600" });

            rig.RunIs ("BMC *",   { "No bookmarks." });
            rig.RunIs ("BMA 700", { "#0 $0700" });
        }



        TEST_METHOD (Clear_Errors)
        {
            SessionRig  rig;



            rig.RunOk ("BMA 300");

            rig.RunFails ("BMC 7", "no such bookmark",  "There is no bookmark #7.");
            rig.RunFails ("BMC",   "invalid arguments", "Give an id, or * for all.");
            rig.RunFails ("BMC x", "invalid arguments", "x is not an id.");

            rig.RunIs ("BML", { "#0 $0300" });
        }



        TEST_METHOD (Clear_EveryDialect)
        {
            SessionRig  rig;



            rig.RunOk ("BMA 300");
            rig.RunOk ("BMA 400");
            rig.RunOk ("BMA 500");

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/BMC 0", { "Bookmark #0 cleared." });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!BMC 1", { "Bookmark #1 cleared." });

            rig.RunOk ("!MODE GSSQUARED");
            rig.RunIs ("BMC 2",  { "Bookmark #2 cleared." });
            rig.RunIs ("BML",    { "No bookmarks." });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  GoToBookmark
        //
        ////////////////////////////////////////////////////////////////////////

        //  Twenty lines at the bookmark, as U lists them: the symbols as
        //  labels and a data block as data.
        TEST_METHOD (GoTo_ListsWhatUListsThere)
        {
            SessionRig  rig;
            Reply       listed;



            //  LDA #$41 / STA $0400 / JSR COUT / RTS, then eight bytes of data.
            rig.RunOk ("MEB 300 A9 41 8D 00 04 20 ED FD 60");
            rig.RunOk ("DB 309:310");
            rig.RunOk ("BMA 300");
            rig.RunOk ("BMA COUT");

            listed = rig.RunOk ("BMG 0");
            Assert::AreEqual ((size_t) 20, listed.text.size());
            Assert::IsTrue (listed.text[0].starts_with ("0300: A9 41 ") && listed.text[0].ends_with (" LDA  #$41"), Check::Widen (listed.text[0]).c_str());
            Assert::AreEqual (Check::Join (rig.RunOk ("U 300").text), Check::Join (listed.text));

            listed = rig.RunOk ("BMG 1");
            Assert::AreEqual ((size_t) 20, listed.text.size());
            Assert::AreEqual (Check::Join (rig.RunOk ("U FDED").text), Check::Join (listed.text));
            Assert::IsTrue (listed.text[0].find ("COUT") != std::string::npos, Check::Widen (listed.text[0]).c_str());
        }



        //  The listing stops at the top of memory.
        TEST_METHOD (GoTo_StopsAtFFFF)
        {
            SessionRig  rig;
            Reply       listed;



            rig.RunOk ("BMA FFF8");

            listed = rig.RunOk ("BMG 0");
            Assert::IsTrue (listed.text.size() < 20, L"fewer than twenty lines fit");
            Assert::AreEqual (Check::Join (rig.RunOk ("U FFF8").text), Check::Join (listed.text));
            Assert::IsTrue (listed.text.back().starts_with ("FFF"), Check::Widen (listed.text.back()).c_str());
        }



        //  BMG lists without moving where U goes on from.
        TEST_METHOD (GoTo_LeavesTheUListingWhereItWas)
        {
            SessionRig   rig;
            std::string  afterU;



            rig.RunOk ("MEB 300 EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA EA");
            rig.RunOk ("U 300");
            rig.RunOk ("BMA 1000");
            rig.RunOk ("BMG 0");

            afterU = rig.RunOk ("U").text.at (0);
            Assert::IsTrue (afterU.starts_with ("0314:"), Check::Widen (afterU).c_str());
        }



        //  In each output format, the listing is the one the dialect's own
        //  disassembly gives at that address.
        TEST_METHOD (GoTo_InEachDialect)
        {
            SessionRig  rig;
            Reply       listed;



            rig.RunOk ("MEB 300 A9 41 8D 00 04 60");
            rig.RunOk ("BMA 300");

            rig.RunOk ("MODE MONITOR");
            listed = rig.RunOk ("/BMG 0");
            Assert::AreEqual (std::string ("0300-   A9 41       LDA   #$41"), listed.text.at (0));
            Assert::AreEqual (Check::Join (rig.RunOk ("300L").text), Check::Join (listed.text));

            rig.RunOk ("/MODE GSSQUARED");
            listed = rig.RunOk ("BMG 0");
            Assert::AreEqual (std::string ("M=8 X=8"), listed.text.at (0));
            Assert::AreEqual (Check::Join (rig.RunOk ("l 300").text), Check::Join (listed.text));

            rig.RunOk ("MODE WINDBG");
            listed = rig.RunOk ("!BMG 0");
            Assert::AreEqual ((size_t) 20, listed.text.size());
            Assert::AreEqual (rig.RunOk ("u 300").text.at (0), listed.text.at (0));
        }



        TEST_METHOD (GoTo_Errors)
        {
            SessionRig  rig;



            rig.RunOk ("BMA 300");

            rig.RunFails ("BMG 7", "no such bookmark",  "There is no bookmark #7.");
            rig.RunFails ("BMG *", "invalid arguments", "BMG takes one bookmark id, not *.");
            rig.RunFails ("BMG x", "invalid arguments", "x is not an id.");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  SaveBookmarks
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Save_WritesAScript_AndRunReplaysIt)
        {
            SessionRig  rig;



            rig.RunOk ("BMA 300");
            rig.RunOk ("BMA 400");
            rig.RunOk ("BMA 500");
            rig.RunOk ("BMC 1");

            rig.RunIs ("BMSAVE b.txt", { "Saved 2 bookmarks to b.txt." });
            Assert::AreEqual (std::string ("BMC *\nBMA 0300\nBMA 0500\n"), rig.files.PeekContent (L"b.txt"));

            rig.RunOk ("BMC *");
            rig.RunOk ("BMA 1234");
            rig.RunOk ("RUN b.txt");

            rig.RunIs ("BML", { "#0 $0300", "#1 $0500" });
        }



        TEST_METHOD (Save_CountsAndErrors)
        {
            SessionRig  rig;



            rig.RunIs ("BMSAVE none.txt", { "Saved 0 bookmarks to none.txt." });
            Assert::AreEqual (std::string ("BMC *\n"), rig.files.PeekContent (L"none.txt"));

            rig.RunOk ("BMA 300");
            rig.RunIs ("BMSAVE one.txt", { "Saved 1 bookmark to one.txt." });

            rig.RunFails ("BMSAVE", "invalid arguments", "BMSAVE takes a file name.");

            Assert::AreEqual (S_OK, rig.files.WriteAllText (L"ro.txt", "kept"));
            Assert::AreEqual (S_OK, rig.files.SetReadOnlyAttribute (L"ro.txt", true));
            rig.RunFails ("BMSAVE ro.txt", "file not written", "ro.txt could not be written.");
        }



        TEST_METHOD (Save_EveryDialect)
        {
            SessionRig  rig;



            rig.RunOk ("BMA 300");

            rig.RunOk ("MODE MONITOR");
            rig.RunIs ("/BMSAVE m.txt", { "Saved 1 bookmark to m.txt." });

            rig.RunOk ("/MODE WINDBG");
            rig.RunIs ("!BMSAVE w.txt", { "Saved 1 bookmark to w.txt." });

            rig.RunOk ("!MODE GSSQUARED");
            rig.RunIs ("BMSAVE g.txt",  { "Saved 1 bookmark to g.txt." });

            for (const wchar_t * name : { L"m.txt", L"w.txt", L"g.txt" })
            {
                Assert::AreEqual (std::string ("BMC *\nBMA 0300\n"), rig.files.PeekContent (name), name);
            }
        }
    };
}

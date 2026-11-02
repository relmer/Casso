#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Debugger/Reply.h"
#include "Debugger/RoutineProfile.h"
#include "Debugger/SymbolTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RoutineProfileTests
//
//  The routines and the call tree, with Record called for each instruction
//  as the profile's hook calls it: where it ran, its opcode, its cycles and
//  the stack pointer after it.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (RoutineProfileTests)
    {
    public:

        static constexpr Byte      kLda   = 0xA9;
        static constexpr Byte      kPla   = 0x68;
        static constexpr Byte      kRts   = 0x60;
        static constexpr Byte      kRti   = 0x40;
        static constexpr uint64_t  kFrame = 17030;

        //  The profile and a clock that advances by each instruction's cycles.
        struct Feed
        {
            RoutineProfile  profile;
            uint64_t        clock = 0;

            void Run (Word pc, Byte opcode, uint64_t cycles, Byte spAfter)
            {
                profile.Record (pc, opcode, cycles, spAfter, clock, kFrame);
                clock += cycles;
            }
        };

        //  The node running a routine at an address, under any caller.
        static int FindNode (const RoutineProfile & profile, Word address, int parent = -2)
        {
            const std::vector<RoutineProfile::Node>  & nodes = profile.GetNodes();



            for (size_t i = 1; i < nodes.size(); i++)
            {
                if (profile.GetRoutines()[nodes[i].routine].address == address && (parent == -2 || nodes[i].parent == parent))
                {
                    return (int) i;
                }
            }

            return -1;
        }



        TEST_METHOD (ACallsCyclesAreItsOwnAndItsCallersTotal)
        {
            Feed                   feed;
            int                    node = 0;
            std::vector<uint64_t>  totals;



            // $0300: JSR $0400, $0400: LDA #0, RTS, then $0303: LDA #0
            feed.Run (0x0300, RoutineProfile::kJsr, 6, 0xFD);
            feed.Run (0x0400, kLda, 2, 0xFD);
            feed.Run (0x0402, kRts, 6, 0xFF);
            feed.Run (0x0303, kLda, 2, 0xFF);

            node   = FindNode (feed.profile, 0x0400);
            totals = feed.profile.GetNodeTotals();

            Assert::AreEqual (1, node > 0 ? 1 : 0, L"a node for the routine");
            Assert::AreEqual ((uint64_t) 8,  feed.profile.GetNodes()[(size_t) node].self,  L"its LDA and its RTS");
            Assert::AreEqual ((uint64_t) 1,  feed.profile.GetNodes()[(size_t) node].calls);
            Assert::AreEqual ((uint64_t) 8,  feed.profile.GetNodes()[0].self,              L"the JSR and the LDA after the return are Top level's");
            Assert::AreEqual ((uint64_t) 16, totals[0]);
        }



        TEST_METHOD (ARoutineCalledFromTwoCallersHasANodeUnderEach)
        {
            Feed  feed;
            int   a = 0;
            int   b = 0;



            // Top calls $0500 and $0600, and each calls $0700.
            for (Word routine : { (Word) 0x0500, (Word) 0x0600 })
            {
                feed.Run (0x0300, RoutineProfile::kJsr, 6, 0xFD);
                feed.Run (routine, RoutineProfile::kJsr, 6, 0xFB);
                feed.Run (0x0700, kRts, 6, 0xFD);
                feed.Run ((Word) (routine + 3), kRts, 6, 0xFF);
            }

            a = FindNode (feed.profile, 0x0500);
            b = FindNode (feed.profile, 0x0600);

            Assert::IsTrue (FindNode (feed.profile, 0x0700, a) > 0);
            Assert::IsTrue (FindNode (feed.profile, 0x0700, b) > 0);
            Assert::AreNotEqual (FindNode (feed.profile, 0x0700, a), FindNode (feed.profile, 0x0700, b));
        }



        //  $0500 calls itself once; its total is the whole chain's, not the
        //  chain's plus its own nested call again.
        TEST_METHOD (ARecursiveRoutineIsCountedOncePerChain)
        {
            Feed                   feed;
            std::vector<uint64_t>  totals;
            size_t                 routine = 0;



            feed.Run (0x0300, RoutineProfile::kJsr, 6, 0xFD);
            feed.Run (0x0500, RoutineProfile::kJsr, 6, 0xFB);
            feed.Run (0x0500, kLda, 2, 0xFB);
            feed.Run (0x0502, kRts, 6, 0xFD);
            feed.Run (0x0503, kRts, 6, 0xFF);

            totals  = feed.profile.GetRoutineTotals();
            routine = feed.profile.GetNodes()[(size_t) FindNode (feed.profile, 0x0500, 0)].routine;

            Assert::AreEqual ((uint64_t) 20, totals[routine], L"6 + 2 + 6 + 6, not 28");
        }



        //  A routine that pulls its return address returns from its caller:
        //  it ends with the pulls, and the RTS ends the caller.
        TEST_METHOD (PullingTheReturnAddressEndsTheRoutine)
        {
            Feed  feed;
            int   a = 0;



            feed.Run (0x0300, RoutineProfile::kJsr, 6, 0xFD);
            feed.Run (0x0500, RoutineProfile::kJsr, 6, 0xFB);
            feed.Run (0x0600, kPla, 4, 0xFC);
            feed.Run (0x0601, kPla, 4, 0xFD);
            feed.Run (0x0602, kRts, 6, 0xFF);
            feed.Run (0x0303, kLda, 2, 0xFF);

            a = FindNode (feed.profile, 0x0500);

            Assert::AreEqual ((uint64_t) 8,  feed.profile.GetNodes()[(size_t) FindNode (feed.profile, 0x0600, a)].self, L"the two pulls");
            Assert::AreEqual ((uint64_t) 12, feed.profile.GetNodes()[(size_t) a].self,                                  L"its JSR and the RTS");
            Assert::AreEqual ((uint64_t) 8,  feed.profile.GetNodes()[0].self);
        }



        //  A routine that pushes more than half the stack runs on until it
        //  returns: its pushes, its pulls and its RTS are billed to it.
        TEST_METHOD (ARoutineThatPushesMostOfTheStackRunsUntilItReturns)
        {
            static constexpr Byte  kPha    = 0x48;
            static constexpr int   kPushes = 200;

            Feed  feed;
            Byte  sp   = 0xFD;
            int   node = 0;



            feed.Run (0x0300, RoutineProfile::kJsr, 6, sp);

            for (int i = 0; i < kPushes; i++)
            {
                feed.Run (0x0500, kPha, 3, --sp);
            }

            for (int i = 0; i < kPushes; i++)
            {
                feed.Run (0x0501, kPla, 4, ++sp);
            }

            feed.Run (0x0502, kRts, 6, 0xFF);
            feed.Run (0x0303, kLda, 2, 0xFF);

            node = FindNode (feed.profile, 0x0500);

            Assert::AreEqual ((uint64_t) (kPushes * 3 + kPushes * 4 + 6), feed.profile.GetNodes()[(size_t) node].self, L"every push, pull and the RTS");
            Assert::AreEqual ((uint64_t) 8, feed.profile.GetNodes()[0].self, L"the JSR and the LDA after the return");
        }



        //  With the tree full, a call to a routine not yet in it runs in its
        //  caller's node, and the caller is not counted as called again; a
        //  merge into a full tree is the same.
        TEST_METHOD (ACallPastAFullTreeIsNotTheCallersCall)
        {
            Feed            feed;
            RoutineProfile  later;
            uint64_t        calls = 0;



            for (size_t i = 0; feed.profile.GetNodes().size() < RoutineProfile::kMaxNodes; i++)
            {
                feed.Run (0x0300, RoutineProfile::kJsr, 6, 0xFD);
                feed.Run ((Word) (0x1000 + i), kRts, 6, 0xFF);
            }

            calls = feed.profile.GetNodes()[0].calls;

            feed.Run (0x0300, RoutineProfile::kJsr, 6, 0xFD);
            feed.Run (0xF000, kRts, 6, 0xFF);

            Assert::AreEqual (calls, feed.profile.GetNodes()[0].calls, L"Top level is not called again");

            later.Record (0x0300, RoutineProfile::kJsr, 6, 0xFD, 0, kFrame);
            later.Record (0xF100, kRts, 6, 0xFF, 6, kFrame);
            feed.profile.Merge (later);

            Assert::AreEqual (calls, feed.profile.GetNodes()[0].calls, L"nor by a merge");
        }



        TEST_METHOD (AnInterruptAndABrkAreRoutinesOfTheirOwn)
        {
            Feed  feed;
            int   irq = 0;
            int   brk = 0;



            feed.Run (0x0300, kLda, 2, 0xFF);
            feed.profile.OnInterrupt (0x0302);
            feed.Run (0xFA62, kLda, 2, 0xFC);
            feed.Run (0xFA64, kRti, 6, 0xFF);
            feed.Run (0x0302, RoutineProfile::kBrk, 7, 0xFC);
            feed.Run (0xFA40, kRti, 6, 0xFF);
            feed.Run (0x0304, kLda, 2, 0xFF);

            irq = FindNode (feed.profile, 0xFA62);
            brk = FindNode (feed.profile, 0xFA40);

            Assert::IsTrue   (feed.profile.GetRoutines()[feed.profile.GetNodes()[(size_t) irq].routine].kind == RoutineKind::Interrupt);
            Assert::IsTrue   (feed.profile.GetRoutines()[feed.profile.GetNodes()[(size_t) brk].routine].kind == RoutineKind::Brk);
            Assert::AreEqual ((uint64_t) 8,  feed.profile.GetNodes()[(size_t) irq].self);
            Assert::AreEqual ((uint64_t) 6,  feed.profile.GetNodes()[(size_t) brk].self);
            Assert::AreEqual ((uint64_t) 11, feed.profile.GetNodes()[0].self, L"the LDA, the BRK and the LDA after");
        }



        //  An interrupt taken before the first instruction of a routine a JSR
        //  just entered: the routine starts there, and the handler runs under
        //  it.
        TEST_METHOD (AnInterruptRightAfterAJsrRunsUnderTheRoutineItEntered)
        {
            Feed  feed;
            int   routine = 0;



            feed.Run (0x0300, RoutineProfile::kJsr, 6, 0xFD);
            feed.profile.OnInterrupt (0x0400);
            feed.Run (0xFA62, kLda, 2, 0xFA);
            feed.Run (0xFA64, kRti, 6, 0xFD);
            feed.Run (0x0400, kLda, 2, 0xFD);
            feed.Run (0x0402, kRts, 6, 0xFF);
            feed.Run (0x0303, kLda, 2, 0xFF);

            routine = FindNode (feed.profile, 0x0400, 0);

            Assert::IsTrue   (routine > 0, L"a node for the routine the JSR entered");
            Assert::AreEqual ((uint64_t) 1, feed.profile.GetNodes()[(size_t) routine].calls);
            Assert::AreEqual ((uint64_t) 8, feed.profile.GetNodes()[(size_t) routine].self, L"its LDA and its RTS");
            Assert::IsTrue   (FindNode (feed.profile, 0xFA62, routine) > 0, L"the handler under it");
            Assert::AreEqual ((uint64_t) 8, feed.profile.GetNodes()[0].self, L"the JSR and the LDA after the return");
        }



        TEST_METHOD (FramesKeepEachFramesCyclesByRoutineAndOnlyTheLast)
        {
            Feed  feed;



            feed.Run (0x0300, RoutineProfile::kJsr, 6, 0xFD);
            feed.Run (0x0400, kRts, 6, 0xFF);

            Assert::AreEqual ((size_t) 1, feed.profile.GetFrames().size());
            Assert::AreEqual ((size_t) 2, feed.profile.GetFrames().back().shares.size(), L"Top level and $0400");

            for (size_t i = 0; i < RoutineProfile::kFrameCount + 5; i++)
            {
                feed.profile.Record (0x0300, kLda, 2, 0xFF, (uint64_t) (i + 1) * kFrame, kFrame);
            }

            Assert::AreEqual (RoutineProfile::kFrameCount, feed.profile.GetFrames().size());
            Assert::AreEqual ((uint64_t) RoutineProfile::kFrameCount + 5, feed.profile.GetFrames().back().index);

            feed.profile.Reset();
            Assert::AreEqual ((size_t) 0, feed.profile.GetFrames().size());
            Assert::AreEqual ((size_t) 1, feed.profile.GetNodes().size(), L"Top level alone");
        }



        //  Covering every frame, the profile has the first frame too, in no
        //  more than kFrameCount bars, each of the same number of frames.
        TEST_METHOD (FramesCoveringAllJoinIntoWiderBars)
        {
            constexpr size_t  kFrames = RoutineProfile::kFrameCount * 3;
            Feed              feed;
            uint64_t          cycles  = 0;



            feed.profile.SetFramesCoverAll (true);

            for (size_t i = 0; i < kFrames; i++)
            {
                feed.profile.Record (0x0300, kLda, 2, 0xFF, (uint64_t) i * kFrame, kFrame);
            }

            Assert::IsTrue   (feed.profile.GetFrames().size() <= RoutineProfile::kFrameCount);
            Assert::AreEqual ((uint64_t) 0, feed.profile.GetFrames().front().index, L"the first frame is in the first bar");
            Assert::AreEqual ((uint64_t) 4, feed.profile.GetFrames().front().count, L"two halvings of 1,800 frames");

            for (const RoutineProfile::Frame & frame : feed.profile.GetFrames())
            {
                cycles += frame.other;

                for (const RoutineProfile::Share & share : frame.shares)
                {
                    cycles += share.cycles;
                }
            }

            Assert::AreEqual ((uint64_t) kFrames * 2, cycles, L"every instruction's cycles are in a bar");
        }



        //  A later profile's routines, nodes and frames merge into the
        //  earlier's: a routine in both is one routine, its node's cycles and
        //  calls are added, and the frames run on in order.
        TEST_METHOD (MergeAddsALaterProfile)
        {
            Feed  first;
            Feed  later;
            int   node = 0;



            first.Run (0x0300, RoutineProfile::kJsr, 6, 0xFD);
            first.Run (0x0400, kRts, 6, 0xFF);

            later.clock = kFrame * 10;
            later.Run (0x0300, RoutineProfile::kJsr, 6, 0xFD);
            later.Run (0x0400, kLda, 2, 0xFD);
            later.Run (0x0402, kRts, 6, 0xFF);
            later.Run (0x0300, RoutineProfile::kJsr, 6, 0xFD);
            later.Run (0x0500, kRts, 6, 0xFF);

            first.profile.Merge (later.profile);

            node = FindNode (first.profile, 0x0400);
            Assert::AreEqual ((size_t) 3, first.profile.GetRoutines().size(), L"Top level, $0400 and $0500");
            Assert::AreEqual ((uint64_t) 14, first.profile.GetNodes()[(size_t) node].self, L"6 here and 8 later");
            Assert::AreEqual ((uint64_t) 2,  first.profile.GetNodes()[(size_t) node].calls);
            Assert::IsTrue   (FindNode (first.profile, 0x0500) > 0);
            Assert::AreEqual ((size_t) 2,  first.profile.GetFrames().size(), L"frame 0 and frame 10");
            Assert::AreEqual ((uint64_t) 10, first.profile.GetFrames().back().index);
        }



        //  Top level's cycles are counted by address, so its tip can list
        //  where they went; a call's cycles are not among them.
        TEST_METHOD (TopLevelsCyclesAreCountedByAddress)
        {
            Feed  feed;



            // $0300: LDA #0, JSR $0400, $0400: LDA #0, RTS, then $0300 again
            feed.Run (0x0300, kLda, 2, 0xFF);
            feed.Run (0x0302, RoutineProfile::kJsr, 6, 0xFD);
            feed.Run (0x0400, kLda, 2, 0xFD);
            feed.Run (0x0402, kRts, 6, 0xFF);
            feed.Run (0x0300, kLda, 2, 0xFF);

            Assert::AreEqual ((uint64_t) 4, feed.profile.GetTopCycles().at (0x0300));
            Assert::AreEqual ((uint64_t) 6, feed.profile.GetTopCycles().at (0x0302));
            Assert::IsFalse  (feed.profile.GetTopCycles().contains (0x0400), L"the call's own");
        }



        //  A profile started inside a call bills the call's cycles to it, not
        //  to Top level, until its RTS takes the stack to the frame's level.
        TEST_METHOD (AProfileSeededInsideACallBillsItUntilItReturns)
        {
            Feed            feed;
            CallStackFrame  frame;
            int             node = 0;



            frame.callSite   = 0x0300;
            frame.target     = 0x0400;
            frame.kind       = CallFrameKind::Call;
            frame.stackLevel = 0xFF;

            // At $0402, inside the call, SP $FD: LDA, RTS, then $0303: LDA
            feed.profile.Seed ({ frame }, 0xFD);
            feed.Run (0x0402, kLda, 2, 0xFD);
            feed.Run (0x0404, kRts, 6, 0xFF);
            feed.Run (0x0303, kLda, 2, 0xFF);

            node = FindNode (feed.profile, 0x0400);

            Assert::IsTrue   (node > 0, L"the open call is a node");
            Assert::AreEqual ((uint64_t) 8, feed.profile.GetNodes()[(size_t) node].self, L"its LDA and RTS");
            Assert::AreEqual ((uint64_t) 2, feed.profile.GetNodes()[0].self, L"only the LDA after the return is Top level's");
            Assert::AreEqual ((uint64_t) 0, feed.profile.GetNodes()[(size_t) node].calls, L"a call already open is not counted again");
        }



        //  A profile from history whose period begins where history does has a
        //  note in its tip, as a call made before then is counted as Top level.
        TEST_METHOD (TopLevelsTipNotesAPeriodThatBeginsWithHistory)
        {
            ProfileData  data;



            data.historyRange = CycleRange { 0, 1000 };
            Assert::IsTrue (data.GetTopLevelTip().find ("no JSR in the period called") != std::string::npos);
            Assert::IsTrue (data.GetTopLevelTip().find ("begins where history does") == std::string::npos);

            data.isFromHistoryStart = true;
            Assert::IsTrue (data.GetTopLevelTip().find ("begins where history does") != std::string::npos);
        }



        //  The tip lists Top level's hottest addresses with the nearest symbol,
        //  and a symbol a few bytes back reads as an offset from it.
        TEST_METHOD (TopLevelsTipListsItsHottestAddresses)
        {
            ProfileData  data;
            SymbolTable  symbols;
            std::string  name;



            symbols.Add (SymbolTableId::User, "NEWSTT", 0xD7D2);

            Assert::IsTrue   (symbols.TryFindNearestName (0xD7D5, 64, name));
            Assert::AreEqual (std::string ("NEWSTT+3"), name);
            Assert::IsTrue   (symbols.TryFindNearestName (0xD7D2, 64, name));
            Assert::AreEqual (std::string ("NEWSTT"), name);
            Assert::IsFalse  (symbols.TryFindNearestName (0xD900, 64, name), L"too far past it");

            data.topAddresses = { { 0xD7D5, "NEWSTT+3", 12345 } };

            Assert::IsTrue (data.GetTopLevelTip().starts_with ("Top level: the cycles in code that no JSR in the period called, such as code entered by a jump"));
            Assert::IsTrue (data.GetTopLevelTip().find ("$D7D5 NEWSTT+3: 12,345 cycles") != std::string::npos, TextEncoding::NarrowToWide (data.GetTopLevelTip()).c_str());
        }
    };
}

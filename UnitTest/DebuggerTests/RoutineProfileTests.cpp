#include "Pch.h"

#include "Debugger/RoutineProfile.h"

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



        TEST_METHOD (AnInterruptAndABrkAreRoutinesOfTheirOwn)
        {
            Feed  feed;
            int   irq = 0;
            int   brk = 0;



            feed.Run (0x0300, kLda, 2, 0xFF);
            feed.profile.OnInterrupt();
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
    };
}

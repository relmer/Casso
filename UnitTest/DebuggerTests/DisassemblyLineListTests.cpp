#include "Pch.h"

#include "Debugger/DisassemblyLineList.h"
#include "Ui/Debugger/DebuggerViewState.h"
#include "Disassembler.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassemblyLineListTests
//
//  The whole address space's lines in one forward pass: a debug file's
//  instructions and data where its bytes are memory's, a fetched start over
//  a misaligned decode, a marked range as data, and a new version at each
//  build.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DisassemblyLineListTests)
    {
    public:

        static constexpr Byte  kLda = 0xAD;    // three bytes
        static constexpr Byte  kNop = 0xEA;    // one byte


        //  Memory of NOPs with an opcode table where $AD is three bytes. The
        //  image is on the heap, as it is too large for a test's stack.
        struct Rig
        {
            std::unique_ptr<DebugMemoryImage>  owned = std::make_unique<DebugMemoryImage>();
            DebugMemoryImage                 & image = *owned;
            DisassemblyLineList                list;
            DisassemblyLineList::Inputs        inputs;

            Rig()
            {
                image.bytes.fill (kNop);
                inputs.image = &image;
                inputs.lengths.fill (1);
                inputs.lengths[kLda] = 3;
            }

            const DisassemblyLineList::Line &  LineAt (Word address)
            {
                return list.GetLines()[(size_t) list.FindLine (address)];
            }

            bool  StartsAt (Word address)
            {
                return list.GetLines()[(size_t) list.FindLine (address)].address == address;
            }
        };


        //  A debug file's instruction whose bytes match is a start; one whose
        //  bytes don't, as a part of the program overwritten since, is not.
        TEST_METHOD (ADebugFilesMatchingInstructionsAreStartsAndAMismatchIsIgnored)
        {
            Rig  rig;



            rig.image.bytes[0x0300] = kNop;
            rig.image.bytes[0x0301] = kLda;
            rig.image.bytes[0x0302] = kLda;
            rig.image.bytes[0x0303] = 0x00;
            rig.image.bytes[0x0304] = 0x20;
            rig.inputs.debugLines   = { { 0x0302, "LDA", 3, { kLda, 0x00, 0x20 } }, { 0x0310, "LDA", 3, { kLda, 0x00, 0x20 } } };

            rig.list.Build (rig.inputs);

            Assert::IsTrue  (rig.StartsAt (0x0302), L"the debug file's LDA, over the LDA a decode from $0301 would take");
            Assert::IsTrue  (rig.LineAt (0x0302).source == DisassemblyLineList::Source::DebugFile);
            Assert::IsTrue  (rig.LineAt (0x0301).isData, L"the byte before it, cut short as data");
            Assert::IsTrue  (rig.LineAt (0x0310).source == DisassemblyLineList::Source::Decoded, L"memory isn't the file's LDA there");
        }


        //  A fetched byte's alignment wins over a decode that would run over
        //  it.
        TEST_METHOD (AFetchedStartIsUsedOverAMisalignedDecode)
        {
            Rig  rig;



            rig.image.bytes[0x0400] = kLda;
            rig.image.fetched.set (0x0401);

            rig.list.Build (rig.inputs);

            Assert::IsTrue (rig.StartsAt (0x0401), L"the fetched start");
            Assert::IsTrue (rig.LineAt (0x0401).source == DisassemblyLineList::Source::Fetched);
            Assert::AreEqual ((int) 1, (int) rig.LineAt (0x0400).length, L"the LDA's decode stops at it");
        }


        //  A marked range is data lines, of the length the data's line takes.
        TEST_METHOD (AMarkedRangeStaysData)
        {
            Rig  rig;



            rig.image.bytes[0x0500] = kLda;
            rig.inputs.dataLine = [] (Word address, bool & isMarked) { isMarked = true; return (address >= 0x0500 && address < 0x0510) ? 8 : 0; };

            rig.list.Build (rig.inputs);

            Assert::IsTrue   (rig.LineAt (0x0500).isData);
            Assert::IsTrue   (rig.LineAt (0x0500).source == DisassemblyLineList::Source::Marked);
            Assert::IsTrue   (rig.StartsAt (0x0508), L"its second line");
            Assert::IsTrue   (rig.StartsAt (0x0510), L"and code after it");
            Assert::IsFalse  (rig.LineAt (0x0510).isData);
        }


        //  Each build is a new version, so a reader can tell its copy is old;
        //  the PC is a start; and lines step up and down.
        TEST_METHOD (EachBuildIsANewVersionAndLinesStep)
        {
            Rig       rig;
            uint64_t  first = 0;



            rig.image.bytes[0x0600] = kLda;
            rig.inputs.starts       = { 0x0602 };

            rig.list.Build (rig.inputs);
            first = rig.list.GetVersion();
            rig.list.Build (rig.inputs);

            Assert::AreNotEqual (first, rig.list.GetVersion());
            Assert::IsTrue      (rig.StartsAt (0x0602), L"the PC");
            Assert::AreEqual    ((int) 0x0600, (int) rig.list.StepLines (0x0602, -1), L"the LDA, cut short at the PC");
            Assert::AreEqual    ((int) 0x0603, (int) rig.list.StepLines (0x0602, 1));
            Assert::AreEqual    ((int) 0x0000, (int) rig.list.StepLines (0x0002, -10), L"stops at the first line");
        }


        //  Code that jumps into the middle of an instruction, as the BIT skip
        //  does: both starts are fetched, and the PC, at the inner one, starts
        //  a line, so the views have a row for it.
        TEST_METHOD (ThePcStartsALineInsideAnotherKnownInstruction)
        {
            static constexpr Byte  kBit = 0x2C;
            Rig                    rig;



            rig.inputs.lengths[kBit] = 3;
            rig.inputs.lengths[0xA9] = 2;
            rig.image.bytes[0x0800]  = kBit;
            rig.image.bytes[0x0801]  = 0xA9;
            rig.image.bytes[0x0802]  = 0x00;
            rig.image.fetched.set (0x0800);
            rig.image.fetched.set (0x0801);
            rig.inputs.starts        = { 0x0801 };
            rig.inputs.pc            = 0x0801;

            rig.list.Build (rig.inputs);

            Assert::IsTrue   (rig.StartsAt (0x0801), L"the PC's instruction");
            Assert::IsTrue   (rig.StartsAt (0x0800), L"and the BIT before it, cut short");
            Assert::AreEqual ((int) 1, (int) rig.LineAt (0x0800).length);
        }


        //  A label inside an instruction, as HOME at $FC58 inside the
        //  instruction decoded from $FC57, starts a line, so Go to on the
        //  symbol has a row; the instruction at the PC is not cut at a label.
        TEST_METHOD (ALabelStartsALineInsideADecodedInstruction)
        {
            static constexpr Byte  kBit = 0x2C;
            Rig                    rig;



            rig.inputs.lengths[kBit] = 3;
            rig.image.bytes[0x0800]  = kBit;
            rig.image.bytes[0x0900]  = kBit;
            rig.inputs.pc            = 0x0900;
            rig.inputs.labels        = { 0x0801, 0x0901 };

            rig.list.Build (rig.inputs);

            Assert::IsTrue   (rig.StartsAt (0x0801), L"the label's row");
            Assert::AreEqual ((int) 1, (int) rig.LineAt (0x0800).length, L"the decode before it, cut short");
            Assert::AreEqual ((int) 3, (int) rig.LineAt (0x0900).length, L"the PC's instruction is whole");
        }


        //  An executed jump into the middle of an instruction, as JMP $6004
        //  into the BIT at $6003: its target starts a line before the PC
        //  gets there. A jump the machine never fetched, an indirect jump
        //  and a jump into the I/O page add no target.
        TEST_METHOD (AnExecutedJumpsTargetStartsALine)
        {
            static constexpr Byte  kBit     = 0x2C;
            static constexpr Byte  kJmp     = 0x4C;
            static constexpr Byte  kJmpInd  = 0x6C;
            static constexpr Byte  kBne     = 0xD0;
            Rig                    rig;
            TestCpu                cpu;
            std::vector<Word>      targets;



            cpu.InitForTest();

            rig.inputs.lengths[kBit] = 3;
            rig.inputs.lengths[kJmp] = 3;

            //  $6000: JMP $6004   $6003: BIT $00A9, with LDA #$00 at $6004
            rig.image.bytes[0x6000] = kJmp;
            rig.image.bytes[0x6001] = 0x04;
            rig.image.bytes[0x6002] = 0x60;
            rig.image.bytes[0x6003] = kBit;
            rig.image.bytes[0x6004] = 0xA9;
            rig.image.bytes[0x6005] = 0x00;
            rig.image.fetched.set (0x6000);

            //  Never fetched: JMP $7004.  Fetched: JMP ($7104), JMP $C050 and BNE *+4.
            rig.image.bytes[0x7000] = kJmp;
            rig.image.bytes[0x7001] = 0x04;
            rig.image.bytes[0x7002] = 0x70;
            rig.image.bytes[0x7100] = kJmpInd;
            rig.image.bytes[0x7101] = 0x04;
            rig.image.bytes[0x7102] = 0x71;
            rig.image.bytes[0x7200] = kJmp;
            rig.image.bytes[0x7201] = 0x50;
            rig.image.bytes[0x7202] = 0xC0;
            rig.image.bytes[0x7300] = kBne;
            rig.image.bytes[0x7301] = 0x02;
            rig.image.fetched.set (0x7100);
            rig.image.fetched.set (0x7200);
            rig.image.fetched.set (0x7300);

            targets = DebuggerViewState::GetBranchTargets (Disassembler (cpu.GetInstructionSet()), rig.image);

            Assert::IsTrue (targets == std::vector<Word> { 0x6004, 0x7304 }, L"the executed JMP's and the executed branch's targets");

            rig.inputs.pc     = 0x6000;
            rig.inputs.starts = { 0x6000 };
            rig.inputs.labels = targets;
            rig.list.Build (rig.inputs);

            Assert::IsTrue   (rig.StartsAt (0x6004), L"the target's row");
            Assert::AreEqual ((int) 1, (int) rig.LineAt (0x6003).length, L"the BIT before it, cut short");
        }


        //  A call frame's JSR is a known start where memory has the JSR, so a
        //  decode from before it is cut short there; an interrupt's frame and
        //  a call site whose byte is no longer a JSR are not.
        TEST_METHOD (ACallFramesJsrIsAKnownStart)
        {
            static constexpr Byte  kJsr = 0x20;
            Rig                    rig;
            CallStackData          calls;
            CallStackFrame         call;
            CallStackFrame         irq;
            CallStackFrame         gone;



            rig.inputs.lengths[kJsr] = 3;
            rig.image.bytes[0x0700]  = kLda;
            rig.image.bytes[0x0702]  = kJsr;

            call.callSite = 0x0702;
            irq.callSite  = 0x0710;
            irq.kind      = CallFrameKind::Irq;
            gone.callSite = 0x0720;
            calls.rows    = { { call, std::nullopt }, { irq, std::nullopt }, { gone, std::nullopt } };

            rig.inputs.starts = DebuggerViewState::GetKnownStarts (calls, 0x0300, rig.image);
            rig.list.Build (rig.inputs);

            Assert::AreEqual ((size_t) 2, rig.inputs.starts.size(), L"the PC and the JSR");
            Assert::IsTrue   (rig.StartsAt (0x0702), L"the JSR, over the LDA's decode from $0700");
            Assert::AreEqual ((int) 3, (int) rig.LineAt (0x0702).length);
        }
    };
}

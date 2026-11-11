#include "Pch.h"

#include "Debugger/DisassemblyLineList.h"

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


        //  Memory of NOPs with an opcode table where $AD is three bytes.
        struct Rig
        {
            DebugMemoryImage               image;
            DisassemblyLineList            list;
            DisassemblyLineList::Inputs    inputs;

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
    };
}

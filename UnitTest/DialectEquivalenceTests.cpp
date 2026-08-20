#include "Pch.h"

#include "TestHelpers.h"
#include "Assembler.h"
#include "AssemblerTypes.h"
#include "DialectRegistry.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DialectEquivalenceTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DialectEquivalenceTests
    //
    //  One program, written twice, assembling to one answer.
    //
    //  Every other test of the dialect mechanism measures a dialect against
    //  something outside Casso -- the vendor's shipped objects for Merlin, the
    //  conformance expectations for as65. Those prove each dialect right on its
    //  own terms and say nothing about the two agreeing, because no source in
    //  either corpus exists in the other spelling.
    //
    //  This is that missing measurement, and the demo is the specimen because it
    //  is REAL: a boot loader with disk I/O, self-modifying reads, cross-stage
    //  entry points at fixed addresses, and 500 lines of it. A toy would agree
    //  by having nothing to disagree about.
    //
    //  THE MERLIN SOURCES ARE A MECHANICAL PORT AND ARE NOT TRUSTED A PRIORI.
    //  That is the point: the as65 build is the oracle, so byte equality is what
    //  proves the port faithful rather than something the port has to be
    //  believed about. A translation that dropped a line, mistook a label for an
    //  opcode, or picked the wrong addressing mode moves at least one byte, and
    //  a `.a65` edited without its `.S` twin fails here rather than silently
    //  leaving the pair describing two different programs.
    //
    //  What the pair exercises, by being what it is: the origin directive, the
    //  equate, column-1 labels, forward and backward branches, indexed and
    //  indirect-indexed addressing, and the case rule -- Merlin wants its
    //  mnemonics in upper case where as65 takes either.
    //
    ////////////////////////////////////////////////////////////////////////////////

    //  One program in both spellings. Held in a table so a third pair joins the
    //  sweep by being listed, and so the count below can assert what is covered.
    struct EquivalentPair
    {
        const char *  name;
        const char *  as65Path;     // relative to Apple2/Demos
        const char *  merlinPath;

        //  Whether the Merlin spelling is Merlin's ALONE. Equality between two
        //  files both dialects accept says only that the files agree; it cannot
        //  say the Merlin profile was consulted, because a mechanism that
        //  quietly fell back to as65 would produce the same bytes. A pair
        //  carrying this flag is required to FAIL under as65, which is what
        //  makes its equality mean something.
        bool          discriminates;
    };


    //  Stage 1 discriminates because Merlin's own name for the carry branch is
    //  BLT, which as65 rejects by name -- a faithful spelling rather than a
    //  contrivance, and it emits the same $90 either way.
    //
    //  Stage 2 does NOT, and the flag says so rather than implying otherwise.
    //  Its whole vocabulary -- loads, stores, compares, branches on zero -- is
    //  shared, and it contains no shift, no carry branch and no data directive
    //  to write in a Merlin-only form. Inventing one to decorate the flag would
    //  make the specimen less like the program it is.
    static constexpr EquivalentPair  s_kEquivalentPairs[] =
    {
        { "casso-rocks stage 1", "casso-rocks.a65",        "casso-rocks.S",        true  },
        { "casso-rocks stage 2", "casso-rocks-stage2.a65", "casso-rocks-stage2.S", false },
    };



    TEST_CLASS (DialectEquivalence)
    {
    public:

        //  Both spellings, byte for byte, with the count asserted first so a
        //  pair added later joins this sweep rather than escaping it.
        TEST_METHOD (EveryPairAssemblesToTheSameBytesUnderBothDialects)
        {
            Assert::AreEqual (static_cast<size_t> (2), std::size (s_kEquivalentPairs),
                              L"two pairs are committed; a new one belongs in this sweep");

            for (const EquivalentPair & pair : s_kEquivalentPairs)
            {
                std::vector<Byte>  fromAs65   = Assemble (pair.as65Path,   DialectId::As65);
                std::vector<Byte>  fromMerlin = Assemble (pair.merlinPath, DialectId::Merlin);

                //  Non-empty first. Two failed assemblies both produce nothing,
                //  and nothing equals nothing -- which is how this sweep would
                //  report agreement between two sources it never assembled.
                Assert::IsFalse (fromAs65.empty(),
                                 Widen (std::string (pair.name) + ": the as65 source assembled to nothing").c_str());
                Assert::IsFalse (fromMerlin.empty(),
                                 Widen (std::string (pair.name) + ": the Merlin source assembled to nothing").c_str());

                Assert::AreEqual (fromAs65.size(), fromMerlin.size(),
                                  Widen (std::string (pair.name) + ": the two spellings produced different lengths").c_str());

                for (size_t i = 0; i < fromAs65.size(); i++)
                {
                    if (fromAs65[i] != fromMerlin[i])
                    {
                        Assert::Fail (Widen (std::string (pair.name) + ": the two spellings differ at offset "
                                             + std::to_string (i) + " -- as65 emitted " + Hex (fromAs65[i])
                                             + " where Merlin emitted " + Hex (fromMerlin[i])).c_str());
                    }
                }
            }
        }



        //  A flagged pair must FAIL under as65, which is what stops the sweep
        //  above from passing on two files that never needed a Merlin profile at
        //  all.
        //
        //  `EQU` and `ORG` are NOT enough for that, which is worth recording
        //  because it is the assumption this test was first written on: as65
        //  spells both the same way, so a port using only those lands in the
        //  shared subset and assembles happily under either dialect.
        TEST_METHOD (EveryDiscriminatingPairIsRefusedByTheOtherDialect)
        {
            size_t  discriminating = 0;

            for (const EquivalentPair & pair : s_kEquivalentPairs)
            {
                if (!pair.discriminates)
                {
                    continue;
                }

                std::vector<Byte>  underAs65 = Assemble (pair.merlinPath, DialectId::As65);

                discriminating++;

                Assert::IsTrue (underAs65.empty(),
                                Widen (std::string (pair.name)
                                       + ": the Merlin source assembled under as65, so its equality proves nothing"
                                         " about the two dialects being different").c_str());
            }

            //  Every flag going false would make this test vacuous while leaving
            //  it green, so the sweep is required to have found work to do.
            Assert::IsTrue (discriminating > 0,
                            L"no committed pair is Merlin-only, so nothing here exercises the dialect mechanism");
        }

    private:

        //  Apple2/Demos, found from this file rather than from the working
        //  directory, which the test host does not promise.
        static std::string DemoDir()
        {
            std::string  thisFile = __FILE__;
            size_t       lastSep  = thisFile.find_last_of ("\\/");
            std::string  unitDir  = thisFile.substr (0, lastSep);

            lastSep = unitDir.find_last_of ("\\/");

            return unitDir.substr (0, lastSep) + "\\Apple2\\Demos";
        }



        //  A missing file reads as empty, so it fails as "assembled to nothing"
        //  -- which names the source -- rather than as an exception, which names
        //  nothing.
        static std::string ReadSource (const std::string & name)
        {
            std::ifstream       file (DemoDir() + "\\" + name);
            std::ostringstream  contents;

            if (file.is_open())
            {
                contents << file.rdbuf();
            }

            return contents.str();
        }



        //  The bytes one dialect makes of one file. Diagnostics are discarded
        //  deliberately: a source that fails to assemble returns no bytes, and
        //  every caller here treats empty as the failure.
        static std::vector<Byte> Assemble (const std::string & name, DialectId dialect)
        {
            TestCpu           cpu;
            AssemblerOptions  options = {};
            std::string       source  = ReadSource (name);

            cpu.InitForTest();
            options.dialect = dialect;

            Assembler       assembler (cpu.GetInstructionSet(), options);
            AssemblyResult  result = assembler.Assemble (source);

            return result.errors.empty() ? result.bytes : std::vector<Byte>();
        }



        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        static std::string Hex (Byte value)
        {
            const char *  digits = "0123456789ABCDEF";

            return std::string ("$") + digits[(value >> 4) & 0x0F] + digits[value & 0x0F];
        }
    };
}

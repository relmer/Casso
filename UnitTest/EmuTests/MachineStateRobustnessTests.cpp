#include "Pch.h"

#include "TestMachine.h"
#include "ReverseSessionRig.h"
#include "Core/StateReader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateRobustnessTests
//
//  A state file's checksum turns away damage, but a file made on purpose can
//  have a good checksum over bad contents, so the machine's sections are
//  read here with no checksum in front: a real machine's state cut short at
//  many places, and copies with bytes changed. Each load succeeds or fails,
//  and a machine a load succeeded on runs; none throws, and under
//  AddressSanitizer none reads or writes out of bounds.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineStateRobustnessTests)
{
public:

    TEST_METHOD (ALoadOfABrokenStateFailsOrSucceedsAndNeverThrows)
    {
        static constexpr const char *  kMachines[] = { "Apple2eEnhanced", "Apple2c" };
        static constexpr int           kCuts       = 120;
        static constexpr int           kChanges    = 300;
        static constexpr size_t        kStride     = 61;

        std::vector<std::string>  failures;
        std::mt19937              random (0x57A7E035u);
        size_t                    loads = 0;



        for (const char * name : kMachines)
        {
            auto               source = std::make_unique<TestMachine> (name, TestMachine::Slots::AsShipped);
            auto               target = std::make_unique<TestMachine> (name, TestMachine::Slots::AsShipped);
            std::vector<Byte>  state;



            source->RunCycles (200'000);
            state = ReverseSessionRig::Save (*source);

            {
                StateReader  reader (state);

                Assert::AreEqual (S_OK, target->LoadState (reader), L"the state as saved loads, so the copies are read through every section");
            }

            //  One byte flipped every kStride bytes through the whole state, so
            //  every section's fields and headers are hit.
            for (size_t at = 0; at < state.size(); at += kStride)
            {
                std::vector<Byte>  bytes = state;

                bytes[at] ^= 0xFF;
                Load (*target, bytes, std::format ("{} with byte {} flipped", name, at), failures);
                loads++;
            }

            for (int cut = 0; cut < kCuts; cut++)
            {
                std::vector<Byte>  bytes (state.begin(), state.begin() + (ptrdiff_t) (state.size() * cut / kCuts));

                Load (*target, bytes, std::format ("{} cut to {} of {} bytes", name, bytes.size(), state.size()), failures);
                loads++;
            }

            for (int copy = 0; copy < kChanges; copy++)
            {
                std::vector<Byte>  bytes = state;

                for (int change = 0; change < 4; change++)
                {
                    bytes[random() % bytes.size()] = (Byte) (random() & 0xFF);
                }

                Load (*target, bytes, std::format ("{} copy {} with bytes changed", name, copy), failures);
                loads++;
            }
        }

        Logger::WriteMessage (std::format ("{} loads\n", loads).c_str());

        for (const std::string & failure : failures)
        {
            Logger::WriteMessage ((failure + "\n").c_str());
        }

        Assert::AreEqual ((size_t) 0, failures.size(), L"loads that threw: see the output");
    }

private:

    static constexpr uint64_t  kRunCycles = 20'000;

    static void  Load (TestMachine & machine, const std::vector<Byte> & bytes, const std::string & what, std::vector<std::string> & failures)
    {
        try
        {
            StateReader  reader (bytes);
            HRESULT      hr     = machine.LoadState (reader);

            //  A load that succeeds may have read values a real machine never
            //  has; the machine then runs with them.
            if (SUCCEEDED (hr))
            {
                (void) machine.RunCycles (kRunCycles);
            }
        }
        catch (const std::exception & e)
        {
            failures.push_back (std::format ("{}: {}", what, e.what()));
        }
    }
};

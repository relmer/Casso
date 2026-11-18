#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "EmuTests/TestMachine.h"
#include "Machines/Apple3/Apple3System.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3StateTests
//
//  Every part of the Apple /// system, from the MMU to the sound, saved from
//  a machine that has run for a while, loads into a new machine and saves
//  again to the same bytes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3StateTests)
{
public:

    static constexpr uint64_t  kTicksPerSecond = 2040968;


    static std::vector<Byte> Save (IMachineState & part)
    {
        StateWriter  writer;
        HRESULT      hr = part.SaveState (writer);



        Assert::AreEqual (S_OK, hr);
        return writer.GetBytes();
    }


    TEST_METHOD (EveryPart_RoundTrips)
    {
        TestMachine                   running ("Apple3");
        TestMachine                   fresh   ("Apple3");
        std::vector<IMachineState *>  saved;
        std::vector<IMachineState *>  loaded;



        running.GetRunner().RunCycles (kTicksPerSecond);

        running.GetApple3()->GetStateParts (saved);
        fresh.GetApple3()->GetStateParts (loaded);

        Assert::AreEqual (saved.size(), loaded.size());

        for (size_t i = 0; i < saved.size(); i++)
        {
            std::vector<Byte>  bytes = Save (*saved[i]);
            StateReader        reader (bytes);
            HRESULT            hr    = loaded[i]->LoadState (reader);



            Assert::AreEqual (S_OK, hr, std::format (L"part {} loads", i).c_str());
            Assert::IsTrue   (reader.IsAtEnd(), std::format (L"part {} reads its whole section", i).c_str());
            Assert::IsTrue   (Save (*loaded[i]) == bytes, std::format (L"part {} saves the same bytes again", i).c_str());
        }
    }


    //  A machine loaded from a running one's state runs on as the running one
    //  does: after the same cycles, every part saves the same bytes.
    TEST_METHOD (ALoadedMachine_RunsAsTheOneItWasSavedFrom)
    {
        TestMachine                   running ("Apple3");
        TestMachine                   loaded  ("Apple3");
        StateWriter                   writer;
        std::vector<IMachineState *>  runningParts;
        std::vector<IMachineState *>  loadedParts;
        HRESULT                       hr = S_OK;



        running.GetRunner().RunCycles (kTicksPerSecond);

        hr = running.GetStateStream().SaveState (writer);
        Assert::AreEqual (S_OK, hr);

        StateReader  reader (writer.GetBytes());

        hr = loaded.GetStateStream().LoadState (reader);
        Assert::AreEqual (S_OK, hr);

        running.GetRunner().RunCycles (kTicksPerSecond / 2);
        loaded.GetRunner().RunCycles  (kTicksPerSecond / 2);

        running.GetApple3()->GetStateParts (runningParts);
        loaded.GetApple3()->GetStateParts  (loadedParts);
        runningParts.insert (runningParts.begin(), running.GetCpu());
        loadedParts.insert  (loadedParts.begin(),  loaded.GetCpu());

        Assert::AreEqual (runningParts.size(), loadedParts.size());

        for (size_t i = 0; i < runningParts.size(); i++)
        {
            Assert::IsTrue (Save (*runningParts[i]) == Save (*loadedParts[i]), std::format (L"part {} of {} matches", i, runningParts.size()).c_str());
        }
    }


    //  A section from another part fails to load, so a state saved from a
    //  different machine cannot be loaded into one of these parts.
    TEST_METHOD (APart_FailsToLoadAnotherPartsSection)
    {
        TestMachine                   machine ("Apple3");
        std::vector<IMachineState *>  parts;



        machine.GetApple3()->GetStateParts (parts);

        for (size_t i = 0; i + 1 < parts.size(); i++)
        {
            std::vector<Byte>  bytes = Save (*parts[i + 1]);
            StateReader        reader (bytes);
            HRESULT            hr    = parts[i]->LoadState (reader);



            Assert::IsTrue (FAILED (hr), std::format (L"part {} fails to load part {}'s section", i, i + 1).c_str());
        }
    }
};

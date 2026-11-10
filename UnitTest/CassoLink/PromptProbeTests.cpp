#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "../EmuTests/TestMachine.h"
#include "../EmuTests/TextScreenScraper.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkBootstrap.h"
#include "CassoLink/Session/CassoLinkConnector.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Seams/InProcessByteStream.h"
#include "InMemoryCassoLinkFolder.h"
#include "SteppedByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PromptProbeTests
//
//  The prompt probe and the typed bootstrap on a //c's firmware, at Applesoft's
//  prompt and under DOS 3.3, with IN#2 and Ctrl-A 14B typed. The probe's and
//  the loader's lines are written to port 2 at the session's pace, converted
//  to the machine's cycles, and each $A5 has to arrive within the session's
//  wait for it, also in cycles. The machine runs only when the test runs it,
//  so nothing depends on the PC's clock.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PromptProbeTests)
{
public:

    static constexpr uint64_t  kClockHz       = 1'020'484;
    static constexpr uint64_t  kKeyCycles     = 100'000;
    static constexpr uint64_t  kStepCycles    = 17'030;
    static constexpr uint64_t  kCharCycles    = (uint64_t) LinkBootstrap::kCharPauseUs * kClockHz / 1'000'000;
    static constexpr uint64_t  kLineCycles    = (uint64_t) LinkBootstrap::kLinePauseMs * kClockHz / 1'000;
    static constexpr uint64_t  kProbeWait     = (uint64_t) CassoLinkConnector::kPromptProbeWaitMs * kClockHz / 1'000;
    static constexpr uint64_t  kLoaderWait    = (uint64_t) LinkBootstrap::kLoaderReadyWaitMs * kClockHz / 1'000;
    static constexpr DWORD     kBootstrapBaud = 9600;
    static constexpr int       kPort          = 2;
    static constexpr Byte      kCatalogTrack  = 17;

    struct Rig
    {
        TestMachine                           machine { "Apple2c" };
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;

        //  At Applesoft's prompt, with IN#2 and Ctrl-A 14B typed.
        Rig()
        {
            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
            machine.PowerCycle();
            machine.RunCycles (2 * kClockHz);
            machine.GetCpu()->SetPC (0xE000);
            machine.RunCycles (2 * kClockHz);

            InProcessByteStream::CreatePair (machineEnd, pcEnd);
            Assert::AreEqual (S_OK, machine.AttachSerialStream (kPort, machineEnd));

            Type ("IN#2\r");
            Type ("\x01" "14B");
        }

        void  Type (const char * text)
        {
            for (size_t i = 0; text[i] != '\0'; i++)
            {
                machine.GetRefs().iieKeyboard->PressKey ((Byte) text[i]);
                machine.RunCycles (kKeyCycles);
            }
        }

        //  Each line a character at a time, as LinkBootstrap::Send writes it,
        //  with its first `dropped` characters left out.
        void  Send (const std::vector<std::string> & lines, size_t dropped = 0)
        {
            for (const std::string & line : lines)
            {
                for (size_t at = std::min (dropped, line.size()); at < line.size(); at++)
                {
                    AssertSucceeded (pcEnd->Write (reinterpret_cast<const Byte *> (line.data() + at), 1));
                    machine.RunCycles (kCharCycles);
                }

                machine.RunCycles (kLineCycles);
            }
        }

        //  Runs the machine until $A5 arrives or `budget` cycles pass, and
        //  returns the cycles it took, or UINT64_MAX when it never arrived.
        uint64_t  AwaitReady (uint64_t budget)
        {
            Byte      got[64] = {};
            size_t    read    = 0;
            uint64_t  spent   = 0;

            while (spent <= budget)
            {
                AssertSucceeded (pcEnd->Read (got, sizeof (got), 0, read));

                if (std::find (got, got + read, LinkBootstrap::kLoaderReady) != got + read)
                {
                    return spent;
                }

                machine.RunCycles (kStepCycles);
                spent += kStepCycles;
            }

            return UINT64_MAX;
        }

        static std::vector<std::string>  ProbeLines()
        {
            std::vector<std::string>  lines (std::begin (CassoLinkConnector::kPromptProbe), std::end (CassoLinkConnector::kPromptProbe));



            lines.insert (lines.begin(), LinkBootstrap::kCancelLine);
            return lines;
        }

        std::string  Screen()
        {
            std::string  text;

            for (const std::string & row : TextScreenScraper::Scrape (machine))
            {
                text += row + "|";
            }

            return text;
        }
    };

    static void  AssertReadyInTime (uint64_t spent, uint64_t budget, const wchar_t * what)
    {
        Assert::IsTrue (spent != UINT64_MAX, std::format (L"{}: no $A5 within {} cycles", what, budget).c_str());
    }



    TEST_METHOD (AtApplesoftsPrompt_TheProbe_SendsA5WithinTheProbesWait)
    {
        Rig  rig;

        rig.Send (Rig::ProbeLines());

        AssertReadyInTime (rig.AwaitReady (kProbeWait), kProbeWait, L"the probe");
    }



    //  The loader typed at the session's pace: its bytes in memory as
    //  assembled before the line that runs it, then its $A5, and the shim
    //  sent to it. A host on the test's thread then loads the program.
    static std::unique_ptr<CassoLinkHost>  Bootstrap (Rig & rig, std::shared_ptr<SteppedByteStream> & outStream, InMemoryCassoLinkFolder & folder)
    {
        std::vector<std::string>  lines;
        std::vector<Byte>         loader;
        std::vector<Byte>         transfer;
        std::string               errors;
        std::string               run;



        AssertSucceeded (LinkBootstrap::BuildLines (lines));
        AssertSucceeded (LinkProgram::AssembleSerialLoader (loader, errors));

        run = lines.back();
        lines.pop_back();
        rig.Send (lines);

        for (size_t i = 0; i < loader.size(); i++)
        {
            Assert::AreEqual<int> (loader[i], rig.machine.GetMemoryBus().ReadByte ((Word) (LinkProgram::kSerialLoaderAddress + i)), std::format (L"the typed loader's byte {}", i).c_str());
        }

        rig.Send ({ run });
        AssertReadyInTime (rig.AwaitReady (kLoaderWait), kLoaderWait, L"the loader");

        AssertSucceeded (LinkBootstrap::BuildShimTransfer (kBootstrapBaud, transfer));
        AssertSucceeded (rig.pcEnd->Write (transfer.data(), transfer.size()));

        outStream = std::make_shared<SteppedByteStream> (rig.machine, rig.pcEnd);
        return std::make_unique<CassoLinkHost> (outStream, folder, [] () { return std::array<Byte, 4> {}; });
    }

    //  Slot 2's screen holes in main memory, $47A to $7FA, one in each 128
    //  bytes of the page; then every screen hole in auxiliary memory, where
    //  the //c has each port's settings from power-on. 80STORE and PAGE2
    //  select auxiliary memory for the text page while it is read.
    static std::vector<Byte>  ReadPortHoles (TestMachine & machine)
    {
        std::vector<Byte>  holes;



        for (Word at = 0x047A; at < 0x0800; at += 0x80)
        {
            holes.push_back (machine.GetMemoryBus().ReadByte (at));
        }

        machine.GetMemoryBus().WriteByte (0xC001, 0);
        machine.GetMemoryBus().ReadByte (0xC055);

        for (Word slot = 0; slot < 8; slot++)
        {
            for (Word at = 0x0478 + slot; at < 0x0800; at += 0x80)
            {
                holes.push_back (machine.GetMemoryBus().ReadByte (at));
            }
        }

        machine.GetMemoryBus().ReadByte (0xC054);
        machine.GetMemoryBus().WriteByte (0xC000, 0);
        return holes;
    }



    //  The typed bootstrap at Applesoft's prompt: the loader is in memory as
    //  assembled, and the program, loaded through the shim, reads a disk.
    TEST_METHOD (AtApplesoftsPrompt_TheTypedBootstrap_LoadsTheProgram)
    {
        Rig                                 rig;
        InMemoryCassoLinkFolder             folder;
        std::shared_ptr<SteppedByteStream>  stream;
        std::unique_ptr<CassoLinkHost>      host;
        FixtureProvider                     fixtures;
        std::vector<uint8_t>                bytes;
        const Byte                          sector[] = { 0 };
        std::vector<Byte>                   data;
        CassoLinkStatus                     status;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        AssertSucceeded (rig.machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, std::vector<Byte> (bytes.begin(), bytes.end())));

        host = Bootstrap (rig, stream, folder);

        AssertSucceeded (host->ReadSectors (2, kCatalogTrack, sector, data, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, L"drive 2's catalog over the bootstrapped link");
        Assert::IsTrue (rig.Screen().starts_with ("CASSOLINK"), L"the program's status screen");
    }



    //  The //c's port 2 settings are in its screen holes, and each port's
    //  settings from power-on are in the holes in auxiliary memory. After a
    //  bootstrap, a link in 80 columns and Escape, they match their values
    //  before the bootstrap, so the port and its Ctrl-A work.
    TEST_METHOD (AfterALinkAndEscape_ThePortsScreenHoles_AreAsBeforeTheBootstrap)
    {
        Rig                                 rig;
        InMemoryCassoLinkFolder             folder;
        std::shared_ptr<SteppedByteStream>  stream;
        std::unique_ptr<CassoLinkHost>      host;
        std::vector<Byte>                   before = ReadPortHoles (rig.machine);
        std::vector<Byte>                   after;



        Assert::IsTrue (std::any_of (before.begin(), before.end(), [] (Byte b) { return b != 0xA0; }), L"IN#2 set the port's holes");

        host = Bootstrap (rig, stream, folder);
        AssertSucceeded (host->SetColumns (true));

        rig.machine.GetRefs().iieKeyboard->PressKey (0x1B);
        rig.machine.RunCycles (2 * kClockHz);
        Assert::AreEqual (std::string::npos, rig.Screen().find ("CASSOLINK"), L"Escape ended CassoLink");

        after = ReadPortHoles (rig.machine);

        for (size_t i = 0; i < before.size(); i++)
        {
            Assert::AreEqual<int> (before[i], after[i], std::format (L"screen hole {}", i).c_str());
        }
    }
};

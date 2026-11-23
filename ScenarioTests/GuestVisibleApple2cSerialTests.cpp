#include "Pch.h"

#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "MachineIdle.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "Core/TextEncoding.h"
#include "Devices/Acia6551.h"
#include "Devices/IAciaEndpoint.h"
#include "CassoLink/LinkBootstrap.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleApple2cSerialTests
//
//  The //c's own serial ports, run by Apple's //c firmware, on the 6551 the
//  Super Serial Card shares. A listing sent to the printer port with PR#1 is
//  transmitted whole, one character time apart at the baud rate the firmware
//  sets, rather than all at once. The CassoLink loader typed into port 1
//  runs and replies on port 1.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleApple2cSerialTests)
{
public:

    //  PR#1, LIST and PR#0 on a //c booted from the DOS 3.3 master, with each
    //  byte of the printer port logged with the cycle it finished sending on.
    TEST_METHOD (ListingSentToThePrinterPortIsPacedAtItsBaudRate)
    {
        std::vector<Byte>          master = GuestSession::RequireDos33Master();
        TestMachine                machine ("Apple2c");
        std::vector<std::string>    rows;
        Acia6551                  * port1      = nullptr;
        Recorder                    recorder;
        std::string                 sent;
        uint64_t                    minGap     = UINT64_MAX;
        double                      charCycles = 0.0;



        BootFromReset (machine, master);

        rows = ReadScreen (machine);
        Assert::IsTrue (AnyRowStartsWith (rows, "]"), Widen ("the //c boots to Applesoft: " + JoinRows (rows)).c_str());

        Assert::IsTrue (machine.GetRefs().serialChips.size() >= 1, L"the //c has its serial ports");
        port1        = machine.GetRefs().serialChips[0];
        recorder.cpu = machine.GetCpu();
        port1->SetEndpoint (&recorder);

        //  A printer on the line drives clear to send, data set ready and
        //  carrier detect on; with CTS off the 6551 does not transmit.
        port1->SetModemInputs (true, true, true);

        TypeLine (machine, "10 PRINT \"HELLO\"");
        TypeLine (machine, "20 END");
        TypeLine (machine, "PR#1");
        TypeLine (machine, "LIST");
        TypeLine (machine, "PR#0");

        port1->SetEndpoint (nullptr);

        for (const Sent & s : recorder.sent)
        {
            sent += static_cast<char> (s.value & 0x7F);
        }

        Assert::IsTrue (sent.find ("10  PRINT \"HELLO\"") != std::string::npos, Widen ("line 10 left the printer port: " + sent).c_str());
        Assert::IsTrue (sent.find ("20  END")             != std::string::npos, Widen ("line 20 too: " + sent).c_str());

        for (size_t i = 1; i < recorder.sent.size(); i++)
        {
            minGap = std::min (minGap, recorder.sent[i].cycle - recorder.sent[i - 1].cycle);
        }

        charCycles = port1->GetTransmitCharacterCycles();

        Assert::IsTrue (recorder.sent.size() > 20, L"enough bytes to measure");
        Assert::IsTrue ((double) minGap >= charCycles * kGapTolerance,
                        Widen (std::format ("no two bytes closer than one character time: {} cycles apart, the character time {:.0f}",
                                            minGap, charCycles)).c_str());
    }


    //  The loader typed into port 1 after Ctrl+Reset, with DOS 3.3 loaded:
    //  IN#1 and the bootstrap dialog's POKEs for port 1, then the loader
    //  lines at the session's pace. The loader sends its $A5 on port 1.
    TEST_METHOD (LoaderTypedIntoPort1UnderDos_SendsItsReadyByteOnPort1)
    {
        std::vector<std::string>  rows;
        bool                      ready = RunPort1Bootstrap (GuestSession::RequireDos33Master(), kDosPort1Line, rows);



        Assert::IsTrue (ready, Widen ("the loader's $A5 on port 1: " + JoinRows (rows)).c_str());
    }


    //  The same under ProDOS's BASIC.SYSTEM, with the dialog's POKEs for it.
    TEST_METHOD (LoaderTypedIntoPort1UnderProDos_SendsItsReadyByteOnPort1)
    {
        std::vector<std::string>  rows;
        bool                      ready = RunPort1Bootstrap (GuestSession::RequireProDosUsersDisk(), kProDosPort1Line, rows);



        Assert::IsTrue (ready, Widen ("the loader's $A5 on port 1: " + JoinRows (rows)).c_str());
    }


private:

    //  Boots the image, presses Ctrl+Reset, types IN#1 and pokeLine, and types
    //  the loader into port 1. Returns whether the loader sent its $A5 there.
    static bool RunPort1Bootstrap (const std::vector<Byte> & image, const char * pokeLine, std::vector<std::string> & rows)
    {
        TestMachine                machine ("Apple2c");
        std::vector<std::string>   lines;
        Acia6551                 * port1   = nullptr;
        Recorder                   recorder;
        bool                       ready   = false;



        BootFromReset (machine, image);
        machine.SoftReset();
        machine.RunCycles (kLineCycles);

        port1        = machine.GetRefs().serialChips[0];
        recorder.cpu = machine.GetCpu();
        port1->SetEndpoint (&recorder);
        port1->SetModemInputs (true, true, true);

        //  The //c drops the first key after Ctrl+Reset.
        TypeLine (machine, "");
        TypeLine (machine, "IN#1");
        TypeLine (machine, pokeLine);

        AssertSucceeded (LinkBootstrap::BuildLines (lines));

        for (const std::string & line : lines)
        {
            for (char c : line)
            {
                port1->EnqueueReceivedBytes (reinterpret_cast<const Byte *> (&c), 1);
                machine.RunCycles (kSerialCharCycles);
            }

            machine.RunCycles (kSerialLineCycles);
        }

        machine.RunCycles (kLoaderCycles);
        port1->SetEndpoint (nullptr);

        for (const Sent & s : recorder.sent)
        {
            ready = ready || s.value == LinkBootstrap::kLoaderReady;
        }

        rows = ReadScreen (machine);
        return ready;
    }


    struct Sent
    {
        Byte      value = 0;
        uint64_t  cycle = 0;
    };


    //  Each byte as it finishes sending, with the CPU cycle it finished on.
    struct Recorder : public IAciaEndpoint
    {
        EmuCpu *           cpu = nullptr;
        std::vector<Sent>  sent;

        void  OnByteTransmitted (Byte value) override
        {
            sent.push_back ({ value, (cpu != nullptr) ? cpu->GetTotalCycles() : 0 });
        }
    };


    //  A byte finishes on the CPU instruction after its last bit time, so two
    //  can land up to an instruction closer than the character time.
    static constexpr double    kGapTolerance  = 0.98;
    static constexpr uint64_t  kKeyGapCycles  = 30'000;
    static constexpr uint64_t  kLineCycles    = 1'000'000;
    static constexpr uint64_t  kBootCycles    = 40'000'000;

    //  The bootstrap's pace: LinkBootstrap::kCharPauseUs after each character
    //  and LinkBootstrap::kLinePauseMs after each line, in cycles.
    static constexpr uint64_t  kSerialCharCycles = 3'100;
    static constexpr uint64_t  kSerialLineCycles = 256'000;
    static constexpr uint64_t  kLoaderCycles     = 3'000'000;

    //  The bootstrap dialog's POKEs for port 1 with DOS 3.3 loaded, and with
    //  ProDOS's BASIC.SYSTEM.
    static constexpr const char *  kDosPort1Line    = "POKE 1913,65:POKE 49306,11:POKE 49307,30:POKE 43605,5:POKE 43606,193";
    static constexpr const char *  kProDosPort1Line = "POKE 1913,65:POKE 49306,11:POKE 49307,30:POKE 48690,5:POKE 48691,193";


    //  The //c starts from its own reset, which sets up the serial ports'
    //  defaults, and boots the mounted disk itself, rather than from the
    //  controller ROM as GuestSession's boot does.
    static void BootFromReset (MachineHost & machine, const std::vector<Byte> & image)
    {
        uint64_t  ran = 0;



        machine.PowerCycle();
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "master.dsk", DiskFormat::Dsk, image), L"mount");

        ran = MachineIdle::RunUntilIdle (machine, kBootCycles);
        IGNORE_RETURN_VALUE (ran, 0);
    }


    static void TypeLine (MachineHost & machine, const char * line)
    {
        for (const char * c = line; *c != '\0'; c++)
        {
            KeystrokeInjector::InjectKey (machine, static_cast<Byte> (*c));
            machine.RunCycles (kKeyGapCycles);
        }

        KeystrokeInjector::InjectKey (machine, KeystrokeInjector::kAppleReturn);
        machine.RunCycles (kLineCycles);
    }


    static std::vector<std::string> ReadScreen (MachineHost & machine)
    {
        std::vector<std::string>  rows = TextScreenScraper::Scrape (machine);

        for (std::string & row : rows)
        {
            row = GuestSession::TrimTrailingBlanks (row);
        }

        return rows;
    }


    static bool AnyRowStartsWith (const std::vector<std::string> & rows, const char * prefix)
    {
        bool  found = false;



        for (const std::string & row : rows)
        {
            found = found || row.starts_with (prefix);
        }

        return found;
    }


    static std::string JoinRows (const std::vector<std::string> & rows)
    {
        std::string  text;

        for (const std::string & row : rows)
        {
            text += row + " | ";
        }

        return text;
    }


    static std::wstring Widen (const std::string & text)
    {
        return TextEncoding::NarrowToWide (text);
    }
};

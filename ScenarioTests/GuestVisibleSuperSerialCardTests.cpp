#include "Pch.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "Core/MachineConfig.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleSuperSerialCardTests
//
//  The Super Serial Card driven by Apple's own firmware, from DOS 3.3 and
//  Applesoft, the way the manual has a user drive it: PR#2 to print, IN#2 to
//  read from the port.
//
//  The firmware is the 2 KB image Casso downloads into
//  %LOCALAPPDATA%\Casso\Devices\SuperSerialCard\SSC.rom. It is not committed,
//  so these cases fail without it, as the stock-disk cases do without theirs.
//
//  The firmware, the switches and the connector all come from the slot entry,
//  so MachineBuilder loads and attaches them as it does for Casso itself.
//
//  Keys are typed with a pause after each one. While it sends a character,
//  the firmware reads the keyboard and discards any key pressed there,
//  which includes a key pressed the moment the previous one is read.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleSuperSerialCardTests)
{
public:

    //  Printer mode, 9,600 baud, 40 characters with the Apple screen on: the
    //  listing is written to the file on the connector and to the screen.
    TEST_METHOD (ListingInPrinterModeIsWrittenToTheFileAndTheScreen)
    {
        std::vector<Byte>          master = GuestSession::RequireDos33Master();
        std::filesystem::path      output = MakeOutputPath (L"printer-screen-on.txt");
        std::string                sent;
        std::vector<std::string>   rows;

        {
            TestMachine  machine ("Apple2e", [&] (MachineConfig & config)
            {
                ConfigureCard (config, kPrinterSw1, kPrinterScreenOnSw2, kpszSerialFileOut, output);
            });

            GuestSession::BootToPrompt (machine, master);
            EnterProgram (machine);
            TypeLine (machine, "PR#2");
            TypeLine (machine, "LIST");
            TypeLine (machine, "PR#0");

            rows = ReadScreen (machine);
        }

        sent = ReadSentText (output);

        Assert::IsTrue (sent.find (kListingLine10) != std::string::npos, Widen ("line 10 is in the file: " + sent).c_str());
        Assert::IsTrue (sent.find (kListingLine20) != std::string::npos, Widen ("line 20 is in the file: " + sent).c_str());
        Assert::IsTrue (GuestSession::AnyRowIs (rows, kListingLine10), L"and on the screen, which is on at this line width");
        Assert::IsTrue (GuestSession::AnyRowIs (rows, kListingLine20));
    }


    //  Printer mode at the switch settings it ships with: 80 characters with
    //  the Apple screen off, and a quarter second after each carriage return.
    //  The listing is written to the file, after the delays, and not to the
    //  screen.
    TEST_METHOD (ListingInPrinterModeWithTheScreenOffIsWrittenOnlyToTheFile)
    {
        std::vector<Byte>          master = GuestSession::RequireDos33Master();
        std::filesystem::path      output = MakeOutputPath (L"printer-screen-off.txt");
        std::string                sent;
        std::vector<std::string>   rows;

        {
            TestMachine  machine ("Apple2e", [&] (MachineConfig & config)
            {
                ConfigureCard (config, kPrinterSw1, kShippedSw2, kpszSerialFileOut, output);
            });

            GuestSession::BootToPrompt (machine, master);
            EnterProgram (machine);
            TypeLine (machine, "PR#2");
            TypeLine (machine, "LIST", kDelayedListingCycles);
            TypeLine (machine, "PR#0");

            rows = ReadScreen (machine);
        }

        sent = ReadSentText (output);

        Assert::IsTrue  (sent.find (kListingLine10) != std::string::npos, Widen ("line 10 is in the file: " + sent).c_str());
        Assert::IsTrue  (sent.find (kListingLine20) != std::string::npos, Widen ("line 20 is in the file: " + sent).c_str());
        Assert::IsFalse (GuestSession::AnyRowIs (rows, kListingLine10), L"and not on the screen, which is off at this line width");
    }


    //  Communications mode with a loopback plug: IN#2 selects the port for
    //  input, so each typed key is sent, comes back through the plug, and is
    //  what Applesoft reads.
    TEST_METHOD (LineTypedInCommunicationsModeComesBackThroughALoopbackPlug)
    {
        std::vector<Byte>          master  = GuestSession::RequireDos33Master();
        TestMachine                machine ("Apple2e", [] (MachineConfig & config)
        {
            ConfigureCard (config, kCommunicationsSw1, kShippedSw2, kpszSerialLoopback, {});
        });
        std::vector<std::string>   rows;



        GuestSession::BootToPrompt (machine, master);
        TypeLine (machine, "IN#2");
        TypeLine (machine, "PRINT 6*7");

        rows = ReadScreen (machine);

        Assert::IsTrue (GuestSession::AnyRowIs (rows, "]PRINT 6*7"), L"the line came back through the plug");
        Assert::IsTrue (GuestSession::AnyRowIs (rows, "42"),         L"and Applesoft ran it");
    }


    //  Communications mode with a file sent into the connector: after IN#2,
    //  Applesoft reads the file's lines as input, the last line, IN#0,
    //  selects the keyboard again, and the program from the file runs.
    TEST_METHOD (ProgramInAFileOnTheConnectorIsEnteredInCommunicationsMode)
    {
        std::vector<Byte>          master = GuestSession::RequireDos33Master();
        std::filesystem::path      input  = MakeOutputPath (L"program-in.txt");
        std::vector<std::string>   rows;



        WriteFile (input, "10 PRINT \"FROM A FILE\"\r20 END\rIN#0\r");

        {
            TestMachine  machine ("Apple2e", [&] (MachineConfig & config)
            {
                ConfigureCard (config, kCommunications300Sw1, kShippedSw2, kpszSerialFileIn, input);
            });

            GuestSession::BootToPrompt (machine, master);
            TypeLine (machine, "IN#2", kFileInCycles);
            TypeLine (machine, "RUN");

            rows = ReadScreen (machine);
        }

        Assert::IsTrue (GuestSession::AnyRowIs (rows, "FROM A FILE"), Widen ("the program from the file ran: " + JoinRows (rows)).c_str());
    }


private:

    //  Apple's switch settings as the card ships, but in printer or
    //  communications mode (SW1-5), at 9,600 baud.
    static constexpr const char *  kPrinterSw1        = "off off off on off on on";
    static constexpr const char *  kCommunicationsSw1 = "off off off on on on on";
    static constexpr const char *  kShippedSw2        = "on on off on off off off";

    //  Communications mode at 300 baud. With no handshake on the line, a
    //  character arriving every 1,070 cycles at 9,600 baud overruns the 6551
    //  while Applesoft stores a line, as it would on the real card.
    static constexpr const char *  kCommunications300Sw1 = "on off off on on on on";

    //  In printer mode: no delay after a carriage return (SW2-2 off), 40
    //  characters with the Apple screen on (SW2-3 and SW2-4 on).
    static constexpr const char *  kPrinterScreenOnSw2 = "on off on on off off off";

    //  How LIST prints the program EnterProgram types.
    static constexpr const char *  kListingLine10 = "10  PRINT \"HELLO\"";
    static constexpr const char *  kListingLine20 = "20  END";

    //  Long enough for each key's echo to be sent at 9,600 baud.
    static constexpr uint64_t  kKeyGapCycles = 30'000;

    //  After a line: room for its output. With the screen off, the firmware
    //  pauses a quarter second after each carriage return of the listing.
    static constexpr uint64_t  kLineCycles           = 1'000'000;
    static constexpr uint64_t  kDelayedListingCycles = 3'000'000;

    //  The file's 30-odd characters at 300 baud, and the lines they enter.
    static constexpr uint64_t  kFileInCycles = 3'000'000;


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


    //  The text screen's rows, without their trailing blanks.
    static std::vector<std::string> ReadScreen (MachineHost & machine)
    {
        std::vector<std::string>  rows = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), TextScreenScraper::kTextPage1);

        for (std::string & row : rows)
        {
            row = GuestSession::TrimTrailingBlanks (row);
        }

        return rows;
    }


    static void EnterProgram (MachineHost & machine)
    {
        GuestSession::TypeAndCollect (machine, "10 PRINT \"HELLO\"");
        GuestSession::TypeAndCollect (machine, "20 END");
    }


    static void TypeLine (MachineHost & machine, const char * line, uint64_t settleCycles = kLineCycles)
    {
        for (const char * c = line; *c != '\0'; c++)
        {
            KeystrokeInjector::InjectKey (machine, static_cast<Byte> (*c));
            machine.RunCycles (kKeyGapCycles);
        }

        KeystrokeInjector::InjectKey (machine, KeystrokeInjector::kAppleReturn);
        machine.RunCycles (settleCycles);
    }


    //  The card in slot 2, turned on, with Apple's firmware, the given
    //  switches, and the given attachment on its connector.
    static void ConfigureCard (MachineConfig                & config,
                               const char                   * sw1,
                               const char                   * sw2,
                               const char                   * device,
                               const std::filesystem::path  & path)
    {
        std::filesystem::path  firmware = RequireFirmwarePath();
        PortConfig             port;

        for (SlotConfig & slot : config.slots)
        {
            if (slot.device == kpszSuperSerialCardDevice)
            {
                port.device = device;
                port.path   = path.string();

                slot.enabled        = true;
                slot.firmwarePath   = firmware.string();
                slot.firmwareStatus = SlotFirmwareStatus::Present;
                slot.switchesSw1    = sw1;
                slot.switchesSw2    = sw2;
                slot.ports          = { port };
            }
        }
    }


    static std::filesystem::path RequireFirmwarePath()
    {
        char *                 root = nullptr;
        size_t                 len  = 0;
        std::filesystem::path  path;
        std::error_code        ec;

        if (_dupenv_s (&root, &len, "LOCALAPPDATA") == 0 && root != nullptr)
        {
            path = std::filesystem::path (root) / "Casso" / "Devices" / "SuperSerialCard" / "SSC.rom";
            free (root);
        }

        Assert::AreEqual<uintmax_t> (2048, std::filesystem::file_size (path, ec),
            L"Apple's Super Serial Card firmware is REQUIRED by these cases and was not "
            L"found at %LOCALAPPDATA%\\Casso\\Devices\\SuperSerialCard\\SSC.rom. To get it, "
            L"turn the card on under Settings > Machine and accept the download");

        return path;
    }


    //  A file for one case under the temp folder, absent to start with.
    static std::filesystem::path MakeOutputPath (const wchar_t * name)
    {
        std::filesystem::path  dir = std::filesystem::temp_directory_path() / L"CassoScenario" / L"SuperSerialCard";
        std::filesystem::path  path = dir / name;
        std::error_code        ec;

        std::filesystem::create_directories (dir, ec);
        std::filesystem::remove (path, ec);

        return path;
    }


    static void WriteFile (const std::filesystem::path & path, const std::string & text)
    {
        std::ofstream  out (path, std::ios::binary);

        out << text;
    }


    //  What the card sent, as text: the high bit cleared, and each carriage
    //  return a line break.
    static std::string ReadSentText (const std::filesystem::path & path)
    {
        std::ifstream  in (path, std::ios::binary);
        std::string    text;
        char           c = 0;

        while (in.get (c))
        {
            Byte  value = static_cast<Byte> (c & 0x7F);

            text += (value == 0x0D) ? '\n' : static_cast<char> (value);
        }

        return text;
    }
};

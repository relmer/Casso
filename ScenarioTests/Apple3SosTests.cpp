#include "Pch.h"

#include "EmuTests/GuestSession.h"
#include "EmuTests/TestMachine.h"
#include "HResultAssert.h"
#include "Video/MachineFrameRenderer.h"
#include "Machines/Apple3/Apple3System.h"
#include "Machines/Apple3/Apple3TextCells.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3SosTests
//
//  The Apple /// booting real disks headless: the ROM's screen with an
//  empty drive, SOS and Business BASIC to their prompt on 128K and 256K,
//  and BASIC evaluating a typed expression. The disks are fetched by
//  scripts/FetchStockDisks.ps1; a missing disk or ROM fails the test.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3SosTests)
{
public:

    static constexpr uint64_t      kTicksPerSecond     = 2040968;
    static constexpr int           kSecondsToPrompt    = 40;
    static constexpr int           kDiskSlot           = 6;
    static constexpr int           kDrive1             = 0;
    static constexpr const char *  kpszBusinessBasic   = "Disks/Apple/apple3-business-basic.dsk";
    static constexpr size_t        kDiskBytes          = 143360;
    static constexpr size_t        kMixedFirstLine     = 160;


    //  The screen as text: 80 columns when the 80-column switch is set,
    //  40 otherwise, one line per row, with control codes as spaces.
    static std::string GetScreenText (Apple3System & system)
    {
        bool         is80    = system.GetVideo().Is80Columns();
        int          columns = is80 ? Apple3TextCells::kColumns * 2 : Apple3TextCells::kColumns;
        std::string  text;



        for (int row = 0; row < Apple3TextCells::kRows; row++)
        {
            for (int column = 0; column < columns; column++)
            {
                Word  page = (!is80 || column % 2 == 0) ? Apple3TextCells::kPage1 : Apple3TextCells::kPage2;
                Word  cell = (Word) (Apple3TextCells::GetRowAddress (row, page) + (is80 ? column / 2 : column));
                Byte  c    = (Byte) (system.GetMmu().PeekSystem (cell) & 0x7F);



                text += (c >= 0x20 && c < 0x7F) ? (char) c : ' ';
            }

            text += '\n';
        }

        return text;
    }


    //  Runs a second at a time until the screen contains the text.
    static bool RunUntilScreenHas (TestMachine & machine, const std::string & text, int seconds)
    {
        bool  isFound = false;



        for (int second = 0; second < seconds && !isFound; second++)
        {
            machine.GetRunner().RunCycles (kTicksPerSecond);
            isFound = GetScreenText (*machine.GetApple3()).find (text) != std::string::npos;
        }

        return isFound;
    }


    //  Types each character once the program has taken the one before.
    static void Type (TestMachine & machine, const std::string & text)
    {
        Apple3Keyboard  & keyboard = machine.GetApple3()->GetKeyboard();



        for (char c : text)
        {
            for (int wait = 0; wait < 100 && !keyboard.IsStrobeClear(); wait++)
            {
                machine.GetRunner().RunCycles (kTicksPerSecond / 100);
            }

            keyboard.PressKey ((Byte) c);
            machine.GetRunner().RunCycles (kTicksPerSecond / 50);
        }
    }


    //  Types a command and runs until Business BASIC's prompt is back on the
    //  last line that has text.
    static void RunCommand (TestMachine & machine, const std::string & command, int seconds = 20)
    {
        bool  isBack = false;



        Type (machine, command + "\r");

        for (int tenth = 0; tenth < seconds * 10 && !isBack; tenth++)
        {
            std::string  screen = GetScreenText (*machine.GetApple3());
            size_t       last   = screen.find_last_not_of (" \n");
            size_t       start  = (last == std::string::npos) ? 0 : screen.rfind ('\n', last);



            machine.GetRunner().RunCycles (kTicksPerSecond / 10);
            isBack = last != std::string::npos && screen.substr (start + 1, last - start) == ")";
        }
    }


    static void MountBusinessBasic (TestMachine & machine)
    {
        std::vector<Byte>  bytes = GuestSession::RequireRepoImage (kpszBusinessBasic);
        HRESULT            hr    = machine.GetDiskStore().MountFromBytes (kDiskSlot, kDrive1, "apple3-business-basic.dsk", DiskFormat::Dsk, bytes);



        AssertSucceeded (hr, L"the Business BASIC disk mounts");
        machine.GetStorage().ConnectDiskToController (kDiskSlot, kDrive1);
    }


    static void CheckBusinessBasicPrompt (uint32_t memoryK)
    {
        TestMachine  machine ("Apple3", [memoryK] (MachineConfig & config) { config.memoryK = memoryK; });



        MountBusinessBasic (machine);

        Assert::IsTrue (RunUntilScreenHas (machine, "Apple Business BASIC v1.23", kSecondsToPrompt),
                        L"SOS loads Business BASIC, and its banner is on the screen");
        Assert::IsTrue (RunUntilScreenHas (machine, "BLOCKS FREE", kSecondsToPrompt),
                        L"and the disk's startup program runs to its end");
    }


    TEST_METHOD (NoDisk_RetryIsOnTheScreen)
    {
        TestMachine  machine ("Apple3");



        Assert::IsTrue (RunUntilScreenHas (machine, "RETRY", kSecondsToPrompt));
    }


    TEST_METHOD (BusinessBasic_RunsTheStartupProgram_128K)
    {
        CheckBusinessBasicPrompt (128);
    }


    TEST_METHOD (BusinessBasic_RunsTheStartupProgram_256K)
    {
        CheckBusinessBasicPrompt (256);
    }


    //  A SOS error is on the screen whatever the memory size. The stock
    //  SOS.DRIVER files have no .D4, so CATALOG .D4 is an error.
    static void CheckErrorIsReported (uint32_t memoryK)
    {
        TestMachine  machine ("Apple3", [memoryK] (MachineConfig & config) { config.memoryK = memoryK; });



        MountBusinessBasic (machine);
        Assert::IsTrue (RunUntilScreenHas (machine, "BLOCKS FREE", kSecondsToPrompt));

        Type (machine, "CATALOG .D4\r");

        Assert::IsTrue (RunUntilScreenHas (machine, "?FILE NOT FOUND ERROR", 10), TextEncoding::Utf8ToWide (GetScreenText (*machine.GetApple3())).c_str());
    }


    TEST_METHOD (BusinessBasic_ReportsAnError_128K)
    {
        CheckErrorIsReported (128);
    }


    TEST_METHOD (BusinessBasic_ReportsAnError_256K)
    {
        CheckErrorIsReported (256);
    }


    //  SOS reads the date from the clock chip, which starts at the host's
    //  local time.
    TEST_METHOD (BusinessBasic_DateIsTheHostsDate)
    {
        TestMachine  machine ("Apple3");
        SYSTEMTIME   local = {};
        std::string  date;



        MountBusinessBasic (machine);
        Assert::IsTrue (RunUntilScreenHas (machine, "BLOCKS FREE", kSecondsToPrompt));

        GetLocalTime (&local);
        date = std::format ("\n{:02}/{:02}/{:02}", local.wYear % 100, local.wMonth, local.wDay);

        RunCommand (machine, "PRINT DATE$");
        Assert::IsTrue (GetScreenText (*machine.GetApple3()).find (date) != std::string::npos, TextEncoding::Utf8ToWide (GetScreenText (*machine.GetApple3())).c_str());
    }

    //  The Confidence Program's Machine Configuration screen: the memory
    //  size, the drive online, and every interrupt source checked.
    static void CheckMachineConfiguration (uint32_t memoryK)
    {
        TestMachine        machine ("Apple3", [memoryK] (MachineConfig & config) { config.memoryK = memoryK; });
        std::vector<Byte>  bytes   = GuestSession::RequireRepoImage ("Disks/Apple/apple3-confidence-test.dsk");
        std::string        screen;



        MountBytes (machine, 0, "apple3-confidence-test.dsk", bytes);

        Assert::IsTrue (RunUntilScreenHas (machine, "RETURN to accept", kSecondsToPrompt), L"the menu is on the screen");
        Type (machine, "\r");
        Assert::IsTrue (RunUntilScreenHas (machine, "ESCAPE to exit", kSecondsToPrompt), L"the configuration checks are done");

        screen = GetScreenText (*machine.GetApple3());

        Assert::IsTrue (screen.find (std::format ("Memory Map good for: {}K", memoryK)) != std::string::npos, TextEncoding::Utf8ToWide (screen).c_str());
        Assert::IsTrue (screen.find ("Online          [*]") != std::string::npos, L".D1 is online");
        Assert::IsTrue (screen.find ("D.6522      [*]     [*]     [*]     [*]     [*]     [*]") != std::string::npos, L"D VIA interrupts");
        Assert::IsTrue (screen.find ("E.6522      [*]     [*]     [*]     [*]     [*]     [*]") != std::string::npos, L"E VIA interrupts");
        Assert::IsTrue (screen.find ("IRQ  [*]        I/O NMI  [*]           ACIA  [*]") != std::string::npos, L"IRQ, NMI and ACIA");
    }


    TEST_METHOD (ConfidenceProgram_MachineConfiguration_128K)
    {
        CheckMachineConfiguration (128);
    }


    TEST_METHOD (ConfidenceProgram_MachineConfiguration_256K)
    {
        CheckMachineConfiguration (256);
    }

    //  The Confidence Program's Memory Test on 128K: three passes with no
    //  mark in the rows of banks 0 to 2.
    TEST_METHOD (ConfidenceProgram_MemoryTestPasses)
    {
        TestMachine        machine ("Apple3");
        std::vector<Byte>  bytes   = GuestSession::RequireRepoImage ("Disks/Apple/apple3-confidence-test.dsk");
        std::string        screen;



        MountBytes (machine, 0, "apple3-confidence-test.dsk", bytes);
        Assert::IsTrue (RunUntilScreenHas (machine, "RETURN to accept", kSecondsToPrompt));

        machine.GetApple3()->GetKeyboard().PressSpecialKey (AppleSpecialKey::Down);
        machine.GetRunner().RunCycles (kTicksPerSecond);
        Type (machine, "\r");

        Assert::IsTrue (RunUntilScreenHas (machine, "Pass:      3", 60), L"the test runs three passes");

        screen = GetScreenText (*machine.GetApple3());

        for (const char * row : { "\n 64   0", "\n 96   1", "\n128   2" })
        {
            size_t       at   = screen.find (row);
            std::string  rest = (at != std::string::npos) ? screen.substr (at + strlen (row), Apple3TextCells::kColumns * 2 - strlen (row) + 1) : std::string ("missing");



            Assert::IsTrue (rest.find_first_not_of (' ') == std::string::npos, TextEncoding::Utf8ToWide (screen).c_str());
        }
    }

    TEST_METHOD (BusinessBasic_PrintsSixTimesSeven)
    {
        TestMachine  machine ("Apple3");



        MountBusinessBasic (machine);
        Assert::IsTrue (RunUntilScreenHas (machine, "BLOCKS FREE", kSecondsToPrompt));

        Type (machine, "PRINT 6*7\r");

        Assert::IsTrue (RunUntilScreenHas (machine, "\n42 ", 2), L"BASIC prints 42 on the next line");
    }


    //  A line written to .RS232 comes back through the loopback plug on the
    //  serial port.
    TEST_METHOD (Rs232_ALineComesBackThroughTheLoopbackPlug)
    {
        TestMachine  machine ("Apple3");
        std::string  screen;



        MountBusinessBasic (machine);
        Assert::IsTrue (RunUntilScreenHas (machine, "BLOCKS FREE", kSecondsToPrompt));

        RunCommand (machine, "10 OPEN#1,\".RS232\"");
        RunCommand (machine, "20 PRINT#1;\"CASSO\"");
        RunCommand (machine, "30 INPUT#1;A$");
        RunCommand (machine, "40 PRINT \"READ \";A$");
        RunCommand (machine, "50 CLOSE");
        RunCommand (machine, "RUN");

        screen = GetScreenText (*machine.GetApple3());
        Assert::IsTrue (screen.find ("\nREAD CASSO") != std::string::npos, TextEncoding::Utf8ToWide (screen).c_str());
    }


    //  A blank SOS volume, which has the ProDOS volume format.
    static std::vector<Byte> MakeBlankVolume (const std::string & name)
    {
        std::vector<Byte>  bytes (kDiskBytes);



        AssertSucceeded (ProDosSkeleton::Write (bytes, name), L"the blank volume is built");
        return bytes;
    }


    static void MountBytes (TestMachine & machine, int drive, const std::string & name, const std::vector<Byte> & bytes)
    {
        HRESULT  hr = machine.GetDiskStore().MountFromBytes (kDiskSlot, drive, name, DiskFormat::Dsk, bytes);



        AssertSucceeded (hr, L"the disk mounts");
        machine.GetStorage().ConnectDiskToController (kDiskSlot, drive);
    }


    //  Business BASIC in .D1, and blank volumes TWO, THREE and FOUR in .D2 to
    //  .D4. The SOS.DRIVER files on the stock disks define .D1 to .D3, so
    //  SOS accesses .D4 only with a driver configured for it.
    struct FourDriveMachine
    {
        TestMachine  machine;

        explicit FourDriveMachine (uint32_t memoryK = 128)
            : machine ("Apple3", [memoryK] (MachineConfig & config) { config.externalDrives = 3; config.memoryK = memoryK; })
        {
            MountBusinessBasic (machine);
            MountBytes (machine, 1, "two.dsk",   MakeBlankVolume ("TWO"));
            MountBytes (machine, 2, "three.dsk", MakeBlankVolume ("THREE"));
            MountBytes (machine, 3, "four.dsk",  MakeBlankVolume ("FOUR"));

            Assert::IsTrue (RunUntilScreenHas (machine, "BLOCKS FREE", kSecondsToPrompt), L"Business BASIC is at its prompt");
        }

        //  The disk in a drive as the 140K sector image.
        std::vector<Byte> GetDiskBytes (int drive)
        {
            std::vector<Byte>  bytes;



            AssertSucceeded (NibblizationLayer::Denibblize (*machine.GetDiskStore().GetImage (kDiskSlot, drive), DiskFormat::Dsk, bytes));
            return bytes;
        }

        std::wstring GetScreen() { return TextEncoding::Utf8ToWide (GetScreenText (*machine.GetApple3())); }
    };


    TEST_METHOD (FourDrives_EachVolumeIsListed)
    {
        FourDriveMachine  rig;



        Type (rig.machine, "CATALOG .D2\r");
        Assert::IsTrue (RunUntilScreenHas (rig.machine, "TWO ", 10), rig.GetScreen().c_str());

        Type (rig.machine, "CATALOG .D3\r");
        Assert::IsTrue (RunUntilScreenHas (rig.machine, "THREE ", 10), rig.GetScreen().c_str());
    }


    //  A program saved to .D2 and to .D3 is on each disk, byte for byte the
    //  same, when Casso's disk tools read the disks.
    TEST_METHOD (FourDrives_ASavedProgramReadsBackFromEachDisk)
    {
        FourDriveMachine   rig;
        std::vector<Byte>  two;
        std::vector<Byte>  three;
        Byte               fileType = 0;
        Word               auxType  = 0;



        RunCommand (rig.machine, "10 PRINT \"FOUR DRIVES\"");
        RunCommand (rig.machine, "SAVE .D2/HELLO");
        RunCommand (rig.machine, "SAVE .D3/HELLO");
        RunCommand (rig.machine, "CATALOG .D3");
        Assert::IsTrue (GetScreenText (*rig.machine.GetApple3()).find ("HELLO") != std::string::npos, rig.GetScreen().c_str());

        AssertSucceeded (ProDosReader::ExtractFile (rig.GetDiskBytes (1), "HELLO", two,   fileType, auxType), L"HELLO is on .D2");
        AssertSucceeded (ProDosReader::ExtractFile (rig.GetDiskBytes (2), "HELLO", three, fileType, auxType), L"HELLO is on .D3");

        Assert::IsFalse (two.empty());
        Assert::IsTrue  (two == three, L"the two copies are the same bytes");
    }


    TEST_METHOD (FourDrives_AWriteProtectedDiskTakesNoSave)
    {
        FourDriveMachine  rig;



        rig.machine.GetDiskStore().GetImage (kDiskSlot, 2)->SetWriteProtected (true);

        RunCommand (rig.machine, "10 PRINT 1");
        RunCommand (rig.machine, "SAVE .D3/HELLO");

        Assert::IsTrue (GetScreenText (*rig.machine.GetApple3()).find ("?WRITE PROTECT ERROR") != std::string::npos, rig.GetScreen().c_str());
    }


    //  With one external drive plugged in, .D3 has no drive behind it, so no
    //  volume is found there.
    TEST_METHOD (OneExternalDrive_D3IsNotThere)
    {
        TestMachine  machine ("Apple3", [] (MachineConfig & config) { config.externalDrives = 1; });



        MountBusinessBasic (machine);
        Assert::IsTrue (RunUntilScreenHas (machine, "BLOCKS FREE", kSecondsToPrompt));

        RunCommand (machine, "CATALOG .D3");
        Assert::IsTrue (GetScreenText (*machine.GetApple3()).find ("?VOLUME NOT FOUND ERROR") != std::string::npos, TextEncoding::Utf8ToWide (GetScreenText (*machine.GetApple3())).c_str());
    }


    //  Types the commands into both machines, then compares the top 20 text
    //  rows' worth of the two frames, the area that is graphics on a mixed
    //  screen. The text rows differ, since the Apple ///'s text comes from
    //  its character generator.
    static void CheckGraphicsMatch (TestMachine & apple3, TestMachine & apple2, const std::vector<std::string> & commands, const wchar_t * what)
    {
        std::vector<uint32_t>  apple3Frame;
        std::vector<uint32_t>  apple2Frame;
        size_t                 graphicsPixels = (size_t) MachineFrameRenderer::kWidth * kMixedFirstLine * 2;
        size_t                 lit            = 0;
        HRESULT                hr             = S_OK;



        for (const std::string & command : commands)
        {
            Type (apple3, command + "\r");
            apple3.GetRunner().RunCycles (kTicksPerSecond);
            GuestSession::TypeAndCollect (apple2, command);
        }

        hr = MachineFrameRenderer::Render (apple3, apple3.GetBuilder(), true, apple3Frame);
        AssertSucceeded (hr, what);
        hr = MachineFrameRenderer::Render (apple2, apple2.GetBuilder(), true, apple2Frame);
        AssertSucceeded (hr, what);

        for (size_t i = 0; i < graphicsPixels; i++)
        {
            Assert::AreEqual (apple2Frame[i], apple3Frame[i], std::format (L"{}: pixel {}, {}", what, i % MachineFrameRenderer::kWidth, i / MachineFrameRenderer::kWidth).c_str());
            lit += (apple3Frame[i] & 0x00FFFFFF) != 0 ? 1 : 0;
        }

        Assert::IsTrue (lit > 0, what);
    }


    //  The Apple II Emulation disk's menu, then Return: the machine is locked
    //  into emulation, with INSERT APPLE II DISK on the screen. The DOS 3.3
    //  System Master boots to Applesoft, and lo-res and hi-res drawings match
    //  a ][+ given the same commands.
    TEST_METHOD (Emulation_BootsDos33_AndDrawsAsAnApple2Plus)
    {
        TestMachine        apple3 ("Apple3");
        TestMachine        apple2 ("Apple2Plus");
        std::vector<Byte>  emulation = GuestSession::RequireRepoImage ("Disks/Apple/apple3-apple2-emulation.dsk");
        std::vector<Byte>  master    = GuestSession::RequireDos33Master();



        MountBytes (apple3, kDrive1, "apple3-apple2-emulation.dsk", emulation);
        Assert::IsTrue (RunUntilScreenHas (apple3, "EMULATION MODE", kSecondsToPrompt), L"the emulation disk's menu");
        Assert::IsFalse (apple3.GetApple3()->GetMmu().IsLockedOut(), L"the menu runs before the lockout");

        apple3.GetApple3()->GetKeyboard().PressKey ('\r');
        Assert::IsTrue (RunUntilScreenHas (apple3, "INSERT APPLE II DISK", kSecondsToPrompt), L"INSERT APPLE II DISK is on the screen");
        Assert::IsTrue (apple3.GetApple3()->GetMmu().IsLockedOut(), L"and the machine is locked into emulation");

        MountBytes (apple3, kDrive1, "master.dsk", master);
        Assert::IsTrue (RunUntilScreenHas (apple3, "SYSTEM MASTER", kSecondsToPrompt), L"the System Master boots");
        apple3.GetRunner().RunCycles (kTicksPerSecond * 2);

        RunCommand (apple3, "PRINT 6*7");
        Assert::IsTrue (GetScreenText (*apple3.GetApple3()).find ("\n42 ") != std::string::npos, TextEncoding::Utf8ToWide (GetScreenText (*apple3.GetApple3())).c_str());

        GuestSession::BootToPrompt (apple2, master);

        CheckGraphicsMatch (apple3, apple2, { "GR", "COLOR=13", "PLOT 10,10", "HLIN 0,39 AT 20", "COLOR=6", "VLIN 0,39 AT 30" }, L"lo-res");
        CheckGraphicsMatch (apple3, apple2, { "HGR", "HCOLOR=3", "HPLOT 0,0 TO 279,159", "HCOLOR=5", "HPLOT 0,159 TO 279,0" }, L"hi-res");
    }
};
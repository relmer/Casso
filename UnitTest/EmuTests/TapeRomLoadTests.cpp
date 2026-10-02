#include "Pch.h"

#include "Devices/Tape/TapeImageLoader.h"
#include "KeystrokeInjector.h"
#include "TapeTestEncoder.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRomLoadTests
//
//  Synthetic tapes of known bytes, loaded through each model's own ROM: the
//  Monitor's READ for "addr.addrR", and BASIC LOAD. Casso supplies only the
//  signal on $C060, so a byte-for-byte match proves the ROM decoded the tape
//  the way it would on hardware.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TapeRomLoadTests)
{
public:

    static constexpr uint64_t  kColdBootCycles = 5'000'000ULL;
    static constexpr uint64_t  kAfterCommand   = 2'000'000ULL;
    static constexpr uint64_t  kTailCycles     = 2'000'000ULL;
    static constexpr Word      kLoadStart      = 0x0800;
    static constexpr size_t    kPatternSize    = 256;


    static void MakePattern (std::vector<Byte> & pattern)
    {
        constexpr Byte  kStep   = 37;
        constexpr Byte  kOffset = 11;



        pattern.resize (kPatternSize);

        for (size_t i = 0; i < kPatternSize; i++)
        {
            pattern[i] = (Byte) (i * kStep + kOffset);
        }
    }


    static std::wstring ToWide (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }


    static std::string GetScreen (TestMachine & machine)
    {
        std::string  screen;



        for (const std::string & line : TextScreenScraper::Scrape (machine))
        {
            screen += line;
            screen += '\n';
        }

        return screen;
    }


    static void Boot (TestMachine & machine)
    {
        machine.PowerCycle();
        machine.RunCycles (kColdBootCycles);
    }


    static void EnterMonitor (TestMachine & machine)
    {
        std::vector<std::string>  lines       = TextScreenScraper::Scrape (machine);
        bool                      isAtMonitor = false;



        for (const std::string & line : lines)
        {
            if (!line.empty() && line[0] == '*')
            {
                isAtMonitor = true;
            }
        }

        if (!isAtMonitor)
        {
            KeystrokeInjector::InjectLine (machine, "CALL -151", kAfterCommand);
        }
    }


    static void InsertTape (TestMachine & machine, const std::vector<Byte> & wav)
    {
        NullTapeAudioDecoder  mp3;
        TapeImage             image;
        std::string           error;
        HRESULT               hr = TapeImageLoader::Load (wav, "test.wav", false, mp3, image, error);



        Assert::AreEqual (S_OK, hr, ToWide (error).c_str());
        machine.GetTapeDeck().Insert (std::move (image));
    }


    static uint64_t GetNow (TestMachine & machine)
    {
        return *machine.GetCpu()->GetBusCyclePtr();
    }


    static void PlayToEnd (TestMachine & machine)
    {
        TapeDeck         & deck    = machine.GetTapeDeck();
        const TapeImage  * image   = deck.GetImage();
        double             seconds = 0.0;



        Assert::IsNotNull (image);
        seconds = (double) image->signal.lengthSamples / image->signal.sampleRate;

        deck.Play (GetNow (machine));
        machine.RunCycles ((uint64_t) (seconds * machine.GetConfig().clockSpeed) + kTailCycles);
        deck.Update (GetNow (machine));
    }


    static void CheckMemory (TestMachine & machine, const std::vector<Byte> & expected, const wchar_t * pszContext)
    {
        int  mismatches = 0;



        for (size_t i = 0; i < expected.size(); i++)
        {
            if (machine.GetMemoryBus().ReadByte ((Word) (kLoadStart + i)) != expected[i])
            {
                mismatches++;
            }
        }

        Assert::AreEqual (0, mismatches, pszContext);
    }


    static void LoadThroughMonitor (const char * pszModel, const TapeEncodeOptions & options, const wchar_t * pszContext)
    {
        TestMachine        machine (pszModel, TestMachine::Slots::Empty);
        std::vector<Byte>  pattern;
        std::vector<Byte>  wav;
        std::string        screen;



        MakePattern (pattern);
        TapeTestEncoder::MakeWav ({ pattern }, options, wav);

        Boot (machine);
        EnterMonitor (machine);
        InsertTape (machine, wav);

        KeystrokeInjector::InjectLine (machine, "800.8FFR", kAfterCommand);
        PlayToEnd (machine);

        screen = GetScreen (machine);
        Assert::IsTrue (screen.find ("ERR") == std::string::npos, (std::wstring (pszContext) + L"\n" + ToWide (screen)).c_str());
        CheckMemory (machine, pattern, (std::wstring (pszContext) + L"\n" + ToWide (screen)).c_str());
    }


    TEST_METHOD (MonitorReadLoadsOnTheII)
    {
        TapeEncodeOptions  options;



        LoadThroughMonitor ("Apple2", options, L"Apple2");
    }


    TEST_METHOD (MonitorReadLoadsOnTheIIPlus)
    {
        TapeEncodeOptions  options;



        LoadThroughMonitor ("Apple2Plus", options, L"Apple2Plus");
    }


    TEST_METHOD (MonitorReadLoadsOnTheIIe)
    {
        TapeEncodeOptions  options;



        LoadThroughMonitor ("Apple2e", options, L"Apple2e");
    }


    TEST_METHOD (MonitorReadLoadsOnTheEnhancedIIe)
    {
        TapeEncodeOptions  options;



        LoadThroughMonitor ("Apple2eEnhanced", options, L"Apple2eEnhanced");
    }

    static Word ReadWord (TestMachine & machine, Word address)
    {
        return (Word) (machine.GetMemoryBus().ReadByte (address) | (machine.GetMemoryBus().ReadByte ((Word) (address + 1)) << 8));
    }


    static void ReadRange (TestMachine & machine, Word first, size_t count, std::vector<Byte> & bytes)
    {
        bytes.resize (count);

        for (size_t i = 0; i < count; i++)
        {
            bytes[i] = machine.GetMemoryBus().ReadByte ((Word) (first + i));
        }
    }


    //  Types the program into a fresh machine and lifts the two records an
    //  Applesoft SAVE writes: a 3-byte header holding the program length, then
    //  the program from TXTTAB through PRGEND inclusive.
    static void MakeApplesoftRecords (const char * pszModel, std::vector<std::vector<Byte>> & records)
    {
        constexpr Word     kTxtTab  = 0x67;
        constexpr Word     kPrgEnd  = 0xAF;
        TestMachine        machine (pszModel, TestMachine::Slots::Empty);
        Word               start    = 0;
        Word               end      = 0;
        Word               length   = 0;
        std::vector<Byte>  body;



        Boot (machine);
        KeystrokeInjector::InjectLine (machine, "10 PRINT 12345", kAfterCommand);
        KeystrokeInjector::InjectLine (machine, "20 END",         kAfterCommand);

        start  = ReadWord (machine, kTxtTab);
        end    = ReadWord (machine, kPrgEnd);
        length = (Word) (end - start);
        Assert::IsTrue (length > 2 && length < 0x100, (std::format (L"start {:04X} end {:04X}\n", start, end) + ToWide (GetScreen (machine))).c_str());
        ReadRange (machine, start, (size_t) length + 1, body);

        records = { { (Byte) length, (Byte) (length >> 8), 0 }, body };
    }


    static void LoadApplesoft (const char * pszModel)
    {
        std::vector<std::vector<Byte>>  records;
        std::vector<Byte>               wav;
        TapeEncodeOptions               options;
        TestMachine                     machine (pszModel, TestMachine::Slots::Empty);
        std::string                     screen;



        MakeApplesoftRecords (pszModel, records);
        TapeTestEncoder::MakeWav (records, options, wav);

        Boot (machine);
        InsertTape (machine, wav);
        KeystrokeInjector::InjectLine (machine, "LOAD", kAfterCommand);
        PlayToEnd (machine);
        KeystrokeInjector::InjectLine (machine, "LIST", kAfterCommand);

        screen = GetScreen (machine);
        Assert::IsTrue (screen.find ("ERR") == std::string::npos,              ToWide (screen).c_str());
        Assert::IsTrue (screen.find ("10  PRINT 12345") != std::string::npos,  ToWide (screen).c_str());
        Assert::IsTrue (screen.find ("20  END") != std::string::npos,          ToWide (screen).c_str());
    }


    TEST_METHOD (ApplesoftLoadOnTheIIPlus)
    {
        LoadApplesoft ("Apple2Plus");
    }


    TEST_METHOD (ApplesoftLoadOnTheIIe)
    {
        LoadApplesoft ("Apple2e");
    }


    //  Integer BASIC on the ][: a 2-byte length (HIMEM - PP), then the program
    //  from PP up to HIMEM.
    TEST_METHOD (IntegerBasicLoadOnTheII)
    {
        constexpr Word                  kPp      = 0xCA;
        constexpr Word                  kHimem   = 0x4C;
        constexpr Byte                  kCtrlB   = 0x02;
        TestMachine                     source ("Apple2", TestMachine::Slots::Empty);
        TestMachine                     target ("Apple2", TestMachine::Slots::Empty);
        std::vector<std::vector<Byte>>  records;
        std::vector<Byte>               body;
        std::vector<Byte>               wav;
        TapeEncodeOptions               options;
        Word                            start    = 0;
        Word                            himem    = 0;
        Word                            length   = 0;
        std::string                     screen;



        Boot (source);
        EnterMonitor (source);
        KeystrokeInjector::InjectKey  (source, kCtrlB);
        KeystrokeInjector::InjectLine (source, "", kAfterCommand);
        KeystrokeInjector::InjectLine (source, "10 PRINT 12345", kAfterCommand);
        KeystrokeInjector::InjectLine (source, "20 END",         kAfterCommand);

        start  = ReadWord (source, kPp);
        himem  = ReadWord (source, kHimem);
        length = (Word) (himem - start);
        Assert::IsTrue (length > 2 && length < 0x100, (std::format (L"pp {:04X} himem {:04X}\n", start, himem) + ToWide (GetScreen (source))).c_str());
        ReadRange (source, start, length, body);
        records = { { (Byte) length, (Byte) (length >> 8) }, body };
        TapeTestEncoder::MakeWav (records, options, wav);

        Boot (target);
        EnterMonitor (target);
        KeystrokeInjector::InjectKey  (target, kCtrlB);
        KeystrokeInjector::InjectLine (target, "", kAfterCommand);
        InsertTape (target, wav);
        KeystrokeInjector::InjectLine (target, "LOAD", kAfterCommand);
        PlayToEnd (target);
        KeystrokeInjector::InjectLine (target, "LIST", kAfterCommand);

        screen = GetScreen (target);
        Assert::IsTrue (screen.find ("ERR") == std::string::npos,          ToWide (screen).c_str());
        Assert::IsTrue (screen.find ("PRINT 12345") != std::string::npos,  ToWide (screen).c_str());
    }


    //  Robustness, on the ][+ through the Monitor. Drift of 3% must load.
    TEST_METHOD (LoadsThreePercentFast)  { TapeEncodeOptions o; o.speed = 1.03; LoadThroughMonitor ("Apple2Plus", o, L"+3%"); }
    TEST_METHOD (LoadsThreePercentSlow)  { TapeEncodeOptions o; o.speed = 0.97; LoadThroughMonitor ("Apple2Plus", o, L"-3%"); }
    TEST_METHOD (LoadsFivePercentFast)   { TapeEncodeOptions o; o.speed = 1.05; LoadThroughMonitor ("Apple2Plus", o, L"+5%"); }
    TEST_METHOD (LoadsFivePercentSlow)   { TapeEncodeOptions o; o.speed = 0.95; LoadThroughMonitor ("Apple2Plus", o, L"-5%"); }
    TEST_METHOD (LoadsWithDcOffset)      { TapeEncodeOptions o; o.amplitude = 0.4; o.dcOffset = 0.35; LoadThroughMonitor ("Apple2Plus", o, L"DC offset"); }
    TEST_METHOD (LoadsWithRisingLevel)   { TapeEncodeOptions o; o.amplitude = 0.05; o.gainEnd = 16.0; LoadThroughMonitor ("Apple2Plus", o, L"gain ramp"); }
    TEST_METHOD (LoadsWithNoise)         { TapeEncodeOptions o; o.noiseSnrDb = 15.0; o.tailSeconds = 0.0; LoadThroughMonitor ("Apple2Plus", o, L"noise"); }
    TEST_METHOD (LoadsInvertedPolarity)  { TapeEncodeOptions o; o.isInverted = true; LoadThroughMonitor ("Apple2Plus", o, L"inverted"); }
    TEST_METHOD (LoadsAt8kHz)            { TapeEncodeOptions o; o.sampleRate = 8000;  LoadThroughMonitor ("Apple2Plus", o, L"8 kHz"); }
    TEST_METHOD (LoadsAt96kHz)           { TapeEncodeOptions o; o.sampleRate = 96000; LoadThroughMonitor ("Apple2Plus", o, L"96 kHz"); }
    TEST_METHOD (LoadsFrom8Bit)          { TapeEncodeOptions o; o.bitsPerSample = 8;  LoadThroughMonitor ("Apple2Plus", o, L"8-bit"); }
    TEST_METHOD (LoadsFrom24Bit)         { TapeEncodeOptions o; o.bitsPerSample = 24; LoadThroughMonitor ("Apple2Plus", o, L"24-bit"); }
    TEST_METHOD (LoadsFromFloat)         { TapeEncodeOptions o; o.bitsPerSample = 32; o.isFloat = true; LoadThroughMonitor ("Apple2Plus", o, L"float"); }
    TEST_METHOD (LoadsFromStereo)        { TapeEncodeOptions o; o.channels = 2; LoadThroughMonitor ("Apple2Plus", o, L"stereo"); }
    TEST_METHOD (LoadsSquareWave)        { TapeEncodeOptions o; o.isSquare = true; LoadThroughMonitor ("Apple2Plus", o, L"square"); }


    //  A tape cut short mid-load: what hardware gives, a stall or a checksum
    //  error, and the emulator carries on.
    TEST_METHOD (TapeEndingMidLoadDoesNotCrash)
    {
        TestMachine        machine ("Apple2Plus", TestMachine::Slots::Empty);
        std::vector<Byte>  pattern;
        std::vector<Byte>  wav;
        std::vector<Byte>  cut;
        TapeEncodeOptions  options;



        MakePattern (pattern);
        TapeTestEncoder::MakeWav ({ pattern }, options, wav);
        cut.assign (wav.begin(), wav.begin() + (ptrdiff_t) (wav.size() * 9 / 10));

        Boot (machine);
        EnterMonitor (machine);
        InsertTape (machine, cut);
        KeystrokeInjector::InjectLine (machine, "800.8FFR", kAfterCommand);
        PlayToEnd (machine);

        Assert::IsTrue (machine.GetTapeDeck().GetTransport() == TapeTransport::Stopped);
    }


    //  Two programs on one tape with silence between: the second loads after
    //  the first by playing on.
    TEST_METHOD (SecondProgramAfterSilenceLoads)
    {
        TestMachine          machine ("Apple2Plus", TestMachine::Slots::Empty);
        std::vector<Byte>    first   (kPatternSize, 0x11);
        std::vector<Byte>    second;
        std::vector<double>  halves;
        std::vector<Byte>    wav;
        TapeAudio            audio;
        TapeEncodeOptions    options;



        MakePattern (second);
        TapeTestEncoder::AppendRecord (halves, first,  options);
        halves.push_back (2000000.0);      // two seconds of nothing between the programs
        TapeTestEncoder::AppendRecord (halves, second, options);
        TapeTestEncoder::Render (halves, options, audio);
        TapeTestEncoder::EncodeWav (audio, options, wav);

        Boot (machine);
        EnterMonitor (machine);
        InsertTape (machine, wav);
        KeystrokeInjector::InjectLine (machine, "800.8FFR 800.8FFR", kAfterCommand);
        PlayToEnd (machine);

        CheckMemory (machine, second, L"second program");
    }

    TEST_METHOD (ResetAndPowerCycleStopTheTapeAndKeepItInserted)
    {
        TestMachine        machine ("Apple2e", TestMachine::Slots::Empty);
        std::vector<Byte>  pattern;
        std::vector<Byte>  wav;
        TapeEncodeOptions  options;



        MakePattern (pattern);
        TapeTestEncoder::MakeWav ({ pattern }, options, wav);
        Boot (machine);
        InsertTape (machine, wav);

        machine.GetTapeDeck().Play (GetNow (machine));
        machine.SoftReset();
        Assert::IsTrue    (machine.GetTapeDeck().GetTransport() == TapeTransport::Stopped);
        Assert::IsNotNull (machine.GetTapeDeck().GetImage());

        machine.GetTapeDeck().Play (GetNow (machine));
        machine.PowerCycle();
        Assert::IsTrue    (machine.GetTapeDeck().GetTransport() == TapeTransport::Stopped);
        Assert::IsNotNull (machine.GetTapeDeck().GetImage());
    }
};

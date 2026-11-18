#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/EmulatedCassetteAudio.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/Audio/CassetteByteStream.h"
#include "CassoLink/Audio/DiskAudioRecording.h"
#include "CassoLink/Audio/DualCassetteByteStream.h"
#include "CassoLink/Audio/WasapiCassetteAudio.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CassoLinkThirteenSector.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "CassoLink/CassoLinkSession.h"
#include "FakeDiskFileIo.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Tape/ITapeAudioDecoder.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Devices/Tape/WavCodec.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Seams/IByteStreamFactory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCassetteScenarioTests
//
//  The link over the cassette ports of an emulated //e and ][+, in Casso's
//  audio encoding: the cassette build of the program, read from tape by the
//  monitor or placed where the monitor's read puts it, and the host on the
//  test's thread over a CassetteByteStream wired to the machine's cassette
//  port.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkCassetteScenarioTests)
{
public:

    //  A factory whose cassette link is the machine's cassette port, over the
    //  stream a cassette connection opens; it has no other line.
    class CassetteFactory : public IByteStreamFactory
    {
    public:
        explicit CassetteFactory (TestMachine & machine) : m_machine (machine) {}

        HRESULT  ListComPorts (std::vector<std::wstring> & outPorts) override                                                     { outPorts.clear(); return S_OK; }
        HRESULT  OpenComPort  (const std::wstring &, const ComSettings &, std::unique_ptr<IByteStream> &) override               { return E_NOTIMPL; }
        HRESULT  ListenTcp    (uint16_t, LinkTransport, std::unique_ptr<IByteStream> &) override                                 { return E_NOTIMPL; }
        HRESULT  ConnectTcp   (const std::string &, uint16_t, std::unique_ptr<IByteStream> &) override                           { return E_NOTIMPL; }

        HRESULT  OpenCassette (const std::wstring &, const std::wstring &, std::unique_ptr<IByteStream> & outStream) override
        {
            m_audio   = std::make_shared<EmulatedCassetteAudio> (m_machine);
            outStream = std::make_unique<DualCassetteByteStream> (m_audio);
            return S_OK;
        }

    private:
        TestMachine                             & m_machine;
        std::shared_ptr<EmulatedCassetteAudio>    m_audio;
    };

    struct SessionSink : ICassoLinkSessionSink
    {
        std::mutex                                    lock;
        std::condition_variable                       changed;
        std::vector<std::shared_ptr<LinkOperation>>   done;
        std::vector<LinkState>                        states;

        void  OnStateChanged (LinkState state) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            states.push_back (state);
            changed.notify_all();
        }

        void  OnProgress (const std::shared_ptr<LinkOperation> &) override
        {
        }

        void  OnDone (const std::shared_ptr<LinkOperation> & operation) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            done.push_back (operation);
            changed.notify_all();
        }

        bool  WaitForDone (const std::shared_ptr<LinkOperation> & operation, DWORD timeoutMs)
        {
            std::unique_lock<std::mutex>  guard (lock);

            return changed.wait_for (guard, std::chrono::milliseconds (timeoutMs), [&] () { return std::find (done.begin(), done.end(), operation) != done.end(); });
        }

        bool  WaitForState (LinkState state, DWORD timeoutMs)
        {
            std::unique_lock<std::mutex>  guard (lock);

            return changed.wait_for (guard, std::chrono::milliseconds (timeoutMs), [&] () { return !states.empty() && states.back() == state; });
        }
    };

    static CassoLinkSettings  MakeCassetteSettings()
    {
        CassoLinkSettings  settings;



        settings.transport = LinkTransport::Cassette;
        settings.name      = CassoLinkSettings::MakeName (settings);
        return settings;
    }

    static constexpr uint64_t  kClockHz   = 1'020'484;
    static constexpr int       kDiskSlot  = 6;
    static constexpr int       kSectors   = 16;

    enum class Start
    {
        Placed,
        FromTape,
        AtPrompt,
    };

    struct Rig
    {
        TestMachine                             machine;
        std::shared_ptr<EmulatedCassetteAudio>  audio;
        std::shared_ptr<CassetteByteStream>     stream;
        InMemoryCassoLinkFolder                 folder;
        std::unique_ptr<CassoLinkHost>          host;
        std::vector<Byte>                       disk;

        explicit Rig (const char * model, EmulatedCassetteAudio::Faults faults = EmulatedCassetteAudio::Faults(), uint32_t rate = 44100, Start start = Start::Placed) :
            machine (model)
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;



            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            disk.assign (bytes.begin(), bytes.end());

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

            //  At the BASIC prompt, then the disk in drive 1.
            machine.PowerCycle();
            machine.RunCycles (2 * kClockHz);
            machine.GetCpu()->SetPC (0xE000);
            machine.RunCycles (2 * kClockHz);
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (kDiskSlot, 0, "", DiskFormat::Dsk, disk));

            if (start == Start::FromTape)
            {
                LoadFromTape();
            }
            else if (start == Start::Placed)
            {
                Place();
            }

            audio  = std::make_shared<EmulatedCassetteAudio> (machine, rate, faults);
            stream = std::make_shared<CassetteByteStream> (audio);
            host   = std::make_unique<CassoLinkHost> (stream, folder, [] () { return std::array<Byte, 4> {}; });
            host->SetProgramChip (LinkChip::Cassette);
        }

        //  The program where the monitor's tape read puts it, and run.
        void  Place()
        {
            std::vector<Byte>  program;
            std::string        errors;
            Word               at = LinkProgram::kProgramAddress;



            AssertSucceeded (LinkProgram::AssembleProgram (program, errors, LinkChip::Cassette));

            for (Byte b : program)
            {
                machine.GetCpu()->WriteByte (at++, b);
            }

            machine.GetCpu()->SetPC (LinkProgram::kProgramAddress);
        }

        //  The monitor entered, the read command typed, and the tape's audio
        //  fed into the cassette input.
        void  LoadFromTape()
        {
            TapeAudio             audio;
            std::vector<Byte>     wav;
            std::string           command;
            NullTapeAudioDecoder  mp3;
            TapeImage             image;
            std::string           error;
            double                seconds = 0.0;



            AssertSucceeded (LinkAudioBootstrap::BuildCassetteLinkAudio (audio, command));
            WavCodec::Encode (audio, wav);
            Assert::AreEqual (S_OK, TapeImageLoader::Load (wav, "cassette.wav", false, mp3, image, error), TextEncoding::NarrowToWide (error).c_str());

            seconds = (double) image.signal.lengthSamples / image.signal.sampleRate;

            Type ("CALL -151\r");
            Type (command.c_str());

            machine.GetTapeDeck().Insert (std::move (image));
            machine.GetTapeDeck().Play (*machine.GetCpu()->GetBusCyclePtr());
            machine.RunCycles ((uint64_t) ((seconds + 1.0) * kClockHz));
            machine.GetTapeDeck().Update (*machine.GetCpu()->GetBusCyclePtr());
        }

        void  Type (const char * text)
        {
            for (size_t i = 0; text[i] != '\0'; i++)
            {
                machine.GetRefs().iieKeyboard->PressKey ((Byte) text[i]);
                machine.RunCycles (100'000ULL);
            }
        }

        std::vector<Byte>  ReadTrack (Byte track)
        {
            std::vector<Byte>  sectors (kSectors);
            std::vector<Byte>  data;
            CassoLinkStatus    status;



            for (size_t i = 0; i < sectors.size(); i++)
            {
                sectors[i] = (Byte) i;
            }

            AssertSucceeded (host->ReadSectors (1, track, sectors, data, status));
            Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, std::format (L"track {}", track).c_str());
            return data;
        }

        //  A track's 16 physical sectors from the image, in order.
        std::vector<Byte>  ImageTrack (Byte track)
        {
            std::vector<Byte>  data;



            for (int p = 0; p < kSectors; p++)
            {
                size_t  offset = (size_t) (track * kSectors + NibblizationLayer::GetDosFileIndexForPhysicalSector (p)) * 256;

                data.insert (data.end(), disk.begin() + (ptrdiff_t) offset, disk.begin() + (ptrdiff_t) offset + 256);
            }

            return data;
        }

        std::string  Screen()
        {
            std::string  text;



            for (const std::string & row : TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400))
            {
                text += row + " | ";
            }

            return text;
        }
    };


    TEST_METHOD (ATrack_IsReadOverTheCassettePorts_OfAnApple2e)
    {
        Rig  rig ("Apple2e");



        Assert::IsTrue (rig.ReadTrack (17) == rig.ImageTrack (17), TextEncoding::NarrowToWide (rig.Screen()).c_str());
    }


    //  The monitor reads the program from tape at its prompt, and the link
    //  then runs over the same cassette ports.
    TEST_METHOD (TheProgram_LoadsFromTape_AndReadsATrack)
    {
        Rig          rig    ("Apple2e", EmulatedCassetteAudio::Faults(), 44100, Start::FromTape);
        std::string  screen = rig.Screen();



        std::transform (screen.begin(), screen.end(), screen.begin(), [] (char c) { return (char) toupper ((unsigned char) c); });
        Assert::IsTrue (screen.find ("CASSETTE PORTS") != std::string::npos, TextEncoding::NarrowToWide (screen).c_str());
        Assert::IsTrue (rig.ReadTrack (17) == rig.ImageTrack (17));
    }


    //  A cassette connection's session on a //e whose program is running:
    //  Read disk to image writes the drive's disk over the cassette ports.
    TEST_METHOD (ACassetteSession_ReadsTheDiskToAnImage)
    {
        static constexpr DWORD  kWaitMs = 10 * 60 * 1000;

        Rig                                rig ("Apple2e");
        CassetteFactory                    factory (rig.machine);
        InMemoryCassoLinkFolder            folder;
        FakeDiskFileIo                     fileIo;
        SessionSink                        sink;
        std::unique_ptr<CassoLinkSession>  session;
        std::shared_ptr<LinkOperation>     read;



        session = std::make_unique<CassoLinkSession> (factory, folder, fileIo, sink);
        AssertSucceeded (session->Start (MakeCassetteSettings()));

        read = session->ReadDisk (1, "out.dsk", false);
        Assert::IsTrue (sink.WaitForDone (read, kWaitMs), L"the read finishes");
        AssertSucceeded (read->result);
        Assert::IsTrue (fileIo.files["out.dsk"] == rig.disk, L"the image is the drive's disk");

        session.reset();
    }


    //  Bootstrap on a cassette connection sends the program's tape through
    //  the output, the monitor's read command typed first, and the link
    //  connects on the same cables.
    TEST_METHOD (ACassetteSession_SendsTheTape_AndConnects)
    {
        static constexpr DWORD  kWaitMs = 5 * 60 * 1000;

        Rig                                rig ("Apple2e", EmulatedCassetteAudio::Faults(), 44100, Start::AtPrompt);
        CassetteFactory                    factory (rig.machine);
        InMemoryCassoLinkFolder            folder;
        FakeDiskFileIo                     fileIo;
        SessionSink                        sink;
        std::unique_ptr<CassoLinkSession>  session;
        std::shared_ptr<LinkOperation>     bootstrap;
        std::vector<double>                halves;
        std::string                        command;



        AssertSucceeded (LinkAudioBootstrap::BuildCassetteLinkRecord (halves, command));
        rig.Type ("CALL -151\r");
        rig.Type (command.c_str());

        session = std::make_unique<CassoLinkSession> (factory, folder, fileIo, sink);
        AssertSucceeded (session->StartWithBootstrap (MakeCassetteSettings(), bootstrap));

        Assert::IsTrue (sink.WaitForDone (bootstrap, kWaitMs), L"the bootstrap finishes");
        AssertSucceeded (bootstrap->result);
        Assert::IsTrue (sink.WaitForState (LinkState::Ready, kWaitMs), L"the link is ready");

        session.reset();
    }


    //  A 13-sector disk written over the cassette ports of a //e: each
    //  track's stream is longer than a burst, so it arrives in two, the
    //  second after the Apple stores the first.
    TEST_METHOD (AThirteenSectorDisk_IsWrittenOverTheCassettePorts)
    {
        Rig                 rig ("Apple2e");
        std::vector<Byte>   module;
        std::vector<Byte>   sectors (ThirteenSectorCodec::kImageByteSize, 0x5A);
        CassoLinkStatus     status;
        std::vector<Byte>   onDisk;
        SectorDecodeReport  report;



        AssertSucceeded (LinkProgram::BuildThirteenSectorModule (module, LinkChip::Cassette));
        AssertSucceeded (rig.host->EnsureModule (module));
        AssertSucceeded (CassoLinkThirteenSector::WriteDisk (*rig.host, 1, sectors, nullptr, nullptr, status));
        Assert::AreEqual ((Byte) 0, status.error);
        ThirteenSectorCodec::Denibblize (*rig.machine.GetDiskStore().GetImage (kDiskSlot, 0), onDisk, report);
        Assert::IsTrue (onDisk == sectors, L"the drive's disk has the sectors written");
    }


    //  Convert to disk audio's recording, sent into a //e at the monitor
    //  with its read command typed and no PC answering, writes the disk in
    //  drive 1.
    TEST_METHOD (ADiskRecording_WritesTheDisk_WithNoPcAnswering)
    {
        static constexpr uint32_t  kRate = 44100;

        Rig                 rig ("Apple2e", EmulatedCassetteAudio::Faults(), kRate, Start::AtPrompt);
        std::vector<Byte>   sectors (NibblizationLayer::kImageByteSize);
        std::vector<float>  samples;
        std::vector<float>  heard;
        std::vector<Byte>   written;
        std::string         command;
        double              ms      = 0.0;



        for (size_t at = 0; at < sectors.size(); at++)
        {
            sectors[at] = (Byte) (at * 11 + at / 256 + 3);
        }

        AssertSucceeded (DiskAudioRecording::Build (sectors, kRate, samples, command));

        rig.Type ("CALL -151\r");
        rig.Type (command.c_str());

        AssertSucceeded (rig.audio->Play (samples));
        ms = (double) samples.size() * 1000.0 / kRate;

        for (double played = 0.0; played < ms; played += 1000.0)
        {
            AssertSucceeded (rig.audio->Capture (1000, heard));
        }

        AssertSucceeded (NibblizationLayer::Denibblize (*rig.machine.GetDiskStore().GetImage (kDiskSlot, 0), DiskFormat::Dsk, written));
        Assert::IsTrue (written == sectors, TextEncoding::NarrowToWide (rig.Screen()).c_str());
    }


    //  This PC's default sound devices, run only when CASSO_CASSETTE_AUDIO is
    //  set: they open, a burst is sent, and the input is captured. With
    //  CASSO_CASSETTE_LOOPBACK set too, and the output cabled to the input,
    //  the burst decodes.
    TEST_METHOD (TheSoundDevices_SendABurst)
    {
        static constexpr DWORD  kListenMs = 3000;

        std::vector<AudioDeviceChoice>  outputs;
        std::vector<AudioDeviceChoice>  inputs;
        std::vector<Byte>               bytes   = { 1, 2, 3, 0x55, 0xAA, 0xFF };
        std::vector<double>             halves;
        std::vector<float>              samples;
        std::vector<float>              heard;
        std::vector<Byte>               burst;
        WasapiCassetteAudio             audio;
        CassetteDecoder                 decoder (WasapiCassetteAudio::kSampleRate);
        size_t                          length  = 0;
        char                          * value   = nullptr;
        bool                            enabled = _dupenv_s (&value, &length, "CASSO_CASSETTE_AUDIO") == 0 && value != nullptr;
        bool                            looped  = false;



        free (value);
        value = nullptr;

        if (!enabled)
        {
            return;
        }

        looped = _dupenv_s (&value, &length, "CASSO_CASSETTE_LOOPBACK") == 0 && value != nullptr;
        free (value);

        AssertSucceeded (WasapiCassetteAudio::ListDevices (false, outputs));
        AssertSucceeded (WasapiCassetteAudio::ListDevices (true, inputs));
        Logger::WriteMessage (std::format ("{} outputs, {} inputs\n", outputs.size(), inputs.size()).c_str());

        AssertSucceeded (audio.Open (L"", L""));

        CassetteCodec::AppendBurst (bytes, halves);
        CassetteCodec::Render (halves, audio.GetSampleRate(), CassetteByteStream::kAmplitude, samples);
        AssertSucceeded (audio.Play (samples));
        Assert::IsTrue (audio.GetPlayingMs() > 0.0);

        AssertSucceeded (audio.Capture (kListenMs, heard));
        Assert::IsFalse (heard.empty(), L"the input is captured");

        decoder.Push (heard);

        if (looped)
        {
            Assert::IsTrue (decoder.PopBurst (burst) && burst == bytes, L"the burst comes back through the cable");
        }
    }


    //  A program damaged on its way from the tape fails its CRC, and the host
    //  fails the operation rather than load the program over the cassette
    //  ports, which the cassette build doesn't take.
    TEST_METHOD (ADamagedProgram_FailsItsCrc_AndIsntLoadedAgain)
    {
        Rig                       rig ("Apple2e");
        std::vector<Byte>         program;
        std::string               errors;
        std::vector<Byte>         sectors  = { 0 };
        std::vector<Byte>         data;
        std::vector<std::string>  activity;
        CassoLinkStatus           status;
        Word                      last     = 0;
        bool                      reported = false;



        AssertSucceeded (LinkProgram::AssembleProgram (program, errors, LinkChip::Cassette));
        last = (Word) (LinkProgram::kProgramAddress + program.size() - 1);

        rig.host->SetActivitySink ([&activity] (const std::string & text) { activity.push_back (text); });
        rig.machine.GetCpu()->WriteByte (last, (Byte) (program.back() ^ 0x55));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CRC), rig.host->ReadSectors (1, 17, sectors, data, status));

        reported = std::any_of (activity.begin(), activity.end(), [] (const std::string & text) { return text.find ("failed its CRC check") != std::string::npos; });
        Assert::IsTrue (reported, L"the failed check is in the activity");
    }


    //  The whole disk read, and a track written and read back, on a ][+.
    TEST_METHOD (AWholeDisk_IsRead_AndATrackWritten_OnAnApple2Plus)
    {
        static constexpr Byte  kTracks = 35;
        static constexpr Byte  kTrack  = 20;

        Rig                rig ("Apple2Plus");
        std::vector<Byte>  sectors (kSectors);
        std::vector<Byte>  pattern ((size_t) kSectors * 256);
        CassoLinkStatus    status;



        for (Byte track = 0; track < kTracks; track++)
        {
            Assert::IsTrue (rig.ReadTrack (track) == rig.ImageTrack (track), std::format (L"track {}", track).c_str());
        }

        for (size_t i = 0; i < pattern.size(); i++)
        {
            pattern[i] = (Byte) (i * 7 + i / 256);
        }

        for (size_t i = 0; i < sectors.size(); i++)
        {
            sectors[i] = (Byte) i;
        }

        AssertSucceeded (rig.host->WriteSectors (1, kTrack, sectors, pattern, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        Assert::IsTrue (rig.ReadTrack (kTrack) == pattern, L"the track written over the cassette ports reads back");
    }


    //  A whole disk written over the cassette ports of a //e: the drive's
    //  disk then has every sector written.
    TEST_METHOD (AWholeDisk_IsWritten_OnAnApple2e)
    {
        static constexpr Byte  kTracks = 35;

        Rig                rig ("Apple2e");
        std::vector<Byte>  sectors (kSectors);
        std::vector<Byte>  image;
        std::vector<Byte>  expected (rig.disk.size());
        CassoLinkStatus    status;



        for (size_t i = 0; i < sectors.size(); i++)
        {
            sectors[i] = (Byte) i;
        }

        for (Byte track = 0; track < kTracks; track++)
        {
            std::vector<Byte>  data ((size_t) kSectors * 256);

            for (size_t i = 0; i < data.size(); i++)
            {
                data[i] = (Byte) (i * 3 + track * 29 + i / 256);
            }

            for (int p = 0; p < kSectors; p++)
            {
                size_t  offset = (size_t) (track * kSectors + NibblizationLayer::GetDosFileIndexForPhysicalSector (p)) * 256;

                std::copy_n (data.begin() + (ptrdiff_t) p * 256, 256, expected.begin() + (ptrdiff_t) offset);
            }

            AssertSucceeded (rig.host->WriteSectors (1, track, sectors, data, status));
            Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error, std::format (L"track {}", track).c_str());
        }

        AssertSucceeded (NibblizationLayer::Denibblize (*rig.machine.GetDiskStore().GetImage (kDiskSlot, 0), DiskFormat::Dsk, image));
        Assert::IsTrue (image == expected, L"the drive's disk has every sector written over the cassette ports");
    }


    //  The audio both ways inverted, the PC's input at 48 kHz, weak, through
    //  a low-pass, with hiss and an offset.
    TEST_METHOD (ATrack_IsRead_OverAPoorAudioPath)
    {
        EmulatedCassetteAudio::Faults  faults;



        faults.invertToApple   = true;
        faults.invertFromApple = true;
        faults.level           = 0.03f;
        faults.hiss            = 0.004f;
        faults.lowPassHz       = 5000.0;
        faults.offset          = 0.01f;

        Rig  rig ("Apple2e", faults, 48000);

        Assert::IsTrue (rig.ReadTrack (3) == rig.ImageTrack (3));
    }
};

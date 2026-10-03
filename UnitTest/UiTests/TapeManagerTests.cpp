#include "Pch.h"

#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FakeTapeAudioDecoder.h"
#include "../EmuTests/TapeTestEncoder.h"
#include "Config/UserConfigStore.h"
#include "Devices/Tape/WavCodec.h"
#include "InMemoryFileSystem.h"
#include "Shell/TapeManager.h"

#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeManagerTests
//
//  The shell side of the recorder over fakes: files come from FakeDiskFileIo,
//  existence and read-only from InMemoryFileSystem, and posted commands are
//  written down and then run against a real deck. No machine name is given,
//  so nothing reaches the on-disk prefs.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TapeManagerTests)
{
public:

    struct Harness
    {
        FakeDiskFileIo                                fileIo;
        InMemoryFileSystem                            fileSystem;
        UserConfigStore                               store { L"C:\\Casso\\User" };
        FakeTapeAudioDecoder                          mp3;
        std::vector<std::pair<WORD, std::string>>     posted;
        std::vector<std::wstring>                     notices;
        TapeManager                                   manager;
        TapeDeck                                      deck;

        Harness() :
            manager (fileIo, fileSystem, store, mp3,
                     [this] (WORD id, const std::string & payload) { posted.emplace_back (id, payload); },
                     [] () { return std::wstring(); },
                     [] (std::function<void()> job) { job(); })
        {
            manager.SetNotifyFn ([this] (const std::wstring & text) { notices.push_back (text); });
        }

        //  Runs every posted command on the deck, as the CPU thread would.
        void Drain()
        {
            for (const auto & [id, payload] : posted)
            {
                switch (id)
                {
                    case IDM_TAPE_INSERT: manager.Execute (TapeCommand::Insert, deck, 0); break;
                    case IDM_TAPE_EJECT:  manager.Execute (payload == TapeManager::kKeepSavedPath ? TapeCommand::Unload : TapeCommand::Eject, deck, 0); break;
                    case IDM_TAPE_PLAY:   manager.Execute (TapeCommand::Play,   deck, 0); break;
                    case IDM_TAPE_STOP:   manager.Execute (TapeCommand::Stop,   deck, 0); break;
                    case IDM_TAPE_REWIND: manager.Execute (TapeCommand::Rewind, deck, 0); break;
                    case IDM_TAPE_SEEK:   manager.Execute (TapeCommand::Seek,   deck, 0); break;
                    case IDM_TAPE_FASTFORWARD: manager.Execute (TapeCommand::FastForward, deck, 0); break;
                    case IDM_TAPE_RECORD: manager.Execute (payload == "1" ? TapeCommand::ArmRecord : TapeCommand::ReleaseRecord, deck, 0); break;
                    default: break;
                }
            }

            posted.clear();
        }

        void AddWav (const std::string & path)
        {
            TapeAudio          audio;
            std::vector<Byte>  bytes;



            audio.sampleRate = 44100;
            audio.samples.assign (4410, 0.0f);
            WavCodec::Encode (audio, bytes);
            fileIo.files[path] = bytes;
            fileSystem.WriteAllText (std::filesystem::path (path).wstring(), "x");
        }
    };


    TEST_METHOD (InsertDecodesOnTheCallerAndLoadsOnExecute)
    {
        Harness      h;



        h.AddWav ("C:\\Tapes\\game.wav");
        h.manager.Insert ("C:\\Tapes\\game.wav");

        Assert::IsTrue   (h.notices.empty());
        Assert::AreEqual (size_t (1), h.posted.size());
        Assert::AreEqual ((WORD) IDM_TAPE_INSERT, h.posted[0].first);
        Assert::IsTrue   (h.deck.GetTransport() == TapeTransport::Empty, L"nothing reaches the deck until the CPU thread runs the command");

        h.Drain();

        Assert::IsTrue   (h.deck.GetTransport() == TapeTransport::Stopped);
        Assert::AreEqual (std::string ("C:\\Tapes\\game.wav"), h.deck.GetImage()->path);
        Assert::AreEqual (std::string ("C:\\Tapes\\game.wav"), h.manager.GetInsertedPath());
        Assert::IsTrue   (h.deck.GetImage()->isWritable);
    }


    TEST_METHOD (UnreadableTapeLeavesTheDeckAlone)
    {
        Harness      h;



        h.fileIo.files["C:\\Tapes\\junk.wav"] = { 'j', 'u', 'n', 'k' };
        h.manager.Insert ("C:\\Tapes\\junk.wav");

        Assert::AreEqual (size_t (1), h.notices.size());
        Assert::IsTrue   (h.posted.empty());
        Assert::IsTrue   (h.manager.GetInsertedPath().empty());
    }


    TEST_METHOD (MissingFileFailsWithAReason)
    {
        Harness      h;



        h.manager.Insert ("C:\\Tapes\\gone.wav");

        Assert::AreEqual (size_t (1), h.notices.size());
        Assert::IsTrue  (h.posted.empty());
    }


    TEST_METHOD (ReadOnlyFileIsNotWritable)
    {
        Harness      h;
        std::string  error;
        HRESULT      hr = S_OK;



        h.AddWav ("C:\\Tapes\\locked.wav");
        h.fileSystem.SetReadOnlyAttribute (L"C:\\Tapes\\locked.wav", true);
        h.manager.Insert ("C:\\Tapes\\locked.wav");
        h.Drain();

        Assert::IsTrue   (h.notices.empty());
        Assert::IsFalse  (h.deck.GetImage()->isWritable);
    }


    TEST_METHOD (TransportCommandsPostAndRun)
    {
        Harness      h;
        std::string  error;



        h.AddWav ("C:\\Tapes\\game.wav");
        h.manager.Insert ("C:\\Tapes\\game.wav");
        h.manager.SetRecordArmed (true);
        h.manager.Play();
        h.Drain();
        Assert::IsTrue (h.deck.GetTransport() == TapeTransport::Recording);

        h.manager.Stop();
        h.manager.Seek (0.0);
        h.Drain();
        Assert::IsTrue   (h.deck.GetTransport() == TapeTransport::Stopped);
        Assert::AreEqual (0.0, h.deck.GetPositionSamples (0));

        h.manager.Eject();
        h.Drain();
        Assert::IsTrue (h.deck.GetTransport() == TapeTransport::Empty);
        Assert::IsTrue (h.manager.GetInsertedPath().empty());
    }


    TEST_METHOD (NewBlankTapeIsAnEmptyWavInserted)
    {
        Harness      h;
        std::string  error;
        TapeAudio    audio;
        HRESULT      hr = S_OK;



        h.fileSystem.WriteAllText (L"C:\\Tapes\\blank.wav", "x");
        h.manager.CreateBlank ("C:\\Tapes\\blank.wav");
        Assert::IsTrue   (h.notices.empty());
        Assert::AreEqual (1, h.fileIo.replaceCount);

        hr = WavCodec::Decode (h.fileIo.files["C:\\Tapes\\blank.wav"], audio, error);
        Assert::AreEqual (S_OK, hr);
        Assert::AreEqual (TapeManager::kBlankSampleRate, audio.sampleRate);
        Assert::IsTrue   (audio.samples.empty());

        h.Drain();
        Assert::IsTrue (h.deck.GetTransport() == TapeTransport::Stopped);
        Assert::IsTrue (h.deck.GetImage()->isWritable);
    }


    TEST_METHOD (FailedBlankWriteInsertsNothing)
    {
        Harness      h;
        std::string  error;
        HRESULT      hr = S_OK;



        h.fileIo.failNextWrite = true;
        h.manager.CreateBlank ("C:\\Tapes\\blank.wav");

        Assert::AreEqual (size_t (1), h.notices.size());
        Assert::IsTrue  (h.posted.empty());
    }


    TEST_METHOD (RememberedTapeComesBackRewoundAndStopped)
    {
        Harness  h;
        HRESULT  hr = S_OK;



        h.AddWav ("C:\\Tapes\\game.wav");
        hr = h.manager.RestoreTape (L"C:\\Tapes\\game.wav");
        h.Drain();

        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (h.deck.GetTransport() == TapeTransport::Stopped);
        Assert::AreEqual (0.0, h.deck.GetPositionSamples (0));
    }


    TEST_METHOD (RememberedTapeThatIsGoneLeavesTheDeckEmpty)
    {
        Harness  h;
        HRESULT  hr = h.manager.RestoreTape (L"C:\\Tapes\\gone.wav");



        h.Drain();

        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (h.deck.GetTransport() == TapeTransport::Empty);
    }


    TEST_METHOD (NothingRememberedChangesNothing)
    {
        Harness  h;
        HRESULT  hr = h.manager.RestoreTape (L"");



        Assert::AreEqual (S_OK, hr);
        Assert::IsTrue   (h.posted.empty());
    }


    TEST_METHOD (MachineSwitchEjectsBeforeRestoring)
    {
        Harness      h;
        std::string  error;



        h.AddWav ("C:\\Tapes\\game.wav");
        h.manager.Insert ("C:\\Tapes\\game.wav");
        h.Drain();

        h.manager.OnMachineSwitched();

        Assert::AreEqual ((WORD) IDM_TAPE_EJECT, h.posted.front().first);
        h.Drain();
        Assert::IsTrue (h.deck.GetTransport() == TapeTransport::Empty);
    }

    TEST_METHOD (EjectDuringALoadWinsOverTheLoad)
    {
        FakeDiskFileIo                                fileIo;
        InMemoryFileSystem                            fileSystem;
        UserConfigStore                               store (L"C:\\Casso\\User");
        FakeTapeAudioDecoder                          mp3;
        std::vector<std::pair<WORD, std::string>>     posted;
        std::vector<std::function<void()>>           held;
        TapeAudio                                     audio;
        std::vector<Byte>                             bytes;
        TapeManager                                   manager (fileIo, fileSystem, store, mp3,
                                                               [&posted] (WORD id, const std::string & payload) { posted.emplace_back (id, payload); },
                                                               [] () { return std::wstring(); },
                                                               [&held] (std::function<void()> job) { held.push_back (std::move (job)); });



        audio.sampleRate = 44100;
        WavCodec::Encode (audio, bytes);
        fileIo.files["C:\\Tapes\\slow.wav"] = bytes;

        manager.Insert ("C:\\Tapes\\slow.wav");
        Assert::IsTrue (posted.empty(), L"the decode has not run yet");

        manager.Eject();
        held.front()();

        Assert::AreEqual (size_t (1), posted.size(), L"only the eject reaches the deck");
        Assert::AreEqual ((WORD) IDM_TAPE_EJECT, posted[0].first);
        Assert::IsTrue   (manager.GetInsertedPath().empty());
    }


    TEST_METHOD (MachineSwitchUnloadsWithoutForgetting)
    {
        Harness  h;



        h.manager.OnMachineSwitched();

        Assert::AreEqual ((WORD) IDM_TAPE_EJECT, h.posted.front().first);
        Assert::AreEqual (std::string (TapeManager::kKeepSavedPath), h.posted.front().second);
    }
};

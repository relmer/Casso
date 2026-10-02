#include "Pch.h"

#include "Config/DiskSettings.h"
#include "Config/IFileSystem.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Devices/Tape/TapeRecorder.h"
#include "Devices/Tape/WavCodec.h"
#include "Shell/TapeManager.h"
#include "Ui/AutoMountResolver.h"

#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeManager
//
////////////////////////////////////////////////////////////////////////////////

TapeManager::TapeManager (
    IDiskFileIo        & fileIo,
    IFileSystem        & fileSystem,
    UserConfigStore    & configStore,
    ITapeAudioDecoder  & compressedDecoder,
    PostFn               post,
    MachineNameFn        machineName) :
    m_fileIo            (fileIo),
    m_fileSystem        (fileSystem),
    m_configStore       (configStore),
    m_compressedDecoder (compressedDecoder),
    m_post              (std::move (post)),
    m_machineName       (std::move (machineName))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Insert
//
//  Reads and decodes the tape here, on the calling thread, then hands it to
//  the CPU thread to put in the deck. A tape that cannot be read leaves the
//  deck as it was and says why in error. The path is remembered for the next
//  launch; failing to remember it does not fail the insert.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeManager::Insert (const std::string & path, std::string & error)
{
    HRESULT            hr         = S_OK;
    HRESULT            hrAttr     = S_OK;
    HRESULT            hrSave     = S_OK;
    std::vector<Byte>  bytes;
    TapeImage          image;
    bool               isReadOnly = false;



    hr = m_fileIo.ReadAllBytes (path, bytes);
    CHRF (hr, error = "The file could not be read.");

    // An attribute that cannot be read is taken as writable; a write that
    // then fails is reported when the recording is committed.
    hrAttr = m_fileSystem.GetReadOnlyAttribute (std::filesystem::path (path).wstring(), isReadOnly);
    IGNORE_RETURN_VALUE (hrAttr, S_OK);

    hr = TapeImageLoader::Load (bytes, path, isReadOnly, m_compressedDecoder, image, error);
    CHR (hr);

    {
        std::lock_guard<std::mutex>  lock (m_pendingLock);

        m_pending = std::move (image);
    }

    m_insertedPath = path;
    m_post (IDM_TAPE_INSERT, {});

    hrSave = SaveTapePath (path);
    IGNORE_RETURN_VALUE (hrSave, S_OK);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateBlank
//
//  A zero-length 44.1 kHz 16-bit mono WAV, written whole before it replaces
//  anything, then inserted so it is ready to record on.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeManager::CreateBlank (const std::string & path, std::string & error)
{
    HRESULT            hr       = S_OK;
    TapeAudio          blank;
    std::vector<Byte>  bytes;
    std::string        tempPath = path + ".tmp";



    blank.sampleRate = kBlankSampleRate;
    WavCodec::Encode (blank, bytes);

    hr = m_fileIo.WriteAllBytes (tempPath, bytes);
    CHRF (hr, error = "The new tape could not be written.");

    hr = m_fileIo.ReplaceAtomically (tempPath, path);
    CHRF (hr, error = "The new tape could not be written.");

    hr = Insert (path, error);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Eject
//
////////////////////////////////////////////////////////////////////////////////

void TapeManager::Eject()
{
    HRESULT  hrSave = S_OK;



    m_insertedPath.clear();
    m_post (IDM_TAPE_EJECT, {});

    hrSave = SaveTapePath ({});
    IGNORE_RETURN_VALUE (hrSave, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Play
//
////////////////////////////////////////////////////////////////////////////////

void TapeManager::Play()
{
    m_post (IDM_TAPE_PLAY, {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  Stop
//
////////////////////////////////////////////////////////////////////////////////

void TapeManager::Stop()
{
    m_post (IDM_TAPE_STOP, {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  Rewind
//
////////////////////////////////////////////////////////////////////////////////

void TapeManager::Rewind()
{
    m_post (IDM_TAPE_REWIND, {});
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetRecordArmed
//
////////////////////////////////////////////////////////////////////////////////

void TapeManager::SetRecordArmed (bool isArmed)
{
    m_post (IDM_TAPE_RECORD, isArmed ? "1" : "0");
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreSavedTape
//
//  Puts back the tape this machine had at the end of the last session, stopped
//  at its start. A remembered file that has since gone is forgotten, and the
//  deck stays empty.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeManager::RestoreSavedTape()
{
    HRESULT       hr          = S_OK;
    std::wstring  machineName = m_machineName();
    std::wstring  saved;
    bool          hasMachine  = !machineName.empty();



    BAIL_OUT_IF (!hasMachine, S_OK);

    hr = DiskSettings::ReadSavedTapePath (m_configStore, m_fileSystem, machineName, saved);
    CHR (hr);

    hr = RestoreTape (saved);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RestoreTape
//
//  Inserts the remembered tape when the file is still there, and forgets it
//  when it is not. Nothing remembered leaves the deck as it is.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeManager::RestoreTape (const std::wstring & savedPath)
{
    HRESULT                      hr       = S_OK;
    std::string                  error;
    AutoMountResolver::Decision  decision = AutoMountResolver::Resolve (savedPath, m_fileSystem);



    if (decision.action == AutoMountResolver::Action::Mount)
    {
        hr = Insert (std::filesystem::path (decision.path).string(), error);
        CHR (hr);
    }
    else if (decision.action == AutoMountResolver::Action::ClearStaleEntry)
    {
        hr = SaveTapePath ({});
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMachineSwitched
//
//  Takes the outgoing machine's tape out without forgetting it, then puts in
//  whatever the incoming machine had. The commands queue in that order, so
//  the deck never holds both. A machine with no cassette port has nothing
//  saved, so its deck stays empty.
//
////////////////////////////////////////////////////////////////////////////////

void TapeManager::OnMachineSwitched()
{
    HRESULT  hr = S_OK;



    m_insertedPath.clear();
    m_post (IDM_TAPE_EJECT, {});

    hr = RestoreSavedTape();
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Execute
//
//  On the CPU thread: carries out one posted command against the deck.
//
////////////////////////////////////////////////////////////////////////////////

void TapeManager::Execute (TapeCommand command, TapeDeck & deck, uint64_t nowCycle)
{
    HRESULT                   hr = S_OK;
    std::optional<TapeImage>  pending;



    switch (command)
    {
        case TapeCommand::Insert:
        {
            std::lock_guard<std::mutex>  lock (m_pendingLock);

            pending.swap (m_pending);
            break;
        }

        case TapeCommand::Eject:
            deck.Stop (nowCycle);
            hr = CommitPendingRecording (deck);
            IGNORE_RETURN_VALUE (hr, S_OK);
            deck.Eject (nowCycle);
            break;

        case TapeCommand::Play:          deck.Play   (nowCycle);         break;

        case TapeCommand::Stop:
        case TapeCommand::Rewind:
            deck.Stop (nowCycle);
            hr = CommitPendingRecording (deck);
            IGNORE_RETURN_VALUE (hr, S_OK);

            if (command == TapeCommand::Rewind)
            {
                deck.Rewind (nowCycle);
            }

            break;

        case TapeCommand::ArmRecord:     deck.SetRecordArmed (true);     break;
        case TapeCommand::ReleaseRecord: deck.SetRecordArmed (false);    break;
    }

    if (pending.has_value())
    {
        deck.Insert (std::move (*pending));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommitPendingRecording
//
//  Writes a stopped recording onto the tape and swaps the rewritten tape into
//  the deck. Stop, rewind and eject commit at once; a reset, power cycle or
//  machine switch stops the deck on its own, and the shell commits that on
//  the next slice. A recording that cannot be written is reported and the
//  deck keeps the tape it had.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeManager::CommitPendingRecording (TapeDeck & deck)
{
    HRESULT             hr         = S_OK;
    RecordingCapture    capture;
    TapeImage           image;
    std::string         error;
    const TapeImage   * inserted   = deck.GetImage();
    bool                hasPending = deck.HasPendingRecording() && inserted != nullptr;
    std::string         path;



    BAIL_OUT_IF (!hasPending, S_OK);

    path = inserted->path;
    deck.TakeRecording (capture);

    hr = TapeRecorder::Commit (m_fileIo, path, capture, deck.GetCpuClock(), image, error);
    CHRF (hr, Notify (L"Error: recording not saved\n" + std::filesystem::path (error).wstring()));

    deck.ReplaceImage (std::move (image));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveTapePath
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeManager::SaveTapePath (const std::string & path)
{
    HRESULT       hr          = S_OK;
    std::wstring  machineName = m_machineName();
    bool          hasMachine  = !machineName.empty();



    BAIL_OUT_IF (!hasMachine, S_OK);

    hr = DiskSettings::WriteSavedTapePath (m_configStore, m_fileSystem, machineName, std::filesystem::path (path).wstring());
    CHR (hr);

Error:
    return hr;
}

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTape.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Core/TextEncoding.h"
#include "Devices/Tape/MfTapeAudioEncoder.h"
#include "Devices/Tape/TapeRecordingVolume.h"
#include "Machines/Apple2/Common/Dos33Volume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsTape::TryGetTapeKind
//
//  DOS 3.3's A, I and B types, and ProDOS's BAS, INT and BIN, which an
//  archive's and a tape's files have too.
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsTape::TryGetTapeKind (VolumeKind volume, Byte type, TapeFileKind & outKind)
{
    bool  isDos    = volume == VolumeKind::Dos33;
    bool  isProDos = volume == VolumeKind::ProDos || volume == VolumeKind::Archive || volume == VolumeKind::Tape;
    bool  found    = true;



    if (isDos && type == Dos33Volume::kTypeApplesoft)
    {
        outKind = TapeFileKind::Applesoft;
    }
    else if (isDos && type == Dos33Volume::kTypeInteger)
    {
        outKind = TapeFileKind::IntegerBasic;
    }
    else if (isDos && type == Dos33Volume::kTypeBinary)
    {
        outKind = TapeFileKind::Binary;
    }
    else if (isProDos && type == TapeRecordingVolume::kTypeApplesoft)
    {
        outKind = TapeFileKind::Applesoft;
    }
    else if (isProDos && type == TapeRecordingVolume::kTypeInteger)
    {
        outKind = TapeFileKind::IntegerBasic;
    }
    else if (isProDos && type == TapeRecordingVolume::kTypeBinary)
    {
        outKind = TapeFileKind::Binary;
    }
    else
    {
        found = false;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsTape::CanMakeRecording
//
////////////////////////////////////////////////////////////////////////////////

bool ActionsTape::CanMakeRecording (VolumeKind volume, const std::vector<FileEntry> & entries)
{
    TapeFileKind  kind = TapeFileKind::Binary;



    return std::any_of (entries.begin(), entries.end(),
                        [&] (const FileEntry & entry) { return !entry.isDirectory && TryGetTapeKind (volume, entry.type, kind); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsTape::GetFormat
//
////////////////////////////////////////////////////////////////////////////////

TapeRecordingFormat ActionsTape::GetFormat (const std::wstring & path)
{
    std::wstring  extension = std::filesystem::path (path).extension().wstring();



    std::transform (extension.begin(), extension.end(), extension.begin(), [] (wchar_t c) { return (wchar_t) towlower (c); });

    return (extension == L".mp3") ? TapeRecordingFormat::Mp3
         : (extension == L".ogg") ? TapeRecordingFormat::Ogg
                                  : TapeRecordingFormat::Wav;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsTape::GetSidecarPath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ActionsTape::GetSidecarPath (const std::wstring & path)
{
    std::filesystem::path  sidecar = path;



    sidecar.replace_extension (L".tape.txt");

    return sidecar.wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsTape::MakeRecording
//
////////////////////////////////////////////////////////////////////////////////

ActionsTypes::Outcome ActionsTape::MakeRecording (const std::wstring & target)
{
    ActionsTypes::Outcome           outcome;
    CassoExplorerBrowser          & browser = m_actions.m_browser;
    std::string                     image   = TextEncoding::WideToNarrow (browser.GetLocation().path);
    VolumeKind                      volume  = browser.GetVolumeKind();
    std::vector<FileEntry>          entries;
    std::vector<TapeRecordingFile>  files;
    TapeAudio                       audio;
    std::string                     sidecar;
    std::vector<Byte>               bytes;
    std::string                     error;
    MfTapeAudioEncoder              encoder;
    HRESULT                         hr      = S_OK;



    browser.GetSelectedEntries (entries);

    for (const FileEntry & entry : entries)
    {
        TapeRecordingFile  file;
        FilePayload        payload;

        if (entry.isDirectory || !TryGetTapeKind (volume, entry.type, file.kind))
        {
            ActionsCopy::AppendMessage (outcome, S_OK, std::format (L"{} isn't a BASIC program or a binary, so it was left out.", TextEncoding::NarrowToWide (entry.name)));
            continue;
        }

        CassoExplorerActions::Append (outcome, browser.GetOperations().Read (image, browser.GetEntryPath (entry), payload, entry.catalogIndex));

        if (!outcome.Succeeded())
        {
            return outcome;
        }

        file.name        = entry.name;
        file.bytes       = payload.bytes;
        file.loadAddress = payload.hasLoadAddress ? payload.loadAddress : payload.auxType;

        files.push_back (std::move (file));
    }

    if (files.empty())
    {
        outcome.hr = E_INVALIDARG;
        return outcome;
    }

    TapeRecordingWriter::Render (files, audio, sidecar);

    hr = TapeRecordingWriter::Encode (audio, GetFormat (target), encoder, bytes, error);

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, std::format (L"The recording wasn't made. {}", TextEncoding::NarrowToWide (error)));
        return outcome;
    }

    hr = browser.GetOperations().GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (target), bytes);

    if (SUCCEEDED (hr))
    {
        hr = browser.GetOperations().GetFileIo().WriteAllBytes (TextEncoding::WideToNarrow (GetSidecarPath (target)),
                                                                std::vector<Byte> (sidecar.begin(), sidecar.end()));
    }

    if (FAILED (hr))
    {
        ActionsCopy::AppendMessage (outcome, hr, CassoExplorerActions::GetLeafName (target) + L" could not be written.");
        return outcome;
    }

    outcome.written = (int) files.size();

    return outcome;
}

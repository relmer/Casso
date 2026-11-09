#include "Pch.h"

#include "CassoExplorer/ExplorerInspectorHost.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/InspectorImageLoader.h"
#include "Devices/Disk/MountDiagnosis.h"
#include "Seams/IIntentChannel.h"
#include "Seams/Win32IntentChannel.h"
#include "Shell/DiskManager.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::ExplorerInspectorHost
//
////////////////////////////////////////////////////////////////////////////////

ExplorerInspectorHost::ExplorerInspectorHost (const std::wstring & path, IDiskFileIo & fileIo, IIntentChannel * intents) :
    m_path     (path),
    m_utf8Path (TextEncoding::WideToUtf8 (path)),
    m_fileIo   (fileIo),
    m_intents  (intents),
    m_mediaId  (InspectorImageLoader::MakeMediaId())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::PostInspectorRequest
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ExplorerInspectorHost::PostInspectorRequest (const InspectorRequest & request)
{
    uint64_t  id = m_queue.Post (request);



    m_queue.Drain (*this);

    return id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::TakeInspectorReplies
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerInspectorHost::TakeInspectorReplies (vector<InspectorReply> & outReplies)
{
    m_queue.TakeReplies (outReplies);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::HandleCopyData
//
//  Casso's reply to the intent stated after a save. The note is taken on
//  the window's next frame, since Casso's send does not return until this
//  does.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerInspectorHost::HandleCopyData (const COPYDATASTRUCT & data)
{
    std::wstring  note;



    if (data.dwData != Win32IntentChannel::GetReplyMessageId() || data.lpData == nullptr)
    {
        return false;
    }

    note = DescribeCassoReply (static_cast<const Byte *> (data.lpData), data.cbData);

    if (!note.empty())
    {
        m_hostNote = note;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::TakeHostNote
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerInspectorHost::TakeHostNote()
{
    std::wstring  note;



    note.swap (m_hostNote);

    return note;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::DescribeCassoReply
//
//  A conflict or a refusal has the text Casso Explorer's other windows use
//  for it, so the drive's file is on screen in both programs (033's FR-024).
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerInspectorHost::DescribeCassoReply (const Byte * bytes, size_t byteCount)
{
    Win32IntentChannel::Reply  reply;
    std::wstring               note;



    if (Win32IntentChannel::DecodeReply (bytes, byteCount, reply) &&
        (reply.kind == Win32IntentChannel::ReplyKind::ReloadConflict || reply.kind == Win32IntentChannel::ReplyKind::ReloadRefused))
    {
        note = L"Casso did not reload the changed disk. " + TextEncoding::NarrowToWide (reply.text);
    }

    return note;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::Reload
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerInspectorHost::Reload()
{
    m_isRead = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::IsFileCurrent
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerInspectorHost::IsFileCurrent()
{
    FileStamp  observed;
    HRESULT    hr = m_fileIo.Stat (m_utf8Path, observed);



    return m_isRead && m_opened.stampRecorded && SUCCEEDED (hr) &&
           !CommitPlan::IsStale (m_opened.stamp.sizeBytes, m_opened.stamp.modifiedUnix, observed.sizeBytes, observed.modifiedUnix);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::GetImage
//
//  Drive 0 is the file. Every read has the window's media id, so the
//  selected track stays selected after the file is read again (FR-074).
//
////////////////////////////////////////////////////////////////////////////////

const DiskImage * ExplorerInspectorHost::GetImage (int drive, InspectorDiskIdentity & outIdentity)
{
    if (drive != 0)
    {
        return nullptr;
    }

    if (!m_isRead)
    {
        Load();
    }

    outIdentity = m_identity;

    return (m_image != nullptr && m_image->IsLoaded()) ? m_image.get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::GetImageForEdit
//
////////////////////////////////////////////////////////////////////////////////

DiskImage * ExplorerInspectorHost::GetImageForEdit (int drive)
{
    InspectorDiskIdentity  identity;



    return const_cast<DiskImage *> (GetImage (drive, identity));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::SaveDrive
//
//  The image is serialized in its format and committed as Explorer commits
//  its other changes. The commit fails while another program has the file
//  open, for a WOZ whose write-protect flag is set, and for a file changed
//  since it was read. Otherwise the bytes are written beside the file and
//  moved over it durably, and then the reload intent is sent to Casso
//  (FR-105).
//
////////////////////////////////////////////////////////////////////////////////

InspectorSaveResult ExplorerInspectorHost::SaveDrive (int drive)
{
    HRESULT              hr      = S_OK;
    InspectorSaveResult  save;
    DiskImageSession     session (m_fileIo);
    DiskCommandResult    command;
    vector<Byte>         bytes;
    std::string          refusal;
    FileStamp            written;
    HRESULT              statHr  = S_OK;
    std::wstring         name    = fs::path (m_path).filename().wstring();



    save.path = m_utf8Path;

    CBRAEx (drive == 0 && m_image != nullptr, E_INVALIDARG);

    hr = m_image->Serialize (bytes);
    CHRF (hr, save.reason = name + L" could not be written in its format.");

    hr = session.CommitImage (m_opened, bytes, command);

    if (FAILED (hr))
    {
        refusal = command.diagnostics;
        refusal = refusal.starts_with (m_utf8Path + ": ") ? refusal.substr (m_utf8Path.size() + 2) : refusal;

        while (!refusal.empty() && (refusal.back() == '\n' || refusal.back() == '\r'))
        {
            refusal.pop_back();
        }

        save.reason = name + L" " + TextEncoding::Utf8ToWide (refusal) + L".";
        CHR (hr);
    }

    statHr                 = m_fileIo.Stat (m_utf8Path, written);
    m_opened.fileBytes     = bytes;
    m_opened.stampRecorded = SUCCEEDED (statHr);
    m_opened.stamp         = written;
    m_identity.fileSize    = bytes.size();
    m_image->ClearDirty();

    if (m_intents != nullptr)
    {
        m_intents->StateIntent (m_utf8Path, ExternalChangeIntent::ReloadInPlace);
    }

Error:
    save.hr = hr;
    return save;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::GetOpenError
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerInspectorHost::GetOpenError (int drive)
{
    return (drive == 0) ? m_openError : std::wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerInspectorHost::Load
//
//  The format is chosen by the extension, as for a mount, and the file's
//  size and time are stored with its bytes, for the check a save makes.
//  A read-only file, or one Explorer cannot write, is write-protected, as
//  in a drive (FR-102). For a file that cannot be read or loaded, m_image
//  is empty and m_openError has the reason a mount fails with.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerInspectorHost::Load()
{
    HRESULT         hr           = S_OK;
    std::wstring    name         = fs::path (m_path).filename().wstring();
    DiskFormat      format       = DiskFormat::Dsk;
    bool            readOnly     = false;
    bool            noPermission = false;
    HRESULT         statHr       = S_OK;
    MountDiagnosis  diagnosis;
    vector<Byte>    bytes;



    m_isRead = true;
    m_image  = std::make_unique<DiskImage>();
    m_opened = DiskImageSession::OpenedImage();
    m_openError.clear();

    m_opened.imagePath = m_utf8Path;

    m_identity          = InspectorDiskIdentity();
    m_identity.mediaId  = m_mediaId;
    m_identity.fileName = m_utf8Path;

    hr = DiskImageStore::GetSourceFormatByExtension (m_utf8Path, format);
    CHRF (hr, m_openError = std::format (L"{} is not a disk image Casso opens.", name));

    hr = m_fileIo.ReadAllBytes (m_utf8Path, bytes);
    CHRF (hr, m_openError = std::format (L"{} could not be read.", name));

    statHr                 = m_fileIo.Stat (m_utf8Path, m_opened.stamp);
    m_opened.stampRecorded = SUCCEEDED (statHr);
    m_opened.fileBytes     = bytes;

    if (m_firstBytes == nullptr)
    {
        m_firstBytes = std::make_shared<const vector<Byte>> (bytes);
    }

    DiskManager::ProbeFileWritability (m_utf8Path, readOnly, noPermission);
    m_identity.fileSize   = bytes.size();
    m_identity.isReadOnly = readOnly;

    m_image->LoadFromBytes (format, bytes, m_utf8Path);
    m_image->SetFileWriteProtect (readOnly, noPermission);

    if (!m_image->IsLoaded())
    {
        diagnosis   = DiskImageStore::ClassifyLoadFailure (format, bytes);
        m_openError = std::format (L"{} {}", name, TextEncoding::NarrowToWide (diagnosis.Describe()));
    }

Error:
    return;
}

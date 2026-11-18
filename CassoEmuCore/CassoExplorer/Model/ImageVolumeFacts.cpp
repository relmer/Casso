#include "Pch.h"

#include "CassoExplorer/Model/CollectionWalk.h"
#include "CassoExplorer/Model/ImageVolumeFacts.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageVolumeFacts::ImageVolumeFacts
//
//  The reader is Read unless a test supplies another.
//
////////////////////////////////////////////////////////////////////////////////

ImageVolumeFacts::ImageVolumeFacts (IDiskFileIo & fileIo, Notify notify, Reader read) :
    m_fileIo (fileIo),
    m_notify (std::move (notify)),
    m_read   (std::move (read))
{
    if (!m_read)
    {
        m_read = [this] (const std::string & path) { return Read (m_fileIo, path); };
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageVolumeFacts::~ImageVolumeFacts
//
//  An image being read finishes before the thread is joined.
//
////////////////////////////////////////////////////////////////////////////////

ImageVolumeFacts::~ImageVolumeFacts()
{
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_isStopping = true;
        m_queue.clear();
    }

    m_wake.notify_all();

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageVolumeFacts::TryGet
//
////////////////////////////////////////////////////////////////////////////////

bool ImageVolumeFacts::TryGet (const std::wstring & path, uint64_t sizeBytes, int64_t modifiedUnix, VolumeFacts & outFacts)
{
    std::lock_guard<std::mutex>  lock  (m_lock);
    auto                         found = m_entries.find (path);
    bool                         isHit = found != m_entries.end() && found->second.sizeBytes == sizeBytes && found->second.modifiedUnix == modifiedUnix;



    if (isHit)
    {
        outFacts = found->second.facts;
    }

    return isHit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageVolumeFacts::Queue
//
//  The thread starts with the first file queued.
//
////////////////////////////////////////////////////////////////////////////////

void ImageVolumeFacts::Queue (const std::vector<Request> & requests)
{
    {
        std::lock_guard<std::mutex>  lock (m_lock);

        m_queue.assign (requests.begin(), requests.end());
    }

    if (!m_thread.joinable() && !requests.empty())
    {
        m_thread = std::thread ([this] () { Run(); });
    }

    m_wake.notify_all();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageVolumeFacts::Cancel
//
////////////////////////////////////////////////////////////////////////////////

void ImageVolumeFacts::Cancel()
{
    std::lock_guard<std::mutex>  lock (m_lock);



    m_queue.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageVolumeFacts::Read
//
//  The image decoded to sectors as Disk list decodes it, then its volume's
//  directory, for the volume's name. Only the root is read.
//
////////////////////////////////////////////////////////////////////////////////

VolumeFacts ImageVolumeFacts::Read (IDiskFileIo & fileIo, const std::string & path)
{
    HRESULT                   hr     = S_OK;
    VolumeFacts               facts;
    vector<Byte>              bytes;
    vector<Byte>              sectors;
    SectorDecodeReport        report;
    VolumeKind                kind   = VolumeKind::Unknown;
    std::unique_ptr<IVolume>  volume;
    VolumeListing             listing;



    hr = fileIo.ReadAllBytes (path, bytes);
    CHR (hr);

    hr = VolumeImage::Load (bytes, path, sectors, report);
    CHR (hr);

    kind = VolumeImage::DetectFilesystem (sectors);
    BAIL_OUT_IF (kind == VolumeKind::Unknown || kind == VolumeKind::Archive, S_OK);

    facts.fileSystem = TextEncoding::NarrowToWide (VolumeImage::GetSystemName (kind));

    volume = VolumeImage::CreateVolume (kind, sectors);
    CBR (volume != nullptr);

    hr = volume->Enumerate (listing);
    CHR (hr);

    facts.volumeName = TextEncoding::NarrowToWide (CollectionWalk::GetVolumeName (listing));

Error:
    return facts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageVolumeFacts::Run
//
//  One file at a time from the front of the queue. A file whose facts are
//  already kept at its length and time is skipped.
//
////////////////////////////////////////////////////////////////////////////////

void ImageVolumeFacts::Run()
{
    Request      next;
    VolumeFacts  facts;
    bool         isKnown = false;



    for (;;)
    {
        {
            std::unique_lock<std::mutex>  lock (m_lock);

            m_wake.wait (lock, [this] () { return m_isStopping || !m_queue.empty(); });

            if (m_isStopping)
            {
                return;
            }

            next = m_queue.front();
            m_queue.pop_front();

            isKnown = m_entries.contains (next.path)
                   && m_entries[next.path].sizeBytes == next.sizeBytes
                   && m_entries[next.path].modifiedUnix == next.modifiedUnix;
        }

        if (isKnown)
        {
            continue;
        }

        facts = m_read (TextEncoding::WideToUtf8 (next.path));

        {
            std::lock_guard<std::mutex>  lock (m_lock);

            m_entries[next.path] = Entry { next.sizeBytes, next.modifiedUnix, facts };
        }

        if (m_notify)
        {
            m_notify();
        }
    }
}

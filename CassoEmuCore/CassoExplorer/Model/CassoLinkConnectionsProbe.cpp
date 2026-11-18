#include "Pch.h"

#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoLink/CassoLinkSession.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::ProbeDrives
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::ProbeDrives()
{
    std::lock_guard<std::mutex>  lock (m_lock);



    for (const std::unique_ptr<Link> & link : m_started)
    {
        LinkState  state     = link->session->GetState();
        Byte       map       = link->session->GetDriveMap();
        bool       connected = state == LinkState::Ready || state == LinkState::Busy;
        bool       again     = connected && !link->connected;

        link->connected = connected;

        //  A link connected again drops the reads that finished before:
        //  they may have failed with the line, and the Apple's disks may
        //  have changed while it was gone. A drive read then has its volume
        //  again, for its row and Write image's confirmation.
        if (again)
        {
            std::erase_if (link->requests, [] (const auto & request)
            {
                LinkOperationState  done = request.second->state;

                return done != LinkOperationState::Queued && done != LinkOperationState::Running;
            });
        }

        //  Reads blocked while a Full Send ran are made again once it ends.
        if (!link->session->IsFullSendRunning())
        {
            std::erase_if (link->requests, [] (const auto & request) { return request.second->isBlocked; });
        }

        if (!connected)
        {
            continue;
        }

        for (int drive = 1; drive <= CassoLinkSessionTypes::kDriveCount; drive++)
        {
            std::string  key = MakeKey (drive, std::string(), false);

            if ((map & (1 << (drive - 1))) != 0 && link->requests.find (key) == link->requests.end())
            {
                link->requests[key] = link->session->Enumerate (drive, FilePath());
            }
        }

        ProbeBlockDevices (*link);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::ProbeBlockDevices
//
//  The Apple's block devices are listed once a connection, after the
//  drives' catalogs, and each one's catalog is read once it is listed.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::ProbeBlockDevices (Link & link)
{
    HRESULT  hr     = S_OK;
    auto     listed = link.requests.find (s_kBlockListKey);
    size_t   count  = 0;



    if (listed == link.requests.end())
    {
        link.requests[s_kBlockListKey] = link.session->ListBlockDevices();
        return;
    }

    BAIL_OUT_IF (listed->second->state != LinkOperationState::Done, S_OK);

    count = link.session->GetBlockDevices().size();

    for (size_t each = 0; each < count; each++)
    {
        int          drive = CassoLinkBlockDevices::kFirstDrive + (int) each;
        std::string  key   = MakeKey (drive, std::string(), false);

        if (link.requests.find (key) == link.requests.end())
        {
            link.requests[key] = link.session->Enumerate (drive, FilePath());
        }
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::GetBlockDevices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CassoLinkBlockDevice> CassoLinkConnections::GetBlockDevices (const std::string & name) const
{
    std::lock_guard<std::mutex>  lock (m_lock);
    const Link                 * link = FindLink (name);



    return (link != nullptr && link->session != nullptr) ? link->session->GetBlockDevices() : std::vector<CassoLinkBlockDevice>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnections::AddBlockDevices
//
//  A row for each block device listed, with its volume from its catalog
//  once read, or from block 2 as the list found it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkConnections::AddBlockDevices (const Link & link, LinkTreeEntry & ioEntry)
{
    std::vector<CassoLinkBlockDevice>  devices = link.session->GetBlockDevices();



    for (size_t each = 0; each < devices.size(); each++)
    {
        LinkTreeEntry::Drive  row;
        int                   drive   = CassoLinkBlockDevices::kFirstDrive + (int) each;
        auto                  catalog = link.requests.find (MakeKey (drive, std::string(), false));

        row.slot          = devices[each].slot;
        row.drive         = drive;
        row.isBlockDevice = true;
        row.unit          = devices[each].unit;
        row.blocks        = devices[each].blocks;
        row.volume        = devices[each].volume.empty() ? std::wstring() : L"/" + TextEncoding::NarrowToWide (devices[each].volume);

        if (catalog != link.requests.end() && catalog->second->state == LinkOperationState::Done)
        {
            for (const FileEntry & file : catalog->second->listing.entries)
            {
                if (file.isDirectory)
                {
                    row.folders.push_back (TextEncoding::NarrowToWide (file.name));
                }
            }
        }

        ioEntry.drives.push_back (row);
    }
}

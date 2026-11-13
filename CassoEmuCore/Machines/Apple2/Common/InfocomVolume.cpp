#include "Pch.h"

#include "Machines/Apple2/Common/InfocomVolume.h"
#include "Devices/Disk/FilePath.h"
#include "FileTypes/ZMachineStory.h"
#include "Machines/Apple2/Common/SectorReadObserver.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"





//  The DOS logical sector a physical sector's data is in, and a ProDOS
//  order sector's, for a buffer in DOS logical order. The third table is the
//  first's inverse, the order a real Zork I, release 88, reads its story in.
static constexpr int  s_kPhysicalToDos[16] = { 0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15 };
static constexpr int  s_kProDosToDos[16]   = { 0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15 };
static constexpr int  s_kDosToPhysical[16] = { 0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15 };
static constexpr int  s_kNumberings        = 4;

//  The first track of the story, by the interpreter's version.
static constexpr int  s_kStoryTracks[]     = { 3, 4 };





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::InfocomVolume
//
////////////////////////////////////////////////////////////////////////////////

InfocomVolume::InfocomVolume (const vector<Byte> & sectors) :
    m_sectors (sectors)
{
    m_isFound = TryFindStory (sectors, m_story, m_units, m_name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::MakeOrders
//
//  Each skewed order: sector i of a track is the one at a start plus
//  i times an odd step, counted in DOS, physical, ProDOS or inverse physical
//  numbering, and returned as the buffer's DOS logical sectors. The plain
//  order is first.
//
////////////////////////////////////////////////////////////////////////////////

void InfocomVolume::MakeOrders (std::vector<Order> & outOrders)
{
    std::set<Order>  seen;



    outOrders.clear();

    for (int numbering = 0; numbering < s_kNumberings; numbering++)
    {
        for (int step = 1; step < kSectorsPerTrack; step += 2)
        {
            for (int start = 0; start < kSectorsPerTrack; start++)
            {
                Order  order = {};

                for (int i = 0; i < kSectorsPerTrack; i++)
                {
                    int  sector = (start + i * step) % kSectorsPerTrack;

                    order[(size_t) i] = (numbering == 0) ? sector
                                      : (numbering == 1) ? s_kPhysicalToDos[sector]
                                      : (numbering == 2) ? s_kProDosToDos[sector]
                                                         : s_kDosToPhysical[sector];
                }

                if (seen.insert (order).second)
                {
                    outOrders.push_back (order);
                }
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::TryOrder
//
//  The story read from a track on in one order: the header has its
//  length, and its checksum must match.
//
////////////////////////////////////////////////////////////////////////////////

bool InfocomVolume::TryOrder (const vector<Byte> & sectors, int track, const Order & order, vector<Byte> & outStory, vector<size_t> & outUnits)
{
    size_t                  first  = ((size_t) track * kSectorsPerTrack + (size_t) order[0]) * kSectorBytes;
    uint32_t                length = 0;
    size_t                  count  = 0;



    outStory.clear();
    outUnits.clear();

    //  A version 3 interpreter's story is from track 3, a later one's from 4.
    if (first + kSectorBytes > sectors.size() || sectors[first] < 1 || sectors[first] > 8 || (sectors[first] <= 3) != (track == s_kStoryTracks[0]))
    {
        return false;
    }

    //  The length is in the first sector; the whole header is checked once
    //  the story is read.
    length = (uint32_t) ((sectors[first + 0x1A] << 8) | sectors[first + 0x1B]) * (sectors[first] <= 3 ? 2u : sectors[first] <= 5 ? 4u : 8u);
    count  = (length + kSectorBytes - 1) / kSectorBytes;

    if (length <= ZMachineStory::kHeaderBytes || (size_t) track * kSectorsPerTrack + count > (size_t) kTracks * kSectorsPerTrack)
    {
        return false;
    }

    for (size_t at = 0; at < count; at++)
    {
        size_t  unit = ((size_t) track + at / kSectorsPerTrack) * kSectorsPerTrack + (size_t) order[at % kSectorsPerTrack];

        outUnits.push_back (unit);
        outStory.insert (outStory.end(), sectors.begin() + (ptrdiff_t) (unit * kSectorBytes), sectors.begin() + (ptrdiff_t) ((unit + 1) * kSectorBytes));
    }

    outStory.resize (length);

    return ZMachineStory::IsVerifiedStory (outStory);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::TryFindStory
//
////////////////////////////////////////////////////////////////////////////////

bool InfocomVolume::TryFindStory (const vector<Byte> & sectors, vector<Byte> & outStory, vector<size_t> & outUnits, std::string & outName)
{
    std::vector<Order>  orders;



    outStory.clear();
    outUnits.clear();
    outName.clear();

    if (sectors.size() != s_kImageBytes)
    {
        return false;
    }

    MakeOrders (orders);

    for (int track : s_kStoryTracks)
    {
        for (const Order & order : orders)
        {
            if (TryOrder (sectors, track, order, outStory, outUnits))
            {
                outName = std::format ("STORY.Z{}", outStory[0]);
                return true;
            }
        }
    }

    outStory.clear();
    outUnits.clear();

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::IsFound
//
////////////////////////////////////////////////////////////////////////////////

bool InfocomVolume::IsFound (const vector<Byte> & sectors)
{
    vector<Byte>    story;
    vector<size_t>  units;
    std::string     name;



    return TryFindStory (sectors, story, units, name);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::Enumerate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InfocomVolume::Enumerate (VolumeListing & outListing) const
{
    HRESULT    hr = S_OK;
    FileEntry  entry;



    outListing = VolumeListing();

    CBREx (m_isFound, HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME));

    entry.name        = m_name;
    entry.type        = kStoryType;
    entry.eofBytes    = (uint32_t) m_story.size();
    entry.hasEofBytes = true;
    entry.sizeUnits   = (uint32_t) m_units.size();
    entry.isLocked    = true;

    outListing.totalUnits = (uint32_t) (m_sectors.size() / kSectorBytes);
    outListing.freeUnits  = 0;
    outListing.entries.push_back (entry);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::Read
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InfocomVolume::Read (const FilePath & path, FilePayload & outPayload) const
{
    HRESULT  hr      = S_OK;
    bool     isStory = m_isFound && path.GetDepth() == 1 && _stricmp (path.GetLeaf().c_str(), m_name.c_str()) == 0;



    outPayload = FilePayload();

    CBREx (isStory, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    for (size_t unit : m_units)
    {
        if (m_readObserver != nullptr)
        {
            m_readObserver->OnSectorRead (unit);
        }
    }

    outPayload.bytes = m_story;
    outPayload.type  = kStoryType;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::BuildIntegrityReport
//
//  The story's sectors are in use; the rest of the disk is the
//  interpreter's or unused, and isn't checked.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InfocomVolume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    HRESULT   hr    = S_OK;
    uint32_t  units = (uint32_t) (m_sectors.size() / kSectorBytes);



    CBREx (m_isFound, HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME));

    outReport.Reset (units);
    outReport.SetCatalogFullyParsed (true);

    for (size_t unit : m_units)
    {
        outReport.AddClaim ((uint32_t) unit, 0);
    }

    for (uint32_t unit = 0; unit < units; unit++)
    {
        outReport.SetAllocatedInFreeMap (unit, outReport.IsClaimed (unit));
    }

    outReport.Finish();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::Write, Delete, SetStartupProgram, Rename, SetAttributes
//
//  The disk is read-only.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InfocomVolume::Write (const FilePath & path, const FilePayload & payload, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (payload);
    UNREFERENCED_PARAMETER (outBuffer);

    return s_kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InfocomVolume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return s_kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InfocomVolume::Delete (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);
    UNREFERENCED_PARAMETER (outOutcome);

    return s_kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::SetStartupProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InfocomVolume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return s_kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::Rename
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InfocomVolume::Rename (const FilePath & from, const std::string & to, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (from);
    UNREFERENCED_PARAMETER (to);
    UNREFERENCED_PARAMETER (outBuffer);

    return s_kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume::SetAttributes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InfocomVolume::SetAttributes (const FilePath & path, const FileAttributes & attributes, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (attributes);
    UNREFERENCED_PARAMETER (outBuffer);

    return s_kReadOnly;
}

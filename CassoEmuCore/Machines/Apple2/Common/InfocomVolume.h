#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"

class ISectorReadObserver;





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomVolume
//
//  An Infocom game disk, which has no catalog: the interpreter is on the
//  first tracks, and the story file is the rest of the disk from track 3
//  for a version 3 interpreter, or from track 4 for a version 4 or 5 one,
//  as in "An Apple II Build Chain for Inform" (KansasFest). The
//  volume has one file, the story, whose name is STORY.Z and the version.
//
//  Most interpreters read the story's sectors in an interleaved order.
//  The order isn't published, so the story is read in each skewed order,
//  in DOS, physical and ProDOS numbering, and the story is the one
//  whose checksum is the one its header has. A disk whose story verifies
//  in no order isn't an Infocom disk.
//
//  Read-only: every change fails with ERROR_WRITE_PROTECT.
//
////////////////////////////////////////////////////////////////////////////////

class InfocomVolume : public IVolume
{
public:
    static constexpr size_t  kSectorBytes      = 256;
    static constexpr int     kSectorsPerTrack  = 16;
    static constexpr int     kTracks           = 35;
    static constexpr Byte    kStoryType        = 0x00;

    //  The buffer is accessed by reference and isn't modified.
    explicit InfocomVolume (const vector<Byte> & sectors);

    void     SetReadObserver (ISectorReadObserver * observer) { m_readObserver = observer; }

    HRESULT  Enumerate (VolumeListing & outListing) const override;
    HRESULT  Read      (const FilePath & path, FilePayload & outPayload) const override;

    HRESULT  Write     (const FilePath     & path,
                        const FilePayload  & payload,
                        vector<Byte>       & outBuffer) const override;

    HRESULT  Delete    (const FilePath & path, vector<Byte> & outBuffer) const override;

    HRESULT  Delete    (const FilePath  & path,
                        vector<Byte>    & outBuffer,
                        DeleteOutcome   & outOutcome) const override;

    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const override;
    HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const override;

    HRESULT  Rename    (const FilePath     & from,
                        const std::string  & to,
                        vector<Byte>       & outBuffer) const override;

    HRESULT  SetAttributes (const FilePath        & path,
                            const FileAttributes  & attributes,
                            vector<Byte>          & outBuffer) const override;

    //  The story on a 140K buffer in DOS logical order, the buffer's sector
    //  of each of its sectors, and the story's name; false when the story verifies
    //  in no order.
    static bool  TryFindStory (const vector<Byte> & sectors, vector<Byte> & outStory, vector<size_t> & outUnits, std::string & outName);
    static bool  IsFound      (const vector<Byte> & sectors);

private:
    using Order = std::array<int, kSectorsPerTrack>;

    static void  MakeOrders     (std::vector<Order> & outOrders);
    static bool  TryOrder       (const vector<Byte> & sectors, int track, const Order & order, vector<Byte> & outStory, vector<size_t> & outUnits);

    static constexpr HRESULT  s_kReadOnly    = HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT);
    static constexpr size_t   s_kImageBytes  = 143360;

    const vector<Byte>   & m_sectors;
    vector<Byte>           m_story;
    vector<size_t>         m_units;
    std::string            m_name;
    bool                   m_isFound       = false;
    ISectorReadObserver  * m_readObserver  = nullptr;
};

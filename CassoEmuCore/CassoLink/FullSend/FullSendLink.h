#pragma once

#include "Pch.h"

#include "CassoLink/Session/CassoLinkSessionTypes.h"
#include "Machines/Apple2/Common/TrackLayoutWriter.h"

struct FullSendScreen;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendReadReport
//
//  What a disk's read found: the sectors that weren't clean, how many of its
//  tracks were verified, and each track's layout when the link measures.
//
////////////////////////////////////////////////////////////////////////////////

struct FullSendReadReport
{
    std::vector<SectorMark>   marks;
    int                       tracksVerified = 0;
    int                       emptyTracks    = 0;   // tracks where no sector was found
    std::vector<TrackLayout>  layouts;              // each track's, from track 0, or empty
};





////////////////////////////////////////////////////////////////////////////////
//
//  IFullSendLink
//
//  The Apple, as a Full Send uses it. An image is DOS logical sectors in an
//  image-sized buffer. kQuit from any call is Q on the Apple;
//  ERROR_CANCELLED is Cancel on the PC.
//
////////////////////////////////////////////////////////////////////////////////

enum class FullSendKey
{
    Space,
    Quit,
    Drive1,
    Drive2,
    Verify,
    Prefix,
};

class IFullSendLink
{
public:
    //  HRESULT_FROM_WIN32 (ERROR_OPERATION_ABORTED).
    static constexpr HRESULT  kQuit = (HRESULT) 0x800703E3;

    virtual ~IFullSendLink () = default;

    virtual bool     HasDrive   (int drive) = 0;

    //  A track's sectors into the image; outHasDisk is clear for a drive with
    //  no readable disk, and outWhole for a track with an unreadable sector.
    virtual HRESULT  ReadTrack  (int drive, int track, std::vector<Byte> & ioImage, bool & outHasDisk, bool & outWhole) = 0;

    //  Every track but those already in the image; an unreadable sector is left
    //  as zeros and listed by track and DOS 3.3 sector.
    virtual HRESULT  ReadDisk   (int drive, std::vector<Byte> & ioImage, std::span<const int> haveTracks, std::vector<std::pair<int, int>> & outUnreadable, FullSendReadReport & outReport) = 0;
    virtual HRESULT  ShowScreen (const FullSendScreen & screen) = 0;

    //  Whether ReadDisk measures each track's layout, for a WOZ built to the
    //  disk's layout.
    virtual void     SetMeasuring (bool measuring) { UNREFERENCED_PARAMETER (measuring); }

    //  The layouts of a drive's first `tracks` tracks, measured apart from a
    //  read; empty from a link that doesn't measure.
    virtual HRESULT  MeasureLayouts (int drive, int tracks, std::vector<TrackLayout> & outLayouts)
    {
        UNREFERENCED_PARAMETER (drive);
        UNREFERENCED_PARAMETER (tracks);
        outLayouts.clear();
        return S_OK;
    }

    //  No drive turns until a key arrives. The lines are drawn on the Apple until
    //  then.
    virtual HRESULT  WaitForKey (const std::vector<std::string> & lines, FullSendKey & outKey) = 0;

    //  As WaitForKey, with any key's code, as the Apple's keyboard sends it, for
    //  a line typed there.
    virtual HRESULT  WaitForAnyKey (const std::vector<std::string> & lines, Byte & outCode) = 0;

    //  Whether verified reads are on; V on the Apple switches them.
    virtual bool     IsVerifying    () = 0;
    virtual void     ToggleVerify   () = 0;

    //  Whether this PC bootstrapped the Apple, so no CassoLink disk was booted.
    virtual bool     IsBootstrapped () = 0;

    //  A drive's disk captured raw as a WOZ. outReads has every read of the
    //  capture, as CassoLinkCapture::SerializeReads writes them, when reads
    //  are saved, and is empty otherwise. With withEarlier, the WOZ is built
    //  from this capture's reads and those of the drive's capture before it,
    //  which was of the same disk.
    virtual HRESULT  CaptureRaw     (int drive, bool withEarlier, std::vector<Byte> & outWoz, std::vector<Byte> & outReads, std::string & outMarks) = 0;

    //  An image the session wrote, by its path, for the boot check. True
    //  when the image is booted, and AwaitBoot then returns once its result
    //  is in the session's status, or after a time limit.
    virtual bool     NoteImage      (const std::string & path) { UNREFERENCED_PARAMETER (path); return false; }
    virtual HRESULT  AwaitBoot      (const std::string & file) { UNREFERENCED_PARAMETER (file); return S_OK; }

    //  Whether the Apple sounds a tone for an image's boot result.
    virtual bool     HasBootTones   () { return false; }

    //  Whether the disk in a drive was write-protected when it was last read.
    virtual bool     IsWriteProtected (int drive) { UNREFERENCED_PARAMETER (drive); return false; }

    //  The Apple's model, as Apple //c; empty when it isn't known.
    virtual std::string  GetModelName () { return std::string(); }

    //  A line for the activity log.
    virtual void     Report     (const std::string & text) = 0;
};

#pragma once

#include "Pch.h"

class FullSendContext;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendCheckedDisk
//
//  A drive's disk as a check read it. The tracks read whole are not read a
//  third time when the disk is sent, as each track read costs most of a
//  second.
//
////////////////////////////////////////////////////////////////////////////////

enum class FullSendDiskCheck
{
    None,
    New,
    Repeat,
    Blank,      // formatted, with no files; not sent unless its drive's number is pressed
};

struct FullSendCheckedDisk
{
    FullSendDiskCheck  check = FullSendDiskCheck::None;
    std::string        fingerprint;
    std::vector<Byte>  image;
    std::vector<int>   wholeTracks;
    std::string        system;          // a blank disk's file system

    bool  IsReadWhole (int track) const { return std::find (wholeTracks.begin(), wholeTracks.end(), track) != wholeTracks.end(); }
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendDriveCheck
//
//  Each drive's check: whether its disk is new, a repeat of an image in the
//  folder, or blank, from a few of its tracks.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendDriveCheck
{
public:
    explicit FullSendDriveCheck (FullSendContext & context);

    HRESULT  Check       (int drive, FullSendCheckedDisk & outDisk);
    void     NoteSent    (int drive, const std::string & fingerprint) { GetRecord (drive).counted = fingerprint; }
    void     NoteChanged (const std::string & fingerprint)            { m_changed.insert (fingerprint); }
    void     ClearRepeat (int drive)                                  { GetRecord (drive).repeat = false; }
    bool     IsRepeat    (int drive) const                            { return GetRecord (drive).repeat; }
    bool     IsSwapped   (int drive) const                            { return GetRecord (drive).swapped; }
    bool     IsBlank     (int drive) const                            { return GetRecord (drive).blank; }
    bool     HasRepeat   () const                                     { return m_drives[0].repeat || m_drives[1].repeat; }

private:
    //  What the last check of a drive found.
    struct DriveRecord
    {
        bool         repeat  = false;   // its disk matched the manifest, or was blank
        bool         swapped = false;   // and of those, a disk that changed during its read
        bool         blank   = false;   // its disk was blank
        std::string  counted;           // the repeat it last added to the duplicates
        std::string  blankDisk;         // the blank it last added to the blanks
    };

    static constexpr int  kFingerprintReads = 3;

    HRESULT              ReadFingerprint (int drive, FullSendCheckedDisk & outDisk, bool & outStable);
    HRESULT              ReadStableTrack (int drive, int track, FullSendCheckedDisk & ioDisk, bool & outHasDisk, bool & outStable);
    HRESULT              FindRepeat      (FullSendCheckedDisk & ioDisk, bool & outKnown);
    void                 CountRepeat     (int drive, const FullSendCheckedDisk & disk, bool swapped);
    void                 ReportRepeat    (int drive, const FullSendCheckedDisk & disk);
    HRESULT              CheckBlank      (int drive, FullSendCheckedDisk & ioDisk);
    HRESULT              ShowResult      (int drive, const FullSendCheckedDisk & disk);
    DriveRecord        & GetRecord       (int drive)       { return m_drives[(size_t) drive - 1]; }
    const DriveRecord  & GetRecord       (int drive) const { return m_drives[(size_t) drive - 1]; }

    FullSendContext             & m_context;
    std::array<DriveRecord, 2>    m_drives;
    std::set<std::string>         m_changed;    // fingerprints of disks that changed during a read, sent only after Space
};

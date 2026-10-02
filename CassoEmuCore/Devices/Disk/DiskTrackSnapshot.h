#pragma once

#include "Pch.h"

class DiskImage;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskTrackSnapshot
//
//  The media of one DiskImage at one moment, for reverse-execution keyframes.
//  Track buffers are copy-on-write between snapshots: Capture given the
//  previous keyframe's snapshot of the same medium shares every track whose
//  bits have not changed since, so a keyframe costs only the tracks the guest
//  wrote (about 6.5 KB each) rather than the whole disk.
//
//  Restore puts the tracks, their lengths and dirty flags, the image dirty
//  flag and the write-protect flags back, so stepping back across a guest
//  write leaves the disk as it was at that position. It writes nothing to the
//  host file; when the file is written is the store's business.
//
//  Buffers are immutable once captured and shared by reference count, so a
//  snapshot can be dropped in any order. Capture and Restore run on the
//  thread that owns the disk's writes (the CPU thread).
//
////////////////////////////////////////////////////////////////////////////////

class DiskTrackSnapshot
{
public:
    void     Capture           (const DiskImage & disk, const DiskTrackSnapshot * previous);
    HRESULT  Restore           (DiskImage & disk) const;

    int      GetTrackCount     () const { return static_cast<int> (m_tracks.size()); }
    bool     IsLoaded          () const { return m_loaded; }

    // True when this snapshot and `other` hold the same buffer for `track`,
    // which is how a keyframe avoids copying an unchanged track.
    bool     IsTrackSharedWith (int track, const DiskTrackSnapshot & other) const;

private:
    struct Track
    {
        shared_ptr<const vector<Byte>>  bits;
        size_t                          bitCount   = 0;
        uint64_t                        generation = 0;
        bool                            dirty      = false;
    };

    uint64_t       m_imageId             = 0;
    vector<Track>  m_tracks;
    bool           m_loaded              = false;
    bool           m_dirty               = false;
    bool           m_imageWriteProtected = false;
    bool           m_userWriteProtected  = false;
};

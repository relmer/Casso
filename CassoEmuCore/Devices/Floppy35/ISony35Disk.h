#pragma once

#include "Pch.h"

struct ReplayBlockMedia;
class StateReader;
class StateWriter;





////////////////////////////////////////////////////////////////////////////////
//
//  ISony35Disk
//
//  The disk in a Sony35Drive: a block image, whose tracks are GCR encoded
//  from its blocks and decoded back on Flush, or a WOZ 2 image, whose tracks
//  are its bit streams. A track's key is track * 2 + side.
//
////////////////////////////////////////////////////////////////////////////////

class ISony35Disk
{
public:
    using Tracks = std::map<int, std::vector<Byte>>;

    virtual ~ISony35Disk() = default;

    virtual int                  GetSides         () const = 0;
    virtual bool                 IsWriteProtected () const = 0;
    virtual bool                 IsWoz            () const = 0;
    virtual const std::string  & GetPath          () const = 0;

    //  The bit stream of a track and side, from the disk.
    virtual void     ReadTrack     (int track, int side, std::vector<Byte> & outBits) = 0;

    //  Puts the tracks in `dirty` back on the disk, empties `dirty`, and
    //  writes the disk to its file.
    virtual HRESULT  Flush         (const Tracks & tracks, std::set<int> & dirty) = 0;

    //  Releases the disk's file, after a flush.
    virtual HRESULT  Detach        () = 0;

    virtual HRESULT  SaveState     (StateWriter & writer) const = 0;
    virtual HRESULT  LoadState     (StateReader & reader) = 0;
    virtual HRESULT  CopyForReplay (ReplayBlockMedia & outCopy) = 0;
};

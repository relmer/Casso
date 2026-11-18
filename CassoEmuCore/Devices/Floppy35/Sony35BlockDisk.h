#pragma once

#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/Floppy35/ISony35Disk.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35BlockDisk
//
//  A block image of 800K, two-sided, or 400K, one-sided. A track is built
//  from its blocks the first time the head is on it, and a written track is
//  decoded back to its blocks on Flush.
//
////////////////////////////////////////////////////////////////////////////////

class Sony35BlockDisk : public ISony35Disk
{
public:
    static constexpr int  kSides = 2;

    //  Any size other than 800K or 400K fails, with the drive left empty.
    HRESULT  Attach          (IDiskFileIo & fileIo, const std::string & path, const WriteProtectInfo & hostProtect, std::wstring & outError);
    HRESULT  MountReplayCopy (const ReplayBlockMedia & copy);

    int                  GetSides         () const override { return m_sides; }
    bool                 IsWriteProtected () const override { return m_media.IsWriteProtected(); }
    bool                 IsWoz            () const override { return false; }
    const std::string  & GetPath          () const override { return m_media.GetPath(); }

    void     ReadTrack     (int track, int side, std::vector<Byte> & outBits) override;
    HRESULT  Flush         (const Tracks & tracks, std::set<int> & dirty) override;
    HRESULT  Detach        () override { return m_media.Detach(); }
    HRESULT  SaveState     (StateWriter & writer) const override;
    HRESULT  LoadState     (StateReader & reader) override { return m_media.LoadState (reader); }
    HRESULT  CopyForReplay (ReplayBlockMedia & outCopy) override { return m_media.CopyForReplay (outCopy); }

private:
    BlockDeviceMedia  m_media;
    int               m_sides = kSides;
};

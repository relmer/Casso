#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/IDiskImage.h"
#include "Devices/Floppy35/ISony35Disk.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35WozDisk
//
//  A WOZ 2 image of a 3.5-inch disk. A track's bits are the WOZ track at TMAP
//  entry track * 2 + side, and Flush writes the written tracks back into the
//  WOZ file. A replay's copy has no file and writes nothing.
//
////////////////////////////////////////////////////////////////////////////////

class Sony35WozDisk : public ISony35Disk
{
public:
    //  Write protected when INFO marks it or the host file is read only.
    //  `fileIo` is null for a replay's copy.
    HRESULT  Load (IDiskFileIo * fileIo, const std::string & path, const std::vector<Byte> & bytes, const WriteProtectInfo & hostProtect, std::wstring & outError);

    int                  GetSides         () const override { return m_sides; }
    bool                 IsWriteProtected () const override { return m_isProtected; }
    bool                 IsWoz            () const override { return true; }
    const std::string  & GetPath          () const override { return m_path; }

    void     ReadTrack     (int track, int side, std::vector<Byte> & outBits) override;
    HRESULT  Flush         (const Tracks & tracks, std::set<int> & dirty) override;
    HRESULT  Detach        () override { return S_OK; }
    HRESULT  SaveState     (StateWriter & writer) const override;
    HRESULT  LoadState     (StateReader & reader) override;
    HRESULT  CopyForReplay (ReplayBlockMedia & outCopy) override;

private:
    DiskImage                                 m_image;
    std::shared_ptr<const std::vector<Byte>>  m_bytes;
    IDiskFileIo                             * m_fileIo      = nullptr;
    std::string                               m_path;
    bool                                      m_isProtected = false;
    int                                       m_sides       = 2;
};

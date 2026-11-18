#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolume
//
//  One file system of a hybrid disk: a 140K buffer with a DOS 3.3 volume and
//  a ProDOS or Pascal volume on it, each with the other's area marked in use.
//  Reads go to the volume as they are. A write succeeds only when the sectors
//  of the other file system's files and structures are the same bytes after
//  it, so a damaged free map on one side cannot overwrite the other.
//
//  The other side's free map is not the test: each map has the other's whole
//  area in use, free sectors included.
//
////////////////////////////////////////////////////////////////////////////////

class HybridVolume : public IVolume
{
public:
    //  `sectors` is the buffer `volume` reads and must outlive this object.
    HybridVolume (const vector<Byte> & sectors, std::unique_ptr<IVolume> volume, VolumeKind other);

    HRESULT  Enumerate            (VolumeListing & outListing) const override;
    HRESULT  EnumerateDirectory   (const FilePath & directory, VolumeListing & outListing) const override;
    HRESULT  Read                 (const FilePath & path, FilePayload & outPayload) const override;
    HRESULT  CreateDirectory      (const FilePath & path, vector<Byte> & outBuffer) const override;
    HRESULT  BuildRemovalPlan     (const FilePath & path, DirectoryRemovalPlan & outPlan) const override;
    HRESULT  RemoveDirectory      (const FilePath & path, bool force, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const override;
    HRESULT  Write                (const FilePath & path, const FilePayload & payload, vector<Byte> & outBuffer) const override;
    HRESULT  Delete               (const FilePath & path, vector<Byte> & outBuffer) const override;
    HRESULT  Delete               (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const override;
    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const override;
    HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const override;
    HRESULT  Rename               (const FilePath & from, const std::string & to, vector<Byte> & outBuffer) const override;
    HRESULT  SetAttributes        (const FilePath & path, const FileAttributes & attributes, vector<Byte> & outBuffer) const override;

    //  E_UNEXPECTED when `edited` differs from `before` in a sector of a file
    //  or structure of the `other` file system on `before`.
    static HRESULT  CheckWrite (const vector<Byte> & before, const vector<Byte> & edited, VolumeKind other);

private:
    static HRESULT  MarkSectors    (const vector<Byte> & sectors, VolumeKind other, std::vector<bool> & ioMarked);
    static void     MarkDosCatalog (const vector<Byte> & sectors, std::vector<bool> & ioMarked);
    static void     MarkBlock      (const vector<Byte> & sectors, uint32_t block, std::vector<bool> & ioMarked);

    HRESULT  CheckResult (HRESULT hrWrite, vector<Byte> & ioBuffer) const;

    const vector<Byte>      & m_sectors;
    std::unique_ptr<IVolume>  m_volume;
    VolumeKind                m_other = VolumeKind::Unknown;
};

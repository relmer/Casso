#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/IVolume.h"
#include "Devices/Disk/Archives/NuFxArchive.h"
#include "Devices/Disk/Archives/BinaryTwoArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveVolume
//
//  A NuFX or Binary II archive as a volume: its records listed as files in
//  the folders their names have, read, written, renamed and deleted, each
//  write returning the archive built again. A Binary II file of one NuFX
//  archive, a `.bxy`, is the NuFX archive, written back inside its Binary II
//  header. NuFX has no directory records, so a folder is there while a file
//  is in it.
//
////////////////////////////////////////////////////////////////////////////////

class ArchiveVolume : public IVolume
{
public:
    explicit ArchiveVolume (const vector<Byte> & bytes);

    static bool  IsArchive (const vector<Byte> & bytes);

    //  Whether the archive parsed, and why not.
    HRESULT  GetParseResult (std::string & outError) const { outError = m_error; return m_parsed; }

    HRESULT  Enumerate            (VolumeListing & outListing) const override;
    HRESULT  EnumerateDirectory   (const FilePath & directory, VolumeListing & outListing) const override;
    HRESULT  Read                 (const FilePath & path, FilePayload & outPayload) const override;
    HRESULT  Write                (const FilePath & path, const FilePayload & payload, vector<Byte> & outBuffer) const override;
    HRESULT  Delete               (const FilePath & path, vector<Byte> & outBuffer) const override;
    HRESULT  Delete               (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const override;
    HRESULT  CreateDirectory      (const FilePath & path, vector<Byte> & outBuffer) const override;
    HRESULT  BuildRemovalPlan     (const FilePath & path, DirectoryRemovalPlan & outPlan) const override;
    HRESULT  RemoveDirectory      (const FilePath & path, bool force, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const override;
    HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const override;
    HRESULT  SetStartupProgram    (const FilePath & path, vector<Byte> & outBuffer) const override;
    HRESULT  Rename               (const FilePath & from, const std::string & to, vector<Byte> & outBuffer) const override;
    HRESULT  SetAttributes        (const FilePath & path, const FileAttributes & attributes, vector<Byte> & outBuffer) const override;

    //  Every record whose CRC failed, by name, for Test.
    void     CollectDamage (std::vector<std::string> & outDamage) const;

private:
    static constexpr Byte  kDefaultAccess = 0xE3;

    static std::vector<std::string>  SplitName      (const std::string & name, char separator);
    static std::string               JoinName       (const std::vector<std::string> & components);
    static bool                      IsSameName     (const std::string & a, const std::string & b);
    static void                      ToProDosDate   (const std::array<Byte, 8> & when, Word & outDate, Word & outTime);
    static std::array<Byte, 8>       FromProDosDate (Word date, Word time);

    std::vector<std::string>  GetPath   (size_t index) const;
    size_t                    GetCount  () const;
    bool                      TryFind   (const FilePath & path, size_t & outIndex) const;
    FileEntry                 ToEntry   (size_t index, const std::string & leaf) const;
    void                      BuildBack (const NuFxArchive & nufx, const std::vector<BinaryTwoEntry> & bny, vector<Byte> & outBuffer) const;

    bool                         m_isNuFx       = false;
    bool                         m_wrappedInBny = false;
    HRESULT                      m_parsed       = S_OK;
    std::string                  m_error;
    NuFxArchive                  m_nufx;
    std::vector<BinaryTwoEntry>  m_bny;
};

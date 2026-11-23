#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/Dos33Catalog.h"
#include "Machines/Apple2/Common/Dos33FileChain.h"
#include "Machines/Apple2/Common/Dos33FreeMap.h"
#include "Machines/Apple2/Common/Dos33IntegrityScanner.h"
#include "Machines/Apple2/Common/Dos33SectorAccess.h"
#include "Machines/Apple2/Common/Dos33StartupProgram.h"
#include "Machines/Apple2/Common/Dos33VolumeRemover.h"
#include "Machines/Apple2/Common/Dos33VolumeWriter.h"
#include "Machines/Apple2/Common/IVolume.h"

class ISectorReadObserver;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33Volume
//
//  A DOS 3.3 filesystem over a flat sector buffer.
//
//  Nothing like this existed: the tree had a ProDOS reader and most of a ProDOS
//  writer, and on the DOS 3.3 side only a routine that emits one hardcoded
//  greeting file. Extraction is the first thing a developer migrating off Apple
//  II disks needs, so this is the reader that story rests on.
//
//  The structures are simple by comparison -- a flat catalog chained through
//  track 17, a track/sector list per file, a free bitmap in the VTOC, no
//  subdirectories and no tree. Geometry constants come from Dos33Skeleton
//  rather than being restated here, so the format is described in one place.
//
//  The listing and reads are here. The catalog, the track/sector lists, the
//  free bitmap, the integrity pass, deletion, placement and the startup
//  program are separate components over one Dos33SectorAccess.
//
////////////////////////////////////////////////////////////////////////////////

class Dos33Volume : public IVolume
{
public:
    //  Holds the buffer by reference and never modifies it. Every mutating
    //  operation produces a complete new buffer instead.
    explicit Dos33Volume (const vector<Byte> & sectors);

    //  The components have references to m_access and to each other.
    Dos33Volume             (const Dos33Volume &) = delete;
    Dos33Volume & operator= (const Dos33Volume &) = delete;

    //  Called with each sector the volume reads from here on; null for none.
    void     SetReadObserver (ISectorReadObserver * observer) { m_access.SetReadObserver (observer); }

    HRESULT  Enumerate (VolumeListing & outListing) const override;
    HRESULT  Read      (const FilePath & path, FilePayload & outPayload) const override;

    HRESULT  Write     (const FilePath     & path,
                        const FilePayload  & payload,
                        vector<Byte>       & outBuffer) const override;

    HRESULT  Delete    (const FilePath & path, vector<Byte> & outBuffer) const override;

    //  Delete, plus an account of what it freed, what it refused to free, and
    //  the conditions that bound the answer. The interface form forwards here
    //  and throws the account away; anything reporting to a user wants it.
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

    //  DOS 3.3 file types, as stored in the catalog's type byte below the lock
    //  bit. Exposed because callers name types on the command line.
    static constexpr Byte  kTypeText        = 0x00;
    static constexpr Byte  kTypeInteger     = 0x01;
    static constexpr Byte  kTypeApplesoft   = 0x02;
    static constexpr Byte  kTypeBinary      = 0x04;
    static constexpr Byte  kTypeSpecial     = 0x08;
    static constexpr Byte  kTypeRelocatable = 0x10;

    //  Set in the type byte when the file is locked. Placement over a locked
    //  file is refused, matching how the guest behaves.
    static constexpr Byte  kLockedBit       = 0x80;

    static constexpr uint32_t  kUnitCount = 560;   // 35 tracks x 16 sectors

private:
    Dos33SectorAccess      m_access;
    Dos33Catalog           m_catalog { m_access };
    Dos33FileChain         m_chain   { m_access };
    Dos33FreeMap           m_freeMap { m_access };
    Dos33IntegrityScanner  m_scanner { m_access };
    Dos33VolumeRemover     m_remover { m_access };
    Dos33VolumeWriter      m_writer  { m_access, m_remover };
    Dos33StartupProgram    m_startup { m_access };
};

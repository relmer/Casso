#pragma once

#include "Pch.h"

#include "DiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageStore
//
//  Mount/Eject coordinator. Owns the DiskImage instances mounted across
//  every (slot, drive) pair. Routes Mount() requests by file extension:
//      .dsk → NibblizationLayer (DOS 3.3 sector order)
//      .do  → NibblizationLayer (DOS 3.3 sector order)
//      .po  → NibblizationLayer (ProDOS sector order)
//      .woz → WozLoader (native bit-stream)
//
//  Auto-flush invariants (FR-025):
//      Eject(slot, drive) — flush dirty image, then release.
//      FlushAll()         — flush every dirty mount; called on machine
//                           switch and on emulator exit / PowerCycle.
//      SoftReset()        — keep mounts; flush dirty (Phase 4 contract).
//      PowerCycle()       — unmount everything (auto-flush each).
//
//  Test hook: SetFlushSink lets tests redirect serialized bytes to an
//  in-memory capture buffer instead of the host filesystem so the
//  IFixtureProvider isolation contract is preserved.
//
//  Flush-error reporting: a flush that was supposed to persist a dirty
//  image but couldn't (serialize failure, or the file/sink write failed)
//  used to vanish -- every caller drops FlushEntry's HRESULT (Eject and
//  PowerCycle are void; the shell/SoftReset IGNORE_RETURN_VALUE it), so a
//  user's writes could be silently lost. SetFlushErrorReporter installs a
//  callback that FlushEntry invokes on genuine failure, so the loss is
//  surfaced (the shell logs + notifies the user) regardless of what the
//  caller does with the return. Kept UI-agnostic by injection, like
//  SetFlushSink, so it is headlessly testable.
//
////////////////////////////////////////////////////////////////////////////////

class DiskImageStore
{
public:
    using FlushSink          = std::function<HRESULT (const string &, const vector<Byte> &)>;
    using FlushErrorReporter = std::function<void (const string &, HRESULT)>;

    static constexpr int   kSlotCount  = 8;
    static constexpr int   kDriveCount = 2;

    DiskImageStore ();

    HRESULT       Mount             (int slot, int drive, const string & path);
    HRESULT       MountFromBytes    (int slot, int drive, const string & virtualPath,
                                     DiskFormat fmt, const vector<Byte> & bytes);
    void          Eject             (int slot, int drive);
    HRESULT       Flush             (int slot, int drive);
    HRESULT       FlushAll          ();
    void          SoftReset         ();
    void          PowerCycle        ();

    DiskImage *   GetImage          (int slot, int drive);
    bool          IsMounted         (int slot, int drive) const;
    const string &GetSourcePath     (int slot, int drive) const;

    void          SetFlushSink      (FlushSink sink) { m_flushSink = std::move (sink); }
    void          SetFlushErrorReporter (FlushErrorReporter reporter) { m_flushErrorReporter = std::move (reporter); }

    static HRESULT  DetectFormatByExtension (const string & path, DiskFormat & outFmt);

private:
    struct Entry
    {
        unique_ptr<DiskImage>  image;
        string                 path;
        DiskFormat             format = DiskFormat::Dsk;
        bool                   mounted = false;
    };

    Entry &       At                (int slot, int drive);
    const Entry & At                (int slot, int drive) const;
    HRESULT       FlushEntry        (Entry & entry);

    Entry              m_entries[kSlotCount][kDriveCount];
    FlushSink          m_flushSink;
    FlushErrorReporter m_flushErrorReporter;
    string             m_emptyPath;
};

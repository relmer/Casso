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
//  user's writes could be silently lost. FlushEntry now surfaces the loss
//  itself through the shared EHM notifier (CHRN/CBRN -> EhmNotifyUser),
//  which routes to whatever handler the host registered (a MessageBox in
//  the GUI, stderr headless), so the report reaches the user regardless of
//  what the caller does with the return.
//
////////////////////////////////////////////////////////////////////////////////

class DiskImageStore
{
public:
    using FlushSink = std::function<HRESULT (const string &, const vector<Byte> &)>;

    static constexpr int   kSlotCount  = 8;
    static constexpr int   kDriveCount = 2;

    DiskImageStore ();

    HRESULT       Mount             (int slot, int drive, const string & path);
    HRESULT       MountFromBytes    (int slot, int drive, const string & virtualPath,
                                     DiskFormat fmt, const vector<Byte> & bytes);
    void          Eject             (int slot, int drive);
    HRESULT       Flush             (int slot, int drive);
    HRESULT       FlushAll          ();

    //  Serializer-level persist of the image's CURRENT state, regardless of
    //  the dirty bit or the image's write-protect flag (that flag gates
    //  guest writes, not host persistence). The write-protect toggle uses
    //  this to land the flipped WOZ INFO flag in the backing file.
    HRESULT       ForceFlush        (int slot, int drive);
    void          SoftReset         ();
    void          PowerCycle        ();

    DiskImage *   GetImage          (int slot, int drive);
    bool          IsMounted         (int slot, int drive) const;
    const string &GetSourcePath     (int slot, int drive) const;

    //  One mounted image's backing path with its bay, for consumers that need
    //  the full mounted set (the create dialog's mounted-target refusal).
    struct MountedSource
    {
        string  path;
        int     slot  = 0;
        int     drive = 0;
    };

    std::vector<MountedSource>  MountedSourcePaths() const;

    void          SetFlushSink      (FlushSink sink) { m_flushSink = std::move (sink); }

    static HRESULT  DetectFormatByExtension (const string & path, DiskFormat & outFmt);

    //  Where a recovery image goes for the Nth attempt, when an image cannot be
    //  written back to its own format without loss. Public because the naming
    //  and never-overwrite policy are part of the observable contract, not an
    //  implementation detail -- and pure, so both are testable without a disk.
    static string   MakeRecoveryPath        (const string & imagePath, int attempt);

private:
    struct Entry
    {
        unique_ptr<DiskImage>  image;
        string                 path;
        DiskFormat             format  = DiskFormat::Dsk;
        bool                   mounted = false;
    };

    // Every public accessor takes a caller-supplied slot/drive pair, so each
    // one range-checks before At() indexes the fixed array.
    static bool   IsValidBay        (int slot, int drive);

    Entry &       At                (int slot, int drive);
    const Entry & At                (int slot, int drive) const;
    HRESULT       FlushEntry        (Entry & entry, bool force = false);

    // Builds the user-facing "could not save" message from the mount path;
    // handed to CHRN/CBRN in FlushEntry on a genuine persist failure. When a
    // recovery image was written, its path is named so the user can retrieve
    // the session rather than only being told what was lost.
    static wstring FormatFlushLossMessage (const string & path, const string & recoveryPath);

    // Preserves an image that could not be serialized to its own format,
    // beside the original and losslessly. Leaves the original untouched.
    HRESULT        TryWriteRecoveryImage  (Entry & entry, string & outPath);

    // Enough room to step past collisions without ever spinning.
    static constexpr int  kMaxRecoveryNameAttempts = 64;

    Entry      m_entries[kSlotCount][kDriveCount];
    FlushSink  m_flushSink;
    string     m_emptyPath;
};

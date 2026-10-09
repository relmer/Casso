#pragma once

#include "Pch.h"

#include "IDiskImage.h"
#include "FluxTrack.h"
#include "Machines/Apple2/Common/WozMetadata.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackKind
//
//  How a storage slot holds its track. A bit track is a loop of equal-length
//  cells; a flux track is a timeline of transitions that keeps how long each
//  cell actually was. A WOZ 2.1 image can hold both.
//
////////////////////////////////////////////////////////////////////////////////

enum class TrackKind
{
    Bits,
    Flux,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DamageReason / DamagedTrack
//
//  Why a track could not be read from its file. The image still mounts, read
//  only, with the track blank, and the mount report lists it.
//
////////////////////////////////////////////////////////////////////////////////

enum class DamageReason
{
    OutsideFile,           // the track's blocks lie outside the file
    CountExceedsBlocks,    // its bit or byte count needs more than its blocks hold
    TruncatedRun,          // flux data ending in a 255 with nothing to end the run
    V1RecordPastTrks,      // a WOZ 1 track record past the end of TRKS
};


struct DamagedTrack
{
    int           trkIndex = 0;
    bool          isFlux   = false;
    DamageReason  reason   = DamageReason::OutsideFile;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IPendingWriteOwner
//
//  Whoever is holding a write to this image that has not reached it yet -- a
//  drive in the middle of writing a flux track. A flush asks it to finish
//  first, or the image would be saved without the write.
//
////////////////////////////////////////////////////////////////////////////////

class IPendingWriteOwner
{
public:
    virtual       ~IPendingWriteOwner () = default;
    virtual void  CommitPendingWrite  () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImage
//
//  Concrete IDiskImage. Holds up to 40 variable-length per-track bit
//  streams plus per-track dirty bits and a single image-level write-
//  protect flag. Tracks are bit-packed: 8 bits per byte, MSB-first.
//
//  Phase 9 scope: bit-stream storage + the soft sector-level Load that
//  preserves Phase 8 runtime behavior (.dsk → 6+2 nibblization → packed
//  bit stream). The full WOZ / .DO / .PO loaders + true round-trip
//  Serialize land in Phase 10 (NibblizationLayer / WozLoader).
//
////////////////////////////////////////////////////////////////////////////////

class DiskImage : public IDiskImage
{
public:
    static constexpr int     kMaxTracks                  = 40;
    static constexpr int     kDefaultTrackCount          = 35;
    static constexpr size_t  kDefaultTrackByteSize       = 6400;
    static constexpr size_t  kDos33ImageSize             = 143360;
    static constexpr int     kQuarterTrackCount          = 160;
    static constexpr int     kQuarterTracksPerWholeTrack = 4;

    DiskImage ();

    int           GetTrackCount     () const override;
    size_t        GetTrackBitCount  (int track) const override;
    uint8_t       ReadBit           (int track, size_t bitIndex) const override;
    void          WriteBit          (int track, size_t bitIndex, uint8_t bit) override;
    bool          IsDirty           () const override;
    bool          IsWriteProtected  () const override;
    DiskFormat    GetSourceFormat   () const override;
    HRESULT       Serialize         (vector<Byte> & outBytes) const override;

    // Backward-compat / lifecycle surface used by Disk2Controller and tests.
    HRESULT          Load                (const string & filePath);
    void             Eject               ();
    HRESULT          Flush               ();
    bool             IsLoaded            () const { return m_loaded; }

    // Write-protect sources. The image flag comes from the loaded image
    // (WOZ INFO chunk); the user flag from the Settings / menu toggle;
    // the file-state flags from the host filesystem probe at mount. Any
    // one of them protects the disk (IsWriteProtected == OR of all four).
    // SetWriteProtected is retained as an alias for the image flag so the
    // WOZ loader and legacy tests keep working.
    void             SetWriteProtected      (bool wp) { m_imageWriteProtected = wp; }
    void             SetImageWriteProtected (bool wp) { m_imageWriteProtected = wp; }
    void             SetUserWriteProtected  (bool wp) { m_userWriteProtected  = wp; }
    void             SetFileWriteProtect    (bool readOnly, bool noPermission)
    {
        m_fileReadOnly     = readOnly;
        m_fileNoPermission = noPermission;
    }

    // A WOZ whose stored CRC did not match its contents at load. The image
    // still loads -- a damaged preservation dump is exactly what a user needs
    // to be able to open -- but re-serializing it stamps a freshly computed
    // CRC, which would leave the damage undetectable. The flag rides along so
    // a flush can say so before it does that, and clears once the file has
    // been rewritten and its stored CRC is valid again.
    void             SetSourceCrcMismatch   (bool bad) { m_sourceCrcMismatch = bad; }
    bool             HasSourceCrcMismatch   () const { return m_sourceCrcMismatch; }

    // Tracks that could not be read from the file at load. Like a checksum
    // mismatch, any of them holds the image read-only: rewriting the file
    // would replace the damaged tracks with blank ones and hide the damage.
    void                         AddDamagedTrack     (const DamagedTrack & track) { m_damagedTracks.push_back (track); }
    const vector<DamagedTrack> & GetDamagedTracks    () const { return m_damagedTracks; }
    bool                         HasDamagedTracks    () const { return !m_damagedTracks.empty(); }

    // Either kind of damage: what read-only enforcement, salvage and the
    // mount report all key off.
    bool                         IsDamaged           () const { return m_sourceCrcMismatch || HasDamagedTracks(); }

    // The parts of a source WOZ the track model cannot express -- the INFO
    // chunk's non-geometry fields and every chunk Casso does not parse (META
    // above all). The writer rebuilds INFO/TMAP/TRKS from the live tracks,
    // which is what makes guest writes survive, so anything the model does
    // not hold has to be retained here or the first flush drops it. Empty
    // for a synthesized image, which is also how the writer tells a disk
    // Casso authored from one it merely edited.
    void                  SetWozMetadata         (const WozMetadata & meta) { m_wozMetadata = meta; }
    const WozMetadata &   GetWozMetadata         () const { return m_wozMetadata; }

    bool             IsImageWriteProtected  () const { return m_imageWriteProtected; }
    bool             IsUserWriteProtected   () const { return m_userWriteProtected;  }
    WriteProtectInfo GetWriteProtectInfo    () const;

    const string &   GetFilePath         () const { return m_filePath; }
    void             SetSourceFormat     (DiskFormat fmt) { m_format = fmt; }
    bool             IsTrackDirty        (int track) const;
    void             ClearDirty          ();

    //  Records that a track's bits were replaced wholesale, which the bulk
    //  writers below must do because they bypass WriteBit and so bypass the
    //  bookkeeping it does. A consumer that copies clean tracks and re-derives
    //  dirty ones otherwise sees a rewritten track as untouched.
    void             MarkTrackDirty      (int track);
    void             ResizeTrack         (int track, size_t bitCount);

    // Quarter-track addressing. The head physically steps in quarter-track
    // increments (0..159); ResolveQuarterTrack maps a head position to the
    // backing storage slot that holds its bit stream (-1 == unformatted /
    // no data). Standard sector images map every quarter-track to its whole
    // track (qt / 4); WOZ images install an explicit map from the TMAP so
    // half/quarter-track-formatted protections resolve to distinct streams.
    int              ResolveQuarterTrack (int quarterTrack) const;

    // The slot the map gives a quarter track, whether or not that slot holds
    // data -- which a report about tracks that failed to load needs, since a
    // damaged slot resolves to nothing.
    int              GetMappedSlot       (int quarterTrack) const;

    // Flux slots. A slot is a flux track when the image's FLUX map refers to
    // it; its bit buffer then stays empty and every bit-level accessor sees
    // no data, so only code that asks for the flux reads it.
    TrackKind           GetTrackKind         (int slot) const;
    void                SetFluxTrack         (int slot, const vector<Byte> & fluxBytes);
    const FluxTrack  &  GetFluxTrack         (int slot) const { return m_fluxTracks[slot]; }
    bool                HasFluxTracks        () const;

    // Puts a write into a flux track at the controller's cell timing and
    // marks the track dirty. Does nothing on a write-protected image.
    void                SpliceFluxWrite      (int slot, uint64_t startTick, const vector<uint8_t> & bits);

    // The bulk-writer counterpart, for sector edits made to an image in
    // memory. Like GetTrackBitsForWrite, it bypasses write-protect.
    void                SpliceFluxBulk       (int slot, uint64_t startTick, const vector<uint8_t> & bits);

    // Changes whenever what a quarter track resolves to, or a flux track's
    // bytes, may have changed. A reader that caches a resolved slot or a
    // flux cursor compares it to know when to look again.
    uint64_t            GetLayoutGeneration  () const { return m_layoutGeneration; }

    // A drive holding an unfinished write registers itself here, and clears
    // itself once the write is in. Flush and Eject finish it first.
    void                SetPendingWriteOwner (IPendingWriteOwner * owner) { m_pendingWriteOwner = owner; }
    void                CommitPendingWrite   ();
    void             ClearQuarterTrackMap ();
    void             SetQuarterTrackSlot (int quarterTrack, int slot);
    void             EnsureTrackSlots    (int slotCount);

    // Direct bit-buffer access for bulk writers (NibblizationLayer, WozLoader).
    // ResizeTrack must be called first; the returned buffer length matches the
    // packed-byte size for the track. Bypasses write-protect.
    vector<Byte> &   GetTrackBitsForWrite (int track) { return m_trackBits[track]; }

    // Const counterpart for read-only serializers (WozLoader::Serialize).
    // Returns the packed MSB-first bit bytes backing a track slot.
    const vector<Byte> & GetTrackBits (int track) const { return m_trackBits[track]; }
    void             SetTrackBitCount    (int track, size_t bitCount);
    void             LoadFromBytes       (DiskFormat fmt, const vector<Byte> & raw, const string & sourcePath);

    // Test-only injection. Marks the image as loaded/dirty without touching
    // the host filesystem so reset-semantics tests don't need a real disk
    // file. Track bit streams remain whatever the caller has put there.
    void  SetLoadedForTest (bool loaded, bool dirty);

private:
    HRESULT  LoadDsk          (const vector<Byte> & raw);
    void     InitWholeTrackMap ();

    // Splits a bit index into the byte offset and shift that address it
    // within `track`'s stream, wrapping around the track length because the
    // head runs in a circle. False when the track holds no data at all, in
    // which case neither out-parameter is meaningful.
    bool     TryLocateBit     (int track, size_t bitIndex, size_t & byteIdx, int & shift) const;

    string                m_filePath;
    vector<vector<Byte>>  m_trackBits;
    vector<size_t>        m_trackBitCounts;
    vector<bool>          m_trackDirty;
    vector<int>           m_quarterTrackMap;
    vector<TrackKind>     m_slotKind;
    vector<FluxTrack>     m_fluxTracks;
    uint64_t              m_layoutGeneration    = 0;
    IPendingWriteOwner *  m_pendingWriteOwner   = nullptr;
    vector<DamagedTrack>  m_damagedTracks;
    DiskFormat            m_format              = DiskFormat::Dsk;
    bool                  m_loaded              = false;
    bool                  m_dirty               = false;
    bool                  m_imageWriteProtected = false;
    bool                  m_userWriteProtected  = false;
    bool                  m_fileReadOnly        = false;
    bool                  m_fileNoPermission    = false;
    bool                  m_sourceCrcMismatch   = false;
    vector<Byte>          m_rawSourceBytes;
    WozMetadata           m_wozMetadata;
};

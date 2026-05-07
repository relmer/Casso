#pragma once

#include "Pch.h"

#include "IDiskImage.h"





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
    static constexpr int      kMaxTracks            = 40;
    static constexpr int      kDefaultTrackCount    = 35;
    static constexpr size_t   kDefaultTrackByteSize = 6400;
    static constexpr size_t   kDos33ImageSize       = 143360;

    DiskImage ();

    int           GetTrackCount     () const override;
    size_t        GetTrackBitCount  (int track) const override;
    uint8_t       ReadBit           (int track, size_t bitIndex) const override;
    void          WriteBit          (int track, size_t bitIndex, uint8_t bit) override;
    bool          IsDirty           () const override;
    bool          IsWriteProtected  () const override;
    DiskFormat    GetSourceFormat   () const override;
    HRESULT       Serialize         (vector<Byte> & outBytes) const override;

    // Backward-compat / lifecycle surface used by DiskIIController and tests.
    HRESULT          Load                (const string & filePath);
    void             Eject               ();
    HRESULT          Flush               ();
    bool             IsLoaded            () const { return m_loaded; }
    void             SetWriteProtected   (bool wp)        { m_writeProtected = wp; }
    const string &   GetFilePath         () const { return m_filePath; }
    void             SetSourceFormat     (DiskFormat fmt) { m_format = fmt; }
    bool             IsTrackDirty        (int track) const;
    void             ClearDirty          ();
    void             ResizeTrack         (int track, size_t bitCount);

    // Test-only injection. Marks the image as loaded/dirty without touching
    // the host filesystem so reset-semantics tests don't need a real disk
    // file. Track bit streams remain whatever the caller has put there.
    void  SetLoadedForTest (bool loaded, bool dirty);

private:
    HRESULT  LoadDsk          (const vector<Byte> & raw);
    void     NibblizeTrackToBits (int track, size_t & bitOffset, Byte sectorIdx, const Byte * sectorData, Byte volume);

    string             m_filePath;
    vector<vector<Byte>>     m_trackBits;
    vector<size_t>           m_trackBitCounts;
    vector<bool>             m_trackDirty;
    DiskFormat               m_format         = DiskFormat::Dsk;
    bool                     m_loaded         = false;
    bool                     m_dirty          = false;
    bool                     m_writeProtected = false;
    vector<Byte>             m_rawSourceBytes;
};

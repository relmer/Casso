#pragma once

#include "Pch.h"

class DiskImage;
enum class DiskFormat;





////////////////////////////////////////////////////////////////////////////////
//
//  Nibblizer
//
//  Static helpers that convert flat sector images (.dsk / .do / .po --
//  143360 bytes) to packed nibble bit streams for the Disk II nibble engine.
//
//  Encoding: standard Apple DOS 3.3 6+2 GCR per Sather UTAIIe Ch. 9.
//      Address field: $D5 $AA $96 ... $DE $AA $EB
//      Data    field: $D5 $AA $AD ... $DE $AA $EB
//      4-and-4 encoded volume / track / sector / checksum
//      6-and-2 encoded 256-byte sector → 342 nibble bytes + checksum
//
//  Format differences:
//      .dsk / .do — DOS 3.3 logical sector order (identical layout)
//      .po        — ProDOS order; sectors remapped via po_to_dos table
//
////////////////////////////////////////////////////////////////////////////////

class Nibblizer
{
public:
    static HRESULT  Nibblize    (const vector<Byte> & raw, DiskFormat fmt, DiskImage & out);
    static HRESULT  NibblizeDsk (const vector<Byte> & raw, DiskImage & out);
    static HRESULT  NibblizeDo  (const vector<Byte> & raw, DiskImage & out);
    static HRESULT  NibblizePo  (const vector<Byte> & raw, DiskImage & out);

    //  The volume a sector image's address fields are nibblized with: a DOS
    //  3.3 VTOC's, or 254 for any other disk. The sectors are in DOS 3.3 order.
    static Byte     GetImageVolume (const vector<Byte> & sectors);

    //  Re-encodes ONLY the named tracks; every other track's packed bits are
    //  left byte-identical, so a write cannot disturb what it did not touch.
    static HRESULT  RenibblizeTracks (const vector<Byte>    & sectors,
                                      DiskFormat              fmt,
                                      std::span<const int>    tracks,
                                      DiskImage             & inOutImage);

private:
    static Byte     GetAddressVolume (const vector<Byte> & raw, const int * interleave);
    static HRESULT  NibblizeWithMap  (const vector<Byte>  &  raw,
                                      const int           *  interleave,
                                      DiskImage           &  out);
};

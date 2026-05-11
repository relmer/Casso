#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFormat
//
////////////////////////////////////////////////////////////////////////////////

enum class DiskFormat
{
    Woz,
    Dsk,
    Do,
    Po,
};





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskImage
//
//  Abstract in-memory bit-stream track buffer. Tracks are bit streams
//  (not byte streams) — the controller addresses bits. Serialize produces
//  output in the original source format; for DSK/DO/PO this requires
//  de-nibblization via NibblizationLayer. WOZ tracks are already in
//  native form.
//
////////////////////////////////////////////////////////////////////////////////

class IDiskImage
{
public:
    virtual              ~IDiskImage () = default;

    virtual int           GetTrackCount       () const = 0;
    virtual size_t        GetTrackBitCount    (int track) const = 0;
    virtual uint8_t       ReadBit             (int track, size_t bitIndex) const = 0;
    virtual void          WriteBit            (int track, size_t bitIndex, uint8_t bit) = 0;
    virtual bool          IsDirty             () const = 0;
    virtual bool          IsWriteProtected    () const = 0;
    virtual DiskFormat    GetSourceFormat     () const = 0;
    virtual HRESULT       Serialize           (std::vector<uint8_t> & outBytes) const = 0;
};

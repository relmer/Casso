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
//  WriteProtectInfo
//
//  Why a mounted disk is write-protected. The four causes are
//  independent -- a disk can be protected by several at once (e.g. a
//  read-only WOZ whose backing file is also read-only), so these are
//  plain booleans rather than a single mutually-exclusive reason.
//
//      imageFlag     the image's own embedded write-protect flag
//                    (WOZ INFO chunk). Round-trips through Serialize.
//      userSetting   the user's Settings / menu write-protect toggle.
//      readOnlyFile  the backing host file has the read-only attribute.
//      noPermission  the backing host file cannot be opened for writing
//                    (ACL denial, exclusive lock, etc.) though it is not
//                    marked read-only.
//
////////////////////////////////////////////////////////////////////////////////

struct WriteProtectInfo
{
    bool  imageFlag    = false;
    bool  userSetting  = false;
    bool  readOnlyFile = false;
    bool  noPermission = false;

    bool  Any () const { return imageFlag || userSetting || readOnlyFile || noPermission; }

    bool  operator== (const WriteProtectInfo & o) const
    {
        return imageFlag    == o.imageFlag    &&
               userSetting  == o.userSetting  &&
               readOnlyFile == o.readOnlyFile &&
               noPermission == o.noPermission;
    }

    bool  operator!= (const WriteProtectInfo & o) const { return !(*this == o); }
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
    virtual WriteProtectInfo GetWriteProtectInfo () const = 0;
    virtual DiskFormat    GetSourceFormat     () const = 0;
    virtual HRESULT       Serialize           (std::vector<uint8_t> & outBytes) const = 0;
};

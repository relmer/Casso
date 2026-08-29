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

    //  Headerless nibble images, .nib and .nb2 alike. ONE ENUMERATOR FOR BOTH:
    //  they differ only in track size, which is not a property of the format
    //  but of the individual file, read from its length. A second enumerator
    //  would let the enum disagree with the file it names, since either size
    //  circulates under either extension.
    Nib,
};





////////////////////////////////////////////////////////////////////////////////
//
//  WriteProtectInfo
//
//  Why a mounted disk is write-protected. The causes are independent -- a
//  disk can be protected by several at once (e.g. a read-only WOZ whose
//  backing file is also read-only), so these are plain booleans rather
//  than a single mutually-exclusive reason.
//
//      imageFlag         the image's own embedded write-protect flag
//                        (WOZ INFO chunk). Lives in the file.
//      userSetting       the user's Settings / menu write-protect toggle.
//      readOnlyFile      the backing host file has the read-only attribute.
//      noPermission      the backing host file cannot be opened for writing
//                        (ACL denial, exclusive lock, etc.) though it is not
//                        marked read-only.
//      checksumMismatch  the image's own stored checksum did not match its
//                        contents at load, so the file is damaged. Rewriting
//                        it would stamp a fresh, correct checksum over the
//                        damage and leave nothing able to detect it, so the
//                        disk is held read-only for the session instead.
//
//  checksumMismatch is deliberately NOT the image flag: that flag lives in
//  the file, so setting it would mean writing the very file being protected
//  from writes. It is session state, like userSetting.
//
////////////////////////////////////////////////////////////////////////////////

struct WriteProtectInfo
{
    bool  imageFlag        = false;
    bool  userSetting      = false;
    bool  readOnlyFile     = false;
    bool  noPermission     = false;
    bool  checksumMismatch = false;

    bool  Any () const
    {
        return imageFlag || userSetting || readOnlyFile || noPermission || checksumMismatch;
    }

    bool  operator== (const WriteProtectInfo & o) const
    {
        return imageFlag        == o.imageFlag        &&
               userSetting      == o.userSetting      &&
               readOnlyFile     == o.readOnlyFile     &&
               noPermission     == o.noPermission     &&
               checksumMismatch == o.checksumMismatch;
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
    virtual DiskFormat    GetSourceFormat     () const = 0;
    virtual HRESULT       Serialize           (std::vector<uint8_t> & outBytes) const = 0;
};

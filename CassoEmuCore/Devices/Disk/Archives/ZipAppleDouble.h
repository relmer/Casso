#pragma once

#include "Pch.h"

struct ZipEntry;
struct FilePayload;





////////////////////////////////////////////////////////////////////////////////
//
//  ZipAppleDouble
//
//  The AppleDouble file of a ZIP member, as CiderPress II and the Mac's
//  archiver write it: `__MACOSX/DIR/._NAME` for `DIR/NAME`, or `DIR/._NAME`
//  beside it. Its ProDOS file info entry indicates the file's type and aux
//  type; without one, the Finder type indicates them, in the `p` form (type
//  `p`, the ProDOS type byte and aux type, creator `pdos`) or as `TEXT` or
//  `BINA`.
//
////////////////////////////////////////////////////////////////////////////////

class ZipAppleDouble
{
public:
    //  Whether a member is the AppleDouble file of another member.
    static bool  IsSidecar (const std::string & path);

    //  The AppleDouble entry of `member`, in the `__MACOSX` form first, or
    //  null for a member without one.
    static const ZipEntry *  FindSidecar (const std::vector<ZipEntry> & entries, const std::string & member);

    //  The type, aux type, resource fork and Finder information of `member`
    //  from its AppleDouble file, set on the payload; false, and the payload
    //  as it was, for a member without one.
    static bool  Apply (const std::vector<ZipEntry> & entries, const std::string & member, FilePayload & inOutPayload);

    //  The ProDOS type and aux type the Finder information indicates; false
    //  for a Finder type that has no ProDOS type.
    static bool  TryGetProDosType (std::span<const Byte> finderInfo, Byte & outType, Word & outAuxType);

private:
    static constexpr const char *  kMacFolder   = "__MACOSX/";
    static constexpr const char *  kPrefix      = "._";
    static constexpr uint32_t      kTypeText    = 0x54455854;     // 'TEXT'
    static constexpr uint32_t      kTypeBinary  = 0x42494E41;     // 'BINA'
    static constexpr uint32_t      kCreatorPdos = 0x70646F73;     // 'pdos'
    static constexpr Byte          kProDosForm  = 'p';
    static constexpr Byte          kProDosText  = 0x04;
    static constexpr Byte          kProDosBin   = 0x06;
};

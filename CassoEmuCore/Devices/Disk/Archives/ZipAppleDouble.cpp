#include "Pch.h"

#include "Devices/Disk/Archives/ZipAppleDouble.h"
#include "Core/AppleSingleCodec.h"
#include "Machines/Apple2/Common/VolumeTypes.h"
#include "Update/ZipArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ZipAppleDouble::IsSidecar
//
////////////////////////////////////////////////////////////////////////////////

bool ZipAppleDouble::IsSidecar (const std::string & path)
{
    size_t       slash = path.find_last_of ('/');
    std::string  leaf  = (slash == std::string::npos) ? path : path.substr (slash + 1);



    return path.starts_with (kMacFolder) || leaf.starts_with (kPrefix);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipAppleDouble::FindSidecar
//
////////////////////////////////////////////////////////////////////////////////

const ZipEntry * ZipAppleDouble::FindSidecar (const std::vector<ZipEntry> & entries, const std::string & member)
{
    size_t             slash    = member.find_last_of ('/');
    std::string        folder   = (slash == std::string::npos) ? std::string() : member.substr (0, slash + 1);
    std::string        leaf     = (slash == std::string::npos) ? member : member.substr (slash + 1);
    std::string        paths[]  = { kMacFolder + folder + kPrefix + leaf, folder + kPrefix + leaf };
    const ZipEntry *   sidecar  = nullptr;



    for (const std::string & path : paths)
    {
        for (const ZipEntry & entry : entries)
        {
            if (sidecar == nullptr && entry.path == path)
            {
                sidecar = &entry;
            }
        }
    }

    return sidecar;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipAppleDouble::Apply
//
//  A member whose AppleDouble file doesn't decode is treated as a member
//  without one.
//
////////////////////////////////////////////////////////////////////////////////

bool ZipAppleDouble::Apply (const std::vector<ZipEntry> & entries, const std::string & member, FilePayload & inOutPayload)
{
    HRESULT            hr       = S_OK;
    const ZipEntry *   sidecar  = FindSidecar (entries, member);
    AppleSingleFile    single;
    std::string        error;
    Byte               type     = 0;
    Word               auxType  = 0;
    bool               isDouble = false;
    bool               decoded  = false;



    if (sidecar == nullptr)
    {
        return false;
    }

    isDouble = AppleSingleCodec::IsAppleDouble (sidecar->data);

    if (!isDouble)
    {
        return false;
    }

    hr      = AppleSingleCodec::Decode (sidecar->data, single, error);
    decoded = SUCCEEDED (hr);

    if (!decoded)
    {
        return false;
    }

    if (single.hasProDosInfo)
    {
        inOutPayload.type       = (Byte) single.fileType;
        inOutPayload.auxType    = (Word) single.auxType;
        inOutPayload.hasAuxType = true;
    }
    else if (TryGetProDosType (single.finderInfo, type, auxType))
    {
        inOutPayload.type       = type;
        inOutPayload.auxType    = auxType;
        inOutPayload.hasAuxType = true;
    }

    inOutPayload.resourceFork    = single.resourceFork;
    inOutPayload.finderInfo      = single.finderInfo;
    inOutPayload.hasResourceFork = single.hasResourceFork;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ZipAppleDouble::TryGetProDosType
//
//  FInfo's first eight bytes are the type and the creator, big-endian.
//
////////////////////////////////////////////////////////////////////////////////

bool ZipAppleDouble::TryGetProDosType (std::span<const Byte> finderInfo, Byte & outType, Word & outAuxType)
{
    static constexpr size_t  kTypeAndCreator = 8;



    uint32_t  type    = 0;
    uint32_t  creator = 0;
    bool      found   = false;



    outType    = 0;
    outAuxType = 0;

    if (finderInfo.size() < kTypeAndCreator)
    {
        return false;
    }

    type    = (uint32_t) finderInfo[0] << 24 | (uint32_t) finderInfo[1] << 16 | (uint32_t) finderInfo[2] << 8 | finderInfo[3];
    creator = (uint32_t) finderInfo[4] << 24 | (uint32_t) finderInfo[5] << 16 | (uint32_t) finderInfo[6] << 8 | finderInfo[7];

    if (creator == kCreatorPdos && finderInfo[0] == kProDosForm)
    {
        outType    = finderInfo[1];
        outAuxType = (Word) (finderInfo[2] << 8 | finderInfo[3]);
        found      = true;
    }
    else if (type == kTypeText)
    {
        outType = kProDosText;
        found   = true;
    }
    else if (type == kTypeBinary)
    {
        outType = kProDosBin;
        found   = true;
    }

    return found;
}

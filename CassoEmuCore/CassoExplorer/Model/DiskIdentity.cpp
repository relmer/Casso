#include "Pch.h"

#include "CassoExplorer/Model/DiskIdentity.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Update/Sha256Digest.h"
#include "Update/ZipArchive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIdentity::ComputeVolume
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIdentity::ComputeVolume (const std::vector<Byte> & sectors, const SectorDecodeReport & report, Volume & outVolume)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  digest;



    outVolume = Volume();

    hr = Sha256Digest::Compute (sectors, digest);
    CHR (hr);

    outVolume.sha256  = FormatHex (digest);
    outVolume.crc32   = ZipArchive::ComputeCrc32 (sectors);
    outVolume.hasMore = report.HasDataLoss();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIdentity::ComputeFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskIdentity::ComputeFile (std::span<const Byte> data, std::string & outSha256)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  digest;



    outSha256.clear();

    hr = Sha256Digest::Compute (data, digest);
    CHR (hr);

    outSha256 = FormatHex (digest);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIdentity::GetFileKey
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskIdentity::GetFileKey (const std::string & sha256, Byte type)
{
    return std::format ("{}:{:02X}", sha256, type);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIdentity::FormatHex
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskIdentity::FormatHex (std::span<const Byte> bytes)
{
    std::string  text;



    text.reserve (bytes.size() * 2);

    for (Byte b : bytes)
    {
        text += s_kHexDigits[b >> 4];
        text += s_kHexDigits[b & 0x0F];
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIdentity::FormatCrc32
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskIdentity::FormatCrc32 (uint32_t crc32)
{
    return std::format ("{:08x}", crc32);
}

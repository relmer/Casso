#include "Pch.h"

#include "CassoLink/Session/CassoLinkResume.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "Core/JsonParser.h"
#include "Core/JsonValue.h"
#include "Core/JsonWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkResume::Format
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkResume::Format (const CassoLinkResumeRecord & record)
{
    vector<pair<string, JsonValue>>  root;



    root.emplace_back ("slot",              JsonValue ((double) record.slot));
    root.emplace_back ("unit",              JsonValue ((double) record.unit));
    root.emplace_back ("blockCount",        JsonValue ((double) record.blockCount));
    root.emplace_back ("volumeName",        JsonValue (record.volumeName));
    root.emplace_back ("directoryHash",     JsonValue (record.directoryHash));
    root.emplace_back ("direction",         JsonValue (record.direction));
    root.emplace_back ("lastVerifiedBlock", JsonValue ((double) record.lastVerifiedBlock));
    root.emplace_back ("imagePath",         JsonValue (record.imagePath));

    return JsonWriter::Write (JsonValue (std::move (root)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkResume::Parse
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkResume::Parse (const std::string & text, CassoLinkResumeRecord & outRecord)
{
    HRESULT         hr         = S_OK;
    JsonValue       root;
    JsonParseError  error;
    double          blocks     = 0.0;
    double          verified   = 0.0;
    bool            whole      = false;



    outRecord = {};

    hr = JsonParser::Parse (text, root, error);
    CHRF (hr, hr = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    whole = root.HasInt    ("slot",              outRecord.slot)
         && root.HasInt    ("unit",              outRecord.unit)
         && root.HasNumber ("blockCount",        blocks)
         && root.HasString ("volumeName",        outRecord.volumeName)
         && root.HasString ("directoryHash",     outRecord.directoryHash)
         && root.HasString ("direction",         outRecord.direction)
         && root.HasNumber ("lastVerifiedBlock", verified)
         && root.HasString ("imagePath",         outRecord.imagePath)
         && (outRecord.direction == CassoLinkResumeRecord::kRead || outRecord.direction == CassoLinkResumeRecord::kWrite)
         && blocks >= 0.0 && verified >= -1.0 && verified < blocks;
    CBREx (whole, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outRecord.blockCount        = (uint32_t) blocks;
    outRecord.lastVerifiedBlock = (int64_t) verified;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkResume::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkResume::Matches (const CassoLinkResumeRecord & record, const CassoLinkBlockDevice & device, const std::string & directoryHash, const std::string & direction)
{
    return record.slot == device.slot && record.unit == device.unit && record.blockCount == device.blocks
        && record.volumeName == device.volume && record.directoryHash == directoryHash && record.direction == direction;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkResume::HashDirectory
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoLinkResume::HashDirectory (std::span<const Byte> blocks, std::string & outHex)
{
    return FullSendDisk::ComputeHash (blocks, outHex);
}

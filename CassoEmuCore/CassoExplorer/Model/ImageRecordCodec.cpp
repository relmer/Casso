#include "Pch.h"

#include "CassoExplorer/Model/ImageRecordCodec.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Cursor
//
//  The values of a positional array read in order. A missing value or one
//  of the wrong type clears isGood and reads as zero or empty.
//
////////////////////////////////////////////////////////////////////////////////

struct ImageRecordCodec::Cursor
{
    const JsonValue  & array;
    size_t             next   = 0;
    bool               isGood = true;

    const JsonValue * Take (JsonType type)
    {
        const JsonValue  * value = nullptr;



        isGood = isGood && array.GetType() == JsonType::Array && next < array.GetArraySize() && array.GetArrayElement (next).GetType() == type;
        value  = isGood ? &array.GetArrayElement (next) : nullptr;
        next++;

        return value;
    }

    double Number()
    {
        const JsonValue  * value = Take (JsonType::Number);



        return value != nullptr ? value->GetNumber() : 0.0;
    }

    bool Flag()
    {
        return Number() != 0.0;
    }

    std::string Text()
    {
        const JsonValue  * value = Take (JsonType::String);
        std::string        text;



        isGood = isGood && value != nullptr && ImageRecordCodec::DecodeHex (value->GetString(), text);

        return text;
    }

    const JsonValue & Array()
    {
        static const JsonValue  s_kEmpty;
        const JsonValue       * value = Take (JsonType::Array);



        return value != nullptr ? *value : s_kEmpty;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::MakeText
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ImageRecordCodec::MakeText (const std::string & text)
{
    static constexpr const char *  s_kDigits = "0123456789abcdef";
    std::string                    hex;



    hex.reserve (text.size() * 2);

    for (char each : text)
    {
        hex += s_kDigits[((Byte) each) >> 4];
        hex += s_kDigits[((Byte) each) & 0x0F];
    }

    return JsonValue (hex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::GetNibble
//
//  A lowercase hex digit's value, or -1.
//
////////////////////////////////////////////////////////////////////////////////

int ImageRecordCodec::GetNibble (char digit)
{
    int  value = -1;



    if (digit >= '0' && digit <= '9')
    {
        value = digit - '0';
    }
    else if (digit >= 'a' && digit <= 'f')
    {
        value = digit - 'a' + 10;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::DecodeHex
//
////////////////////////////////////////////////////////////////////////////////

bool ImageRecordCodec::DecodeHex (const std::string & hex, std::string & outText)
{
    bool  isGood = hex.size() % 2 == 0;



    outText.clear();

    for (size_t i = 0; isGood && i < hex.size(); i += 2)
    {
        int  high = GetNibble (hex[i]);
        int  low  = GetNibble (hex[i + 1]);

        isGood = high >= 0 && low >= 0;
        outText += (char) ((high << 4) | low);
    }

    return isGood;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::MakeNumber
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ImageRecordCodec::MakeNumber (double value)
{
    return JsonValue (value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::FormatEntry
//
//  A catalog entry as an array, in the order ParseEntry reads it.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ImageRecordCodec::FormatEntry (const FileEntry & entry)
{
    Values  values;



    values.push_back (MakeText   (entry.name));
    values.push_back (MakeNumber (entry.type));
    values.push_back (MakeNumber (entry.isLocked));
    values.push_back (MakeNumber (entry.isDirectory));
    values.push_back (MakeNumber (entry.sizeUnits));
    values.push_back (MakeNumber (entry.eofBytes));
    values.push_back (MakeNumber (entry.loadAddress));
    values.push_back (MakeNumber (entry.auxType));
    values.push_back (MakeNumber (entry.hasEofBytes));
    values.push_back (MakeNumber (entry.hasLoadAddress));
    values.push_back (MakeNumber (entry.hasAuxType));
    values.push_back (MakeNumber ((double) entry.modifiedUnix));
    values.push_back (MakeNumber (entry.hasModified));
    values.push_back (MakeNumber (entry.access));
    values.push_back (MakeNumber (entry.hasAccess));
    values.push_back (MakeNumber (entry.createdDate));
    values.push_back (MakeNumber (entry.createdTime));
    values.push_back (MakeNumber (entry.modifiedDate));
    values.push_back (MakeNumber (entry.modifiedTime));
    values.push_back (MakeNumber (entry.hasDates));
    values.push_back (MakeNumber (entry.resourceForkBytes));
    values.push_back (MakeNumber (entry.hasResourceFork));
    values.push_back (MakeNumber ((double) entry.catalogIndex));
    values.push_back (MakeNumber (entry.firstBlock));
    values.push_back (MakeNumber (entry.lastBlockBytes));
    values.push_back (MakeNumber (entry.records));
    values.push_back (MakeNumber (entry.extents));
    values.push_back (MakeNumber (entry.user));
    values.push_back (MakeNumber (entry.isReadOnly));
    values.push_back (MakeNumber (entry.isSystem));

    return JsonValue (std::move (values));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::ParseEntry
//
////////////////////////////////////////////////////////////////////////////////

bool ImageRecordCodec::ParseEntry (const JsonValue & value, FileEntry & outEntry)
{
    Cursor  in { value };



    outEntry.name              = in.Text();
    outEntry.type              = (Byte) in.Number();
    outEntry.isLocked          = in.Flag();
    outEntry.isDirectory       = in.Flag();
    outEntry.sizeUnits         = (uint32_t) in.Number();
    outEntry.eofBytes          = (uint32_t) in.Number();
    outEntry.loadAddress       = (Word) in.Number();
    outEntry.auxType           = (Word) in.Number();
    outEntry.hasEofBytes       = in.Flag();
    outEntry.hasLoadAddress    = in.Flag();
    outEntry.hasAuxType        = in.Flag();
    outEntry.modifiedUnix      = (int64_t) in.Number();
    outEntry.hasModified       = in.Flag();
    outEntry.access            = (Byte) in.Number();
    outEntry.hasAccess         = in.Flag();
    outEntry.createdDate       = (Word) in.Number();
    outEntry.createdTime       = (Word) in.Number();
    outEntry.modifiedDate      = (Word) in.Number();
    outEntry.modifiedTime      = (Word) in.Number();
    outEntry.hasDates          = in.Flag();
    outEntry.resourceForkBytes = (uint32_t) in.Number();
    outEntry.hasResourceFork   = in.Flag();
    outEntry.catalogIndex      = (size_t) in.Number();
    outEntry.firstBlock        = (int) in.Number();
    outEntry.lastBlockBytes    = (Word) in.Number();
    outEntry.records           = (uint32_t) in.Number();
    outEntry.extents           = (uint32_t) in.Number();
    outEntry.user              = (int) in.Number();
    outEntry.isReadOnly        = in.Flag();
    outEntry.isSystem          = in.Flag();

    return in.isGood;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::FormatPartition
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ImageRecordCodec::FormatPartition (const PartitionEntry & entry)
{
    Values  values;



    values.push_back (MakeNumber (entry.partition.index));
    values.push_back (MakeNumber (entry.partition.firstBlock));
    values.push_back (MakeNumber (entry.partition.blockCount));
    values.push_back (MakeNumber ((double) entry.partition.layout));
    values.push_back (MakeNumber ((double) entry.partition.volumeKind));
    values.push_back (MakeNumber (entry.partition.damaged));
    values.push_back (MakeText   (entry.partition.name));
    values.push_back (MakeText   (entry.partition.type));
    values.push_back (MakeNumber ((double) entry.partition.span));
    values.push_back (MakeNumber (entry.partition.dosSectors));
    values.push_back (MakeNumber (entry.partition.isHfs));
    values.push_back (MakeText   (entry.step));
    values.push_back (MakeNumber ((double) entry.byteCount));
    values.push_back (MakeText   (entry.volumeName));
    values.push_back (MakeNumber (entry.hasVolumeNumber));
    values.push_back (MakeNumber (entry.volumeNumber));

    return JsonValue (std::move (values));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::ParsePartition
//
////////////////////////////////////////////////////////////////////////////////

bool ImageRecordCodec::ParsePartition (const JsonValue & value, PartitionEntry & outEntry)
{
    Cursor  in { value };



    outEntry.partition.index      = (int) in.Number();
    outEntry.partition.firstBlock = (uint32_t) in.Number();
    outEntry.partition.blockCount = (uint32_t) in.Number();
    outEntry.partition.layout     = (PartitionLayoutKind) (int) in.Number();
    outEntry.partition.volumeKind = (VolumeKind) (int) in.Number();
    outEntry.partition.damaged    = in.Flag();
    outEntry.partition.name       = in.Text();
    outEntry.partition.type       = in.Text();
    outEntry.partition.span       = (PartitionSpan) (int) in.Number();
    outEntry.partition.dosSectors = in.Flag();
    outEntry.partition.isHfs      = in.Flag();
    outEntry.step                 = in.Text();
    outEntry.byteCount            = (uint64_t) in.Number();
    outEntry.volumeName           = in.Text();
    outEntry.hasVolumeNumber      = in.Flag();
    outEntry.volumeNumber         = (Byte) in.Number();

    return in.isGood;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::FormatContents
//
//  The listing's result and contents. A result's payload and output are
//  for a get and a command's text, which a listing doesn't have.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue ImageRecordCodec::FormatContents (const ImageContents & contents)
{
    Values  values;
    Values  entries;
    Values  damage;
    Values  subdirectories;
    Values  partitions;



    for (const FileEntry & entry : contents.listing.entries)
    {
        entries.push_back (FormatEntry (entry));
    }

    for (const std::string & line : contents.listing.damage)
    {
        damage.push_back (MakeText (line));
    }

    for (const ImageSubdirectory & each : contents.subdirectories)
    {
        subdirectories.push_back (JsonValue (Values { MakeText (each.path), MakeText (each.name), MakeNumber (each.hasSubdirectories) }));
    }

    for (const PartitionEntry & entry : contents.partitions)
    {
        partitions.push_back (FormatPartition (entry));
    }

    values.push_back (MakeNumber ((double) contents.result.hr));
    values.push_back (MakeNumber (contents.result.exitStatus));
    values.push_back (MakeText   (contents.result.message));
    values.push_back (MakeNumber (contents.result.unreadSectors));
    values.push_back (MakeNumber (contents.result.listingDamaged));
    values.push_back (MakeNumber ((double) contents.kind));
    values.push_back (MakeText   (contents.listing.volumeName));
    values.push_back (MakeNumber (contents.listing.volumeNumber));
    values.push_back (MakeNumber (contents.listing.hasVolumeName));
    values.push_back (MakeNumber (contents.listing.hasVolumeNumber));
    values.push_back (MakeNumber (contents.listing.totalUnits));
    values.push_back (MakeNumber (contents.listing.freeUnits));
    values.push_back (JsonValue  (std::move (entries)));
    values.push_back (JsonValue  (std::move (damage)));
    values.push_back (MakeNumber (contents.listing.directoryFiles));
    values.push_back (MakeNumber (contents.listing.startup.appleII));
    values.push_back (MakeNumber (contents.listing.startup.appleIII));
    values.push_back (JsonValue  (std::move (subdirectories)));
    values.push_back (MakeNumber ((double) contents.layout));
    values.push_back (JsonValue  (std::move (partitions)));
    values.push_back (MakeText   (TextEncoding::WideToNarrow (contents.error)));
    values.push_back (MakeNumber (contents.broken));

    return JsonValue (std::move (values));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::ParseContents
//
////////////////////////////////////////////////////////////////////////////////

bool ImageRecordCodec::ParseContents (const JsonValue & value, ImageContents & outContents)
{
    Cursor  in     { value };
    bool    isGood = true;



    outContents = ImageContents();

    outContents.result.hr              = (HRESULT) (int64_t) in.Number();
    outContents.result.exitStatus      = (int) in.Number();
    outContents.result.message         = in.Text();
    outContents.result.unreadSectors   = (int) in.Number();
    outContents.result.listingDamaged  = in.Flag();
    outContents.kind                   = (VolumeKind) (int) in.Number();
    outContents.listing.volumeName     = in.Text();
    outContents.listing.volumeNumber   = (Byte) in.Number();
    outContents.listing.hasVolumeName  = in.Flag();
    outContents.listing.hasVolumeNumber = in.Flag();
    outContents.listing.totalUnits     = (uint32_t) in.Number();
    outContents.listing.freeUnits      = (uint32_t) in.Number();

    {
        const JsonValue  & entries = in.Array();

        for (size_t i = 0; in.isGood && isGood && i < entries.GetArraySize(); i++)
        {
            FileEntry  entry;

            isGood = ParseEntry (entries.GetArrayElement (i), entry);
            outContents.listing.entries.push_back (std::move (entry));
        }
    }

    {
        const JsonValue  & damage = in.Array();

        for (size_t i = 0; in.isGood && isGood && i < damage.GetArraySize(); i++)
        {
            std::string  line;

            isGood = ReadText (damage.GetArrayElement (i), line);
            outContents.listing.damage.push_back (std::move (line));
        }
    }

    outContents.listing.directoryFiles   = (uint32_t) in.Number();
    outContents.listing.startup.appleII  = in.Flag();
    outContents.listing.startup.appleIII = in.Flag();

    {
        const JsonValue  & subdirectories = in.Array();

        for (size_t i = 0; in.isGood && isGood && i < subdirectories.GetArraySize(); i++)
        {
            Cursor             each { subdirectories.GetArrayElement (i) };
            ImageSubdirectory  subdirectory;

            subdirectory.path              = each.Text();
            subdirectory.name              = each.Text();
            subdirectory.hasSubdirectories = each.Flag();

            isGood = each.isGood;
            outContents.subdirectories.push_back (std::move (subdirectory));
        }
    }

    outContents.layout = (PartitionLayoutKind) (int) in.Number();

    {
        const JsonValue  & partitions = in.Array();

        for (size_t i = 0; in.isGood && isGood && i < partitions.GetArraySize(); i++)
        {
            PartitionEntry  entry;

            isGood = ParsePartition (partitions.GetArrayElement (i), entry);
            outContents.partitions.push_back (std::move (entry));
        }
    }

    outContents.error  = TextEncoding::NarrowToWide (in.Text());
    outContents.broken = in.Flag();

    return in.isGood && isGood;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::ReadText
//
////////////////////////////////////////////////////////////////////////////////

bool ImageRecordCodec::ReadText (const JsonValue & value, std::string & outText)
{
    return value.GetType() == JsonType::String && DecodeHex (value.GetString(), outText);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::Format
//
////////////////////////////////////////////////////////////////////////////////

std::string ImageRecordCodec::Format (const Record & record)
{
    std::vector<std::pair<std::string, JsonValue>>  fields;



    fields.emplace_back ("version",   MakeNumber (kVersion));
    fields.emplace_back ("hash",      JsonValue (record.hash));
    fields.emplace_back ("savedGame", MakeNumber ((double) record.savedGame));

    if (record.hasContents)
    {
        fields.emplace_back ("contents", FormatContents (record.contents));
    }

    return JsonWriter::Write (JsonValue (std::move (fields)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec::Parse
//
//  False for a record that isn't this version or doesn't parse.
//
////////////////////////////////////////////////////////////////////////////////

bool ImageRecordCodec::Parse (const std::string & json, Record & outRecord)
{
    HRESULT             hr        = S_OK;
    JsonValue           root;
    JsonParseError      error;
    int                 version   = 0;
    int                 savedGame = 0;
    const JsonValue   * contents  = nullptr;
    bool                isGood    = false;



    outRecord = Record();

    hr     = JsonParser::Parse (json, root, error);
    isGood = SUCCEEDED (hr) && root.GetType() == JsonType::Object && root.HasInt ("version", version) && version == kVersion;
    isGood = isGood && root.HasInt ("savedGame", savedGame) && savedGame >= 0 && savedGame <= (int) IImageRecords::SavedGame::Present;
    isGood = isGood && root.HasString ("hash", outRecord.hash);

    if (isGood)
    {
        outRecord.savedGame = (IImageRecords::SavedGame) savedGame;
        hr                  = root.GetArray ("contents", contents);
    }

    if (isGood && SUCCEEDED (hr) && contents != nullptr)
    {
        outRecord.hasContents = true;
        isGood                = ParseContents (*contents, outRecord.contents);
    }

    return isGood;
}

#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/DiskImageCatalog.h"
#include "CassoExplorer/Model/IImageRecords.h"
#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageRecordCodec
//
//  An image record as JSON and back. Text read from the image, such as a
//  file's name, is stored as hex, since that text can have any byte.
//  A record of another version doesn't parse, so a change to the decoder or
//  the catalog reader reads each image once more.
//
////////////////////////////////////////////////////////////////////////////////

class ImageRecordCodec
{
public:
    struct Record
    {
        std::string               hash;
        bool                      hasContents = false;
        ImageContents             contents;
        IImageRecords::SavedGame  savedGame   = IImageRecords::SavedGame::Unknown;
    };

    static constexpr int  kVersion = 1;

    static std::string  Format (const Record & record);
    static bool         Parse  (const std::string & json, Record & outRecord);

    //  Hex of any bytes back to the bytes; false for a string that isn't hex.
    static bool         DecodeHex (const std::string & hex, std::string & outText);

private:
    using Values = std::vector<JsonValue>;

    struct Cursor;

    static int          GetNibble       (char digit);
    static JsonValue    MakeText        (const std::string & text);
    static JsonValue    MakeNumber      (double value);
    static JsonValue    FormatEntry     (const FileEntry & entry);
    static JsonValue    FormatPartition (const PartitionEntry & entry);
    static JsonValue    FormatContents  (const ImageContents & contents);
    static bool         ReadText        (const JsonValue & value, std::string & outText);
    static bool         ParseEntry      (const JsonValue & value, FileEntry & outEntry);
    static bool         ParsePartition  (const JsonValue & value, PartitionEntry & outEntry);
    static bool         ParseContents   (const JsonValue & value, ImageContents & outContents);
};

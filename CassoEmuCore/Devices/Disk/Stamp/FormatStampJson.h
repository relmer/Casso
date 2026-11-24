#pragma once

#include "Pch.h"

#include "Devices/Disk/Stamp/FormatStamp.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson
//
//  A FormatStamp as the JSON text of a CSSO chunk, and back. Parse fails with
//  ERROR_INVALID_DATA for text that is not a version 1 stamp, so a caller
//  treats it as an unreadable chunk and writes its bytes back unmodified.
//
////////////////////////////////////////////////////////////////////////////////

class FormatStampJson
{
public:
    static constexpr char  kFormatName[] = "casso-woz-stamp";
    static constexpr int   kMaxNesting   = 16;

    static HRESULT      Write             (const FormatStamp & stamp, std::string & outText);
    static HRESULT      Parse             (std::string_view text, FormatStamp & outStamp);

    static std::string  NibblesToText     (std::span<const Byte> nibbles);
    static bool         TryParseNibbles   (std::string_view text, vector<Byte> & outNibbles);

    static const char * EncodingToText    (StampEncoding encoding);
    static const char * ChecksumToText    (StampChecksum checksum);
    static const char * SourceToText      (StampSource source);

private:
    static bool         IsNestingInLimit  (std::string_view text);

    static JsonValue    MakeNibbleList    (const vector<vector<Byte>> & lists);
    static JsonValue    MakeByteList      (const vector<Byte> & values);
    static JsonValue    MakeIntList       (const vector<int> & values);
    static JsonValue    MakeMarkList      (const vector<DiskMarkPattern> & marks);
    static JsonValue    MakeNotes         (const vector<ProtectionNote> & notes);
    static JsonValue    MakeRange         (const DecodeRange & range);
    static JsonValue    MakeCapture       (const CaptureFacts & capture);
    static JsonValue    MakeTrack         (const StampTrack & track);

    static HRESULT      ReadNibbleList    (const JsonValue & object, const char * key, vector<vector<Byte>> & outLists);
    static HRESULT      ReadByteList      (const JsonValue & object, const char * key, vector<Byte> & outValues);
    static HRESULT      ReadIntList       (const JsonValue & object, const char * key, vector<int> & outValues);
    static HRESULT      ReadMarkList      (const JsonValue & object, const char * key, vector<DiskMarkPattern> & outMarks);
    static HRESULT      ReadNotes         (const JsonValue & object, vector<ProtectionNote> & outNotes);
    static HRESULT      ReadRange         (const JsonValue & value, DecodeRange & outRange);
    static void         ReadCapture       (const JsonValue & value, CaptureFacts & outCapture);
    static HRESULT      ReadTrackCapture  (const JsonValue & value, TrackCaptureFacts & outCapture);
    static HRESULT      ReadTrack         (const JsonValue & value, StampTrack & outTrack);
    static HRESULT      ReadHeader        (const JsonValue & root, FormatStamp & outStamp);
    static HRESULT      ReadVersion       (const std::string & text, int & outMajor, int & outMinor);
    static HRESULT      ReadCrc           (const std::string & text, uint32_t & outCrc);

    static bool         TryParseEncoding  (const std::string & text, StampEncoding & outEncoding);
    static bool         TryParseChecksum  (const std::string & text, StampChecksum & outChecksum);
    static bool         TryParseSource    (const std::string & text, StampSource & outSource);
};

#include "Pch.h"

#include "Devices/Disk/Stamp/FormatStampJson.h"

#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::Write
//
//  The stamp as pretty-printed JSON, with every key the format defines. An
//  empty list is written as [], so a reader never has to distinguish a missing key
//  from an empty one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampJson::Write (const FormatStamp & stamp, std::string & outText)
{
    HRESULT                          hr      = S_OK;
    vector<pair<string, JsonValue>>  root;
    vector<JsonValue>                ranges;
    vector<JsonValue>                tracks;
    JsonWriter::Options              options;



    outText.clear();

    for (const DecodeRange & range : stamp.decodeRanges)
    {
        ranges.push_back (MakeRange (range));
    }

    for (const StampTrack & track : stamp.tracks)
    {
        tracks.push_back (MakeTrack (track));
    }

    root.push_back ({ "format",         JsonValue (std::string (kFormatName)) });
    root.push_back ({ "version",        JsonValue (std::format ("{}.{}", stamp.versionMajor, stamp.versionMinor)) });
    root.push_back ({ "writer",         JsonValue (stamp.writer) });
    root.push_back ({ "written",        JsonValue (stamp.written) });
    root.push_back ({ "source",         JsonValue (std::string (SourceToText (stamp.source))) });
    root.push_back ({ "decodeSettings", JsonValue (std::move (ranges)) });
    root.push_back ({ "capture",        stamp.hasCapture ? MakeCapture (stamp.capture) : JsonValue (nullptr) });
    root.push_back ({ "notes",          MakeNotes (stamp.notes) });
    root.push_back ({ "tracks",         JsonValue (std::move (tracks)) });

    hr = JsonWriter::Write (JsonValue (std::move (root)), options, outText);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::Parse
//
//  Reads a stamp. Fails with ERROR_INVALID_DATA for text over the size
//  limit, nested deeper than any stamp is, that is not JSON, that has another
//  format, a major version other than this build's, or a required key
//  missing or of the wrong type. Keys the format does not define are
//  ignored, which is how a later minor version adds one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampJson::Parse (std::string_view text, FormatStamp & outStamp)
{
    HRESULT              hr          = S_OK;
    JsonValue            root;
    JsonParseError       error;
    const JsonValue    * tracks      = nullptr;
    const JsonValue    * ranges      = nullptr;
    const JsonValue    * capture     = nullptr;
    size_t               count       = 0;
    bool                 isSmall     = text.size() <= FormatStamp::kMaxBytes;
    bool                 isShallow   = false;
    bool                 hasTracks   = false;
    bool                 isInRange   = false;



    outStamp = FormatStamp();

    CBREx (isSmall, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isShallow = IsNestingInLimit (text);
    CBREx (isShallow, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = JsonParser::Parse (std::string (text), root, error);
    CHREx (hr, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = ReadHeader (root, outStamp);
    CHR (hr);

    if (root.HasArray ("decodeSettings", ranges))
    {
        count = ranges->GetArraySize();

        for (size_t i = 0; i < count; i++)
        {
            DecodeRange  range;

            hr = ReadRange (ranges->GetArrayElement (i), range);
            CHR (hr);

            outStamp.decodeRanges.push_back (range);
        }
    }

    if (root.HasObject ("capture", capture))
    {
        ReadCapture (*capture, outStamp.capture);

        outStamp.hasCapture = true;
    }

    hr = ReadNotes (root, outStamp.notes);
    CHR (hr);

    hasTracks = root.HasArray ("tracks", tracks);
    CBREx (hasTracks, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    count     = tracks->GetArraySize();
    isInRange = count <= FormatStamp::kMaxPositions;
    CBREx (isInRange, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (size_t i = 0; i < count; i++)
    {
        StampTrack  track;

        hr = ReadTrack (tracks->GetArrayElement (i), track);
        CHR (hr);

        outStamp.tracks.push_back (std::move (track));
    }

Error:
    if (FAILED (hr))
    {
        outStamp = FormatStamp();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::ReadHeader
//
//  The format, version, writer, date and source. The writer and the date
//  are informational, so a stamp without them reads.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampJson::ReadHeader (const JsonValue & root, FormatStamp & outStamp)
{
    HRESULT      hr          = S_OK;
    std::string  format;
    std::string  version;
    std::string  source;
    bool         isObject    = root.GetType() == JsonType::Object;
    bool         isOurs      = false;
    bool         isKnown     = false;



    CBREx (isObject, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = root.GetString ("format", format);
    CHREx (hr, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isOurs = format == kFormatName;
    CBREx (isOurs, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = root.GetString ("version", version);
    CHREx (hr, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = ReadVersion (version, outStamp.versionMajor, outStamp.versionMinor);
    CHR (hr);

    hr = root.GetString ("source", source);
    CHREx (hr, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isKnown = TryParseSource (source, outStamp.source);
    CBREx (isKnown, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    root.HasString ("writer",  outStamp.writer);
    root.HasString ("written", outStamp.written);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::ReadVersion
//
//  "major.minor", accepted only for this build's major version.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampJson::ReadVersion (const std::string & text, int & outMajor, int & outMinor)
{
    HRESULT                 hr          = S_OK;
    size_t                  dot         = text.find ('.');
    bool                    hasDot      = dot != std::string::npos;
    std::from_chars_result  major;
    std::from_chars_result  minor;
    bool                    isMajorRead = false;
    bool                    isMinorRead = false;
    bool                    isKnown     = false;



    CBREx (hasDot, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    major       = std::from_chars (text.data(), text.data() + dot, outMajor);
    minor       = std::from_chars (text.data() + dot + 1, text.data() + text.size(), outMinor);
    isMajorRead = major.ec == std::errc() && major.ptr == text.data() + dot;
    isMinorRead = minor.ec == std::errc() && minor.ptr == text.data() + text.size();

    CBREx (isMajorRead, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (isMinorRead, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isKnown = outMajor == FormatStamp::kVersionMajor && outMinor >= 0;
    CBREx (isKnown, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::IsNestingInLimit
//
//  Whether no array or object in the text is nested deeper than
//  kMaxNesting. The JSON parser recurses once per level, so a file built to
//  nest thousands of levels would otherwise exhaust the stack. Brackets
//  inside strings do not count.
//
////////////////////////////////////////////////////////////////////////////////

bool FormatStampJson::IsNestingInLimit (std::string_view text)
{
    int   depth     = 0;
    int   deepest   = 0;
    bool  isString  = false;
    bool  isEscaped = false;



    for (char ch : text)
    {
        if (isString)
        {
            isString  = isEscaped || ch != '"';
            isEscaped = !isEscaped && ch == '\\';
            continue;
        }

        if (ch == '"')
        {
            isString = true;
        }
        else if (ch == '[' || ch == '{')
        {
            depth++;
            deepest = std::max (deepest, depth);
        }
        else if (ch == ']' || ch == '}')
        {
            depth--;
        }
    }

    return deepest <= kMaxNesting;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::NibblesToText
//
//  "D5 AA 96": two uppercase hex digits per nibble, separated by spaces.
//
////////////////////////////////////////////////////////////////////////////////

std::string FormatStampJson::NibblesToText (std::span<const Byte> nibbles)
{
    std::string  text;



    for (Byte nibble : nibbles)
    {
        if (!text.empty())
        {
            text += ' ';
        }

        text += std::format ("{:02X}", nibble);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::TryParseNibbles
//
//  The reverse of NibblesToText. False for anything but one to three
//  two-digit hex values separated by single spaces.
//
////////////////////////////////////////////////////////////////////////////////

bool FormatStampJson::TryParseNibbles (std::string_view text, vector<Byte> & outNibbles)
{
    constexpr size_t  kTokenChars = 2;
    constexpr size_t  kStride     = 3;
    constexpr int     kHexBase    = 16;
    constexpr size_t  kMaxNibbles = 3;
    bool              isValid     = !text.empty() && (text.size() + 1) % kStride == 0;
    size_t            at          = 0;



    outNibbles.clear();

    while (isValid && at < text.size())
    {
        Byte                    value  = 0;
        std::from_chars_result  result = std::from_chars (text.data() + at, text.data() + at + kTokenChars, value, kHexBase);

        isValid = result.ec == std::errc() && result.ptr == text.data() + at + kTokenChars;
        isValid = isValid && (at + kTokenChars == text.size() || text[at + kTokenChars] == ' ');

        outNibbles.push_back (value);
        at += kStride;
    }

    isValid = isValid && outNibbles.size() <= kMaxNibbles;

    if (!isValid)
    {
        outNibbles.clear();
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::EncodingToText
//
////////////////////////////////////////////////////////////////////////////////

const char * FormatStampJson::EncodingToText (StampEncoding encoding)
{
    const char *  text = "empty";



    switch (encoding)
    {
        case StampEncoding::Empty:       text = "empty";       break;
        case StampEncoding::Sixteen:     text = "16-sector";   break;
        case StampEncoding::Thirteen:    text = "13-sector";   break;
        case StampEncoding::Both:        text = "both";        break;
        case StampEncoding::Unformatted: text = "unformatted"; break;
        case StampEncoding::Nonstandard: text = "nonstandard"; break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::ChecksumToText
//
////////////////////////////////////////////////////////////////////////////////

const char * FormatStampJson::ChecksumToText (StampChecksum checksum)
{
    const char *  text = "standard";



    switch (checksum)
    {
        case StampChecksum::Standard:   text = "standard";    break;
        case StampChecksum::NotChecked: text = "not-checked"; break;
        case StampChecksum::Failing:    text = "failing";     break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::SourceToText
//
////////////////////////////////////////////////////////////////////////////////

const char * FormatStampJson::SourceToText (StampSource source)
{
    return source == StampSource::Inspector ? "inspector" : "capture";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::TryParseEncoding
//
////////////////////////////////////////////////////////////////////////////////

bool FormatStampJson::TryParseEncoding (const std::string & text, StampEncoding & outEncoding)
{
    constexpr StampEncoding  kAll[] = { StampEncoding::Empty, StampEncoding::Sixteen, StampEncoding::Thirteen,
                                        StampEncoding::Both,  StampEncoding::Unformatted, StampEncoding::Nonstandard };
    bool  isKnown = false;



    for (StampEncoding encoding : kAll)
    {
        if (text == EncodingToText (encoding))
        {
            outEncoding = encoding;
            isKnown     = true;
        }
    }

    return isKnown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::TryParseChecksum
//
////////////////////////////////////////////////////////////////////////////////

bool FormatStampJson::TryParseChecksum (const std::string & text, StampChecksum & outChecksum)
{
    constexpr StampChecksum  kAll[]  = { StampChecksum::Standard, StampChecksum::NotChecked, StampChecksum::Failing };
    bool                     isKnown = false;



    for (StampChecksum checksum : kAll)
    {
        if (text == ChecksumToText (checksum))
        {
            outChecksum = checksum;
            isKnown     = true;
        }
    }

    return isKnown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::TryParseSource
//
////////////////////////////////////////////////////////////////////////////////

bool FormatStampJson::TryParseSource (const std::string & text, StampSource & outSource)
{
    bool  isCapture   = text == SourceToText (StampSource::Capture);
    bool  isInspector = text == SourceToText (StampSource::Inspector);



    outSource = isInspector ? StampSource::Inspector : StampSource::Capture;

    return isCapture || isInspector;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::MakeNibbleList
//
////////////////////////////////////////////////////////////////////////////////

JsonValue FormatStampJson::MakeNibbleList (const vector<vector<Byte>> & lists)
{
    vector<JsonValue>  values;



    for (const vector<Byte> & nibbles : lists)
    {
        values.push_back (JsonValue (NibblesToText (nibbles)));
    }

    return JsonValue (std::move (values));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::MakeByteList
//
////////////////////////////////////////////////////////////////////////////////

JsonValue FormatStampJson::MakeByteList (const vector<Byte> & values)
{
    vector<JsonValue>  list;



    for (Byte value : values)
    {
        list.push_back (JsonValue ((double) value));
    }

    return JsonValue (std::move (list));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::MakeIntList
//
////////////////////////////////////////////////////////////////////////////////

JsonValue FormatStampJson::MakeIntList (const vector<int> & values)
{
    vector<JsonValue>  list;



    for (int value : values)
    {
        list.push_back (JsonValue ((double) value));
    }

    return JsonValue (std::move (list));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::MakeMarkList
//
//  Custom decode marks as their text, ?? for a nibble that matches any.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue FormatStampJson::MakeMarkList (const vector<DiskMarkPattern> & marks)
{
    vector<JsonValue>  list;



    for (const DiskMarkPattern & mark : marks)
    {
        list.push_back (JsonValue (mark.ToText()));
    }

    return JsonValue (std::move (list));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::MakeNotes
//
////////////////////////////////////////////////////////////////////////////////

JsonValue FormatStampJson::MakeNotes (const vector<ProtectionNote> & notes)
{
    vector<JsonValue>  list;



    for (const ProtectionNote & note : notes)
    {
        vector<pair<string, JsonValue>>  object;

        object.push_back ({ "kind",   JsonValue (note.kind) });
        object.push_back ({ "params", note.params });

        list.push_back (JsonValue (std::move (object)));
    }

    return JsonValue (std::move (list));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::MakeRange
//
////////////////////////////////////////////////////////////////////////////////

JsonValue FormatStampJson::MakeRange (const DecodeRange & range)
{
    vector<pair<string, JsonValue>>  object;



    object.push_back ({ "firstTrack",       JsonValue ((double) range.firstTrack) });
    object.push_back ({ "lastTrack",        JsonValue ((double) range.lastTrack) });
    object.push_back ({ "address16",        MakeMarkList (range.customMarks.address16) });
    object.push_back ({ "address13",        MakeMarkList (range.customMarks.address13) });
    object.push_back ({ "data",             MakeMarkList (range.customMarks.data) });
    object.push_back ({ "addressEpilogue",  MakeMarkList (range.customMarks.addressEpilogue) });
    object.push_back ({ "dataEpilogue",     MakeMarkList (range.customMarks.dataEpilogue) });
    object.push_back ({ "matchStandardToo", JsonValue (range.matchStandardToo) });
    object.push_back ({ "addressChecksum",  JsonValue (range.checks.isAddressChecksumOn) });
    object.push_back ({ "dataChecksum",     JsonValue (range.checks.isDataChecksumOn) });
    object.push_back ({ "epilogue",         JsonValue (range.checks.isEpilogueOn) });

    return JsonValue (std::move (object));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::MakeCapture
//
//  The disk-level capture facts. The revolution is left out when the
//  capture did not measure it.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue FormatStampJson::MakeCapture (const CaptureFacts & capture)
{
    vector<pair<string, JsonValue>>  object;



    object.push_back ({ "reads", JsonValue ((double) capture.reads) });

    if (capture.hasRevolution)
    {
        object.push_back ({ "revolutionCycles", JsonValue (capture.revolutionCycles) });
    }

    object.push_back ({ "synchronized", JsonValue (capture.isSynchronized) });
    object.push_back ({ "betweenRead",  JsonValue ((double) capture.betweenRead) });
    object.push_back ({ "betweenTotal", JsonValue ((double) capture.betweenTotal) });

    return JsonValue (std::move (object));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::MakeTrack
//
////////////////////////////////////////////////////////////////////////////////

JsonValue FormatStampJson::MakeTrack (const StampTrack & track)
{
    vector<pair<string, JsonValue>>  object;
    vector<pair<string, JsonValue>>  capture;



    object.push_back ({ "position",            JsonValue ((double) track.position) });
    object.push_back ({ "encoding",            JsonValue (std::string (EncodingToText (track.encoding))) });
    object.push_back ({ "addressPrologs",      MakeNibbleList (track.addressPrologs) });
    object.push_back ({ "addressEpilogs",      MakeNibbleList (track.addressEpilogs) });
    object.push_back ({ "dataPrologs",         MakeNibbleList (track.dataPrologs) });
    object.push_back ({ "dataEpilogs",         MakeNibbleList (track.dataEpilogs) });
    object.push_back ({ "sectors",             MakeByteList (track.sectors) });
    object.push_back ({ "volumes",             MakeByteList (track.volumes) });
    object.push_back ({ "addressChecksum",     JsonValue (std::string (ChecksumToText (track.addressChecksum))) });
    object.push_back ({ "dataChecksum",        JsonValue (std::string (ChecksumToText (track.dataChecksum))) });
    object.push_back ({ "sectorsFound",        JsonValue ((double) track.sectorsFound) });
    object.push_back ({ "sectorsGood",         JsonValue ((double) track.sectorsGood) });
    object.push_back ({ "verified",            JsonValue (track.isVerified) });
    object.push_back ({ "changedAfterCapture", JsonValue (track.isChangedAfterCapture) });

    if (track.hasCapture)
    {
        capture.push_back ({ "weak",            JsonValue (track.capture.isWeak) });
        capture.push_back ({ "unstable",        JsonValue (track.capture.isUnstable) });
        capture.push_back ({ "ownStream",       JsonValue (track.capture.hasSeparateStream) });
        capture.push_back ({ "replacedSectors", MakeIntList (track.capture.replacedSectors) });

        object.push_back ({ "capture", JsonValue (std::move (capture)) });
    }

    object.push_back ({ "notes", MakeNotes (track.notes) });

    return JsonValue (std::move (object));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::ReadNibbleList
//
//  A list of nibble strings. A missing key is an empty list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampJson::ReadNibbleList (const JsonValue & object, const char * key, vector<vector<Byte>> & outLists)
{
    HRESULT            hr      = S_OK;
    const JsonValue  * list    = nullptr;
    size_t             count   = 0;
    bool               hasList = false;



    outLists.clear();

    hasList = object.HasArray (key, list);
    BAIL_OUT_IF (!hasList, S_OK);

    count = list->GetArraySize();

    for (size_t i = 0; i < count; i++)
    {
        const JsonValue  & item     = list->GetArrayElement (i);
        vector<Byte>       nibbles;
        bool               isString = item.GetType() == JsonType::String;
        bool               isParsed = false;

        CBREx (isString, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        isParsed = TryParseNibbles (item.GetString(), nibbles);
        CBREx (isParsed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        outLists.push_back (std::move (nibbles));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::ReadByteList
//
//  A list of integers 0 to 255. A missing key is an empty list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampJson::ReadByteList (const JsonValue & object, const char * key, vector<Byte> & outValues)
{
    constexpr double    kMaxByte = 255.0;
    HRESULT             hr       = S_OK;
    const JsonValue   * list     = nullptr;
    size_t              count    = 0;
    bool                hasList  = false;



    outValues.clear();

    hasList = object.HasArray (key, list);
    BAIL_OUT_IF (!hasList, S_OK);

    count = list->GetArraySize();

    for (size_t i = 0; i < count; i++)
    {
        const JsonValue  & item     = list->GetArrayElement (i);
        bool               isNumber = item.GetType() == JsonType::Number;
        double             value    = item.GetNumber();
        bool               isByte   = value >= 0.0 && value <= kMaxByte;

        CBREx (isNumber, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
        CBREx (isByte,   HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        outValues.push_back ((Byte) value);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::ReadIntList
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampJson::ReadIntList (const JsonValue & object, const char * key, vector<int> & outValues)
{
    HRESULT            hr      = S_OK;
    const JsonValue  * list    = nullptr;
    size_t             count   = 0;
    bool               hasList = false;



    outValues.clear();

    hasList = object.HasArray (key, list);
    BAIL_OUT_IF (!hasList, S_OK);

    count = list->GetArraySize();

    for (size_t i = 0; i < count; i++)
    {
        const JsonValue  & item     = list->GetArrayElement (i);
        bool               isNumber = item.GetType() == JsonType::Number;

        CBREx (isNumber, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        outValues.push_back (item.GetInt());
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::ReadMarkList
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampJson::ReadMarkList (const JsonValue & object, const char * key, vector<DiskMarkPattern> & outMarks)
{
    HRESULT            hr      = S_OK;
    const JsonValue  * list    = nullptr;
    size_t             count   = 0;
    bool               hasList = false;



    outMarks.clear();

    hasList = object.HasArray (key, list);
    BAIL_OUT_IF (!hasList, S_OK);

    count = list->GetArraySize();

    for (size_t i = 0; i < count; i++)
    {
        const JsonValue  & item     = list->GetArrayElement (i);
        DiskMarkPattern    mark;
        std::string        error;
        bool               isString = item.GetType() == JsonType::String;
        bool               isParsed = false;

        CBREx (isString, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        isParsed = DiskMarkPattern::TryParse (item.GetString(), mark, error);
        CBREx (isParsed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        outMarks.push_back (mark);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::ReadNotes
//
//  The object's notes, each with its params as they were read. A missing
//  list is empty.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampJson::ReadNotes (const JsonValue & object, vector<ProtectionNote> & outNotes)
{
    HRESULT            hr      = S_OK;
    const JsonValue  * list    = nullptr;
    size_t             count   = 0;
    bool               hasList = false;



    outNotes.clear();

    hasList = object.HasArray ("notes", list);
    BAIL_OUT_IF (!hasList, S_OK);

    count = list->GetArraySize();

    for (size_t i = 0; i < count; i++)
    {
        const JsonValue  & item     = list->GetArrayElement (i);
        const JsonValue  * params   = nullptr;
        ProtectionNote     note;
        bool               isObject = item.GetType() == JsonType::Object;

        CBREx (isObject, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        hr = item.GetString ("kind", note.kind);
        CHREx (hr, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        if (item.HasObject ("params", params))
        {
            note.params = *params;
        }

        outNotes.push_back (std::move (note));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::ReadRange
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampJson::ReadRange (const JsonValue & value, DecodeRange & outRange)
{
    HRESULT  hr       = S_OK;
    bool     isObject = value.GetType() == JsonType::Object;



    CBREx (isObject, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = value.GetInt ("firstTrack", outRange.firstTrack);
    CHREx (hr, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = value.GetInt ("lastTrack", outRange.lastTrack);
    CHREx (hr, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = ReadMarkList (value, "address16", outRange.customMarks.address16);
    CHR (hr);

    hr = ReadMarkList (value, "address13", outRange.customMarks.address13);
    CHR (hr);

    hr = ReadMarkList (value, "data", outRange.customMarks.data);
    CHR (hr);

    hr = ReadMarkList (value, "addressEpilogue", outRange.customMarks.addressEpilogue);
    CHR (hr);

    hr = ReadMarkList (value, "dataEpilogue", outRange.customMarks.dataEpilogue);
    CHR (hr);

    value.HasBool ("matchStandardToo", outRange.matchStandardToo);
    value.HasBool ("addressChecksum",  outRange.checks.isAddressChecksumOn);
    value.HasBool ("dataChecksum",     outRange.checks.isDataChecksumOn);
    value.HasBool ("epilogue",         outRange.checks.isEpilogueOn);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::ReadCapture
//
////////////////////////////////////////////////////////////////////////////////

void FormatStampJson::ReadCapture (const JsonValue & value, CaptureFacts & outCapture)
{
    value.HasInt  ("reads",        outCapture.reads);
    value.HasBool ("synchronized", outCapture.isSynchronized);
    value.HasInt  ("betweenRead",  outCapture.betweenRead);
    value.HasInt  ("betweenTotal", outCapture.betweenTotal);

    outCapture.hasRevolution = value.HasNumber ("revolutionCycles", outCapture.revolutionCycles);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::ReadTrackCapture
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampJson::ReadTrackCapture (const JsonValue & value, TrackCaptureFacts & outCapture)
{
    HRESULT  hr = S_OK;



    value.HasBool ("weak",      outCapture.isWeak);
    value.HasBool ("unstable",  outCapture.isUnstable);
    value.HasBool ("ownStream", outCapture.hasSeparateStream);

    hr = ReadIntList (value, "replacedSectors", outCapture.replacedSectors);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJson::ReadTrack
//
//  One track entry. The position and the encoding are required; every
//  other key defaults to its empty value.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampJson::ReadTrack (const JsonValue & value, StampTrack & outTrack)
{
    HRESULT              hr          = S_OK;
    std::string          encoding;
    std::string          checksum;
    const JsonValue    * capture     = nullptr;
    bool                 isObject    = value.GetType() == JsonType::Object;
    bool                 isInRange   = false;
    bool                 isKnown     = false;



    CBREx (isObject, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = value.GetInt ("position", outTrack.position);
    CHREx (hr, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isInRange = outTrack.position >= 0 && outTrack.position < FormatStamp::kMaxPositions;
    CBREx (isInRange, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = value.GetString ("encoding", encoding);
    CHREx (hr, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isKnown = TryParseEncoding (encoding, outTrack.encoding);
    CBREx (isKnown, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = ReadNibbleList (value, "addressPrologs", outTrack.addressPrologs);
    CHR (hr);

    hr = ReadNibbleList (value, "addressEpilogs", outTrack.addressEpilogs);
    CHR (hr);

    hr = ReadNibbleList (value, "dataPrologs", outTrack.dataPrologs);
    CHR (hr);

    hr = ReadNibbleList (value, "dataEpilogs", outTrack.dataEpilogs);
    CHR (hr);

    hr = ReadByteList (value, "sectors", outTrack.sectors);
    CHR (hr);

    hr = ReadByteList (value, "volumes", outTrack.volumes);
    CHR (hr);

    if (value.HasString ("addressChecksum", checksum))
    {
        isKnown = TryParseChecksum (checksum, outTrack.addressChecksum);
        CBREx (isKnown, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    }

    if (value.HasString ("dataChecksum", checksum))
    {
        isKnown = TryParseChecksum (checksum, outTrack.dataChecksum);
        CBREx (isKnown, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    }

    value.HasInt  ("sectorsFound",        outTrack.sectorsFound);
    value.HasInt  ("sectorsGood",         outTrack.sectorsGood);
    value.HasBool ("verified",            outTrack.isVerified);
    value.HasBool ("changedAfterCapture", outTrack.isChangedAfterCapture);

    if (value.HasObject ("capture", capture))
    {
        hr = ReadTrackCapture (*capture, outTrack.capture);
        CHR (hr);

        outTrack.hasCapture = true;
    }

    hr = ReadNotes (value, outTrack.notes);
    CHR (hr);

Error:
    return hr;
}

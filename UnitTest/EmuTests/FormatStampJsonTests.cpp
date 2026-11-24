#include "Pch.h"

#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampJson.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampJsonTests
//
//  A format stamp written as JSON and read back, and the text a read rejects.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FormatStampJsonTests)
{
public:

    static FormatStamp MakeFullStamp()
    {
        FormatStamp     stamp;
        StampTrack      track;
        DecodeRange     range;
        DiskMarkPattern mark;
        std::string     error;
        ProtectionNote  note;



        stamp.writer                   = "Casso 1.32.0";
        stamp.written                  = "2026-11-23T21:04:00Z";
        stamp.source                   = StampSource::Capture;
        stamp.hasCapture               = true;
        stamp.capture.reads            = 3;
        stamp.capture.hasRevolution    = true;
        stamp.capture.revolutionCycles = 199850.5;
        stamp.capture.isSynchronized   = true;
        stamp.capture.betweenRead      = 4;
        stamp.capture.betweenTotal     = 102;

        DiskMarkPattern::TryParse ("D4 AA ??", mark, error);
        range.firstTrack = 1;
        range.lastTrack  = 34;
        range.customMarks.address16.push_back (mark);
        range.matchStandardToo           = false;
        range.checks.isDataChecksumOn    = false;
        stamp.decodeRanges.push_back (range);

        note.kind = "track-alignment";
        note.params = JsonValue (vector<pair<string, JsonValue>> { { "tracks", JsonValue (vector<JsonValue> { JsonValue (1.0), JsonValue (2.0) }) } });
        stamp.notes.push_back (note);

        track.position              = 4;
        track.encoding              = StampEncoding::Thirteen;
        track.addressPrologs        = { { 0xD7, 0xAA, 0xB7 }, { 0xDF, 0xAA, 0xF7 } };
        track.addressEpilogs        = { { 0xDE, 0xAA, 0xEB } };
        track.dataPrologs           = { { 0xD7, 0xAA, 0xBD } };
        track.dataEpilogs           = { { 0xDE, 0xAA } };
        track.sectors               = { 0, 10, 7 };
        track.volumes               = { 254 };
        track.addressChecksum       = StampChecksum::Standard;
        track.dataChecksum          = StampChecksum::Failing;
        track.sectorsFound          = 13;
        track.sectorsGood           = 12;
        track.isVerified            = false;
        track.isChangedAfterCapture = true;
        track.hasBitsCrc            = true;
        track.bitsCrc               = 0x1A2B3C4D;
        track.hasCapture            = true;
        track.capture.isWeak        = true;
        track.capture.isUnstable    = false;
        track.capture.hasSeparateStream = true;
        track.capture.replacedSectors   = { 4, 9 };
        track.notes.push_back ({ "weak-bits", JsonValue (vector<pair<string, JsonValue>> {}) });
        stamp.tracks.push_back (track);

        return stamp;
    }



    TEST_METHOD (Write_ThenParse_RoundTripsEveryField)
    {
        FormatStamp  stamp = MakeFullStamp();
        FormatStamp  read;
        std::string  text;
        HRESULT      hr    = S_OK;



        hr = FormatStampJson::Write (stamp, text);
        Assert::IsTrue (SUCCEEDED (hr));

        hr = FormatStampJson::Parse (text, read);
        Assert::IsTrue (SUCCEEDED (hr), L"the written text parses");

        Assert::AreEqual (stamp.writer,  read.writer);
        Assert::AreEqual (stamp.written, read.written);
        Assert::IsTrue   (read.source == StampSource::Capture);
        Assert::IsTrue   (read.hasCapture);
        Assert::AreEqual (3,      read.capture.reads);
        Assert::IsTrue   (read.capture.hasRevolution);
        Assert::AreEqual (199850.5, read.capture.revolutionCycles);
        Assert::IsTrue   (read.capture.isSynchronized);
        Assert::AreEqual (4,      read.capture.betweenRead);
        Assert::AreEqual (102,    read.capture.betweenTotal);

        Assert::AreEqual ((size_t) 1, read.decodeRanges.size());
        Assert::AreEqual (1,  read.decodeRanges[0].firstTrack);
        Assert::AreEqual (34, read.decodeRanges[0].lastTrack);
        Assert::AreEqual (std::string ("D4 AA ??"), read.decodeRanges[0].customMarks.address16[0].ToText());
        Assert::IsFalse  (read.decodeRanges[0].matchStandardToo);
        Assert::IsFalse  (read.decodeRanges[0].checks.isDataChecksumOn);
        Assert::IsTrue   (read.decodeRanges[0].checks.isAddressChecksumOn);

        Assert::AreEqual ((size_t) 1, read.notes.size());
        Assert::AreEqual (std::string ("track-alignment"), read.notes[0].kind);
        Assert::IsTrue   (read.notes[0].params.GetType() == JsonType::Object, L"a note's params are kept");

        Assert::AreEqual ((size_t) 1, read.tracks.size());

        const StampTrack &  track = read.tracks[0];

        Assert::AreEqual (4, track.position);
        Assert::IsTrue   (track.encoding == StampEncoding::Thirteen);
        Assert::IsTrue   (track.addressPrologs == stamp.tracks[0].addressPrologs);
        Assert::IsTrue   (track.addressEpilogs == stamp.tracks[0].addressEpilogs);
        Assert::IsTrue   (track.dataPrologs    == stamp.tracks[0].dataPrologs);
        Assert::IsTrue   (track.dataEpilogs    == stamp.tracks[0].dataEpilogs);
        Assert::IsTrue   (track.sectors        == stamp.tracks[0].sectors);
        Assert::IsTrue   (track.volumes        == stamp.tracks[0].volumes);
        Assert::IsTrue   (track.dataChecksum   == StampChecksum::Failing);
        Assert::AreEqual (13, track.sectorsFound);
        Assert::AreEqual (12, track.sectorsGood);
        Assert::IsFalse  (track.isVerified);
        Assert::IsTrue   (track.isChangedAfterCapture);
        Assert::IsTrue   (track.hasBitsCrc);
        Assert::AreEqual (0x1A2B3C4Du, track.bitsCrc);
        Assert::IsTrue   (track.hasCapture);
        Assert::IsTrue   (track.capture.isWeak);
        Assert::IsTrue   (track.capture.hasSeparateStream);
        Assert::IsTrue   (track.capture.replacedSectors == vector<int> { 4, 9 });
        Assert::AreEqual ((size_t) 1, track.notes.size());
    }



    TEST_METHOD (Write_ListsNibblesAsUppercaseHexText)
    {
        FormatStamp  stamp = MakeFullStamp();
        std::string  text;



        FormatStampJson::Write (stamp, text);

        Assert::IsTrue (text.find ("\"D7 AA B7\"") != std::string::npos, L"a prolog is two-digit uppercase hex separated by spaces");
        Assert::IsTrue (text.find ("\"casso-woz-stamp\"") != std::string::npos);
        Assert::IsTrue (text.find ("\"1.0\"") != std::string::npos);
    }



    TEST_METHOD (Parse_AnotherFormat_IsInvalidData)
    {
        FormatStamp  read;
        HRESULT      hr = FormatStampJson::Parse (R"({ "format": "other", "version": "1.0", "source": "capture", "tracks": [] })", read);



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }



    TEST_METHOD (Parse_AnotherMajorVersion_IsInvalidData)
    {
        FormatStamp  read;
        HRESULT      hr = FormatStampJson::Parse (R"({ "format": "casso-woz-stamp", "version": "2.0", "source": "capture", "tracks": [] })", read);



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }



    TEST_METHOD (Parse_LaterMinorVersionWithAnUnknownKey_Reads)
    {
        FormatStamp  read;
        HRESULT      hr = FormatStampJson::Parse (R"({ "format": "casso-woz-stamp", "version": "1.5", "source": "inspector", "later": { "x": 1 },
                                                      "tracks": [ { "position": 8, "encoding": "16-sector", "later": true } ] })", read);



        Assert::IsTrue   (SUCCEEDED (hr));
        Assert::AreEqual (5, read.versionMinor);
        Assert::IsTrue   (read.source == StampSource::Inspector);
        Assert::AreEqual ((size_t) 1, read.tracks.size());
        Assert::IsTrue   (read.tracks[0].encoding == StampEncoding::Sixteen);
    }



    TEST_METHOD (Parse_TextOverTheSizeLimit_IsInvalidData)
    {
        FormatStamp  read;
        std::string  text = R"({ "format": "casso-woz-stamp", "version": "1.0", "source": "capture", "tracks": [] })";
        HRESULT      hr   = S_OK;



        text.append (FormatStamp::kMaxBytes, ' ');

        hr = FormatStampJson::Parse (text, read);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }



    TEST_METHOD (Parse_DeeplyNestedText_IsInvalidData)
    {
        FormatStamp  read;
        std::string  text = R"({ "format": "casso-woz-stamp", "version": "1.0", "source": "capture", "tracks": [], "deep": )";
        HRESULT      hr   = S_OK;



        text.append (FormatStampJson::kMaxNesting * 2, '[');
        text.append (FormatStampJson::kMaxNesting * 2, ']');
        text += " }";

        hr = FormatStampJson::Parse (text, read);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }



    TEST_METHOD (Parse_BracketsInsideAString_DoNotCountAsNesting)
    {
        FormatStamp  read;
        std::string  text = R"({ "format": "casso-woz-stamp", "version": "1.0", "source": "capture", "writer": ")";
        HRESULT      hr   = S_OK;



        text.append (FormatStampJson::kMaxNesting * 2, '[');
        text += R"(", "tracks": [] })";

        hr = FormatStampJson::Parse (text, read);

        Assert::IsTrue (SUCCEEDED (hr));
    }



    TEST_METHOD (Parse_BadNibbleText_IsInvalidData)
    {
        FormatStamp  read;
        HRESULT      hr = FormatStampJson::Parse (R"({ "format": "casso-woz-stamp", "version": "1.0", "source": "capture",
                                                      "tracks": [ { "position": 0, "encoding": "16-sector", "addressPrologs": [ "D5AA96" ] } ] })", read);



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
        Assert::IsTrue   (read.tracks.empty(), L"a failed parse leaves an empty stamp");
    }



    TEST_METHOD (Parse_PositionPastTheLastQuarterTrack_IsInvalidData)
    {
        FormatStamp  read;
        HRESULT      hr = FormatStampJson::Parse (R"({ "format": "casso-woz-stamp", "version": "1.0", "source": "capture",
                                                      "tracks": [ { "position": 160, "encoding": "16-sector" } ] })", read);



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }



    TEST_METHOD (Parse_NotJson_IsInvalidData)
    {
        FormatStamp  read;
        HRESULT      hr = FormatStampJson::Parse ("CASS but not JSON", read);



        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr);
    }



    TEST_METHOD (TryParseNibbles_AcceptsOneToThreeHexPairs)
    {
        vector<Byte>  nibbles;



        Assert::IsTrue  (FormatStampJson::TryParseNibbles ("D5 AA 96", nibbles));
        Assert::IsTrue  (nibbles == vector<Byte> { 0xD5, 0xAA, 0x96 });
        Assert::IsTrue  (FormatStampJson::TryParseNibbles ("DE AA", nibbles));
        Assert::IsTrue  (nibbles == vector<Byte> { 0xDE, 0xAA });
        Assert::IsFalse (FormatStampJson::TryParseNibbles ("",            nibbles));
        Assert::IsFalse (FormatStampJson::TryParseNibbles ("D5 AA 96 AD", nibbles));
        Assert::IsFalse (FormatStampJson::TryParseNibbles ("D5  AA",      nibbles));
        Assert::IsFalse (FormatStampJson::TryParseNibbles ("G5 AA",       nibbles));
    }
};

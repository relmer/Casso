#include "Pch.h"

#include "Core/IMachineState.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StateStreamTests
//
//  The machine state stream: sections with a tag, a version and a payload
//  size, holding fixed-width little-endian values. A snapshot is only as safe
//  as the reader's refusals, so most of these feed it something wrong -- a
//  foreign tag, a newer version, a short stream, a payload not fully read --
//  and check that it fails rather than loading what it was given.
//
////////////////////////////////////////////////////////////////////////////////

namespace StateStream
{
    static constexpr uint32_t  kTagA        = IMachineState::MakeTag ('T', 'S', 'T', 'A');
    static constexpr uint32_t  kTagB        = IMachineState::MakeTag ('T', 'S', 'T', 'B');
    static constexpr uint16_t  kVersion     = 3;
    static constexpr Byte      kByteValue   = 0xA5;
    static constexpr Byte      kBadBool     = 2;
    static constexpr Word      kWordValue   = 0x1234;
    static constexpr uint32_t  kDwordValue  = 0x89ABCDEF;
    static constexpr uint64_t  kQwordValue  = 0x0123456789ABCDEFull;
    static constexpr HRESULT   kInvalidData = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
    static constexpr HRESULT   kRevision    = HRESULT_FROM_WIN32 (ERROR_REVISION_MISMATCH);
    static constexpr Byte      kRun[]       = { 1, 2, 3, 4, 5 };

    // Offsets in a stream holding one section: the version field, the size
    // field and the first payload byte.
    static constexpr size_t    kVersionOffset = sizeof (uint32_t);
    static constexpr size_t    kPayloadOffset = StateWriter::kSectionHeaderSize;
    static constexpr size_t    kSizeOffset    = kPayloadOffset - sizeof (uint32_t);


    static std::vector<Byte> MakeOneWordSection (uint32_t tag, uint16_t version)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        writer.BeginSection (tag, version);
        writer.WriteWord    (kWordValue);

        hr = writer.EndSection();
        Assert::AreEqual (S_OK, hr);

        return writer.GetBytes();
    }


    static std::vector<Byte> MakeOneByteSection (Byte value)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        writer.BeginSection (kTagA, kVersion);
        writer.WriteByte    (value);

        hr = writer.EndSection();
        Assert::AreEqual (S_OK, hr);

        return writer.GetBytes();
    }


    static std::vector<Byte> MakeEveryWidthSection()
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        writer.BeginSection (kTagA, kVersion);
        writer.WriteByte    (kByteValue);
        writer.WriteBool    (true);
        writer.WriteWord    (kWordValue);
        writer.WriteUInt32  (kDwordValue);
        writer.WriteUInt64  (kQwordValue);
        writer.WriteBytes   (kRun, sizeof (kRun));

        hr = writer.EndSection();
        Assert::AreEqual (S_OK, hr);
        Assert::IsFalse  (writer.HasOpenSection());

        return writer.GetBytes();
    }


    // Section A holding section B (one word), then one byte of A's own.
    static std::vector<Byte> MakeNestedSections()
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        writer.BeginSection (kTagA, kVersion);
        writer.BeginSection (kTagB, kVersion);
        writer.WriteWord    (kWordValue);

        hr = writer.EndSection();
        Assert::AreEqual (S_OK, hr);

        writer.WriteByte (kByteValue);

        hr = writer.EndSection();
        Assert::AreEqual (S_OK, hr);

        return writer.GetBytes();
    }


    TEST_CLASS (StateStreamTests)
    {
    public:
        TEST_METHOD (EveryWidthRoundTrips)
        {
            std::vector<Byte>  bytes     = MakeEveryWidthSection();
            StateReader        reader (bytes);
            HRESULT            hr        = S_OK;
            uint16_t           version   = 0;
            Byte               byteOut   = 0;
            bool               boolOut   = false;
            Word               wordOut   = 0;
            uint32_t           dwordOut  = 0;
            uint64_t           qwordOut  = 0;
            Byte               runOut[sizeof (kRun)] = {};



            hr = reader.BeginSection (kTagA, kVersion, version);
            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual (kVersion, version);

            reader.ReadByte   (byteOut);
            reader.ReadBool   (boolOut);
            reader.ReadWord   (wordOut);
            reader.ReadUInt32 (dwordOut);
            reader.ReadUInt64 (qwordOut);
            reader.ReadBytes  (runOut, sizeof (runOut));

            hr = reader.EndSection();
            Assert::AreEqual (S_OK, hr);
            Assert::IsTrue   (reader.IsAtEnd());

            Assert::AreEqual (kByteValue,  byteOut);
            Assert::IsTrue   (boolOut);
            Assert::AreEqual (kWordValue,  wordOut);
            Assert::AreEqual (kDwordValue, dwordOut);
            Assert::AreEqual (kQwordValue, qwordOut);
            Assert::AreEqual (0, memcmp (kRun, runOut, sizeof (kRun)));
        }


        TEST_METHOD (ValuesAreLittleEndianAndSizeIsPatched)
        {
            std::vector<Byte>  bytes = MakeOneWordSection (kTagA, kVersion);
            size_t             size  = bytes.size();



            Assert::AreEqual (kPayloadOffset + sizeof (Word), size);

            // Tag, low byte first so it reads as text.
            Assert::AreEqual (static_cast<Byte> ('T'), bytes[0]);
            Assert::AreEqual (static_cast<Byte> ('A'), bytes[kVersionOffset - 1]);

            // Payload size, patched by EndSection.
            Assert::AreEqual (static_cast<Byte> (sizeof (Word)), bytes[kSizeOffset]);

            Assert::AreEqual (static_cast<Byte> (kWordValue & 0xFF), bytes[kPayloadOffset]);
            Assert::AreEqual (static_cast<Byte> (kWordValue >> 8),   bytes[kPayloadOffset + 1]);
        }


        TEST_METHOD (WrongTagFails)
        {
            std::vector<Byte>  bytes   = MakeOneWordSection (kTagA, kVersion);
            StateReader        reader (bytes);
            uint16_t           version = 0;
            HRESULT            hr      = S_OK;



            hr = reader.BeginSection (kTagB, kVersion, version);
            Assert::AreEqual (kInvalidData, hr);
            Assert::AreEqual (kInvalidData, reader.GetResult());
        }


        TEST_METHOD (NewerVersionFails)
        {
            std::vector<Byte>  bytes   = MakeOneWordSection (kTagA, kVersion + 1);
            StateReader        reader (bytes);
            uint16_t           version = 0;
            HRESULT            hr      = S_OK;



            hr = reader.BeginSection (kTagA, kVersion, version);
            Assert::AreEqual (kRevision, hr);
        }


        TEST_METHOD (OlderVersionIsReported)
        {
            std::vector<Byte>  bytes   = MakeOneWordSection (kTagA, kVersion - 1);
            StateReader        reader (bytes);
            uint16_t           version = 0;
            HRESULT            hr      = S_OK;



            hr = reader.BeginSection (kTagA, kVersion, version);
            Assert::AreEqual (S_OK, hr);
            Assert::AreEqual (static_cast<uint16_t> (kVersion - 1), version);
        }


        TEST_METHOD (VersionZeroFails)
        {
            std::vector<Byte>  bytes   = MakeOneWordSection (kTagA, kVersion);
            StateReader        reader (bytes);
            uint16_t           version = 0;
            HRESULT            hr      = S_OK;



            bytes[kVersionOffset]     = 0;
            bytes[kVersionOffset + 1] = 0;

            hr = reader.BeginSection (kTagA, kVersion, version);
            Assert::AreEqual (kRevision, hr);
        }


        TEST_METHOD (UnreadPayloadFailsEndSection)
        {
            std::vector<Byte>  bytes   = MakeOneWordSection (kTagA, kVersion);
            StateReader        reader (bytes);
            uint16_t           version = 0;
            Byte               value   = 0;
            HRESULT            hr      = S_OK;



            hr = reader.BeginSection (kTagA, kVersion, version);
            Assert::AreEqual (S_OK, hr);

            reader.ReadByte (value);

            hr = reader.EndSection();
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (ReadPastSectionFailsAndSticks)
        {
            std::vector<Byte>  bytes   = MakeOneWordSection (kTagA, kVersion);
            StateReader        reader (bytes);
            uint16_t           version = 0;
            uint32_t           tooWide = 0;
            Byte               after   = 0xFF;
            HRESULT            hr      = S_OK;



            hr = reader.BeginSection (kTagA, kVersion, version);
            Assert::AreEqual (S_OK, hr);

            reader.ReadUInt32 (tooWide);
            Assert::AreEqual (0u,           tooWide);
            Assert::AreEqual (kInvalidData, reader.GetResult());

            // Every later read is zeroed, even one that would have fit.
            reader.ReadByte (after);
            Assert::AreEqual (static_cast<Byte> (0), after);

            hr = reader.EndSection();
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (TruncatedStreamFails)
        {
            std::vector<Byte>  bytes   = MakeOneWordSection (kTagA, kVersion);
            StateReader        reader (bytes.data(), bytes.size() - 1);
            uint16_t           version = 0;
            HRESULT            hr      = S_OK;



            hr = reader.BeginSection (kTagA, kVersion, version);
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (BoolOutOfRangeFails)
        {
            std::vector<Byte>  bytes   = MakeOneByteSection (kBadBool);
            StateReader        reader (bytes);
            uint16_t           version = 0;
            bool               value   = true;
            HRESULT            hr      = S_OK;



            hr = reader.BeginSection (kTagA, kVersion, version);
            Assert::AreEqual (S_OK, hr);

            reader.ReadBool (value);
            Assert::IsFalse (value);

            hr = reader.EndSection();
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (NestedSectionsRoundTrip)
        {
            std::vector<Byte>  bytes   = MakeNestedSections();
            StateReader        reader (bytes);
            uint16_t           version = 0;
            Word               inner   = 0;
            Byte               outer   = 0;
            HRESULT            hr      = S_OK;



            hr = reader.BeginSection (kTagA, kVersion, version);
            Assert::AreEqual (S_OK, hr);

            hr = reader.BeginSection (kTagB, kVersion, version);
            Assert::AreEqual (S_OK, hr);

            reader.ReadWord (inner);

            hr = reader.EndSection();
            Assert::AreEqual (S_OK, hr);

            reader.ReadByte (outer);

            hr = reader.EndSection();
            Assert::AreEqual (S_OK,       hr);
            Assert::AreEqual (kWordValue, inner);
            Assert::AreEqual (kByteValue, outer);
            Assert::IsTrue   (reader.IsAtEnd());
        }


        TEST_METHOD (InnerSectionCannotReadOuterBytes)
        {
            std::vector<Byte>  bytes   = MakeNestedSections();
            StateReader        reader (bytes);
            uint16_t           version = 0;
            Word               inner   = 0;
            Byte               beyond  = 0;
            HRESULT            hr      = S_OK;



            hr = reader.BeginSection (kTagA, kVersion, version);
            Assert::AreEqual (S_OK, hr);

            hr = reader.BeginSection (kTagB, kVersion, version);
            Assert::AreEqual (S_OK, hr);

            reader.ReadWord (inner);
            reader.ReadByte (beyond);
            Assert::AreEqual (static_cast<Byte> (0), beyond);

            hr = reader.EndSection();
            Assert::AreEqual (kInvalidData, hr);
        }
    };
}

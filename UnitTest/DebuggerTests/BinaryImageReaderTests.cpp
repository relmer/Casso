#include "Pch.h"

#include "Core/AppleSingleCodec.h"
#include "Debugger/BinaryImageReader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BinaryImageReaderTests
//
//  Intel HEX and S-records with their checksums and several segments,
//  AppleSingle with its aux type, DOS 3.3 binary and raw chosen explicitly,
//  and detection that never guesses a header.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (BinaryImageReaderTests)
    {
    public:

        static std::vector<Byte> Bytes (const std::string & text)
        {
            return std::vector<Byte> (text.begin(), text.end());
        }

        static BinaryImage ReadOk (const std::vector<Byte> & content, std::optional<BinaryFormat> format, std::optional<Word> address)
        {
            BinaryImage  image;
            std::string  error;
            HRESULT      hr = BinaryImageReader::Read (content, format, address, image, error);



            Assert::AreEqual (S_OK, hr, std::wstring (error.begin(), error.end()).c_str());
            Assert::IsTrue   (!image.segments.empty(), L"an image has at least one segment");
            return image;
        }

        static void ReadFails (const std::vector<Byte> & content, std::optional<BinaryFormat> format, std::optional<Word> address, const char * fragment)
        {
            BinaryImage  image;
            std::string  error;
            HRESULT      hr = BinaryImageReader::Read (content, format, address, image, error);



            Assert::IsTrue (FAILED (hr));
            Assert::IsTrue (error.find (fragment) != std::string::npos, std::wstring (error.begin(), error.end()).c_str());
        }



        TEST_METHOD (IntelHex_Checksums_Segments_Entry)
        {
            //  Two data records that join, one apart, and a start address.
            static constexpr const char * kFile =
                ":03030000A94160B0\r\n"
                ":0103030060" "99" "\n"
                ":020400000102F7\n"
                ":0400000500000300F4\n"
                ":00000001FF\n";

            BinaryImage  image = ReadOk (Bytes (kFile), std::nullopt, std::nullopt);



            Assert::AreEqual ((int) BinaryFormat::IntelHex, (int) image.format);
            Assert::AreEqual ((size_t) 2,      image.segments.size());
            Assert::AreEqual ((Word) 0x0300,   image.segments[0].address);
            Assert::AreEqual ((size_t) 4,      image.segments[0].bytes.size(), L"$0300-$0303 joined into one segment");
            Assert::AreEqual ((Byte) 0x60,     image.segments[0].bytes[3]);
            Assert::AreEqual ((Word) 0x0400,   image.segments[1].address);
            Assert::AreEqual ((Byte) 0x02,     image.segments[1].bytes[1]);
            Assert::IsTrue   (image.entry.has_value());
            Assert::AreEqual ((Word) 0x0300,   *image.entry);

            ReadFails (Bytes (":03030000A94160B1\n"), BinaryFormat::IntelHex, std::nullopt, "checksum");
            ReadFails (Bytes (kFile), std::nullopt, (Word) 0x0300, "holds its own addresses");
            Assert::AreEqual ((int) BinaryFormat::Raw, (int) ReadOk (Bytes (":03030000A94160B1\n"), std::nullopt, (Word) 0x0300).format,
                              L"a record whose checksum fails is not taken for Intel HEX");
        }



        TEST_METHOD (SRecord_Checksums_Segments_Entry)
        {
            static constexpr const char * kFile =
                "S00600004844521B\n"
                "S1060300A94160AC\n"
                "S10403036095\n"
                "S2060004000102F2\n"
                "S9030300F9\n";

            BinaryImage  image = ReadOk (Bytes (kFile), std::nullopt, std::nullopt);



            Assert::AreEqual ((int) BinaryFormat::SRecord, (int) image.format);
            Assert::AreEqual ((size_t) 2,    image.segments.size());
            Assert::AreEqual ((Word) 0x0300, image.segments[0].address);
            Assert::AreEqual ((size_t) 4,    image.segments[0].bytes.size());
            Assert::AreEqual ((Word) 0x0400, image.segments[1].address, L"an S2 record's 24-bit address");
            Assert::AreEqual ((Word) 0x0300, image.entry.value_or (0));

            ReadFails (Bytes ("S1060300A9416011\n"), BinaryFormat::SRecord, std::nullopt, "checksum");
        }



        TEST_METHOD (AppleSingle_AddressFromAuxType_OrGiven)
        {
            AppleSingleFile    file;
            std::vector<Byte>  bytes;
            BinaryImage        image;



            file.data          = { 0xA9, 0x41, 0x60 };
            file.hasProDosInfo = true;
            file.fileType      = 0x06;
            file.auxType       = 0x2000;
            AppleSingleCodec::Encode (file, bytes);

            image = ReadOk (bytes, std::nullopt, std::nullopt);
            Assert::AreEqual ((int) BinaryFormat::AppleSingle, (int) image.format);
            Assert::AreEqual ((Word) 0x2000, image.segments.at (0).address, L"the aux type is the load address");
            Assert::AreEqual ((size_t) 3,    image.segments[0].bytes.size());

            image = ReadOk (bytes, std::nullopt, (Word) 0x0300);
            Assert::AreEqual ((Word) 0x0300, image.segments.at (0).address, L"a given address overrides it");

            file.hasProDosInfo = false;
            AppleSingleCodec::Encode (file, bytes);
            ReadFails (bytes, std::nullopt, std::nullopt, "AppleSingle file has no load address");
            Assert::AreEqual ((Word) 0x0400, ReadOk (bytes, std::nullopt, (Word) 0x0400).segments.at (0).address);
        }



        TEST_METHOD (Dos33AndRaw_ExplicitOnly_NeverGuessed)
        {
            std::vector<Byte>  dos = { 0x00, 0x03, 0x03, 0x00, 0xA9, 0x41, 0x60 };
            BinaryImage        image;



            image = ReadOk (dos, BinaryFormat::Dos33Binary, std::nullopt);
            Assert::AreEqual ((Word) 0x0300, image.segments.at (0).address);
            Assert::AreEqual ((size_t) 3,    image.segments[0].bytes.size());
            Assert::AreEqual ((Byte) 0xA9,   image.segments[0].bytes[0]);

            image = ReadOk (dos, BinaryFormat::Dos33Binary, (Word) 0x0800);
            Assert::AreEqual ((Word) 0x0800, image.segments.at (0).address, L"a given address overrides the header");

            image = ReadOk (dos, std::nullopt, (Word) 0x0300);
            Assert::AreEqual ((int) BinaryFormat::Raw, (int) image.format, L"a header is never guessed");
            Assert::AreEqual ((size_t) 7,    image.segments.at (0).bytes.size());

            ReadFails (dos, std::nullopt, std::nullopt, "Give the address");
            ReadFails (std::vector<Byte> { 0x00, 0x03, 0x10, 0x00, 0xA9 }, BinaryFormat::Dos33Binary, std::nullopt, "header says");
            ReadFails (std::vector<Byte>(), std::nullopt, (Word) 0x0300, "empty");
        }



        TEST_METHOD (FormatWords)
        {
            BinaryFormat  format = BinaryFormat::Raw;



            Assert::IsTrue   (BinaryImageReader::TryGetFormatName ("dos", format));
            Assert::AreEqual ((int) BinaryFormat::Dos33Binary, (int) format);
            Assert::IsTrue   (BinaryImageReader::TryGetFormatName ("HEX", format));
            Assert::AreEqual ((int) BinaryFormat::IntelHex, (int) format);
            Assert::IsFalse  (BinaryImageReader::TryGetFormatName ("BIN", format));
            Assert::AreEqual (std::string ("DOS 3.3 binary"), std::string (BinaryImageReader::GetFormatName (BinaryFormat::Dos33Binary)));
        }
    };
}

#include "Pch.h"

#include "OutputFormats.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace OutputFormatTests
{
    TEST_CLASS (SRecordTests)
    {
    public:

        TEST_METHOD (SRecord_HasS0Header)
        {
            std::vector<Byte>   data   = { 0xEA };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteSRecord (data, 0x1000, 0x1001, 0x1000, oss);
            output = oss.str();

            Assert::IsTrue (output.find ("S0") == 0, L"Should start with S0");
        }





        TEST_METHOD (SRecord_HasS1DataRecord)
        {
            std::vector<Byte>   data   = { 0xEA, 0x00 };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteSRecord (data, 0x1000, 0x1002, 0x1000, oss);
            output = oss.str();

            Assert::IsTrue (output.find ("S1") != std::string::npos, L"Should have S1 record");
        }





        TEST_METHOD (SRecord_HasS9EndRecord)
        {
            std::vector<Byte>   data   = { 0xEA };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteSRecord (data, 0x1000, 0x1001, 0x1000, oss);
            output = oss.str();

            Assert::IsTrue (output.find ("S9") != std::string::npos, L"Should have S9 record");
        }





        TEST_METHOD (SRecord_DataRecordContainsBytes)
        {
            std::vector<Byte>   data   = { 0xA9, 0x42 };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteSRecord (data, 0x1000, 0x1002, 0x1000, oss);
            output = oss.str();

            // S1 record should contain "A942"
            Assert::IsTrue (output.find ("A942") != std::string::npos, L"Should contain data bytes");
        }
    };





    TEST_CLASS (IntelHexTests)
    {
    public:

        TEST_METHOD (IntelHex_HasDataRecord)
        {
            std::vector<Byte>   data   = { 0xEA };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteIntelHex (data, 0x1000, 0x1001, 0x1000, oss);
            output = oss.str();

            Assert::IsTrue (output[0] == ':', L"Should start with colon");
        }





        TEST_METHOD (IntelHex_HasEOFRecord)
        {
            std::vector<Byte>   data   = { 0xEA };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteIntelHex (data, 0x1000, 0x1001, 0x1000, oss);
            output = oss.str();

            Assert::IsTrue (output.find (":00000001FF") != std::string::npos, L"Should have EOF record");
        }





        TEST_METHOD (IntelHex_DataRecordContainsBytes)
        {
            std::vector<Byte>   data   = { 0xA9, 0x42 };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteIntelHex (data, 0x1000, 0x1002, 0x1000, oss);
            output = oss.str();

            Assert::IsTrue (output.find ("A942") != std::string::npos, L"Should contain data bytes");
        }





        TEST_METHOD (IntelHex_AddressInRecord)
        {
            std::vector<Byte>   data   = { 0xEA };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteIntelHex (data, 0x2000, 0x2001, 0x2000, oss);
            output = oss.str();

            // Record should contain address 2000
            Assert::IsTrue (output.find ("200000") != std::string::npos, L"Should contain address");
        }





        TEST_METHOD (IntelHex_StartAddressRecord)
        {
            std::vector<Byte>   data   = { 0xA9, 0x42, 0xEA };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteIntelHex (data, 0x1000, 0x1003, 0x1000, oss);
            output = oss.str();

            // Should have data record starting with colon and EOF record
            Assert::IsTrue (output[0] == ':', L"First record should start with colon");
            Assert::IsTrue (output.find (":00000001FF") != std::string::npos, L"Should have EOF record");
            Assert::IsTrue (output.find ("A942EA") != std::string::npos, L"Should contain all data bytes");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BinaryShapeTests
    //
    //  The three binary shapes. What separates them is entirely about padding
    //  and headers, so each test asserts the exact byte count as well as the
    //  content -- a shape that quietly gains or loses a prefix is the failure
    //  mode that matters.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BinaryShapeTests)
    {
    public:

        TEST_METHOD (Raw_WritesOnlyTheAssembledBytes)
        {
            std::vector<Byte>   data = { 0xA9, 0x42, 0xEA };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteRaw (data, oss);
            output = oss.str();

            Assert::AreEqual ((size_t) 3, output.size(), L"no padding before or after");
            Assert::AreEqual ((int) 0xA9, (int) (Byte) output[0]);
            Assert::AreEqual ((int) 0xEA, (int) (Byte) output[2]);
        }


        TEST_METHOD (Raw_OfNothing_WritesNothing)
        {
            std::vector<Byte>   data;
            std::ostringstream  oss;



            OutputFormats::WriteRaw (data, oss);

            Assert::AreEqual ((size_t) 0, oss.str().size());
        }


        //  DOS stores the load address IN the file, which is why BLOAD needs
        //  no address argument. Both header fields are little-endian.
        TEST_METHOD (DosBinary_PrefixesLoadAddressAndLength)
        {
            std::vector<Byte>   data = { 0xA9, 0x42, 0xEA };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteDosBinary (data, 0x6000, oss);
            output = oss.str();

            Assert::AreEqual ((size_t) 7, output.size(), L"4-byte header plus 3 bytes of payload");
            Assert::AreEqual ((int) 0x00, (int) (Byte) output[0], L"load address low byte");
            Assert::AreEqual ((int) 0x60, (int) (Byte) output[1], L"load address high byte");
            Assert::AreEqual ((int) 0x03, (int) (Byte) output[2], L"length low byte");
            Assert::AreEqual ((int) 0x00, (int) (Byte) output[3], L"length high byte");
            Assert::AreEqual ((int) 0xA9, (int) (Byte) output[4], L"payload follows the header");
        }


        TEST_METHOD (DosBinary_WritesLengthLittleEndianAcrossBothBytes)
        {
            std::vector<Byte>   data (0x0123, 0xEA);
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteDosBinary (data, 0x0803, oss);
            output = oss.str();

            Assert::AreEqual ((int) 0x03, (int) (Byte) output[0], L"load address low byte");
            Assert::AreEqual ((int) 0x08, (int) (Byte) output[1], L"load address high byte");
            Assert::AreEqual ((int) 0x23, (int) (Byte) output[2], L"length low byte");
            Assert::AreEqual ((int) 0x01, (int) (Byte) output[3], L"length high byte");
        }


        //  File offset must equal absolute address, which is the entire point
        //  of the flat shape.
        TEST_METHOD (FlatImage_PlacesBytesAtTheirAbsoluteAddress)
        {
            std::vector<Byte>   data = { 0xA9, 0x42 };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteFlatImage (data, 0x6000, 0xFF, oss);
            output = oss.str();

            Assert::AreEqual ((size_t) 0x10000, output.size(), L"a full 64 KB address space");
            Assert::AreEqual ((int) 0xA9, (int) (Byte) output[0x6000]);
            Assert::AreEqual ((int) 0x42, (int) (Byte) output[0x6001]);
        }


        TEST_METHOD (FlatImage_PadsWithTheGivenFillByte)
        {
            std::vector<Byte>   data = { 0xA9 };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteFlatImage (data, 0x0800, 0x00, oss);
            output = oss.str();

            Assert::AreEqual ((int) 0x00, (int) (Byte) output[0x0000], L"fill before the span");
            Assert::AreEqual ((int) 0xA9, (int) (Byte) output[0x0800]);
            Assert::AreEqual ((int) 0x00, (int) (Byte) output[0xFFFF], L"fill after the span");
        }


        //  The fill byte is visible output, so matching a reference image
        //  requires matching what its gaps were padded with.
        TEST_METHOD (FlatImage_FillByteIsHonoredNotAssumed)
        {
            std::vector<Byte>   data = { 0xA9 };
            std::ostringstream  oss;
            std::string         output;



            OutputFormats::WriteFlatImage (data, 0x0800, 0xEA, oss);
            output = oss.str();

            Assert::AreEqual ((int) 0xEA, (int) (Byte) output[0x0000]);
            Assert::AreEqual ((int) 0xEA, (int) (Byte) output[0xFFFF]);
        }
    };
}

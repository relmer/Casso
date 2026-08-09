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
}

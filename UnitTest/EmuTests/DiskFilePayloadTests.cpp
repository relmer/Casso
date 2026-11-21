#include "Pch.h"

#include "Devices/Disk/DiskFilePayload.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFilePayloadTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskFilePayloadTests)
{
public:

    //  Text for the PC ends each line with CR LF, whether the conversion
    //  wrote LF, as Apple text's does, or CR LF, as Pascal text's does.
    TEST_METHOD (ToWindowsLineEndings_MakesEachLineEndCrLfOnce)
    {
        std::string   text  = "A\nB\r\nC\n\nD";
        vector<Byte>  bytes (text.begin(), text.end());

        DiskFilePayload::ToWindowsLineEndings (bytes);

        Assert::AreEqual (std::string ("A\r\nB\r\nC\r\n\r\nD"), std::string (bytes.begin(), bytes.end()));
    }
};

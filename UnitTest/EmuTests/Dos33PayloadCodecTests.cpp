#include "Pch.h"

#include "Machines/Apple2/Common/Dos33PayloadCodec.h"
#include "Machines/Apple2/Common/Dos33Volume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33PayloadCodecTests
//
//  The length and load-address header DOS 3.3 stores at the front of a
//  file's data, over in-memory payloads.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Dos33PayloadCodecTests)
{
public:

    static constexpr Word  kLoadAddress = 0x0300;
    static constexpr Byte  kPadByte     = 0xEE;

    TEST_METHOD (ApplyTypeHeader_Binary_StripsTheHeaderAndReportsTheLoadAddress)
    {
        vector<Byte>  bytes   = { 0x00, 0x03, 0x02, 0x00, 0x11, 0x22, kPadByte, kPadByte };
        FilePayload   payload;

        Dos33PayloadCodec::ApplyTypeHeader (Dos33Volume::kTypeBinary, bytes, payload);

        Assert::IsTrue (payload.hasLoadAddress);
        Assert::AreEqual (kLoadAddress, payload.loadAddress);
        Assert::IsTrue (bytes == vector<Byte> { 0x11, 0x22 }, L"the stored length cuts the padding");
    }

    TEST_METHOD (ApplyTypeHeader_LengthPastTheData_KeepsWhatIsPresent)
    {
        vector<Byte>  bytes   = { 0x10, 0x00, 0x41, 0x42 };
        FilePayload   payload;

        Dos33PayloadCodec::ApplyTypeHeader (Dos33Volume::kTypeApplesoft, bytes, payload);

        Assert::IsFalse (payload.hasLoadAddress);
        Assert::IsTrue (bytes == vector<Byte> { 0x41, 0x42 });
    }

    TEST_METHOD (ApplyTypeHeader_Text_EndsAtTheFirstZero)
    {
        vector<Byte>  bytes   = { 0xC1, 0xC2, 0x00, 0xC3 };
        FilePayload   payload;

        Dos33PayloadCodec::ApplyTypeHeader (Dos33Volume::kTypeText, bytes, payload);

        Assert::IsTrue (bytes == vector<Byte> { 0xC1, 0xC2 });
    }

    TEST_METHOD (ComposeStoredBytes_Binary_IsTheInverseOfApplyTypeHeader)
    {
        FilePayload   payload;
        FilePayload   readBack;
        vector<Byte>  stored;

        payload.type           = Dos33Volume::kTypeBinary;
        payload.bytes          = { 0x11, 0x22, 0x33 };
        payload.loadAddress    = kLoadAddress;
        payload.hasLoadAddress = true;

        Assert::AreEqual (S_OK, Dos33PayloadCodec::ComposeStoredBytes (payload, stored));
        Assert::IsTrue (stored == vector<Byte> { 0x00, 0x03, 0x03, 0x00, 0x11, 0x22, 0x33 });

        Dos33PayloadCodec::ApplyTypeHeader (Dos33Volume::kTypeBinary, stored, readBack);

        Assert::IsTrue (stored == payload.bytes);
        Assert::AreEqual (kLoadAddress, readBack.loadAddress);
    }

    TEST_METHOD (ComposeStoredBytes_BinaryWithoutALoadAddress_IsRefused)
    {
        FilePayload   payload;
        vector<Byte>  stored;

        payload.type  = Dos33Volume::kTypeBinary;
        payload.bytes = { 0x11 };

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER),
                          Dos33PayloadCodec::ComposeStoredBytes (payload, stored));
    }
};






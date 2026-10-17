#include "Pch.h"

#include "Seams/ByteStreamSettings.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ByteStreamSettingsTests
//
//  The Win32 streams' settings, tested with no port or socket: the COM
//  port's line settings, and the address a TCP port binds to.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ByteStreamSettingsTests)
{
public:

    TEST_METHOD (ComSettingsAreEightBitsNoParityOneStopAtTheSpeedGiven)
    {
        for (DWORD baud : { 9600ul, 115200ul })
        {
            DCB  dcb = {};

            ByteStreamSettings::ToDcb (ComSettings { baud, 8, false }, dcb);

            Assert::AreEqual<DWORD> (sizeof (DCB), dcb.DCBlength);
            Assert::AreEqual<DWORD> (baud, dcb.BaudRate);
            Assert::AreEqual<int> (8, dcb.ByteSize);
            Assert::AreEqual<int> (NOPARITY, dcb.Parity);
            Assert::AreEqual<int> (ONESTOPBIT, dcb.StopBits);
            Assert::AreEqual<DWORD> (TRUE, dcb.fBinary);
        }
    }


    TEST_METHOD (HardwareHandshakeUsesRtsAndCts)
    {
        DCB  dcb = {};

        ByteStreamSettings::ToDcb (ComSettings { 115200, 8, true }, dcb);

        Assert::AreEqual<DWORD> (TRUE, dcb.fOutxCtsFlow);
        Assert::AreEqual<DWORD> (RTS_CONTROL_HANDSHAKE, dcb.fRtsControl);
    }


    TEST_METHOD (NoHandshakeHoldsRtsAndIgnoresCts)
    {
        DCB  dcb = {};

        ByteStreamSettings::ToDcb (ComSettings { 115200, 8, false }, dcb);

        Assert::AreEqual<DWORD> (FALSE, dcb.fOutxCtsFlow);
        Assert::AreEqual<DWORD> (RTS_CONTROL_ENABLE, dcb.fRtsControl);
        Assert::AreEqual<DWORD> (DTR_CONTROL_ENABLE, dcb.fDtrControl);
    }


    TEST_METHOD (OnlyTheWifiModemListensOnTheNetwork)
    {
        Assert::AreEqual ("0.0.0.0",   ByteStreamSettings::GetBindAddress (LinkTransport::WifiModem));
        Assert::AreEqual ("127.0.0.1", ByteStreamSettings::GetBindAddress (LinkTransport::Tcp));
        Assert::AreEqual ("127.0.0.1", ByteStreamSettings::GetBindAddress (LinkTransport::Emulated));
        Assert::AreEqual ("127.0.0.1", ByteStreamSettings::GetBindAddress (LinkTransport::Serial));
    }
};

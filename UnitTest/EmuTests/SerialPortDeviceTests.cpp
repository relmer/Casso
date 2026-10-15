#include "Pch.h"

#include "Devices/SerialPortDevice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortDeviceTests
//
//  The device on an emulated serial port, as the machine's configuration
//  writes it: nothing, a TCP port, or a COM port with an optional speed.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SerialPortDeviceTests)
{
public:

    TEST_METHOD (EmptyIsLoopback)
    {
        SerialPortDevice  device;

        Assert::AreEqual (S_OK, SerialPortDevice::Parse ("", device));
        Assert::IsTrue (device.kind == SerialPortDevice::Kind::Loopback);
    }


    TEST_METHOD (TcpHasItsPortNumber)
    {
        SerialPortDevice  device;

        Assert::AreEqual (S_OK, SerialPortDevice::Parse ("tcp:6502", device));
        Assert::IsTrue (device.kind == SerialPortDevice::Kind::Tcp);
        Assert::AreEqual<uint16_t> (6502, device.tcpPort);
    }


    TEST_METHOD (ComDefaultsTo115200AndTakesASpeed)
    {
        SerialPortDevice  device;

        Assert::AreEqual (S_OK, SerialPortDevice::Parse ("com:COM3", device));
        Assert::IsTrue (device.kind == SerialPortDevice::Kind::Com);
        Assert::AreEqual (std::wstring (L"COM3"), device.comPort);
        Assert::AreEqual<DWORD> (115200, device.baud);

        Assert::AreEqual (S_OK, SerialPortDevice::Parse ("com:COM12@9600", device));
        Assert::AreEqual (std::wstring (L"COM12"), device.comPort);
        Assert::AreEqual<DWORD> (9600, device.baud);
    }


    TEST_METHOD (MalformedDevicesFailAndReadAsLoopback)
    {
        const char *  bad[] = { "tcp:", "tcp:0", "tcp:70000", "tcp:12x", "com:", "com:@9600", "com:COM3@", "com:COM3@0", "printer", "TCP:6502" };

        for (const char * text : bad)
        {
            SerialPortDevice  device;

            Assert::AreEqual (E_INVALIDARG, SerialPortDevice::Parse (text, device));
            Assert::IsTrue (device.kind == SerialPortDevice::Kind::Loopback);
        }
    }


    TEST_METHOD (FormatWritesBackWhatParseRead)
    {
        const char *  texts[] = { "", "tcp:1977", "com:COM3", "com:COM4@9600" };

        for (const char * text : texts)
        {
            SerialPortDevice  device;

            Assert::AreEqual (S_OK, SerialPortDevice::Parse (text, device));
            Assert::AreEqual (std::string (text), SerialPortDevice::Format (device));
        }
    }
};

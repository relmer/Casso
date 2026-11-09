#include "Pch.h"

#include "Devices/SerialTrafficLog.h"
#include "Shell/EmulatorShell.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialTrafficLogTests
//
//  The capture file's lines: a new line for each change of port or
//  direction, every 32 bytes, and after a pause.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SerialTrafficLogTests)
{
public:

    static std::vector<std::string>  SplitLines (const std::string & text)
    {
        std::vector<std::string>  lines (1);

        for (char c : text)
        {
            if (c == '\n')
            {
                lines.emplace_back();
            }
            else
            {
                lines.back() += c;
            }
        }

        return lines;
    }


    TEST_METHOD (EachDirectionChangeStartsALine)
    {
        std::string  text;

        {
            SerialTrafficLog  log ([&text] (std::string_view piece) { text += piece; });

            log.Append (2, false, 0xC1, 100);
            log.Append (2, false, 0x02, 189);
            log.Append (2, true,  0x06, 300);
        }

        std::vector<std::string>  lines = SplitLines (text);

        Assert::AreEqual<size_t> (4, lines.size());
        Assert::IsTrue (lines[0].starts_with ("#"), L"a comment line first");
        Assert::AreEqual (std::string ("         100 2 < C1 02"), lines[1]);
        Assert::AreEqual (std::string ("         300 2 > 06"),    lines[2]);
        Assert::AreEqual (std::string(), lines[3], L"the file ends with a newline");
    }


    TEST_METHOD (APortChangeStartsALine)
    {
        std::string  text;

        {
            SerialTrafficLog  log ([&text] (std::string_view piece) { text += piece; });

            log.Append (1, true, 0x41, 10);
            log.Append (2, true, 0x42, 20);
        }

        std::vector<std::string>  lines = SplitLines (text);

        Assert::AreEqual (std::string ("          10 1 > 41"), lines[1]);
        Assert::AreEqual (std::string ("          20 2 > 42"), lines[2]);
    }


    TEST_METHOD (ThirtyTwoBytesFillALine)
    {
        std::string  text;

        {
            SerialTrafficLog  log ([&text] (std::string_view piece) { text += piece; });

            for (uint64_t i = 0; i < SerialTrafficLog::kBytesPerLine + 1; i++)
            {
                log.Append (2, true, (Byte) i, 1000 + i * 89);
            }
        }

        std::vector<std::string>  lines = SplitLines (text);

        Assert::AreEqual<size_t> (4, lines.size());
        Assert::AreEqual<size_t> (12 + 4 + 3 * SerialTrafficLog::kBytesPerLine, lines[1].size());
        Assert::AreEqual (std::string ("        3848 2 > 20"), lines[2], L"the 33rd byte, at its own cycle");
    }


    TEST_METHOD (APauseStartsALine)
    {
        std::string  text;

        {
            SerialTrafficLog  log ([&text] (std::string_view piece) { text += piece; });

            log.Append (2, false, 0x01, 0);
            log.Append (2, false, 0x02, SerialTrafficLog::kPauseCycles);
            log.Append (2, false, 0x03, SerialTrafficLog::kPauseCycles * 2 + 1);
        }

        std::vector<std::string>  lines = SplitLines (text);

        Assert::AreEqual (std::string ("           0 2 < 01 02"), lines[1], L"a gap of exactly the pause stays on the line");
        Assert::AreEqual (std::string ("       20001 2 < 03"),    lines[2]);
    }


    TEST_METHOD (ABareFlagLogsToATimestampedFileUnderSerialLogs)
    {
        SYSTEMTIME  now = {};

        now.wYear   = 2026;
        now.wMonth  = 10;
        now.wDay    = 5;
        now.wHour   = 9;
        now.wMinute = 7;
        now.wSecond = 3;

        Assert::AreEqual (std::wstring (L"C:\\Data\\Casso\\SerialLogs\\serial-20261005-090703.txt"),
                          EmulatorShell::GetDefaultSerialLogPath (L"C:\\Data\\Casso", now).wstring());
    }
};

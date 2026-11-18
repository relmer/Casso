#include "Pch.h"

#include "Debugger/Channel/ChannelLineBuffer.h"
#include "Debugger/Channel/ChannelProtocol.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ChannelLineBufferTests
    //
    //  The pipe client's read buffer: lines split on LF across reads, and a
    //  partial line past ChannelProtocol::kMaxLineBytes failing the read
    //  instead of growing the buffer.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ChannelLineBufferTests)
    {
    public:

        static bool Append (ChannelLineBuffer & buffer, const std::string & text)
        {
            return buffer.TryAppend (text.data(), text.size());
        }



        TEST_METHOD (LinesSplitAcrossReadsAreTakenWhole)
        {
            ChannelLineBuffer  buffer;
            std::string        line;
            bool               isAppended = false;



            isAppended = Append (buffer, "{\"a\":");
            Assert::IsTrue  (isAppended);
            Assert::IsFalse (buffer.TryTakeLine (line), L"no LF yet");

            isAppended = Append (buffer, "1}\r\nnext\n");
            Assert::IsTrue   (isAppended);
            Assert::IsTrue   (buffer.TryTakeLine (line));
            Assert::AreEqual (std::string ("{\"a\":1}"), line, L"the CR before the LF is removed");
            Assert::IsTrue   (buffer.TryTakeLine (line));
            Assert::AreEqual (std::string ("next"), line);
            Assert::IsFalse  (buffer.TryTakeLine (line));
        }



        TEST_METHOD (APartialLineAtTheLimitIsKept)
        {
            ChannelLineBuffer  buffer;
            std::string        bytes      (ChannelProtocol::kMaxLineBytes, 'x');
            std::string        line;
            bool               isAppended = false;



            isAppended = Append (buffer, bytes);
            Assert::IsTrue (isAppended);

            isAppended = Append (buffer, "\n");
            Assert::IsTrue   (isAppended);
            Assert::IsTrue   (buffer.TryTakeLine (line));
            Assert::AreEqual (ChannelProtocol::kMaxLineBytes, line.size());
        }



        //  The partial line is counted from the last LF, so complete lines
        //  ahead of it in the same read are not counted against it.
        TEST_METHOD (APartialLinePastTheLimitFailsTheAppend)
        {
            constexpr size_t  kChunkBytes = 4096;

            ChannelLineBuffer  buffer;
            std::string        chunk      (kChunkBytes, 'x');
            std::string        line;
            bool               isAppended = true;
            size_t             appended   = 0;



            isAppended = Append (buffer, "done\n");
            Assert::IsTrue (isAppended);

            while (isAppended && appended <= ChannelProtocol::kMaxLineBytes)
            {
                isAppended  = Append (buffer, chunk);
                appended   += chunk.size();
            }

            Assert::IsFalse (isAppended, L"a server sending bytes with no LF fails the read");
            Assert::IsTrue  (appended > ChannelProtocol::kMaxLineBytes);
            Assert::IsTrue  (appended <= ChannelProtocol::kMaxLineBytes + kChunkBytes, L"the read fails as soon as the line passes the limit");
            Assert::IsFalse (buffer.TryTakeLine (line), L"the buffer is emptied");
        }
    };
}

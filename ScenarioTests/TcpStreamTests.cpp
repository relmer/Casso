#include "Pch.h"

#include "Seams/Win32TcpStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TcpStreamTests
//
//  Win32TcpStream over real Winsock on 127.0.0.1: a connect that gets no
//  answer ends at a stop, and a listening stream set to drop does not block
//  its writer when the client stops reading.
//
////////////////////////////////////////////////////////////////////////////////

namespace TcpStreamTests
{
    //  No program listens on these. A connect to a closed port of this PC
    //  fails after about two seconds, which a stop cuts short.
    static constexpr uint16_t  kClosedPort = 6597;
    static constexpr uint16_t  kDropPort   = 6596;


    TEST_CLASS (TcpStreamTests)
    {
    public:

        TEST_METHOD (AStop_EndsAConnectThatGetsNoAnswer)
        {
            Win32TcpStream     stream;
            std::stop_source   source;
            HRESULT            hr      = S_OK;
            ULONGLONG          started = GetTickCount64();
            ULONGLONG          elapsed = 0;
            std::thread        stopper ([&source] ()
            {
                Sleep (200);
                source.request_stop();
            });



            hr      = stream.Connect ("127.0.0.1", kClosedPort, source.get_token());
            elapsed = GetTickCount64() - started;
            stopper.join();

            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), hr);
            Assert::IsTrue (elapsed < 1000, L"well before the system's refusal");
        }


        //  A client that never reads fills the socket's buffers; the writes
        //  after that are dropped, and none of them blocks.
        TEST_METHOD (ADroppingStream_DoesNotBlockWhenTheClientStopsReading)
        {
            Win32TcpStream     stream;
            SOCKET             client  = INVALID_SOCKET;
            sockaddr_in        to      = {};
            int                buffer  = 4096;
            std::vector<Byte>  chunk (64 * 1024, 0x5A);
            ULONGLONG          started = 0;
            int                i       = 0;



            stream.SetDropWhenFull (true);
            Assert::AreEqual (S_OK, stream.Listen ("127.0.0.1", kDropPort));

            client = ::socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
            Assert::IsTrue (client != INVALID_SOCKET);
            Assert::AreEqual (0, setsockopt (client, SOL_SOCKET, SO_RCVBUF, (const char *) &buffer, sizeof (buffer)));

            to.sin_family = AF_INET;
            to.sin_port   = htons (kDropPort);
            Assert::AreEqual (1, inet_pton (AF_INET, "127.0.0.1", &to.sin_addr));
            Assert::AreEqual (0, ::connect (client, (const sockaddr *) &to, sizeof (to)));
            Sleep (200);

            started = GetTickCount64();

            for (i = 0; i < 32; i++)
            {
                Assert::AreEqual (S_OK, stream.Write (chunk.data(), chunk.size()));
            }

            Assert::IsTrue (GetTickCount64() - started < 5000, L"two megabytes, most of them dropped, in moments");

            closesocket (client);
            stream.Close();
        }
    };
}

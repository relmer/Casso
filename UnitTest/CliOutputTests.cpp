#include "Pch.h"

#include "Cli/CliOutput.h"

#include "CppUnitTest.h"




using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace CliOutputTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PipeRig
    //
    //  An anonymous pipe owned by this process, with its write end wrapped in an
    //  unbuffered FILE so every write reaches the pipe immediately. No file, no
    //  console and no state another test can see: the handles are private and
    //  not inheritable, so a child process started elsewhere cannot hold the
    //  read end open and keep a "closed" pipe writable.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PipeRig
    {
    public:
        static constexpr unsigned  kPipeBytes = 4096;

        PipeRig()
        {
            int  fds[2]  = { -1, -1 };
            int  created = _pipe (fds, kPipeBytes, _O_BINARY | _O_NOINHERIT);



            Assert::AreEqual (0, created, L"the pipe was not created");

            m_readFd = fds[0];
            m_stream = _fdopen (fds[1], "wb");
            Assert::IsNotNull (m_stream, L"the write end did not open as a stream");

            setvbuf (m_stream, nullptr, _IONBF, 0);
        }

        ~PipeRig()
        {
            CloseReader();

            if (m_stream != nullptr)
            {
                fclose (m_stream);
            }
        }

        //  What a pager or Select-Object does when it has read enough.
        void CloseReader()
        {
            if (m_readFd != -1)
            {
                _close (m_readFd);
                m_readFd = -1;
            }
        }

        std::string ReadAvailable (unsigned count)
        {
            std::string  bytes (count, '\0');
            int          got   = _read (m_readFd, bytes.data(), count);



            Assert::IsTrue (got >= 0, L"the read end could not be read");
            bytes.resize ((size_t) got);

            return bytes;
        }

        std::FILE * GetStream() { return m_stream; }

    private:
        int           m_readFd = -1;
        std::FILE   * m_stream = nullptr;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ClosedReaderTests
    //
    //  std::println throws std::system_error when its write fails, and nothing
    //  in the console tool caught it, so a reader closing the pipe early aborted
    //  the process: `CassoCli disk --help | Select-Object -First 40`. These
    //  write to exactly that pipe. Routing CliOutput back through std::println
    //  turns them red with the exception.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ClosedReaderTests)
    {
    public:
        TEST_METHOD (PrintLine_ToAClosedPipe_DoesNotThrow)
        {
            PipeRig  rig;



            rig.CloseReader();

            CliOutput::PrintLine (rig.GetStream(), "{}", "a line nobody is reading");
            CliOutput::PrintLine (rig.GetStream());
            CliOutput::Print     (rig.GetStream(), "{} {}", "and", "another");
        }

        TEST_METHOD (TryWrite_ToAClosedPipe_ReportsFailure)
        {
            PipeRig  rig;
            bool     isWritten = true;



            rig.CloseReader();

            isWritten = CliOutput::TryWrite (rig.GetStream(), "dropped\n");
            Assert::IsFalse (isWritten, L"a write to a closed pipe reported success");
        }

        //  The failure path must not have been bought by breaking the ordinary
        //  one: what arrives is what std::println would have written.
        TEST_METHOD (PrintLine_ToAnOpenPipe_DeliversTheFormattedLine)
        {
            constexpr unsigned  kReadBytes = 64;
            PipeRig             rig;
            std::string         arrived;



            CliOutput::PrintLine (rig.GetStream(), "{} ${:04X}", "Start:", 0x0800);
            CliOutput::PrintLine (rig.GetStream());
            CliOutput::Print     (rig.GetStream(), "{}", "end");

            arrived = rig.ReadAvailable (kReadBytes);
            Assert::AreEqual (std::string ("Start: $0800\n\nend"), arrived);
        }

        TEST_METHOD (TryWrite_ToAnOpenPipe_ReportsSuccess)
        {
            PipeRig  rig;
            bool     isWritten = false;



            isWritten = CliOutput::TryWrite (rig.GetStream(), "kept\n");
            Assert::IsTrue (isWritten);
        }
    };
}

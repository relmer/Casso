#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkSession.h"
#include "Seams/IByteStreamFactory.h"
#include "Seams/InProcessByteStream.h"
#include "CassoLinkCaptures.h"
#include "InMemoryCassoLinkFolder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AdtProServerSessionTests
//
//  A CassoLinkSession for an ADTPro-compatible server: it opens its line
//  once and serves ADTPro's client on it. The client is the requests in a
//  capture of ADTPro's client, written to the line.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AdtProServerSessionTests)
{
public:

    static constexpr DWORD     kReplyMs     = 5000;
    static constexpr DWORD     kSettleMs    = 500;
    static constexpr DWORD     kRetryMs     = 200;

    //  The one line, with each connection counted.
    struct CountingFactory : IByteStreamFactory
    {
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::atomic<int>                      connects { 0 };

        struct Handle : IByteStream
        {
            std::shared_ptr<InProcessByteStream>  end;

            explicit Handle (std::shared_ptr<InProcessByteStream> e) : end (std::move (e)) {}

            HRESULT  Write            (const Byte * data, size_t count) override                                   { return end->Write (data, count); }
            HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override { return end->Read (buffer, capacity, timeoutMs, outRead); }
            size_t   GetReadableCount() override                                                                  { return end->GetReadableCount(); }
            bool     IsClosed         () const override                                                           { return end->IsClosed(); }
            void     Close            () override                                                                 { }
        };

        HRESULT  ListComPorts (std::vector<std::wstring> & outPorts) override                                      { outPorts.clear(); return S_OK; }
        HRESULT  OpenComPort  (const std::wstring &, const ComSettings &, std::unique_ptr<IByteStream> &) override { return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND); }
        HRESULT  ListenTcp    (uint16_t, LinkTransport, std::unique_ptr<IByteStream> &) override                  { return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED); }

        HRESULT  ConnectTcp (const std::string &, uint16_t, std::unique_ptr<IByteStream> & outStream) override
        {
            connects++;
            outStream = std::make_unique<Handle> (pcEnd);
            return S_OK;
        }
    };

    struct QuietSink : ICassoLinkSessionSink
    {
        void  OnStateChanged (LinkState) override {}
        void  OnProgress     (const std::shared_ptr<LinkOperation> &) override {}
        void  OnDone         (const std::shared_ptr<LinkOperation> &) override {}
    };

    static std::vector<Byte>  Receive (InProcessByteStream & end, size_t count)
    {
        std::vector<Byte>  bytes;
        Byte               chunk[4096];
        size_t             got   = 0;
        auto               until = std::chrono::steady_clock::now() + std::chrono::milliseconds (kReplyMs);



        while (bytes.size() < count && std::chrono::steady_clock::now() < until)
        {
            AssertSucceeded (end.Read (chunk, std::min (sizeof (chunk), count - bytes.size()), 50, got));
            bytes.insert (bytes.end(), chunk, chunk + got);
        }

        return bytes;
    }



    //  The client's DIR request, before and after a pause longer than the
    //  session's retry interval: each is served, and the line was opened once.
    TEST_METHOD (TheSession_OpensItsLineOnce_AndServesTheClient)
    {
        CountingFactory                       factory;
        std::shared_ptr<InProcessByteStream>  clientEnd;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        QuietSink                             sink;
        CassoLinkSettings                     settings;
        std::vector<CassoLinkRun>             runs = CassoLinkCaptures::LoadRuns ("adtpro-filespec.txt");



        InProcessByteStream::CreatePair (factory.pcEnd, clientEnd);

        folder.files["A.DSK"] = std::vector<Byte> (512, 0);
        folder.files["B.PO"]  = std::vector<Byte> (512, 0);
        folder.files["C.PO"]  = std::vector<Byte> (512, 0);

        settings.transport = LinkTransport::AdtProServer;
        settings.address   = "127.0.0.1:6502";
        settings.name      = CassoLinkSettings::MakeName (settings);
        settings.use40     = true;

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            session.SetRetryInterval (kRetryMs);
            AssertSucceeded (session.Start (settings));

            for (int pass = 0; pass < 2; pass++)
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (kSettleMs));

                for (const CassoLinkRun & run : runs)
                {
                    if (run.direction == '>')
                    {
                        AssertSucceeded (clientEnd->Write (run.bytes.data(), run.bytes.size()));
                    }
                    else
                    {
                        Assert::IsTrue (Receive (*clientEnd, run.bytes.size()) == run.bytes, std::format (L"the listing, request {}", pass + 1).c_str());
                    }
                }
            }
        }

        Assert::AreEqual (1, factory.connects.load(), L"the line was opened once");
    }
};

#include "Pch.h"

#include "InMemoryCassoLinkFolder.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkTraceTests
//
//  The packets the host passes to its trace sink, for the CassoLink log.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkTraceTests)
{
public:

    struct Row
    {
        bool         sent = false;
        std::string  text;
    };

    static void  SendReady (InProcessByteStream & clientEnd, Byte driveMap)
    {
        std::vector<Byte>  wire;
        std::vector<Byte>  payload = { (Byte) (LinkProgram::kProgramVersion & 0xFF), (Byte) (LinkProgram::kProgramVersion >> 8), LinkProgram::kShimVersion, driveMap };



        Assert::AreEqual (S_OK, CassoLinkCodec::Encode (CassoLinkCodec::kReady, payload, wire));
        Assert::AreEqual (S_OK, clientEnd.Write (wire.data(), wire.size()));
    }



    //  Each Idle is left out, and a Ready is logged once, then again only when
    //  it differs.
    TEST_METHOD (IdlePolls_AreLeftOut_AndAReadyLoggedWhenItDiffers)
    {
        std::shared_ptr<InProcessByteStream>  hostEnd;
        std::shared_ptr<InProcessByteStream>  clientEnd;
        InMemoryCassoLinkFolder               folder;
        std::vector<Row>                      rows;



        InProcessByteStream::CreatePair (hostEnd, clientEnd);

        CassoLinkHost  host (hostEnd, folder, [] () { return std::array<Byte, 4> {}; });

        host.SetOperationTimeout (5000);
        host.SetTraceSink ([&] (bool sent, const std::string & text) { rows.push_back ({ sent, text }); });

        for (int i = 0; i < 3; i++)
        {
            SendReady (*clientEnd, 0x03);
            Assert::AreEqual (S_OK, host.ServeOne (1000));
        }

        SendReady (*clientEnd, 0x01);
        Assert::AreEqual (S_OK, host.ServeOne (1000));

        Assert::AreEqual ((size_t) 2, rows.size());
        Assert::IsFalse (rows[0].sent);
        Assert::IsFalse (rows[1].sent);
        Assert::IsTrue  (rows[0].text.find ("drive map 3") != std::string::npos, L"the first Ready");
        Assert::IsTrue  (rows[1].text.find ("drive map 1") != std::string::npos, L"the Ready that differs");
    }
};
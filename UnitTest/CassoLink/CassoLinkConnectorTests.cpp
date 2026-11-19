#include "Pch.h"

#include "CassoLink/Session/CassoLinkConnector.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkConnectorTests
//
//  Which failed connects are followed by the prompt probe, without a link.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkConnectorTests)
{
public:

    TEST_METHOD (AFailedConnect_IsProbed_OnlyOnALineABootstrapIsTypedOn)
    {
        Assert::IsTrue  (CassoLinkConnector::IsProbed (LinkTransport::Serial,       false, false));
        Assert::IsTrue  (CassoLinkConnector::IsProbed (LinkTransport::Emulated,     false, false));
        Assert::IsFalse (CassoLinkConnector::IsProbed (LinkTransport::Tcp,          false, false));
        Assert::IsFalse (CassoLinkConnector::IsProbed (LinkTransport::WifiModem,    false, false));
        Assert::IsFalse (CassoLinkConnector::IsProbed (LinkTransport::Uthernet,     false, false));
        Assert::IsFalse (CassoLinkConnector::IsProbed (LinkTransport::AdtProServer, false, false));
        Assert::IsFalse (CassoLinkConnector::IsProbed (LinkTransport::Cassette,     false, false));
        Assert::IsFalse (CassoLinkConnector::IsProbed (LinkTransport::Udp,          false, false));
    }




    TEST_METHOD (AFailedConnect_IsNotProbed_ForAnAdtProServer_OrAfterAStop)
    {
        Assert::IsFalse (CassoLinkConnector::IsProbed (LinkTransport::Serial, true,  false));
        Assert::IsFalse (CassoLinkConnector::IsProbed (LinkTransport::Serial, false, true));
    }
};

#include "Pch.h"

#include "Update/AuthenticodeVerifier.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AuthenticodeVerifierTests
//
//  The signer subject comparison. WinVerifyTrust itself reads real files
//  and is not exercised here.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AuthenticodeVerifierTests)
{
public:

    TEST_METHOD (Subjects_EqualInEitherAttributeOrder)
    {
        Assert::IsTrue (AuthenticodeVerifier::AreSubjectsEqual (
            AuthenticodeVerifier::kpszOfficialPublisher,
            L"C=US, S=Washington, L=Redmond, O=Robert Elmer, CN=Robert Elmer"));

        Assert::IsTrue (AuthenticodeVerifier::AreSubjectsEqual (
            AuthenticodeVerifier::kpszOfficialPublisher,
            L"CN=Robert Elmer,O=Robert Elmer,L=Redmond,S=Washington,C=US"));
    }



    TEST_METHOD (Subjects_DifferentPublisherIsNotEqual)
    {
        Assert::IsFalse (AuthenticodeVerifier::AreSubjectsEqual (
            AuthenticodeVerifier::kpszOfficialPublisher,
            L"CN=Someone Else, O=Robert Elmer, L=Redmond, S=Washington, C=US"));

        Assert::IsFalse (AuthenticodeVerifier::AreSubjectsEqual (
            AuthenticodeVerifier::kpszOfficialPublisher,
            L"CN=Robert Elmer, O=Robert Elmer, L=Redmond, S=Washington"),
            L"a missing attribute is a different subject");

        Assert::IsFalse (AuthenticodeVerifier::AreSubjectsEqual (L"", L""),
                         L"an empty subject matches nothing");
    }



    TEST_METHOD (Subjects_QuotedCommaStaysInValue)
    {
        Assert::IsTrue  (AuthenticodeVerifier::AreSubjectsEqual (L"O=\"A, B\", CN=X", L"CN=X, O=\"A, B\""));
        Assert::IsFalse (AuthenticodeVerifier::AreSubjectsEqual (L"O=\"A, B\", CN=X", L"CN=X, O=A, B"));
    }
};

#include "Pch.h"

#include "Update/InstallTypeDetector.h"
#include "FakeInstallEnvironment.h"
#include "FakeSignatureVerifier.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InstallTypeDetectorTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InstallTypeDetectorTests)
{
public:

    TEST_METHOD (PackageIdentity_IsMsix_WithoutSignatureCheck)
    {
        FakeInstallEnvironment  environment;
        FakeSignatureVerifier   verifier;
        InstallType             type        = InstallType::Unknown;
        HRESULT                 hr          = S_OK;



        environment.isPackaged = true;

        hr = InstallTypeDetector::Detect (environment, verifier, type);
        AssertSucceeded (hr);

        Assert::IsTrue   (type == InstallType::Msix);
        Assert::AreEqual (0, verifier.calls, L"Windows verified the package at install");
    }



    TEST_METHOD (OfficialSignature_IsZip)
    {
        FakeInstallEnvironment  environment;
        FakeSignatureVerifier   verifier;
        InstallType             type        = InstallType::Unknown;
        HRESULT                 hr          = S_OK;



        verifier.isOfficial = true;

        hr = InstallTypeDetector::Detect (environment, verifier, type);
        AssertSucceeded (hr);

        Assert::IsTrue   (type == InstallType::Zip);
        Assert::AreEqual (environment.exePath, verifier.lastPath, L"the running executable is what gets checked");
    }



    TEST_METHOD (NoOfficialSignature_IsDeveloper)
    {
        FakeInstallEnvironment  environment;
        FakeSignatureVerifier   verifier;
        InstallType             type        = InstallType::Unknown;
        HRESULT                 hr          = S_OK;



        hr = InstallTypeDetector::Detect (environment, verifier, type);
        AssertSucceeded (hr);

        Assert::IsTrue (type == InstallType::Developer);
    }



    TEST_METHOD (CheckFailure_IsDeveloperAndReported)
    {
        FakeInstallEnvironment  environment;
        FakeSignatureVerifier   verifier;
        InstallType             type        = InstallType::Unknown;
        HRESULT                 hr          = S_OK;



        verifier.isOfficial = true;
        verifier.result     = E_ACCESSDENIED;

        hr = InstallTypeDetector::Detect (environment, verifier, type);
        Assert::AreEqual (E_ACCESSDENIED, hr);
        Assert::IsTrue   (type == InstallType::Developer, L"a copy that cannot be classified is never replaced");

        verifier.result        = S_OK;
        environment.pathResult = E_OUTOFMEMORY;

        hr = InstallTypeDetector::Detect (environment, verifier, type);
        Assert::AreEqual (E_OUTOFMEMORY, hr);
        Assert::IsTrue   (type == InstallType::Developer);
    }
};

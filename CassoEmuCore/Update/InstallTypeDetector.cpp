#include "Pch.h"

#include "Update/InstallTypeDetector.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InstallTypeDetector::Detect
//
//  Package identity means the MSIX: Windows verified the package when it
//  installed it, so no signature check follows. Otherwise an executable
//  signed by Casso's publisher is a release zip, and anything else is a
//  developer build. When the signature check itself fails the copy is
//  classed as a developer build, the one type that is never replaced, and
//  the failure is returned so the caller can report it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InstallTypeDetector::Detect (
    IInstallEnvironment  & environment,
    ISignatureVerifier   & verifier,
    InstallType          & outType)
{
    HRESULT       hr         = S_OK;
    bool          isPackaged = false;
    bool          isOfficial = false;
    std::wstring  exePath;



    outType = InstallType::Developer;

    isPackaged = environment.HasPackageIdentity();

    if (isPackaged)
    {
        outType = InstallType::Msix;
        BAIL_OUT_IF (true, S_OK);
    }

    hr = environment.GetExecutablePath (exePath);
    CHR (hr);

    hr = verifier.IsOfficialFile (exePath, isOfficial);
    CHR (hr);

    outType = isOfficial ? InstallType::Zip : InstallType::Developer;

Error:
    return hr;
}

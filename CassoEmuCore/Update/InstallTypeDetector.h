#pragma once

#include "Pch.h"

#include "Update/IInstallEnvironment.h"
#include "Update/ISignatureVerifier.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InstallType
//
//  How this copy of Casso was installed, which decides how it updates.
//  Unknown until the check worker has classified it; Developer is any
//  executable without an official signature, which never updates in place.
//
////////////////////////////////////////////////////////////////////////////////

enum class InstallType
{
    Unknown,
    Msix,
    Zip,
    Developer,
};





////////////////////////////////////////////////////////////////////////////////
//
//  InstallTypeDetector
//
////////////////////////////////////////////////////////////////////////////////

class InstallTypeDetector
{
public:
    static HRESULT  Detect (IInstallEnvironment  & environment,
                            ISignatureVerifier   & verifier,
                            InstallType          & outType);
};

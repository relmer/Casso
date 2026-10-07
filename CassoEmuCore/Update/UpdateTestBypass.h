#pragma once

#include "Pch.h"

#include "Update/ISignatureVerifier.h"





#ifdef _DEBUG





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateTestBypass
//
//  UPDATE TEST BYPASS -- Debug builds only; a Release build has none of
//  this. With CASSO_UPDATE_TEST_UNSIGNED=1, an unsigned build can take the
//  in-place update path end to end: every file counts as official, so the
//  running copy is classed as a zip copy rather than a developer build and
//  the staged exe passes the publisher check, and the MSIX deployer accepts
//  an unsigned test bundle. The download's digest, the version check and
//  everything else stay enforced.
//
////////////////////////////////////////////////////////////////////////////////

class UpdateTestBypass
{
public:
    static constexpr LPCWSTR  kpszVariable = L"CASSO_UPDATE_TEST_UNSIGNED";

    static bool  IsRequested (const wchar_t * variableValue);
};





////////////////////////////////////////////////////////////////////////////////
//
//  UnsignedTestVerifier
//
//  The bypass's one point for signatures: asks the real verifier, and
//  reports any file it does not count as official as official anyway, with
//  a debug-output line each time.
//
////////////////////////////////////////////////////////////////////////////////

class UnsignedTestVerifier : public ISignatureVerifier
{
public:
    explicit UnsignedTestVerifier (ISignatureVerifier & inner) : m_inner (inner) {}

    HRESULT IsOfficialFile (const std::wstring & path, bool & outIsOfficial) override;

private:
    ISignatureVerifier & m_inner;
};

#endif

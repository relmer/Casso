#pragma once

#include "Pch.h"

#include "Update/ISignatureVerifier.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AuthenticodeVerifier
//
//  ISignatureVerifier over WinVerifyTrust. No revocation check and no
//  network retrieval: the signature, its chain and its timestamp are judged
//  from what is on the machine. The signer's subject must equal the
//  publisher below, which is also the Publisher in
//  Installer/Package.appxmanifest; CI checks the two agree.
//
////////////////////////////////////////////////////////////////////////////////

class AuthenticodeVerifier : public ISignatureVerifier
{
public:
    static constexpr LPCWSTR  kpszOfficialPublisher = L"CN=Robert Elmer, O=Robert Elmer, L=Redmond, S=Washington, C=US";

    HRESULT         IsOfficialFile        (const std::wstring & path, bool & outIsOfficial) override;

    static bool     AreSubjectsEqual      (std::wstring_view a, std::wstring_view b);

private:
    static HRESULT  GetSignerSubject      (HANDLE hStateData, std::wstring & outSubject);
    static void     SplitSubject          (std::wstring_view subject, std::vector<std::wstring> & outParts);
};

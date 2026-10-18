#include "Pch.h"

#include "Update/AuthenticodeVerifier.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AuthenticodeVerifier::IsOfficialFile
//
//  WinVerifyTrust with the generic Authenticode policy, no UI, no revocation
//  check and cache-only URL retrieval, so a machine offline gives the same
//  answer as one online. A trust failure is "not official", not an error.
//  The state data is always closed, whatever happened after it was opened.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AuthenticodeVerifier::IsOfficialFile (const std::wstring & path, bool & outIsOfficial)
{
    HRESULT             hr         = S_OK;
    GUID                action     = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_FILE_INFO  fileInfo   = {};
    WINTRUST_DATA       trustData  = {};
    LONG                status     = 0;
    bool                isOpened   = false;
    std::wstring        subject;



    outIsOfficial = false;

    fileInfo.cbStruct      = sizeof (fileInfo);
    fileInfo.pcwszFilePath = path.c_str();

    trustData.cbStruct            = sizeof (trustData);
    trustData.dwUIChoice          = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice       = WTD_CHOICE_FILE;
    trustData.pFile               = &fileInfo;
    trustData.dwStateAction       = WTD_STATEACTION_VERIFY;
    trustData.dwProvFlags         = WTD_CACHE_ONLY_URL_RETRIEVAL;

    status   = WinVerifyTrust ((HWND) INVALID_HANDLE_VALUE, &action, &trustData);
    isOpened = true;

    BAIL_OUT_IF (status != ERROR_SUCCESS, S_OK);

    hr = GetSignerSubject (trustData.hWVTStateData, subject);
    CHR (hr);

    outIsOfficial = AreSubjectsEqual (subject, kpszOfficialPublisher);

Error:
    if (isOpened)
    {
        trustData.dwStateAction = WTD_STATEACTION_CLOSE;
        status = WinVerifyTrust ((HWND) INVALID_HANDLE_VALUE, &action, &trustData);
        IGNORE_RETURN_VALUE (status, 0);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AuthenticodeVerifier::GetSignerSubject
//
//  The X.500 subject of the leaf certificate of the first signer, from the
//  state WinVerifyTrust left open.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AuthenticodeVerifier::GetSignerSubject (HANDLE hStateData, std::wstring & outSubject)
{
    HRESULT                 hr        = S_OK;
    CRYPT_PROVIDER_DATA   * provData  = nullptr;
    CRYPT_PROVIDER_SGNR   * signer    = nullptr;
    CRYPT_PROVIDER_CERT   * cert      = nullptr;
    DWORD                   length    = 0;



    outSubject.clear();

    provData = WTHelperProvDataFromStateData (hStateData);
    CBREx (provData != nullptr, TRUST_E_NOSIGNATURE);

    signer = WTHelperGetProvSignerFromChain (provData, 0, FALSE, 0);
    CBREx (signer != nullptr, TRUST_E_NOSIGNATURE);

    cert = WTHelperGetProvCertFromChain (signer, 0);
    CBREx (cert != nullptr && cert->pCert != nullptr, TRUST_E_NOSIGNATURE);

    length = CertNameToStrW (X509_ASN_ENCODING,
                             &cert->pCert->pCertInfo->Subject,
                             CERT_X500_NAME_STR,
                             nullptr,
                             0);
    CBREx (length > 1, TRUST_E_NOSIGNATURE);

    outSubject.resize (length);

    length = CertNameToStrW (X509_ASN_ENCODING,
                             &cert->pCert->pCertInfo->Subject,
                             CERT_X500_NAME_STR,
                             outSubject.data(),
                             length);
    CBREx (length > 1, TRUST_E_NOSIGNATURE);

    // The count includes the terminator.
    outSubject.resize (length - 1);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AuthenticodeVerifier::AreSubjectsEqual
//
//  Whether two X.500 subjects hold the same attributes. Certificates store
//  their attributes in either order (C first or CN first), and the string
//  form follows the certificate, so the attributes are compared as a set.
//
////////////////////////////////////////////////////////////////////////////////

bool AuthenticodeVerifier::AreSubjectsEqual (std::wstring_view a, std::wstring_view b)
{
    std::vector<std::wstring>  partsA;
    std::vector<std::wstring>  partsB;



    SplitSubject (a, partsA);
    SplitSubject (b, partsB);

    std::sort (partsA.begin(), partsA.end());
    std::sort (partsB.begin(), partsB.end());

    return !partsA.empty() && partsA == partsB;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AuthenticodeVerifier::SplitSubject
//
//  "CN=A, O=B" to { "CN=A", "O=B" }. A comma inside double quotes is part of
//  its value.
//
////////////////////////////////////////////////////////////////////////////////

void AuthenticodeVerifier::SplitSubject (std::wstring_view subject, std::vector<std::wstring> & outParts)
{
    std::wstring  part;
    bool          isQuoted = false;
    size_t        first    = 0;
    size_t        last     = 0;



    outParts.clear();

    for (size_t i = 0; i <= subject.size(); i++)
    {
        if (i < subject.size() && (subject[i] != L',' || isQuoted))
        {
            if (subject[i] == L'"')
            {
                isQuoted = !isQuoted;
            }

            part += subject[i];
            continue;
        }

        first = part.find_first_not_of (L' ');
        last  = part.find_last_not_of (L' ');

        if (first != std::wstring::npos)
        {
            outParts.push_back (part.substr (first, last - first + 1));
        }

        part.clear();
    }
}

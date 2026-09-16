#include "Pch.h"

#include "Debugger/Channel/PipeSecurity.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PipeSecurityDescriptor::~PipeSecurityDescriptor
//
////////////////////////////////////////////////////////////////////////////////

PipeSecurityDescriptor::~PipeSecurityDescriptor()
{
    if (m_dacl != nullptr)
    {
        LocalFree (m_dacl);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PipeSecurityDescriptor::BuildForUser
//
//  One allow entry and no inheritance. A descriptor with a present, non-null
//  DACL denies everyone the list does not name, which is the whole point: a
//  NULL DACL would instead grant everyone everything.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PipeSecurityDescriptor::BuildForUser (PSID userSid)
{
    HRESULT             hr       = S_OK;
    EXPLICIT_ACCESS_W   access   = {};
    DWORD               result   = ERROR_SUCCESS;
    BOOL                fSuccess = FALSE;



    CBRAEx (userSid != nullptr && m_dacl == nullptr, E_INVALIDARG);

    access.grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
    access.grfAccessMode        = SET_ACCESS;
    access.grfInheritance       = NO_INHERITANCE;
    access.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
    access.Trustee.TrusteeType  = TRUSTEE_IS_USER;
    access.Trustee.ptstrName    = (LPWSTR) userSid;

    result = SetEntriesInAclW (1, &access, nullptr, &m_dacl);
    CHR (HRESULT_FROM_WIN32 (result));

    fSuccess = InitializeSecurityDescriptor (&m_descriptor, SECURITY_DESCRIPTOR_REVISION);
    CWR (fSuccess);

    fSuccess = SetSecurityDescriptorDacl (&m_descriptor, TRUE, m_dacl, FALSE);
    CWR (fSuccess);

    m_attributes.nLength              = sizeof (m_attributes);
    m_attributes.lpSecurityDescriptor = &m_descriptor;
    m_attributes.bInheritHandle       = FALSE;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PipeSecurityDescriptor::GetCurrentUserSid
//
//  The token is asked for its size first, so the buffer is exactly what the
//  token user needs rather than a guess.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PipeSecurityDescriptor::GetCurrentUserSid (std::vector<BYTE> & sid)
{
    HRESULT            hr       = S_OK;
    HANDLE             token    = nullptr;
    DWORD              needed   = 0;
    std::vector<BYTE>  buffer;
    TOKEN_USER       * user     = nullptr;
    BOOL               fSuccess = FALSE;
    DWORD              sidBytes = 0;
    DWORD              error    = ERROR_SUCCESS;



    fSuccess = OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &token);
    CWR (fSuccess);

    fSuccess = GetTokenInformation (token, TokenUser, nullptr, 0, &needed);
    error = GetLastError();
    CBREx (!fSuccess && error == ERROR_INSUFFICIENT_BUFFER, HRESULT_FROM_WIN32 (error));

    buffer.resize (needed);

    fSuccess = GetTokenInformation (token, TokenUser, buffer.data(), needed, &needed);
    CWR (fSuccess);

    user     = (TOKEN_USER *) buffer.data();
    sidBytes = GetLengthSid (user->User.Sid);

    sid.assign ((BYTE *) user->User.Sid, (BYTE *) user->User.Sid + sidBytes);

Error:
    if (token != nullptr)
    {
        CloseHandle (token);
    }

    return hr;
}

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
//  The SACL has one mandatory label entry. Without it the pipe takes the
//  implicit Medium label, so when Casso runs elevated, an unelevated process
//  of the same user passes the DACL and can open the pipe.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PipeSecurityDescriptor::BuildForUser (PSID userSid, DWORD integrityRid)
{
    HRESULT                   hr                              = S_OK;
    EXPLICIT_ACCESS_W         access                          = {};
    DWORD                     result                          = ERROR_SUCCESS;
    BOOL                      fSuccess                        = FALSE;
    SID_IDENTIFIER_AUTHORITY  labelAuthority                  = SECURITY_MANDATORY_LABEL_AUTHORITY;
    BYTE                      labelSid[SECURITY_MAX_SID_SIZE] = {};
    DWORD                     saclBytes                       = 0;
    PACL                      sacl                            = nullptr;



    CBRAEx (userSid != nullptr && m_dacl == nullptr, E_INVALIDARG);

    fSuccess = InitializeSid ((PSID) labelSid, &labelAuthority, 1);
    CWRA (fSuccess);

    *GetSidSubAuthority ((PSID) labelSid, 0) = integrityRid;

    saclBytes = sizeof (ACL) + sizeof (SYSTEM_MANDATORY_LABEL_ACE) - sizeof (DWORD) + GetLengthSid ((PSID) labelSid);
    m_sacl.resize (saclBytes);
    sacl      = (PACL) m_sacl.data();

    fSuccess = InitializeAcl (sacl, saclBytes, ACL_REVISION);
    CWRA (fSuccess);

    fSuccess = AddMandatoryAce (sacl, ACL_REVISION, 0, SYSTEM_MANDATORY_LABEL_NO_WRITE_UP | SYSTEM_MANDATORY_LABEL_NO_READ_UP, (PSID) labelSid);
    CWRA (fSuccess);

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

    fSuccess = SetSecurityDescriptorSacl (&m_descriptor, TRUE, sacl, FALSE);
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





////////////////////////////////////////////////////////////////////////////////
//
//  PipeSecurityDescriptor::GetCurrentIntegrityLevel
//
//  The token's label SID is S-1-16-<rid>, and the level is its last
//  sub-authority.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PipeSecurityDescriptor::GetCurrentIntegrityLevel (DWORD & integrityRid)
{
    HRESULT                  hr       = S_OK;
    HANDLE                   token    = nullptr;
    DWORD                    size     = 0;
    std::vector<BYTE>        buffer;
    TOKEN_MANDATORY_LABEL  * label    = nullptr;
    BOOL                     fSuccess = FALSE;
    DWORD                    error    = ERROR_SUCCESS;
    UCHAR                    count    = 0;



    fSuccess = OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &token);
    CWR (fSuccess);

    fSuccess = GetTokenInformation (token, TokenIntegrityLevel, nullptr, 0, &size);
    error = GetLastError();
    CBREx (!fSuccess && error == ERROR_INSUFFICIENT_BUFFER, HRESULT_FROM_WIN32 (error));

    buffer.resize (size);

    fSuccess = GetTokenInformation (token, TokenIntegrityLevel, buffer.data(), size, &size);
    CWR (fSuccess);

    label = (TOKEN_MANDATORY_LABEL *) buffer.data();
    count = *GetSidSubAuthorityCount (label->Label.Sid);
    CBRA (count > 0);

    integrityRid = *GetSidSubAuthority (label->Label.Sid, count - 1);

Error:
    if (token != nullptr)
    {
        CloseHandle (token);
    }

    return hr;
}

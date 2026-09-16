#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  PipeSecurityDescriptor
//
//  The access list the debug pipe is created with: read and write for one user
//  and nobody else.
//
//  AN EXPLICIT ACCESS LIST IS REQUIRED, NOT A HARDENING NICETY. A pipe created
//  with the default descriptor grants read to Everyone and to the anonymous
//  account, and this pipe can read and write the machine's memory. Built from
//  a SID handed in rather than read from the process token, so a test can hand
//  in a known SID and check the list names that SID and only that SID.
//
////////////////////////////////////////////////////////////////////////////////

class PipeSecurityDescriptor
{
public:
    PipeSecurityDescriptor () = default;
    ~PipeSecurityDescriptor();

    PipeSecurityDescriptor             (const PipeSecurityDescriptor &) = delete;
    PipeSecurityDescriptor & operator= (const PipeSecurityDescriptor &) = delete;

    //  Grants GENERIC_READ | GENERIC_WRITE to `userSid` alone. May be called
    //  once; the attributes point into this object and live as long as it does.
    HRESULT                BuildForUser  (PSID userSid);

    SECURITY_ATTRIBUTES  * GetAttributes () { return &m_attributes; }
    PACL                   GetDacl       () const { return m_dacl; }

    //  The user the current process runs as, copied out of its token.
    static HRESULT         GetCurrentUserSid (std::vector<BYTE> & sid);

private:
    PACL                  m_dacl       = nullptr;
    SECURITY_DESCRIPTOR   m_descriptor = {};
    SECURITY_ATTRIBUTES   m_attributes = {};
};

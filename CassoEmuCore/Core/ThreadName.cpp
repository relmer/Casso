#include "Pch.h"

#include "Core/ThreadName.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadName::Set
//
//  A thread that cannot be named still runs; the name is only for people
//  reading a profile, so a failure is not reported.
//
////////////////////////////////////////////////////////////////////////////////

void ThreadName::Set (const wchar_t * name)
{
    HRESULT  hr = S_OK;



    hr = SetThreadDescription (GetCurrentThread(), name);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadName::Get
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ThreadName::Get()
{
    HRESULT        hr          = S_OK;
    PWSTR          description = nullptr;
    std::wstring   name;



    hr = GetThreadDescription (GetCurrentThread(), &description);

    if (SUCCEEDED (hr) && description != nullptr)
    {
        name = description;
    }

    if (description != nullptr)
    {
        LocalFree (description);
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadName::Scope
//
////////////////////////////////////////////////////////////////////////////////

ThreadName::Scope::Scope (const wchar_t * name) :
    m_previous (ThreadName::Get()),
    m_isNamed  (name != nullptr)
{
    if (m_isNamed)
    {
        ThreadName::Set (name);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadName::~Scope
//
////////////////////////////////////////////////////////////////////////////////

ThreadName::Scope::~Scope()
{
    if (m_isNamed)
    {
        ThreadName::Set (m_previous.c_str());
    }
}

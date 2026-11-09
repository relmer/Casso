#include "Pch.h"

#include "Core/ThreadName.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ThreadName::SetForCurrentThread
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThreadName::SetForCurrentThread (const wchar_t * name)
{
    HRESULT  hr = S_OK;



    CBRAEx (name != nullptr, E_INVALIDARG);

    hr = SetThreadDescription (GetCurrentThread(), name);
    CHR (hr);

Error:
    return hr;
}

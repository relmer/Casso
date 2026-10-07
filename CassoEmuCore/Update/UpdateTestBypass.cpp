#include "Pch.h"

#include "Update/UpdateTestBypass.h"





#ifdef _DEBUG





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateTestBypass::IsRequested
//
//  Only the exact value "1" turns the bypass on.
//
////////////////////////////////////////////////////////////////////////////////

bool UpdateTestBypass::IsRequested (const wchar_t * variableValue)
{
    return variableValue != nullptr && std::wstring_view (variableValue) == L"1";
}





////////////////////////////////////////////////////////////////////////////////
//
//  UnsignedTestVerifier::IsOfficialFile
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UnsignedTestVerifier::IsOfficialFile (const std::wstring & path, bool & outIsOfficial)
{
    HRESULT  hr = S_OK;



    hr = m_inner.IsOfficialFile (path, outIsOfficial);

    if (FAILED (hr) || !outIsOfficial)
    {
        OutputDebugStringW (std::format (L"Casso: UPDATE TEST BYPASS -- treating {} as official (CASSO_UPDATE_TEST_UNSIGNED)\n", path).c_str());

        hr            = S_OK;
        outIsOfficial = true;
    }

    return hr;
}

#endif






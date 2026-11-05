#include "Pch.h"

#include "Seams/ShellVerbs/Win32SearchSources.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32SearchSources::Win32SearchSources
//
////////////////////////////////////////////////////////////////////////////////

Win32SearchSources::Win32SearchSources (const std::wstring & path, const ContentsLoader & loader)
    : m_path   (path),
      m_loader (loader)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32SearchSources::ContainsText
//
//  A file whose contents can't be read doesn't contain the text.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32SearchSources::ContainsText (const std::wstring & lowerText)
{
    HRESULT  hr = S_OK;



    if (!m_contentsRead && m_loader)
    {
        m_contentsRead = true;
        hr             = m_loader (m_path, m_contents);

        if (hr != S_OK)
        {
            m_contents.clear();
        }
    }

    return !lowerText.empty() && m_contents.find (lowerText) != std::wstring::npos;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32SearchSources::OpenStore
//
//  Once per file, whether or not it opens.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32SearchSources::OpenStore()
{
    HRESULT  hr       = S_OK;
    bool     opened   = m_storeOpened;
    HRESULT  hrBefore = (m_store != nullptr) ? S_OK : E_FAIL;



    BAIL_OUT_IF (opened, hrBefore);

    m_storeOpened = true;

    hr = SHGetPropertyStoreFromParsingName (m_path.c_str(), nullptr, GPS_BESTEFFORT, IID_PPV_ARGS (&m_store));
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32SearchSources::GetProperty
//
//  A value the store has, as text, lowercased; a list's values are joined by
//  semicolons, as the shell writes them.
//
////////////////////////////////////////////////////////////////////////////////

ISearchSources::Lookup Win32SearchSources::GetProperty (const std::wstring & name, std::wstring & outLowerValue)
{
    HRESULT      hr     = S_OK;
    PROPERTYKEY  key    = {};
    PROPVARIANT  value  = {};
    PWSTR        text   = nullptr;
    Lookup       lookup = Lookup::NoValue;
    bool         known  = TryGetPropertyKey (name, key);



    BAIL_OUT_IF (!known, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = OpenStore();
    CHR (hr);

    hr = m_store->GetValue (key, &value);
    CHR (hr);

    BAIL_OUT_IF (value.vt == VT_EMPTY, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = PropVariantToStringAlloc (value, &text);
    CHR (hr);

    outLowerValue = ToLower (text);
    lookup        = Lookup::Value;

Error:
    CoTaskMemFree (text);
    PropVariantClear (&value);

    return known ? lookup : Lookup::UnknownProperty;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32SearchSources::GetKinds
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> Win32SearchSources::GetKinds()
{
    HRESULT                    hr    = S_OK;
    PROPERTYKEY                key   = {};
    PROPVARIANT                value = {};
    ULONG                      count = 0;
    std::vector<std::wstring>  kinds;
    bool                       known = TryGetPropertyKey (L"System.Kind", key);



    CBRA (known);

    hr = OpenStore();
    CHR (hr);

    hr = m_store->GetValue (key, &value);
    CHR (hr);

    count = PropVariantGetElementCount (value);

    for (ULONG at = 0; at < count; at++)
    {
        PWSTR    kind   = nullptr;
        HRESULT  hrKind = PropVariantGetStringElem (value, at, &kind);

        if (SUCCEEDED (hrKind) && kind != nullptr)
        {
            kinds.push_back (ToLower (kind));
        }

        CoTaskMemFree (kind);
    }

Error:
    PropVariantClear (&value);

    return kinds;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32SearchSources::GetTypeText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32SearchSources::GetTypeText()
{
    HRESULT       hr    = S_OK;
    PROPERTYKEY   key   = {};
    PROPVARIANT   value = {};
    PWSTR         text  = nullptr;
    std::wstring  type;
    bool          known = TryGetPropertyKey (L"System.ItemTypeText", key);



    CBRA (known);

    hr = OpenStore();
    CHR (hr);

    hr = m_store->GetValue (key, &value);
    CHR (hr);

    hr = PropVariantToStringAlloc (value, &text);
    CHR (hr);

    type = ToLower (text);

Error:
    CoTaskMemFree (text);
    PropVariantClear (&value);

    return type;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32SearchSources::TryGetPropertyKey
//
//  The lookup table is built once, from every property description the
//  system has, and stored for the life of the process. A canonical name
//  missing from the table is looked up with PSGetPropertyKeyFromName.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32SearchSources::TryGetPropertyKey (const std::wstring & name, PROPERTYKEY & outKey)
{
    static std::mutex                           keysLock;
    static std::map<std::wstring, PROPERTYKEY>  keys;
    static bool                                 described = false;



    std::lock_guard<std::mutex>  guard (keysLock);
    std::wstring                 bare;
    auto                         found = keys.end();
    HRESULT                      hr    = S_OK;



    if (!described)
    {
        AddDisplayNames (keys);
        described = true;
    }

    for (wchar_t c : ToLower (name))
    {
        if (!iswspace (c))
        {
            bare += c;
        }
    }

    found = keys.find (bare);

    if (found != keys.end())
    {
        outKey = found->second;
        return true;
    }

    hr = PSGetPropertyKeyFromName (name.c_str(), &outKey);

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32SearchSources::AddDisplayNames
//
//  Each property's display name without spaces, its canonical name, and the
//  part of the canonical name after its last period, all lowercased. When
//  two properties have the same text, the first one is used.
//
////////////////////////////////////////////////////////////////////////////////

void Win32SearchSources::AddDisplayNames (std::map<std::wstring, PROPERTYKEY> & ioKeys)
{
    HRESULT                           hr    = S_OK;
    ComPtr<IPropertyDescriptionList>  list;
    UINT                              count = 0;



    hr = PSEnumeratePropertyDescriptions (PDEF_ALL, IID_PPV_ARGS (&list));
    CHR (hr);

    hr = list->GetCount (&count);
    CHR (hr);

    for (UINT at = 0; at < count; at++)
    {
        ComPtr<IPropertyDescription>  description;
        PROPERTYKEY                   key       = {};
        PWSTR                         canonical = nullptr;
        PWSTR                         display   = nullptr;
        std::wstring                  bare;
        HRESULT                       hrAt      = list->GetAt (at, IID_PPV_ARGS (&description));

        hrAt = SUCCEEDED (hrAt) ? description->GetPropertyKey (&key) : hrAt;
        hrAt = SUCCEEDED (hrAt) ? description->GetCanonicalName (&canonical) : hrAt;

        if (SUCCEEDED (hrAt) && canonical != nullptr)
        {
            std::wstring  lower = ToLower (canonical);
            size_t        dot   = lower.find_last_of (L'.');

            ioKeys.emplace (lower, key);
            ioKeys.emplace ((dot == std::wstring::npos) ? lower : lower.substr (dot + 1), key);
        }

        hrAt = SUCCEEDED (hrAt) ? description->GetDisplayName (&display) : hrAt;

        if (SUCCEEDED (hrAt) && display != nullptr)
        {
            for (wchar_t c : ToLower (display))
            {
                if (!iswspace (c))
                {
                    bare += c;
                }
            }

            ioKeys.emplace (bare, key);
        }

        CoTaskMemFree (canonical);
        CoTaskMemFree (display);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32SearchSources::ToLower
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32SearchSources::ToLower (const std::wstring & text)
{
    std::wstring  lower;



    lower.reserve (text.size());

    for (wchar_t c : text)
    {
        lower += (wchar_t) towlower (c);
    }

    return lower;
}

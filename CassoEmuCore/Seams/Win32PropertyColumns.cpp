#include "Pch.h"

#include "Core/ThreadName.h"
#include "Seams/Win32PropertyColumns.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns::~Win32PropertyColumns
//
////////////////////////////////////////////////////////////////////////////////

Win32PropertyColumns::~Win32PropertyColumns()
{
    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_stopping = true;
    }

    m_wake.notify_all();

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns::Start
//
////////////////////////////////////////////////////////////////////////////////

void Win32PropertyColumns::Start (HWND hwnd, UINT message)
{
    m_hwnd    = hwnd;
    m_message = message;

    if (!m_thread.joinable())
    {
        m_thread = std::thread ([this] { HRESULT hrName = ThreadName::SetForCurrentThread (L"Casso Explorer property columns"); IGNORE_RETURN_VALUE (hrName, S_OK); Run(); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns::SetProperties
//
//  A name missing from the property system gets an empty key in its place,
//  so each value is at its property's index.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PropertyColumns::SetProperties (const std::vector<std::wstring> & canonicalNames)
{
    std::vector<PROPERTYKEY>  keys;



    for (const std::wstring & name : canonicalNames)
    {
        PROPERTYKEY  key = {};
        HRESULT      hr  = PSGetPropertyKeyFromName (name.c_str(), &key);

        keys.push_back (SUCCEEDED (hr) ? key : PROPERTYKEY {});
    }

    std::lock_guard<std::mutex>  guard (m_lock);

    if (keys.size() == m_keys.size() && std::equal (keys.begin(), keys.end(), m_keys.begin(), [] (const PROPERTYKEY & a, const PROPERTYKEY & b) { return IsEqualPropertyKey (a, b); }))
    {
        return;
    }

    m_keys = std::move (keys);
    m_keysVersion++;
    m_values.clear();
    m_queue.clear();
    m_queued.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns::TryGet
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PropertyColumns::TryGet (const std::wstring & path, const std::wstring & stamp, Values & outValues)
{
    std::lock_guard<std::mutex>  guard (m_lock);
    std::wstring                 key   = MakeCacheKey (path, stamp);
    auto                         found = m_values.find (key);



    if (found != m_values.end())
    {
        outValues = found->second;
        return true;
    }

    if (!m_keys.empty() && m_queued.insert (key).second)
    {
        m_queue.push_back (key);
        m_wake.notify_one();
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns::ReadNow
//
////////////////////////////////////////////////////////////////////////////////

Win32PropertyColumns::Values Win32PropertyColumns::ReadNow (const std::wstring & path, const std::wstring & stamp)
{
    std::wstring              key     = MakeCacheKey (path, stamp);
    std::vector<PROPERTYKEY>  keys;
    uint64_t                  version = 0;
    Values                    values;



    {
        std::lock_guard<std::mutex>  guard (m_lock);
        auto                         found = m_values.find (key);

        if (found != m_values.end())
        {
            return found->second;
        }

        keys    = m_keys;
        version = m_keysVersion;
    }

    values = Read (path, keys);

    {
        std::lock_guard<std::mutex>  guard (m_lock);

        if (version == m_keysVersion)
        {
            m_values[key] = values;
        }
    }

    return values;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns::TakeArrived
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PropertyColumns::TakeArrived()
{
    std::lock_guard<std::mutex>  guard   (m_lock);
    bool                         arrived = m_arrived;



    m_arrived = false;
    m_posted  = false;

    return arrived;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns::Clear
//
////////////////////////////////////////////////////////////////////////////////

void Win32PropertyColumns::Clear()
{
    std::lock_guard<std::mutex>  guard (m_lock);



    m_values.clear();
    m_queue.clear();
    m_queued.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns::Run
//
//  One message for however many arrive before the window reads them.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PropertyColumns::Run()
{
    HRESULT  hrCom = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);



    for (;;)
    {
        std::wstring              key;
        std::vector<PROPERTYKEY>  keys;
        uint64_t                  version = 0;
        Values                    values;
        bool                      post    = false;

        {
            std::unique_lock<std::mutex>  guard (m_lock);

            m_wake.wait (guard, [this] { return m_stopping || !m_queue.empty(); });

            if (m_stopping)
            {
                break;
            }

            key     = m_queue.back();
            keys    = m_keys;
            version = m_keysVersion;
            m_queue.pop_back();
        }

        values = Read (key.substr (0, key.rfind (L'|')), keys);

        {
            std::lock_guard<std::mutex>  guard (m_lock);

            //  Forgotten while it was read: the folder or the columns changed.
            if (version == m_keysVersion && m_queued.erase (key) > 0)
            {
                m_values[key]  = std::move (values);
                m_arrived      = true;
                post           = !m_posted;
                m_posted       = true;
            }
        }

        if (post && m_hwnd != nullptr)
        {
            PostMessageW (m_hwnd, m_message, 0, 0);
        }
    }

    if (SUCCEEDED (hrCom))
    {
        CoUninitialize();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns::Read
//
//  Each value formatted for display, and a number for sorting when it is
//  one: a count, a size or a date.
//
////////////////////////////////////////////////////////////////////////////////

Win32PropertyColumns::Values Win32PropertyColumns::Read (const std::wstring & path, const std::vector<PROPERTYKEY> & keys)
{
    HRESULT                 hr     = S_OK;
    ComPtr<IShellItem2>     item;
    ComPtr<IPropertyStore>  store;
    Values                  values (keys.size());



    BAIL_OUT_IF (keys.empty(), S_OK);

    hr = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&item));
    CHR (hr);

    hr = item->GetPropertyStore (GPS_BESTEFFORT, IID_PPV_ARGS (&store));
    CHR (hr);

    for (size_t index = 0; index < keys.size(); index++)
    {
        CatalogModel::PropertyKey &  value   = values[index];
        PROPVARIANT                  raw     = {};
        PWSTR                        shown   = nullptr;
        FILETIME                     when    = {};
        HRESULT                      hrValue = store->GetValue (keys[index], &raw);

        if (SUCCEEDED (hrValue) && raw.vt != VT_EMPTY)
        {
            hrValue = PSFormatForDisplayAlloc (keys[index], raw, PDFF_DEFAULT, &shown);
        }

        if (SUCCEEDED (hrValue) && shown != nullptr && *shown != L'\0')
        {
            value.has  = true;
            value.text = shown;

            switch (raw.vt)
            {
                case VT_FILETIME:
                    hrValue      = PropVariantToFileTime (raw, PSTF_UTC, &when);
                    value.number = (double) (((uint64_t) when.dwHighDateTime << 32) | when.dwLowDateTime);
                    break;

                case VT_I1: case VT_UI1: case VT_I2: case VT_UI2: case VT_I4: case VT_UI4:
                case VT_I8: case VT_UI8: case VT_INT: case VT_UINT: case VT_R4: case VT_R8: case VT_BOOL:
                    hrValue = PropVariantToDouble (raw, &value.number);
                    break;

                default:
                    hrValue = E_NOTIMPL;
                    break;
            }

            value.numeric = SUCCEEDED (hrValue);
        }

        CoTaskMemFree (shown);
        PropVariantClear (&raw);
    }

Error:
    return values;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns::ListViewable
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Win32PropertyColumns::Property> Win32PropertyColumns::ListViewable()
{
    HRESULT                           hr    = S_OK;
    ComPtr<IPropertyDescriptionList>  list;
    UINT                              count = 0;
    std::vector<Property>             properties;



    hr = PSEnumeratePropertyDescriptions (PDEF_VIEWABLE, IID_PPV_ARGS (&list));
    CHR (hr);

    hr = list->GetCount (&count);
    CHR (hr);

    for (UINT index = 0; index < count; index++)
    {
        ComPtr<IPropertyDescription>  description;
        Property                      property;
        HRESULT                       hrAt     = list->GetAt (index, IID_PPV_ARGS (&description));

        if (SUCCEEDED (hrAt) && ReadDescription (*description.Get(), property))
        {
            properties.push_back (std::move (property));
        }
    }

    std::stable_sort (properties.begin(), properties.end(),
                      [] (const Property & a, const Property & b) { return _wcsicmp (a.title.c_str(), b.title.c_str()) < 0; });

Error:
    return properties;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns::TryDescribe
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PropertyColumns::TryDescribe (const std::wstring & canonicalName, Property & outProperty)
{
    ComPtr<IPropertyDescription>  description;
    HRESULT                       hr = PSGetPropertyDescriptionByName (canonicalName.c_str(), IID_PPV_ARGS (&description));



    return SUCCEEDED (hr) && ReadDescription (*description.Get(), outProperty);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns::ReadDescription
//
//  The shell's default column width is in characters; a character is taken
//  as seven DIPs, about Segoe UI's average at the list's size. A property
//  with no title isn't offered.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PropertyColumns::ReadDescription (IPropertyDescription & description, Property & outProperty)
{
    static constexpr int  s_kCharDip = 7;
    HRESULT               hr         = S_OK;
    PWSTR                 canonical  = nullptr;
    PWSTR                 title      = nullptr;
    UINT                  chars      = 0;
    PROPDESC_VIEW_FLAGS   flags      = PDVF_DEFAULT;
    bool                  described  = false;



    hr = description.GetCanonicalName (&canonical);
    CHR (hr);

    hr = description.GetDisplayName (&title);
    CHR (hr);

    BAIL_OUT_IF (title == nullptr || *title == L'\0', S_OK);

    hr = description.GetDefaultColumnWidth (&chars);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = description.GetViewFlags (&flags);
    IGNORE_RETURN_VALUE (hr, S_OK);

    outProperty.canonicalName = canonical;
    outProperty.title         = title;
    outProperty.widthDip      = (int) chars * s_kCharDip;
    outProperty.rightAligned  = (flags & PDVF_RIGHTALIGN) != 0;
    described                 = true;

Error:
    CoTaskMemFree (canonical);
    CoTaskMemFree (title);

    return described;
}

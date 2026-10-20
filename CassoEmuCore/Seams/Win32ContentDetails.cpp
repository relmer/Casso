#include "Pch.h"

#include "Core/ThreadName.h"
#include "Seams/Win32ContentDetails.h"




static constexpr const wchar_t *  s_kPlaceHolder = L"System.LayoutPattern.PlaceHolder";
static constexpr const wchar_t *  s_kContentList = L"System.PropList.ContentViewModeForBrowse";





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContentDetails::~Win32ContentDetails
//
////////////////////////////////////////////////////////////////////////////////

Win32ContentDetails::~Win32ContentDetails()
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
//  Win32ContentDetails::Start
//
////////////////////////////////////////////////////////////////////////////////

void Win32ContentDetails::Start (HWND hwnd, UINT message)
{
    m_hwnd    = hwnd;
    m_message = message;

    if (!m_thread.joinable())
    {
        m_thread = std::thread ([this] { HRESULT hrName = ThreadName::SetForCurrentThread (L"Casso Explorer content details"); IGNORE_RETURN_VALUE (hrName, S_OK); Run(); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContentDetails::TryGet
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ContentDetails::TryGet (const std::wstring & path, Slots & outSlots)
{
    std::lock_guard<std::mutex>  guard (m_lock);
    auto                         found = m_details.find (path);



    if (found != m_details.end())
    {
        outSlots = found->second;
        return true;
    }

    if (m_queued.insert (path).second)
    {
        m_queue.push_back (path);
        m_wake.notify_one();
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContentDetails::TakeArrived
//
////////////////////////////////////////////////////////////////////////////////

bool Win32ContentDetails::TakeArrived()
{
    std::lock_guard<std::mutex>  guard   (m_lock);
    bool                         arrived = m_arrived;



    m_arrived = false;
    m_posted  = false;

    return arrived;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContentDetails::Clear
//
////////////////////////////////////////////////////////////////////////////////

void Win32ContentDetails::Clear()
{
    std::lock_guard<std::mutex>  guard (m_lock);



    m_details.clear();
    m_queue.clear();
    m_queued.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContentDetails::Run
//
//  One message for however many arrive before the window reads them.
//
////////////////////////////////////////////////////////////////////////////////

void Win32ContentDetails::Run()
{
    HRESULT  hrCom = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);



    for (;;)
    {
        std::wstring  path;
        Slots         slots;
        bool          post = false;

        {
            std::unique_lock<std::mutex>  guard (m_lock);

            m_wake.wait (guard, [this] { return m_stopping || !m_queue.empty(); });

            if (m_stopping)
            {
                break;
            }

            path = m_queue.back();
            m_queue.pop_back();
        }

        slots = Read (path);

        {
            std::lock_guard<std::mutex>  guard (m_lock);

            //  Forgotten while it was read: the folder changed under it.
            if (m_queued.erase (path) > 0)
            {
                m_details[path] = slots;
                m_arrived       = true;
                post            = !m_posted;
                m_posted        = true;
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
//  Win32ContentDetails::Read
//
//  The item's Content view list, after its name, one property a place. A
//  placeholder in the list, or a property with no value for the item, is a
//  blank place.
//
////////////////////////////////////////////////////////////////////////////////

Win32ContentDetails::Slots Win32ContentDetails::Read (const std::wstring & path)
{
    HRESULT                           hr    = S_OK;
    ComPtr<IShellItem2>               item;
    ComPtr<IPropertyDescriptionList>  list;
    ComPtr<IPropertyStore>            store;
    PROPERTYKEY                       which = {};
    UINT                              count = 0;
    Slots                             slots;



    hr = PSGetPropertyKeyFromName (s_kContentList, &which);
    CHR (hr);

    hr = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&item));
    CHR (hr);

    hr = item->GetPropertyDescriptionList (which, IID_PPV_ARGS (&list));
    CHR (hr);

    hr = item->GetPropertyStore (GPS_BESTEFFORT, IID_PPV_ARGS (&store));
    CHR (hr);

    hr = list->GetCount (&count);
    CHR (hr);

    //  The first is the name, which the row draws itself.
    for (UINT index = 1; index < count && index <= kSlotCount; index++)
    {
        ComPtr<IPropertyDescription>  description;
        PROPERTYKEY                   key       = {};
        PROPVARIANT                   value     = {};
        PWSTR                         canonical = nullptr;
        PWSTR                         label     = nullptr;
        PWSTR                         shown     = nullptr;
        Slot &                        slot      = slots[index - 1];
        HRESULT                       hrSlot    = list->GetAt (index, IID_PPV_ARGS (&description));

        PropVariantInit (&value);

        if (SUCCEEDED (hrSlot))
        {
            hrSlot = description->GetCanonicalName (&canonical);
        }

        if (SUCCEEDED (hrSlot) && canonical != nullptr && _wcsicmp (canonical, s_kPlaceHolder) != 0)
        {
            hrSlot = description->GetPropertyKey (&key);

            if (SUCCEEDED (hrSlot))
            {
                hrSlot = store->GetValue (key, &value);
            }

            if (SUCCEEDED (hrSlot) && value.vt != VT_EMPTY)
            {
                hrSlot = PSFormatForDisplayAlloc (key, value, PDFF_DEFAULT, &shown);
            }

            if (SUCCEEDED (hrSlot) && shown != nullptr && *shown != L'\0')
            {
                hrSlot = description->GetDisplayName (&label);
            }

            if (SUCCEEDED (hrSlot) && label != nullptr)
            {
                slot.label = std::wstring (label) + L":";
                slot.value = shown;
            }
        }

        CoTaskMemFree (canonical);
        CoTaskMemFree (label);
        CoTaskMemFree (shown);
        PropVariantClear (&value);
    }

Error:
    return slots;
}

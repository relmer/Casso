#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CatalogModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PropertyColumns
//
//  The shell properties File Explorer offers as list columns from Choose
//  Details, as Authors or Dimensions, and each item's values of the ones
//  chosen, as the shell formats them.
//
//  READING THE VALUES OPENS THE FILE, through its property handler, so they
//  are read on a separate thread, the latest request first, as the rows
//  on screen are the last ones requested. A message is posted to the window
//  when any arrive, and the window reads them from the cache. A sort reads
//  the values it lacks at once.
//
////////////////////////////////////////////////////////////////////////////////

class Win32PropertyColumns
{
public:
    struct Property
    {
        std::wstring  canonicalName;
        std::wstring  title;
        int           widthDip     = 0;
        bool          rightAligned = false;
    };

    //  One value an item has for each chosen property, in their order.
    using Values = std::vector<CatalogModel::PropertyKey>;

    Win32PropertyColumns() = default;
    ~Win32PropertyColumns();

    Win32PropertyColumns (const Win32PropertyColumns &)             = delete;
    Win32PropertyColumns & operator= (const Win32PropertyColumns &) = delete;

    //  Starts the thread; `message` goes to `hwnd` when values arrive.
    void  Start (HWND hwnd, UINT message);

    //  The properties read for each item, by canonical name. A change clears
    //  the values read for the last ones.
    void  SetProperties (const std::vector<std::wstring> & canonicalNames);

    //  An item's values when they have been read; otherwise queues the path
    //  and returns false. stamp is the item's date and size, so an item that
    //  changes is read again.
    bool  TryGet (const std::wstring & path, const std::wstring & stamp, Values & outValues);

    //  An item's values, read now when they haven't been.
    Values  ReadNow (const std::wstring & path, const std::wstring & stamp);

    //  Whether values arrived since the last call.
    bool  TakeArrived ();

    //  Clears every item's values, as a folder's change may have changed them.
    void  Clear ();

    //  Every property the shell offers as a column, by title.
    static std::vector<Property>  ListViewable ();

    //  A property's title and width by its canonical name; false for a name
    //  missing from the property system.
    static bool  TryDescribe (const std::wstring & canonicalName, Property & outProperty);

    static Values  Read (const std::wstring & path, const std::vector<PROPERTYKEY> & keys);

private:
    void  Run ();

    static std::wstring  MakeCacheKey (const std::wstring & path, const std::wstring & stamp) { return path + L'|' + stamp; }

    static bool  ReadDescription (IPropertyDescription & description, Property & outProperty);

    HWND                                      m_hwnd        = nullptr;
    UINT                                      m_message     = 0;
    std::thread                               m_thread;
    std::mutex                                m_lock;
    std::condition_variable                   m_wake;
    std::vector<PROPERTYKEY>                  m_keys;
    uint64_t                                  m_keysVersion = 0;   // counts key sets, so values read for an earlier one are dropped
    std::vector<std::wstring>                 m_queue;
    std::unordered_set<std::wstring>          m_queued;
    std::unordered_map<std::wstring, Values>  m_values;
    bool                                      m_arrived     = false;
    bool                                      m_posted      = false;
    bool                                      m_stopping    = false;
};

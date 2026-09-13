#pragma once

#include "Pch.h"
#include "Config/IFileSystem.h"

#include "Cassque/Model/CatalogModel.h"
#include "Cassque/Model/Location.h"
#include "Machines/Apple2/Common/VolumeTypes.h"


enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserModel
//
//  Every tab the window holds and what each one is looking at: its location,
//  the history behind and ahead of it, what is selected, how the list is
//  sorted, where the preview is scrolled, and whether the hex view is
//  disassembling. Preview-pane visibility is not here; it is one setting
//  for the window.
//
//  Each tab keeps the last catalog it read, so switching back to a tab does
//  not re-read a disk nothing wrote to. A write invalidates the cache in
//  every tab looking at that image, whichever tab wrote.
//
////////////////////////////////////////////////////////////////////////////////

class BrowserModel
{
public:
    struct Tab
    {
        Location                   location;
        std::vector<Location>      back;
        std::vector<Location>      forward;
        std::vector<std::wstring>  selection;
        CatalogModel::Column       sortColumn     = CatalogModel::Column::Name;
        bool                       sortDescending = false;
        int                        previewScroll  = 0;
        bool                       disassemble    = false;

        //  The cached catalog and the image it belongs to.
        bool                       hasCatalog     = false;
        std::wstring               catalogImage;
        VolumeListing              catalog;
        VolumeKind                 catalogKind    = VolumeKind {};
    };

    //  One step of the address bar: what it reads and where it goes.
    struct AddressSegment
    {
        std::wstring  label;
        Location      location;
    };

    size_t  OpenTab   (const Location & location);
    bool    CloseTab  (size_t index);
    bool    MoveTab   (size_t from, size_t to);
    bool    SwitchTo  (size_t index);

    size_t       GetTabCount    () const { return m_tabs.size(); }
    size_t       GetActiveIndex () const { return m_active; }
    size_t       GetNextIndex   () const;
    const Tab &  GetTab         (size_t index) const { return m_tabs[index]; }
    Tab &        GetActiveTab   ()                   { return m_tabs[m_active]; }
    const Tab &  GetActiveTab   () const             { return m_tabs[m_active]; }
    bool         HasTabs        () const             { return !m_tabs.empty(); }

    //  Navigation within the active tab.
    void  NavigateTo   (const Location & location);
    bool  CanGoBack    () const;
    bool  CanGoForward () const;
    bool  GoBack       ();
    bool  GoForward    ();

    //  Selection and sort within the active tab.
    void  SetSelection (std::vector<std::wstring> ids);
    void  SetSort      (CatalogModel::Column column, bool descending);

    //  The catalog cache.
    bool  TryGetCachedCatalog  (const std::wstring & imagePath, VolumeListing & outListing, VolumeKind & outKind) const;
    void  CacheCatalog         (const std::wstring & imagePath, const VolumeListing & listing, VolumeKind kind);
    void  InvalidateCatalog    (const std::wstring & imagePath);
    void  InvalidateAllCatalogs ();

    //  Every open tab's location, for persistence.
    void  GetLocations (std::vector<Location> & outLocations) const;

    //  The address bar's segments for a location, from the drive or share
    //  down, and the path shown for editing. ParseAddress reads a typed path
    //  back: a host folder, a disk image, or a directory inside an image,
    //  which is the image's path followed by the directory's.
    static std::vector<AddressSegment>  GetAddressSegments (const Location & location);
    static std::wstring                 FormatAddress      (const Location & location);
    static bool                         ParseAddress       (IFileSystem & fs, const std::wstring & text, Location & outLocation);

private:
    static bool  IsSameImage (const std::wstring & a, const std::wstring & b);
    static void  AppendHostSegments (const std::wstring & path, std::vector<AddressSegment> & outSegments);
    static bool  IsHostFolder       (IFileSystem & fs, const std::wstring & path);

    std::vector<Tab>  m_tabs;
    size_t            m_active = 0;
};

#include "Pch.h"

#include "CassoExplorer/Picker/PickerShellRoots.h"
#include "Seams/Win32ShellItemVerbs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PickerShellRoots::PickerShellRoots
//
////////////////////////////////////////////////////////////////////////////////

PickerShellRoots::PickerShellRoots (Reader reader)
    : m_reader (std::move (reader))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickerShellRoots::~PickerShellRoots
//
//  A read under way finishes first.
//
////////////////////////////////////////////////////////////////////////////////

PickerShellRoots::~PickerShellRoots()
{
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickerShellRoots::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void PickerShellRoots::Refresh()
{
    {
        std::lock_guard<std::mutex>  guard (m_lock);



        if (m_isReading)
        {
            return;
        }

        m_isReading = true;
    }

    //  The last read's thread has finished, so the join returns at once.
    if (m_thread.joinable())
    {
        m_thread.join();
    }

    m_thread = std::thread ([this] () { Read(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickerShellRoots::TryGet
//
////////////////////////////////////////////////////////////////////////////////

bool PickerShellRoots::TryGet (Folders & outNavRoots, Folders & outPinned) const
{
    std::lock_guard<std::mutex>  guard (m_lock);



    if (m_hasRead)
    {
        outNavRoots = m_navRoots;
        outPinned   = m_pinned;
    }

    return m_hasRead;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickerShellRoots::Read
//
//  After a failed read, TryGet returns the folders of the last good one.
//
////////////////////////////////////////////////////////////////////////////////

void PickerShellRoots::Read()
{
    Folders  navRoots;
    Folders  pinned;
    HRESULT  hr = m_reader ? m_reader (navRoots, pinned) : E_NOTIMPL;



    {
        std::lock_guard<std::mutex>  guard (m_lock);



        if (SUCCEEDED (hr))
        {
            m_navRoots = std::move (navRoots);
            m_pinned   = std::move (pinned);
            m_hasRead  = true;
        }

        m_isReading = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PickerShellRoots::ReadFromShell
//
//  When Quick access can't be read, the pinned list is empty, as in
//  TreeModel::RefreshShellRoots.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PickerShellRoots::ReadFromShell (Folders & outNavRoots, Folders & outPinned)
{
    HRESULT  hr     = S_OK;
    HRESULT  hrCom  = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    HRESULT  hrPins = S_OK;



    //  The verbs are released before COM is.
    {
        Win32ShellItemVerbs  verbs;



        hr = verbs.ListNavigationRoots (outNavRoots);

        if (SUCCEEDED (hr))
        {
            hrPins = verbs.ListPinnedFolders (outPinned);
        }
    }

    if (FAILED (hrPins))
    {
        outPinned.clear();
    }

    if (SUCCEEDED (hrCom))
    {
        CoUninitialize();
    }

    return hr;
}

#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/DiskImageCatalog.h"
#include "CassoExplorer/Model/IImageRecords.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeImageRecords
//
//  Image contents in memory, by path. Each path passed to Remove is added to
//  a list. The facts worker calls FindContents on its thread, so the map is under
//  a lock.
//
////////////////////////////////////////////////////////////////////////////////

class FakeImageRecords : public IImageRecords
{
public:
    bool  FindContents (const std::wstring & path, ImageContents & outContents) override
    {
        std::lock_guard  lock  (m_lock);
        auto             found = m_contents.find (path);



        if (found != m_contents.end())
        {
            outContents = found->second;
        }

        return found != m_contents.end();
    }



    void  StoreContents (const std::wstring & path, const ImageContents & contents) override
    {
        std::lock_guard  lock (m_lock);



        m_contents[path] = contents;
    }



    void  Remove (const std::wstring & path) override
    {
        std::lock_guard  lock (m_lock);



        m_contents.erase (path);
        m_removed.push_back (path);
    }



    bool  HasContents (const std::wstring & path)
    {
        std::lock_guard  lock (m_lock);



        return m_contents.contains (path);
    }



    std::vector<std::wstring>  GetRemoved()
    {
        std::lock_guard  lock (m_lock);



        return m_removed;
    }



    SavedGame  FindSavedGame  (const std::wstring &) override       { return SavedGame::Unknown; }
    void       StoreSavedGame (const std::wstring &, bool) override {}

private:
    std::mutex                                m_lock;
    std::map<std::wstring, ImageContents>     m_contents;
    std::vector<std::wstring>                 m_removed;
};

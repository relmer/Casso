#include "Pch.h"

#include "CassoExplorer/Model/FolderWatch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FolderWatch::FolderWatch
//
////////////////////////////////////////////////////////////////////////////////

FolderWatch::FolderWatch (IFolderWatcher & watcher)
    : m_watcher (watcher)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderWatch::~FolderWatch
//
////////////////////////////////////////////////////////////////////////////////

FolderWatch::~FolderWatch()
{
    StopAll();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderWatch::Normalize
//
////////////////////////////////////////////////////////////////////////////////

std::wstring FolderWatch::Normalize (const std::wstring & directory)
{
    std::wstring  result = directory;



    std::transform (result.begin(), result.end(), result.begin(), towlower);

    //  A trailing separator names the same directory, and a watch on one
    //  spelling would not match the other.
    while (result.size() > 3 && (result.back() == L'\\' || result.back() == L'/'))
    {
        result.pop_back();
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderWatch::SetOnChanged
//
////////////////////////////////////////////////////////////////////////////////

void FolderWatch::SetOnChanged (std::function<void()> onChanged)
{
    m_onChanged = std::move (onChanged);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderWatch::SetWatched
//
////////////////////////////////////////////////////////////////////////////////

void FolderWatch::SetWatched (const std::vector<std::wstring> & directories)
{
    std::vector<std::wstring>  wanted;
    std::vector<std::wstring>  kept;
    size_t                     i = 0;



    for (const std::wstring & directory : directories)
    {
        std::wstring  normalized = Normalize (directory);

        if (!normalized.empty()
            && std::find (wanted.begin(), wanted.end(), normalized) == wanted.end())
        {
            wanted.push_back (normalized);
        }
    }

    //  Gone: watched before, not listed now.
    for (i = 0; i < m_watched.size(); i++)
    {
        if (std::find (wanted.begin(), wanted.end(), m_watched[i]) == wanted.end())
        {
            m_watcher.Unwatch (m_watched[i]);
        }
        else
        {
            kept.push_back (m_watched[i]);
        }
    }

    m_watched = std::move (kept);

    //  New: listed now, not watched before. One that cannot be watched is left
    //  unrecorded, so the next call tries it again.
    for (const std::wstring & directory : wanted)
    {
        if (std::find (m_watched.begin(), m_watched.end(), directory) != m_watched.end())
        {
            continue;
        }

        if (m_watcher.Watch (directory, [this] (const std::wstring & changed) { Record (changed); }))
        {
            m_watched.push_back (directory);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderWatch::Record
//
//  Runs on the watcher's thread: notes the directory and wakes the owner.
//
////////////////////////////////////////////////////////////////////////////////

void FolderWatch::Record (const std::wstring & directory)
{
    {
        std::lock_guard<std::mutex>  held (m_lock);

        m_changed.insert (directory);
    }

    if (m_onChanged)
    {
        m_onChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderWatch::TakeChanged
//
////////////////////////////////////////////////////////////////////////////////

bool FolderWatch::TakeChanged (std::vector<std::wstring> & outDirectories)
{
    std::set<std::wstring>  taken;



    {
        std::lock_guard<std::mutex>  held (m_lock);

        taken.swap (m_changed);
    }

    outDirectories.assign (taken.begin(), taken.end());

    return !outDirectories.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FolderWatch::StopAll
//
////////////////////////////////////////////////////////////////////////////////

void FolderWatch::StopAll()
{
    for (const std::wstring & directory : m_watched)
    {
        m_watcher.Unwatch (directory);
    }

    m_watched.clear();
}

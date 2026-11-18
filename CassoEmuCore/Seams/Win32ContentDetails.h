#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ContentDetails
//
//  The lines in File Explorer's Content view for a file or folder beside its
//  name: the properties its kind lists for that view, each with its name and
//  its value as the shell formats it, as "Size: 3 bytes" or "Dimensions:
//  1360 x 1384". The list is the shell's own, so a picture has its type and
//  dimensions where a text file has its date and size.
//
//  READING THE VALUES OPENS THE FILE, through its property handler, so they
//  are read on a thread of their own, the latest request first, as the rows
//  on screen are the last ones requested. A message is posted to the window
//  when any arrive, and the window reads them from the cache.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ContentDetails
{
public:
    //  One place in the Content row, after the name: under it, then the
    //  middle column's two lines, then the right column's two. A place with
    //  an empty value is drawn blank.
    struct Slot
    {
        std::wstring  label;
        std::wstring  value;
    };

    static constexpr size_t  kSlotCount = 5;

    using Slots = std::array<Slot, kSlotCount>;

    Win32ContentDetails() = default;
    ~Win32ContentDetails();

    Win32ContentDetails (const Win32ContentDetails &)             = delete;
    Win32ContentDetails & operator= (const Win32ContentDetails &) = delete;

    //  Starts the thread; `message` goes to `hwnd` when details arrive.
    void  Start (HWND hwnd, UINT message);

    //  The details for a path when they have been read; otherwise queues the
    //  path and returns false.
    bool  TryGet (const std::wstring & path, Slots & outSlots);

    //  Whether details arrived since the last call.
    bool  TakeArrived ();

    //  Clears every item's details, as a folder's change may have changed
    //  them.
    void  Clear ();

    //  The shell's Content view lines for an item. Empty slots when the shell
    //  has none.
    static Slots  Read (const std::wstring & path);

private:
    void  Run ();

    HWND                                     m_hwnd     = nullptr;
    UINT                                     m_message  = 0;
    std::thread                              m_thread;
    std::mutex                               m_lock;
    std::condition_variable                  m_wake;
    std::vector<std::wstring>                m_queue;
    std::unordered_set<std::wstring>         m_queued;
    std::unordered_map<std::wstring, Slots>  m_details;
    bool                                     m_arrived  = false;
    bool                                     m_posted   = false;
    bool                                     m_stopping = false;
};

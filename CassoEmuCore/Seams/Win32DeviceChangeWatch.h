#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32DeviceChangeWatch
//
//  A message-only window registered for disk and volume arrivals and
//  removals: a card reader plugged in or pulled, and a card put in a reader
//  or taken out, which adds or removes the card's volumes. Each change calls
//  back on the thread that started the watch. Nothing is read from a device:
//  Windows sends the notifications.
//
////////////////////////////////////////////////////////////////////////////////

class Win32DeviceChangeWatch
{
public:
    using ChangeFn = std::function<void()>;

    Win32DeviceChangeWatch () = default;
    ~Win32DeviceChangeWatch ();

    Win32DeviceChangeWatch (const Win32DeviceChangeWatch &)             = delete;
    Win32DeviceChangeWatch & operator= (const Win32DeviceChangeWatch &) = delete;

    HRESULT  Start (ChangeFn onChange);
    void     Stop  ();

private:
    static LRESULT CALLBACK  WindowProc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HWND                 m_window = nullptr;
    vector<HDEVNOTIFY>   m_notifies;
    ChangeFn             m_onChange;
};

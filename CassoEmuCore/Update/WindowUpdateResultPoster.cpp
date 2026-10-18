#include "Pch.h"

#include "Update/WindowUpdateResultPoster.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WindowUpdateResultPoster::WindowUpdateResultPoster
//
////////////////////////////////////////////////////////////////////////////////

WindowUpdateResultPoster::WindowUpdateResultPoster (HWND hwnd, UINT message) :
    m_hwnd    (hwnd),
    m_message (message)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  WindowUpdateResultPoster::Post
//
////////////////////////////////////////////////////////////////////////////////

void WindowUpdateResultPoster::Post (std::unique_ptr<UpdateResult> result)
{
    UpdateResult  * carried  = result.release();
    BOOL            isPosted = FALSE;



    isPosted = PostMessageW (m_hwnd, m_message, 0, reinterpret_cast<LPARAM> (carried));

    if (!isPosted)
    {
        delete carried;
    }
}

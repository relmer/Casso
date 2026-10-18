#pragma once

#include "Pch.h"

#include "Update/IUpdateResultPoster.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WindowUpdateResultPoster
//
//  IUpdateResultPoster that posts each result to a window as `message`,
//  with the result's address in lParam. The window's handler takes
//  ownership and deletes it; a post that fails deletes it here.
//
////////////////////////////////////////////////////////////////////////////////

class WindowUpdateResultPoster : public IUpdateResultPoster
{
public:
    WindowUpdateResultPoster (HWND hwnd, UINT message);

    void Post (std::unique_ptr<UpdateResult> result) override;

private:
    HWND  m_hwnd    = nullptr;
    UINT  m_message = 0;
};

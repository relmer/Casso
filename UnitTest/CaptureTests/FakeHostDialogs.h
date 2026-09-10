#pragma once

#include "Pch.h"

#include "Seams/IHostDialogs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostDialogs
//
//  Pickers that answer at once with whatever the test decided, and remember
//  what they were asked for.
//
////////////////////////////////////////////////////////////////////////////////

class FakeHostDialogs : public IHostDialogs
{
public:

    bool                   picks     = true;      // false: the user backs out
    HRESULT                failure   = S_OK;      // a failure to report instead of an answer
    std::filesystem::path  path;
    FileDialogSpec         lastSpec;
    HWND                   lastOwner = nullptr;
    int                    opens     = 0;
    int                    saves     = 0;
    int                    folders   = 0;


    HRESULT  PickFileToOpen (HWND owner, const FileDialogSpec & spec, std::filesystem::path & outPath,
                             bool & outPicked) override
    {
        opens++;
        return Answer (owner, spec, outPath, outPicked);
    }


    HRESULT  PickFileToSave (HWND owner, const FileDialogSpec & spec, std::filesystem::path & outPath,
                             bool & outPicked) override
    {
        saves++;
        return Answer (owner, spec, outPath, outPicked);
    }


    HRESULT  PickFolder (HWND owner, std::filesystem::path & outFolder, bool & outPicked) override
    {
        folders++;
        return Answer (owner, FileDialogSpec(), outFolder, outPicked);
    }

private:

    HRESULT  Answer (HWND owner, const FileDialogSpec & spec, std::filesystem::path & outPath, bool & outPicked)
    {
        lastOwner = owner;
        lastSpec  = spec;
        outPicked = false;

        if (FAILED (failure))
        {
            return failure;
        }

        outPicked = picks;

        if (picks)
        {
            outPath = path;
        }

        return S_OK;
    }
};

#pragma once

#include "Pch.h"

#include "AppleLink/AppleLinkHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InMemoryAppleLinkFolder
//
//  An image folder for AppleLinkHost's tests: files in a map, a path for the
//  listing, and a count of writes, so a test can check that a transfer wrote
//  nothing.
//
////////////////////////////////////////////////////////////////////////////////

class InMemoryAppleLinkFolder : public IAppleLinkFolder
{
public:
    bool                                        open   = true;
    std::string                                 path   = "C:\\Images\\";
    std::map<std::string, std::vector<Byte>>    files;
    std::vector<std::string>                    changes;
    int                                         writes = 0;

    bool         IsOpen  () const override { return open; }
    std::string  GetPath () const override { return path; }

    HRESULT  List (std::vector<std::string> & outNames) override
    {
        outNames.clear();

        for (const auto & [name, bytes] : files)
        {
            outNames.push_back (name);
        }

        return S_OK;
    }

    bool  Exists (const std::string & name) override { return files.contains (name); }

    HRESULT  Read (const std::string & name, std::vector<Byte> & outBytes) override
    {
        HRESULT  hr    = S_OK;
        bool     found = files.contains (name);

        CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));
        outBytes = files[name];

    Error:
        return hr;
    }

    HRESULT  Write (const std::string & name, const std::vector<Byte> & bytes) override
    {
        files[name] = bytes;
        writes++;
        return S_OK;
    }

    HRESULT  ChangeDirectory (const std::string & name) override
    {
        HRESULT  hr      = S_OK;
        bool     allowed = name != "MISSING";

        CBREx (allowed, HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND));
        changes.push_back (name);

    Error:
        return hr;
    }
};

#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InMemoryCassoLinkFolder
//
//  An image folder for CassoLinkHost's tests: files in a map, a path for the
//  listing, and counts of whole-file and block writes, so a test can check
//  what a transfer wrote.
//
////////////////////////////////////////////////////////////////////////////////

class InMemoryCassoLinkFolder : public ICassoLinkFolder
{
public:
    bool                                        open        = true;
    std::string                                 path        = "C:\\Images\\";
    std::map<std::string, std::vector<Byte>>    files;
    std::vector<std::string>                    changes;
    int                                         writes      = 0;
    int                                         blockWrites = 0;

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

    HRESULT  WriteAt (const std::string & name, uint64_t offset, std::span<const Byte> bytes) override
    {
        HRESULT  hr    = S_OK;
        bool     found = files.contains (name);
        bool     fits  = found && offset + bytes.size() <= files[name].size();

        CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));
        CBREx (fits,  HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF));

        std::copy (bytes.begin(), bytes.end(), files[name].begin() + (ptrdiff_t) offset);
        blockWrites++;

    Error:
        return hr;
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

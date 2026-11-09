#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ISearchSources
//
//  What a search reads about a file beyond the facts its folder's listing
//  has: its contents and its other properties. Each is read only for a term
//  whose result can change whether the file matches.
//
////////////////////////////////////////////////////////////////////////////////

class ISearchSources
{
public:
    enum class Lookup
    {
        Value,
        NoValue,
        UnknownProperty,
    };

    virtual ~ISearchSources() = default;

    //  Text, lowercased, somewhere in the file's contents.
    virtual bool                       ContainsText (const std::wstring & lowerText) = 0;

    //  A property by its display name or canonical name, lowercased, and its
    //  value as lowercased text.
    virtual Lookup                     GetProperty  (const std::wstring & name, std::wstring & outLowerValue) = 0;

    //  The file's kinds, as System.Kind has them, and its type's text, all
    //  lowercased.
    virtual std::vector<std::wstring>  GetKinds     () = 0;
    virtual std::wstring               GetTypeText  () = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  NullSearchSources
//
//  A file with no contents and no properties: a file in a disk image, or one
//  in a zip.
//
////////////////////////////////////////////////////////////////////////////////

class NullSearchSources : public ISearchSources
{
public:
    bool                       ContainsText (const std::wstring &) override                 { return false; }
    Lookup                     GetProperty  (const std::wstring &, std::wstring &) override { return Lookup::UnknownProperty; }
    std::vector<std::wstring>  GetKinds     () override                                     { return {}; }
    std::wstring               GetTypeText  () override                                     { return {}; }
};

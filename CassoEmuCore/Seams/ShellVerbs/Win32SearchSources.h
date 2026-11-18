#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/ISearchSources.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32SearchSources
//
//  A file's contents and properties for a walked search, each read on its
//  first use by a term and reused for the file's other terms. The contents
//  come through the loader; the properties come from the shell's property
//  store for the path.
//
////////////////////////////////////////////////////////////////////////////////

class Win32SearchSources : public ISearchSources
{
public:
    using ContentsLoader = std::function<HRESULT (const std::wstring & path, std::wstring & outLowerText)>;

    Win32SearchSources (const std::wstring & path, const ContentsLoader & loader);

    bool                       ContainsText (const std::wstring & lowerText) override;
    Lookup                     GetProperty  (const std::wstring & name, std::wstring & outLowerValue) override;
    std::vector<std::wstring>  GetKinds     () override;
    std::wstring               GetTypeText  () override;

    //  A property's key by its canonical name, as System.Author, the part
    //  after System., or its display name without spaces, in any case.
    static bool  TryGetPropertyKey (const std::wstring & name, PROPERTYKEY & outKey);

private:
    HRESULT              OpenStore          ();
    static std::wstring  ToLower            (const std::wstring & text);
    static void          AddDisplayNames    (std::map<std::wstring, PROPERTYKEY> & ioKeys);

    const std::wstring    & m_path;
    const ContentsLoader  & m_loader;
    bool                    m_contentsRead = false;
    std::wstring            m_contents;
    bool                    m_storeOpened  = false;
    ComPtr<IPropertyStore>  m_store;
};

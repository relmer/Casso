#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemBindData
//
//  A file's WIN32_FIND_DATAW for a bind context's STR_FILE_SYS_BIND_DATA. A
//  path parsed with it has an IDList made from that data, not from what the
//  shell reads from the volume.
//
////////////////////////////////////////////////////////////////////////////////

class FileSystemBindData : public IFileSystemBindData
{
public:
    explicit FileSystemBindData (const WIN32_FIND_DATAW & found);

    //  A bind context with the data registered as STR_FILE_SYS_BIND_DATA.
    static HRESULT  CreateBindContext (const WIN32_FIND_DATAW & found, IBindCtx ** outContext);

    // IUnknown. These signatures are fixed by COM.
    HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void ** ppvObject) override;
    ULONG   STDMETHODCALLTYPE AddRef         () override;
    ULONG   STDMETHODCALLTYPE Release        () override;

    // IFileSystemBindData
    HRESULT STDMETHODCALLTYPE SetFindData (const WIN32_FIND_DATAW * pfd) override;
    HRESULT STDMETHODCALLTYPE GetFindData (WIN32_FIND_DATAW * pfd) override;

private:
    std::atomic<ULONG>  m_refCount { 1 };
    WIN32_FIND_DATAW    m_found    = {};
};

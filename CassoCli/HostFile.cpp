#include "Pch.h"

#include "HostFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostFile::ReadAll
//
////////////////////////////////////////////////////////////////////////////////

HRESULT HostFile::ReadAll (const std::string & path, std::string & contents)
{
    HRESULT             hr     = S_OK;
    std::ostringstream  ss;
    bool                isOpen = false;
    std::ifstream       file (path, std::ios::binary);



    isOpen = file.is_open();
    CBR (isOpen);

    ss << file.rdbuf();
    contents = ss.str();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFile::Exists
//
////////////////////////////////////////////////////////////////////////////////

bool HostFile::Exists (const std::string & path)
{
    std::ifstream f (path);
    return f.good();
}

#include "Pch.h"

#include "ProDosSkeleton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::Write
//
//  Empty-volume skeleton; not yet implemented.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosSkeleton::Write (vector<Byte> & buffer, const std::string & volumeName)
{
    UNREFERENCED_PARAMETER (buffer);
    UNREFERENCED_PARAMETER (volumeName);

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::InstallBoot
//
//  Boot-payload install; not yet implemented.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosSkeleton::InstallBoot (vector<Byte> & buffer, const vector<Byte> & usersDisk)
{
    UNREFERENCED_PARAMETER (buffer);
    UNREFERENCED_PARAMETER (usersDisk);

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader::ExtractFile
//
//  Master-image file extraction; not yet implemented.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosReader::ExtractFile (
    const vector<Byte> & volume,
    const std::string  & fileName,
    vector<Byte>       & outBytes,
    Byte               & outFileType,
    Word               & outAuxType)
{
    UNREFERENCED_PARAMETER (volume);
    UNREFERENCED_PARAMETER (fileName);
    UNREFERENCED_PARAMETER (outBytes);
    UNREFERENCED_PARAMETER (outFileType);
    UNREFERENCED_PARAMETER (outAuxType);

    return E_NOTIMPL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosFileWriter::WriteFile
//
//  Volume file writer; not yet implemented.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ProDosFileWriter::WriteFile (
    vector<Byte>       & buffer,
    const std::string  & fileName,
    Byte                 fileType,
    Word                 auxType,
    const vector<Byte> & bytes)
{
    UNREFERENCED_PARAMETER (buffer);
    UNREFERENCED_PARAMETER (fileName);
    UNREFERENCED_PARAMETER (fileType);
    UNREFERENCED_PARAMETER (auxType);
    UNREFERENCED_PARAMETER (bytes);

    return E_NOTIMPL;
}

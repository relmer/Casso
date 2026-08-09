#include "Pch.h"

#include "ProDosSkeleton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton::Write
//
//  Implemented in T013.
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
//  Implemented in T018.
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
//  Implemented in T016.
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
//  Implemented in T017.
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

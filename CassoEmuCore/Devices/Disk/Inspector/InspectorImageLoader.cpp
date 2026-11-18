#include "Pch.h"

#include "Devices/Disk/Inspector/InspectorImageLoader.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/MountDiagnosis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorImageLoader::LoadFile
//
//  The file's bytes and its read-only attribute, then the same load as
//  LoadBytes. A file that cannot be read says so.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InspectorImageLoader::LoadFile (const std::string & utf8Path, std::shared_ptr<const DiskCopy> & outCopy, std::wstring & outReason)
{
    HRESULT       hr         = S_OK;
    std::wstring  widePath   = TextEncoding::Utf8ToWide (utf8Path);
    DWORD         attributes = GetFileAttributesW (widePath.c_str());
    bool          isReadOnly = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) != 0;
    vector<Byte>  bytes;



    outCopy = nullptr;

    hr = DiskImageStore::ReadFileBytes (utf8Path, bytes);
    CHRF (hr, outReason = std::format (L"{} could not be read.", fs::path (widePath).filename().wstring()));

    hr = LoadBytes (bytes, utf8Path, isReadOnly, outCopy, outReason);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorImageLoader::LoadBytes
//
//  The extension gives the format, as for a mount; the reason for a file
//  the loader cannot take is the mount's own, after the file's name.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InspectorImageLoader::LoadBytes (const vector<Byte> & bytes, const std::string & utf8Path, bool isReadOnly,
                                         std::shared_ptr<const DiskCopy> & outCopy, std::wstring & outReason)
{
    HRESULT         hr     = S_OK;
    DiskFormat      format = DiskFormat::Dsk;
    std::wstring    name   = fs::path (TextEncoding::Utf8ToWide (utf8Path)).filename().wstring();
    MountDiagnosis  diagnosis;
    DiskImage       image;



    outCopy = nullptr;
    outReason.clear();

    hr = DiskImageStore::GetSourceFormatByExtension (utf8Path, format);
    CHRF (hr, outReason = std::format (L"{} is not a disk image Casso opens.", name));

    image.LoadFromBytes (format, bytes, utf8Path);

    if (!image.IsLoaded())
    {
        diagnosis = DiskImageStore::ClassifyLoadFailure (format, bytes);
        outReason = std::format (L"{} {}", name, TextEncoding::NarrowToWide (diagnosis.Describe()));
        CHR (E_FAIL);
    }

    outCopy = DiskCopy::MakeFromImage (image, MakeMediaId(), utf8Path, bytes.size(), isReadOnly);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorImageLoader::MakeMediaId
//
////////////////////////////////////////////////////////////////////////////////

uint64_t InspectorImageLoader::MakeMediaId()
{
    static std::atomic<uint64_t>  s_next = kFirstMediaId;



    return s_next++;
}

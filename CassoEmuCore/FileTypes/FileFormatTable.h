#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"
#include "Machines/Apple2/Common/VolumeTypes.h"

enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  FileFacts
//
//  What a catalog entry has that a format's row matches on: the volume
//  kind, the type in that file system's numbering, the aux type or load
//  address, and the name.
//
////////////////////////////////////////////////////////////////////////////////

struct FileFacts
{
    VolumeKind   volume         = {};
    Byte         type           = 0;
    Word         auxType        = 0;
    bool         hasAuxType     = false;
    Word         loadAddress    = 0;
    bool         hasLoadAddress = false;
    std::string  name;

    static FileFacts  From (const FileEntry & entry, VolumeKind volume);

    bool  IsProDos       () const;
    bool  IsDos33        () const;
    bool  IsPascal       () const;
    bool  HasProDosType  (Byte type) const               { return IsProDos() && this->type == type; }
    bool  HasProDosType  (Byte type, Word auxType) const { return HasProDosType (type) && hasAuxType && this->auxType == auxType; }
    bool  IsNameEnding   (std::string_view suffix) const;
    bool  IsNameStarting (std::string_view prefix) const;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatTable
//
//  The formats of contracts/file-types.md, each a row: whether a file is in
//  the format, by its type, name and size and then a check of its content;
//  its decoder; the suffix its conversion is written under; and for an
//  import row, the encoder a host file with that suffix is put through.
//  Identify returns the first row a file matches.
//
////////////////////////////////////////////////////////////////////////////////

class FileFormatTable
{
public:
    using MatchFn  = bool    (*) (const FileFacts & facts, std::span<const Byte> bytes);
    using DecodeFn = HRESULT (*) (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);
    using ImportFn = HRESULT (*) (std::span<const Byte> hostBytes, VolumeKind volume, FilePayload & outPayload, std::wstring & outError);

    struct Row
    {
        FileFormat       format  = FileFormat::None;
        MatchFn          matches = nullptr;
        DecodeFn         decode  = nullptr;
        ImportFn         import  = nullptr;
        const wchar_t  * suffix  = L"";
        bool             iigs    = false;
    };

    static FileFormat  Identify  (const FileEntry & entry, VolumeKind volume, std::span<const Byte> bytes);
    static HRESULT     Decode    (FileFormat format, std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);
    static HRESULT     Export    (const DecodedFile & file, std::vector<Byte> & outBytes);
    static HRESULT     Import    (FileFormat format, VolumeKind volume, std::span<const Byte> hostBytes, FilePayload & outPayload, std::wstring & outError);

    //  The suffix a conversion is written under, such as ".Pascal text.txt",
    //  and the format a host name's suffix is an import row's, or None.
    static const wchar_t *  GetSuffix          (FileFormat format);
    static FileFormat       GetImportBySuffix  (const std::wstring & hostName);
    static bool             IsIigsFormat       (FileFormat format);

    static const std::vector<Row> &  GetRows ();

private:
    static const Row *  FindRow (FileFormat format);
};

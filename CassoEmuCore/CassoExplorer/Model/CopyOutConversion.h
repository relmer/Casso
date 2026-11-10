#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/DiskOperations.h"
#include "Config/IFileSystem.h"
#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CopyOutConversion
//
//  A file's bytes for the host on a drag or Get out. A file in a format of
//  FileFormatTable is decoded and written as that format's conversion: text,
//  RTF, CSV or PNG. A text file whose preview has a character set other than
//  Apple text is written in that set, as UTF-8. Any other file is read by
//  DiskOperations::Get in the encoding given.
//
////////////////////////////////////////////////////////////////////////////////

class CopyOutConversion
{
public:
    //  The format of a file in an image, or None for a file with no row and
    //  for one that can't be read.
    static FileFormat  Find (DiskOperations     & operations,
                             const std::string  & image,
                             const std::string  & catalogPath,
                             const FileEntry    & entry,
                             VolumeKind           kind);

    //  Whether Get decodes the file itself: it has a format, or it is text
    //  with a chosen character set.
    static bool  Applies (FileFormat format, DiskOperations::Encoding encoding, const DecodeOptions & options);

    //  The bytes are in the result's payload. For a file that fails to
    //  decode the result is a failure whose message is outDecodeFailure,
    //  "Unable to copy NAME: " and the decoder's reason.
    static DiskOperations::Result  Get (DiskOperations            & operations,
                                        const DecodeOptions       & options,
                                        const std::string         & image,
                                        const std::string         & catalogPath,
                                        const FileEntry           & entry,
                                        VolumeKind                  kind,
                                        FileFormat                  format,
                                        DiskOperations::Encoding    encoding,
                                        std::wstring              & outDecodeFailure);

    //  Get, written to hostPath. The result is a failure with a message when
    //  the file can't be written.
    static DiskOperations::Result  GetToFile (DiskOperations            & operations,
                                              IFileSystem               & fs,
                                              const DecodeOptions       & options,
                                              const std::string         & image,
                                              const std::string         & catalogPath,
                                              const FileEntry           & entry,
                                              VolumeKind                  kind,
                                              FileFormat                  format,
                                              DiskOperations::Encoding    encoding,
                                              const std::wstring        & hostPath);
};

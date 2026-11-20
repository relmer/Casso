#include "Pch.h"

#include "DiskCommandText.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandText::DescribeListingRefusal
//
//  WHICH LINE, AND THE LINE ITSELF. A number on its own is useless against a
//  file numbered by tens, and useless in the other direction too: a program
//  read off a disk has line numbers the file on the host does not have. So the number
//  is given when there is one, the file's own position when there is not, and
//  the offending text is quoted underneath whenever it is known.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandText::DescribeListingRefusal (
    const char                   *  leadIn,
    const ApplesoftListingError  &  error)
{
    char         note[96] = {};
    std::string  message  = leadIn;



    message += ": ";

    if (error.hasLineNumber)
    {
        snprintf (note, sizeof (note), "line %u ", (unsigned) error.lineNumber);
        message += note;
    }
    else if (error.sourceLineIndex > 0)
    {
        snprintf (note, sizeof (note), "the line at file position %u ",
                  (unsigned) error.sourceLineIndex);
        message += note;
    }

    message += error.reason;
    message += "\n";

    if (!error.sourceLine.empty())
    {
        message += "    ";
        message += error.sourceLine;
        message += "\n";
    }

    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandText::DescribeDamage
//
//  Both volumes return ERROR_HANDLE_EOF for a file whose sectors cannot be
//  gathered: DOS 3.3 from its track/sector list, ProDOS from its index
//  block.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandText::DescribeDamage (HRESULT hr, VolumeKind kind)
{
    std::string  damage;



    if (hr == HRESULT_FROM_WIN32 (ERROR_HANDLE_EOF))
    {
        damage = (kind == VolumeKind::ProDos) ? "the index block is damaged"
               : (kind == VolumeKind::Dos33)  ? "the track/sector list is damaged"
                                              : "the directory entry is damaged";
    }

    return damage;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandText::DescribeVolumeRefusal
//
//  A verdict from the filesystem layer, in words.
//
//  That layer answers in Win32 codes on purpose -- they carry the meanings a
//  refusal needs without asserting the way E_INVALIDARG does, since everything
//  it refuses came from a user rather than from a caller's bug. What they are
//  not is an explanation. Printing the code would tell someone whose file is
//  locked that the tool returned a number, and the one thing they could have
//  done about it would be missing from the message.
//
//  Anything unrecognized still gets a sentence rather than a code, because a
//  case added below this layer must not be able to leak one upward.
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandText::DescribeVolumeRefusal (HRESULT hr, VolumeKind kind)
{
    //  A lookup rather than a ladder: eleven ifs were a table wearing
    //  control flow, and every row added was another early return.
    static constexpr struct { HRESULT code; const char * sentence; }  kSentences[] =
    {
        { HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED),
          "it is locked. Unlock it on the disk first" },
        { HRESULT_FROM_WIN32 (ERROR_DISK_FULL),
          "the volume has no room for the file's contents or for another "
          "catalog entry" },
        { HRESULT_FROM_WIN32 (ERROR_DISK_TOO_FRAGMENTED),
          "the volume has enough free blocks, but in pieces, and an Apple Pascal "
          "file has to be contiguous. Run 'disk krunch' to combine the free space" },
        { HRESULT_FROM_WIN32 (ERROR_CANNOT_MAKE),
          "the volume directory is full. ProDOS has room for 51 entries "
          "there, so put it in a subdirectory" },
        { HRESULT_FROM_WIN32 (ERROR_INVALID_NAME),
          "the name is not valid on this file system. It must be a single "
          "component starting with a letter, short enough for the catalog, "
          "and free of commas and control characters" },
        { HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
          "it is not on this volume" },
        { HRESULT_FROM_WIN32 (ERROR_DUP_NAME),
          "the name is not unique. Run 'disk list' for the indexes, then "
          "specify both <name> and --index to select that file" },
        { HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE),
          "it is larger than this file system can record for one file" },
        { HRESULT_FROM_WIN32 (ERROR_INVALID_PARAMETER),
          "it is a binary file and requires a load address. Use %Lload $XXXX" },
        { HRESULT_FROM_WIN32 (ERROR_DIRECTORY_NOT_SUPPORTED),
          "it is a directory, and delete accepts files only. Use rmdir, which "
          "removes a directory and, with --recurse, everything below it" },
        { HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED),
          "the image has no operating system on the boot tracks" },
        { HRESULT_FROM_WIN32 (ERROR_BAD_FILE_TYPE),
          "ProDOS starts only a file of type SYS, and not the kernel itself" },
    };
    std::string  sentence = DescribeDamage (hr, kind);
    bool         noBoot   = hr == HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED) && kind != VolumeKind::Dos33 && kind != VolumeKind::ProDos;



    if (noBoot)
    {
        sentence = (kind == VolumeKind::Pascal) ? "Apple Pascal has no setting for this. It runs a code file with the name SYSTEM.STARTUP"
                 : (kind == VolumeKind::Cpm)    ? "CP/M has no setting for this. Its command processor runs what is typed at it"
                                                : "this file system has no setting for a startup program";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_NAME) && kind == VolumeKind::Pascal)
    {
        sentence = "the name is not valid on this file system. Apple Pascal names are 1 to 15 characters, "
                   "with no spaces or any of $ = ? , : [ ]";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_INVALID_NAME) && kind == VolumeKind::Cpm)
    {
        sentence = "the name is not valid on this file system. CP/M names are 1 to 8 characters, an optional "
                   "period and up to 3 more, with no spaces or any of < > , ; : = ? * [ ], in a user of 0 to 15";
    }

    if (hr == HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND) && (kind == VolumeKind::Pascal || kind == VolumeKind::Cpm))
    {
        sentence = std::string (VolumeImage::GetSystemName (kind)) + " has no directories";
    }

    for (const auto & row : kSentences)
    {
        if (sentence.empty() && row.code == hr)
        {
            sentence = row.sentence;
        }
    }

    if (sentence.empty())
    {
        sentence = "the operation is not possible on this volume";
    }

    return sentence;
}

#include "Pch.h"

#include "MountDiagnosis.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "DiskCommandRunner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MountDiagnosis::Describe
//
//  The refusal in the user's terms, as a clause that follows the file's name.
//
//  EVERY CLAUSE STATES WHAT IS TRUE OF THE FILE, never just that something
//  went wrong. "Not a valid disk image" gives a person nothing they did not
//  already know; "is 4,096 bytes, but a .dsk image is 143,360 bytes" shows
//  them their download stopped early. The size is the whole point of carrying
//  it this far. Each clause is short: it is read in a dialog, a pane and a
//  console, and the file's own name comes before it.
//
//  A diagnosis nobody filled in gets a clause of its own rather than the
//  generic refusal, because a reason that was never recorded and a reason that
//  was recorded as "unrecognized" are different bugs and must not read alike.
//
////////////////////////////////////////////////////////////////////////////////

string MountDiagnosis::Describe() const
{
    char    note[400] = {};
    string  observed;
    string  required;
    string  second;
    string  text;



    switch (failure)
    {
        case MountFailure::UnknownExtension:
            //  The list is read off the container table rather than typed out:
            //  typed out, it named four kinds for a release that read five.
            text = "is not a disk image Casso can read. Casso reads "
                 + DiskCommandRunner::FormatContainerWordList (".", "and") + " images";
            break;

        case MountFailure::FileUnreadable:
            text = "cannot be read. It may have been moved or deleted, or another "
                   "program may have it open";
            break;

        case MountFailure::EmptyFile:
            text = "is empty";
            break;

        case MountFailure::WrongSizeForFormat:
            observed = FormatByteCount (fileByteSize);
            required = FormatByteCount ((size_t) NibblizationLayer::kImageByteSize);

            //  An 800K image is a 3.5-inch disk, a real format rather than a damaged
            //  140K one, so it is refused as unsupported, not as the wrong size.
            if (fileByteSize == 800u * 1024u)
            {
                snprintf (note, sizeof (note), "is an 800K 3.5-inch disk image, and 800K images are not supported yet");
            }
            else
            {
                snprintf (note, sizeof (note),
                          "is %s but should be %s, so it is not a valid %s image",
                          observed.c_str(), required.c_str(), GetPrimaryExtension (format));
            }

            text = note;
            break;

        case MountFailure::NotAWozFile:
            text = "has a .woz extension but no WOZ file header, so it is not a "
                   "WOZ image. It was probably renamed from another kind of file";
            break;

        case MountFailure::MalformedWoz:
            text = "has a WOZ header, but its INFO, TMAP or TRKS data is missing. "
                   "The file is damaged or incomplete";
            break;

        case MountFailure::WrongSizeForNibble:
            observed = FormatByteCount (fileByteSize);
            required = FormatByteCount (NibbleImageCodec::kNibImageSize);
            second   = FormatByteCount (NibbleImageCodec::kNb2ImageSize);

            snprintf (note, sizeof (note),
                      "is %s but should be %s or %s, so it is not a valid nibble "
                      "image",
                      observed.c_str(), required.c_str(), second.c_str());

            text = note;
            break;

        case MountFailure::NotANibbleStream:
            text = "is the size of a nibble image, but it holds no disk nibbles, so "
                   "it is not a disk image";
            break;

        case MountFailure::AlreadyMounted:
            //  ONE FILE, ONE DRIVE. Two bays reading and writing one file are
            //  two independent copies of the disk from the moment the guest
            //  writes to either: each flush lands the whole image, so each
            //  overwrites whatever the other saved, and one external change
            //  raises the conflict twice.
            snprintf (note, sizeof (note),
                      "is already in drive %d. A disk image can be in only one "
                      "drive at a time",
                      occupiedDrive + 1);

            text = note;
            break;

        case MountFailure::Unrecognized:
            text = "is not in a disk image format Casso can read";
            break;

        case MountFailure::None:
        default:
            text = "could not be opened for an unknown reason. Please report this "
                   "as a Casso bug";
            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDiagnosis::FormatByteCount
//
//  Digits grouped in threes, with the unit. Written out here rather than
//  handed to a locale so that two users comparing the same refusal see the
//  same number.
//
////////////////////////////////////////////////////////////////////////////////

string MountDiagnosis::FormatByteCount (size_t byteCount)
{
    string  digits = std::to_string (byteCount);
    size_t  count  = digits.size();
    size_t  i      = 0;
    string  text;



    for (i = 0; i < count; i++)
    {
        bool  startsAGroup = (i > 0) && (((count - i) % 3) == 0);

        if (startsAGroup)
        {
            text += ',';
        }

        text += digits[i];
    }

    text += (byteCount == 1) ? " byte" : " bytes";

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDiagnosis::GetPrimaryExtension
//
//  The name a user would recognize their file by. The cases are the whole of
//  DiskFormat; anything else answers "disk", which keeps the sentence around it
//  grammatical instead of leaving a hole in it.
//
//  PRIMARY, not the file's own. Nibble images answer to .nib and .nb2 alike and
//  share one enumerator, so this returns the representative name. Any message
//  that has to be right about which of the two the user actually has must take
//  it from the path instead -- which is why the nibble size clause above names
//  no extension at all.
//
////////////////////////////////////////////////////////////////////////////////

const char * MountDiagnosis::GetPrimaryExtension (DiskFormat fmt)
{
    switch (fmt)
    {
        case DiskFormat::Woz: return ".woz";
        case DiskFormat::Dsk: return ".dsk";
        case DiskFormat::Do:  return ".do";
        case DiskFormat::Po:  return ".po";
        case DiskFormat::Nib: return ".nib";
        default:              break;
    }

    return "disk";
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDiagnosis::GetPrimaryExtensionText
//
//  The extension as wide text, for the interfaces that speak it.
//
//  IN CORE BECAUSE THE ANSWER IS, not because the conversion is interesting.
//  A dialog that widens this itself is a dialog holding a decision, and the
//  create dialog held two of them until recently -- its own switch over the
//  format, ending in a default arm that answered with the WOZ name, so a
//  container added without an arm was presented as a WOZ rather than refused.
//  Being in the executable, no test could reach it to notice.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MountDiagnosis::GetPrimaryExtensionText (DiskFormat fmt)
{
    std::string  narrow = GetPrimaryExtension (fmt);



    return std::wstring (narrow.begin(), narrow.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  MountDiagnosis::GetContainerCaption
//
//  How a chooser names the container: ".dsk" reads as "DSK" in a dropdown.
//
//  DERIVED FROM THE EXTENSION RATHER THAN LISTED, so the caption and the name
//  the file will actually be given cannot disagree. A second list here would
//  be a third place to add a container to, and the first two have already
//  drifted apart once.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring MountDiagnosis::GetContainerCaption (DiskFormat fmt)
{
    std::wstring  caption = GetPrimaryExtensionText (fmt);



    if (!caption.empty() && caption[0] == L'.')
    {
        caption.erase (0, 1);
    }

    for (wchar_t & letter : caption)
    {
        letter = (wchar_t) towupper (letter);
    }

    return caption;
}

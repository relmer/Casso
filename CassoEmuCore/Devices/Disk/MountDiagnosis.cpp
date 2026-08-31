#include "Pch.h"

#include "MountDiagnosis.h"
#include "NibblizationLayer.h"
#include "NibbleImageCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MountDiagnosis::Describe
//
//  The refusal in the user's terms, as a clause that follows the file's name.
//
//  EVERY CLAUSE SAYS WHAT TO DO OR WHAT IS TRUE OF THE FILE, never just that
//  something went wrong. "Not a valid disk image" tells a person nothing they
//  did not already know from the refusal itself; "is 4,096 bytes, but a .dsk
//  image must be exactly 143,360 bytes" tells them their download stopped
//  early. The size is the whole point of carrying it this far.
//
//  A diagnosis nobody filled in gets a clause that says so rather than the
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
            text = "is not a kind of file Casso reads as a disk image. Casso reads "
                   ".dsk, .do and .po sector images, and .woz bit-stream images";
            break;

        case MountFailure::FileUnreadable:
            text = "cannot be read. It may have been moved or deleted since it was "
                   "chosen, or another program may be holding it open";
            break;

        case MountFailure::EmptyFile:
            text = "is empty. There is nothing in it to read as a disk";
            break;

        case MountFailure::WrongSizeForFormat:
            observed = FormatByteCount (fileByteSize);
            required = FormatByteCount ((size_t) NibblizationLayer::kImageByteSize);

            snprintf (note, sizeof (note),
                      "is %s, but a %s image must be exactly %s -- 35 tracks of 16 "
                      "sectors of 256 bytes. A file of any other size was either "
                      "truncated on its way here or was never a disk image",
                      observed.c_str(), GetPrimaryExtension (format), required.c_str());

            text = note;
            break;

        case MountFailure::NotAWozFile:
            text = "is named .woz, but it does not begin with a WOZ file header, so "
                   "its contents are not a WOZ image. It was most likely renamed "
                   "from some other kind of file";
            break;

        case MountFailure::MalformedWoz:
            text = "begins with a WOZ file header, but the chunks behind it do not "
                   "hold together -- Casso could not find the INFO, TMAP and TRKS "
                   "data every WOZ image carries. The file is damaged or incomplete";
            break;

        case MountFailure::WrongSizeForNibble:
            observed = FormatByteCount (fileByteSize);
            required = FormatByteCount (NibbleImageCodec::kNibImageSize);
            second   = FormatByteCount (NibbleImageCodec::kNb2ImageSize);

            snprintf (note, sizeof (note),
                      "is %s, but a nibble image must be exactly %s -- 35 tracks "
                      "of 6,656 bytes -- or exactly %s, which is 35 tracks of "
                      "6,384. Both sizes are in circulation and either can carry "
                      "either name, so the length is what decides",
                      observed.c_str(), required.c_str(), second.c_str());

            text = note;
            break;

        case MountFailure::NotANibbleStream:
            text = "is the right size for a nibble image, but no part of it reads "
                   "as one -- not one byte anywhere has the high bit that every "
                   "nibble carries. It was most likely renamed from some other "
                   "kind of file";
            break;

        case MountFailure::Unrecognized:
            text = "could not be read as a disk image. Its contents are not a "
                   "layout this loader accepts";
            break;

        case MountFailure::None:
        default:
            text = "could not be opened as a disk image, and Casso did not record "
                   "why. That is a defect in Casso rather than a fault in the file";
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

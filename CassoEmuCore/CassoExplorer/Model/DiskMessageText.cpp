#include "Pch.h"

#include "CassoExplorer/Model/DiskMessageText.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMessageText::ShortenPaths
//
//  Each full path in a message cut to its last part, the file or folder's own
//  name: the address bar and the tree already show where it is, and a whole
//  path makes a message too long to read. A path runs from its drive to the
//  colon, quote or line end that follows; a sentence's period after it is not
//  part of it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskMessageText::ShortenPaths (const std::wstring & message)
{
    std::wstring  out;
    size_t        i = 0;



    while (i < message.size())
    {
        bool  isPath = i + 2 < message.size() && iswalpha (message[i]) && message[i + 1] == L':' && message[i + 2] == L'\\'
                       && (i == 0 || !iswalnum (message[i - 1]));

        if (!isPath)
        {
            out += message[i++];
            continue;
        }

        size_t        end  = message.find_first_of (L":\r\n\"'", i + 2);
        std::wstring  path  = message.substr (i, (end == std::wstring::npos) ? std::wstring::npos : end - i);
        std::wstring  tail;
        size_t        slash = 0;

        while (!path.empty() && (path.back() == L' ' || path.back() == L'.' || path.back() == L','))
        {
            tail.insert (tail.begin(), path.back());
            path.pop_back();
        }

        //  A file's extension ends in a letter, so a period only moved to the
        //  tail when it closed the sentence -- unless the name itself ended
        //  there, which the tail cannot tell apart and leaves as it was.
        while (!path.empty() && path.back() == L'\\')
        {
            path.pop_back();
        }

        slash = path.find_last_of (L'\\');
        out  += ((slash == std::wstring::npos) ? path : path.substr (slash + 1)) + tail;
        i     = (end == std::wstring::npos) ? message.size() : end;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMessageText::FormatLinkError
//
//  A disk whose sectors read but whose data is no volume detection recognizes
//  gets the same sentence as an image with no file system; any other failure
//  with no message is a drive that could not be read.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskMessageText::FormatLinkError (HRESULT result, const std::string & message)
{
    if (!message.empty())
    {
        return TextEncoding::NarrowToWide (message);
    }

    if (result == HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME))
    {
        return std::wstring (L"The disk in this drive ") + DiskMessageText::kNoFilesystemText;
    }

    return L"This drive could not be read.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMessageText::FormatImageError
//
//  The command line's refusal, "path: reason", as a sentence about the file
//  by its own name, since its path is in the address bar and the tree. An
//  image with no file system gets one plain sentence, and the rest is in the
//  preview's details. A display name, such as a partition's label, is used in
//  place of the file's name.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskMessageText::FormatImageError (const std::wstring & imagePath, const std::string & message, const std::wstring & displayName)
{
    std::wstring  text   = TextEncoding::NarrowToWide (message);
    std::wstring  prefix = imagePath + L": ";
    size_t        slash  = imagePath.find_last_of (L"\\/");
    std::wstring  leaf   = displayName.empty() ? imagePath.substr ((slash == std::wstring::npos) ? 0 : slash + 1) : displayName;



    if (message.find (DiskImageSession::kNoFilesystemText) != std::string::npos)
    {
        return leaf + L" " + DiskMessageText::kNoFilesystemText;
    }

    while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r' || text.back() == L' '))
    {
        text.pop_back();
    }

    if (text.compare (0, prefix.size(), prefix) == 0)
    {
        text = leaf + L" " + text.substr (prefix.size());
    }

    //  The general part leads and the detail follows on a line of its own.
    text = DiskMessageText::BreakAfterFirstSentence (text);

    if (!text.empty() && text.back() != L'.')
    {
        text += L'.';
    }

    return ShortenPaths (text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMessageText::BreakAfterFirstSentence
//
//  A message about an image leads with what is wrong and follows with the
//  detail on a line of its own.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskMessageText::BreakAfterFirstSentence (std::wstring text)
{
    size_t  stop = text.find (L". ");



    if (stop != std::wstring::npos)
    {
        text.replace (stop, 2, L".\n");
    }

    return text;
}

#include "Pch.h"

#include "CassoExplorer/Model/BasicListingOutput.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "ApplesoftTokenizer.h"
#include "Core/TextEncoding.h"
#include "FileTypes/RtfWriter.h"
#include "IntegerBasicDetokenizer.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicListingOutput::IsInteger
//
////////////////////////////////////////////////////////////////////////////////

bool BasicListingOutput::IsInteger (Byte type, VolumeKind kind)
{
    return (kind == VolumeKind::Dos33) ? type == Dos33Volume::kTypeInteger : type == kProDosInteger;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicListingOutput::RenameForOptions
//
////////////////////////////////////////////////////////////////////////////////

void BasicListingOutput::RenameForOptions (std::wstring & inOutName, const DecodeOptions & options)
{
    for (const wchar_t * suffix : { HostFileNaming::kApplesoftSuffix, HostFileNaming::kIntegerSuffix })
    {
        std::wstring_view  ending (suffix);

        if (options.basicColor && inOutName.ends_with (ending))
        {
            inOutName = inOutName.substr (0, inOutName.size() - 4) + HostFileNaming::kColorSuffix;
            return;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicListingOutput::Write
//
//  A program that doesn't list fails with the detokenizer's reason. Text
//  lines end in CR LF, as every text conversion's do.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BasicListingOutput::Write (std::span<const Byte> program, bool integer, const DecodeOptions & options, std::vector<Byte> & outBytes, std::wstring & outError)
{
    HRESULT                   hr       = S_OK;
    std::vector<Byte>         bytes (program.begin(), program.end());
    std::string               listing;
    std::string               text;
    ApplesoftListingError     applesoftError;
    IntegerBasicListingError  integerError;
    std::vector<Paragraph>    paragraphs;
    size_t                    start    = 0;
    size_t                    end      = 0;



    outBytes.clear();

    hr = integer ? IntegerBasicDetokenizer::Detokenize (bytes, listing, integerError)
                 : ApplesoftTokenizer::Detokenize (bytes, listing, applesoftError);
    CHRF (hr, outError = TextEncoding::NarrowToWide (integer ? integerError.reason : applesoftError.reason));

    if (!integer && options.basicRawControls && !options.basicColor)
    {
        RestoreControls (listing);
    }

    if (!options.basicColor)
    {
        for (char c : listing)
        {
            if (c == '\n')
            {
                text += '\r';
            }

            text += c;
        }

        outBytes.assign (text.begin(), text.end());
        return S_OK;
    }

    while (start < listing.size())
    {
        Paragraph  paragraph;

        end = listing.find ('\n', start);
        end = (end == std::string::npos) ? listing.size() : end;

        ColorLine (listing.substr (start, end - start), integer, paragraph.runs);
        paragraphs.push_back (std::move (paragraph));

        start = end + 1;
    }

    RtfWriter::Write (paragraphs, outBytes);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicListingOutput::RestoreControls
//
//  A caret before anything other than @ to _ or another caret is a caret.
//
////////////////////////////////////////////////////////////////////////////////

void BasicListingOutput::RestoreControls (std::string & inOutListing)
{
    std::string  out;
    char         next = 0;



    out.reserve (inOutListing.size());

    for (size_t at = 0; at < inOutListing.size(); at++)
    {
        next = (at + 1 < inOutListing.size()) ? inOutListing[at + 1] : '\0';

        if (inOutListing[at] == kCaret && next == kCaret)
        {
            out += kCaret;
            at++;
        }
        else if (inOutListing[at] == kCaret && next >= '@' && next <= '_' && next != kCaret)
        {
            out += (char) (next - kControlBase);
            at++;
        }
        else
        {
            out += inOutListing[at];
        }
    }

    inOutListing = std::move (out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicListingOutput::ColorLine
//
//  The line number is muted, a string runs to its closing quote or the end
//  of the line, and a remark to the end of the line. DATA is text up to a
//  colon outside a string.
//
////////////////////////////////////////////////////////////////////////////////

void BasicListingOutput::ColorLine (const std::string & line, bool integer, std::vector<Run> & outRuns)
{
    size_t       at     = 0;
    size_t       end    = 0;
    size_t       length = 0;
    std::string  plain;
    std::string  keyword;
    bool         data   = false;



    outRuns.clear();

    while (at < line.size() && (line[at] == ' ' || isdigit ((unsigned char) line[at])))
    {
        at++;
    }

    AddRun (outRuns, line.substr (0, at), RunColor::Muted);

    while (at < line.size())
    {
        if (line[at] == '"')
        {
            end = line.find ('"', at + 1);
            end = (end == std::string::npos) ? line.size() : end + 1;

            AddRun (outRuns, plain, RunColor::Text);
            AddRun (outRuns, line.substr (at, end - at), RunColor::String);

            plain.clear();
            at = end;
            continue;
        }

        if (data && line[at] == ':')
        {
            data = false;
        }

        length = data ? 0 : MatchKeyword (line, at, integer);

        if (length == 0)
        {
            plain += line[at++];
            continue;
        }

        keyword = line.substr (at, length);

        AddRun (outRuns, plain, RunColor::Text);
        AddRun (outRuns, keyword, RunColor::Keyword);

        plain.clear();
        at += length;

        if (keyword == "REM")
        {
            AddRun (outRuns, line.substr (at), RunColor::Comment);
            return;
        }

        data = keyword == "DATA";
    }

    AddRun (outRuns, plain, RunColor::Text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicListingOutput::GetKeywords
//
//  Each token's keyword that starts with a letter, longest first, so that
//  ATN is found ahead of AT.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<std::string> & BasicListingOutput::GetKeywords (bool integer)
{
    static const std::vector<std::string>  s_applesoft = [] ()
    {
        std::vector<std::string>  keywords;

        for (int token = 0x80; token <= 0xFF; token++)
        {
            const char *  keyword = ApplesoftTokenizer::GetKeyword ((Byte) token);

            if (keyword != nullptr && isalpha ((unsigned char) keyword[0]))
            {
                keywords.push_back (keyword);
            }
        }

        std::stable_sort (keywords.begin(), keywords.end(), [] (const std::string & a, const std::string & b) { return a.size() > b.size(); });
        return keywords;
    } ();

    static const std::vector<std::string>  s_integer = [] ()
    {
        std::vector<std::string>  keywords;

        for (int token = 0; token <= IntegerBasicDetokenizer::kLastToken; token++)
        {
            const char *  keyword = IntegerBasicDetokenizer::GetKeyword ((Byte) token);

            if (keyword != nullptr && isalpha ((unsigned char) keyword[0]))
            {
                keywords.push_back (keyword);
            }
        }

        std::stable_sort (keywords.begin(), keywords.end(), [] (const std::string & a, const std::string & b) { return a.size() > b.size(); });
        return keywords;
    } ();



    return integer ? s_integer : s_applesoft;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicListingOutput::MatchKeyword
//
//  The length of the keyword at `at`, trailing spaces trimmed, or 0. Integer
//  BASIC doesn't tokenize a keyword inside a variable's name, so there a
//  keyword starts only after a character that isn't a letter or a digit.
//
////////////////////////////////////////////////////////////////////////////////

size_t BasicListingOutput::MatchKeyword (const std::string & line, size_t at, bool integer)
{
    size_t  length = 0;



    if (integer && at > 0 && isalnum ((unsigned char) line[at - 1]))
    {
        return 0;
    }

    for (const std::string & keyword : GetKeywords (integer))
    {
        if (line.compare (at, keyword.size(), keyword) == 0)
        {
            length = keyword.size();

            while (length > 0 && keyword[length - 1] == ' ')
            {
                length--;
            }

            return length;
        }
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicListingOutput::AddRun
//
////////////////////////////////////////////////////////////////////////////////

void BasicListingOutput::AddRun (std::vector<Run> & ioRuns, const std::string & text, RunColor color)
{
    if (!text.empty())
    {
        ioRuns.push_back (Run { TextEncoding::NarrowToWide (text), false, false, false, color });
    }
}

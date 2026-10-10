#include "Pch.h"

#include "Update/ReleaseNotesFormatter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::Format
//
//  One FormattedLine per heading, bullet or paragraph. A non-blank line that
//  follows a bullet or paragraph line directly continues it, which is how a
//  wrapped CHANGELOG bullet reads. Runs of blank lines collapse to one
//  Blank, and none is kept at the end.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesFormatter::Format (
    const std::string           & markdown,
    std::vector<FormattedLine>  & outLines)
{
    std::string_view             rest      = markdown;
    std::string_view             line;
    std::string_view             trimmed;
    size_t                       lf        = 0;
    size_t                       first     = 0;
    bool                         isOpen    = false;
    FormattedLine                parsed;
    std::vector<FormattedImage>  images;
    std::string                  imageRest;



    outLines.clear();

    while (!rest.empty())
    {
        lf   = rest.find ('\n');
        line = rest.substr (0, lf);
        rest = (lf == std::string_view::npos) ? std::string_view() : rest.substr (lf + 1);

        if (line.ends_with ('\r'))
        {
            line.remove_suffix (1);
        }

        first   = line.find_first_not_of (" \t");
        trimmed = (first == std::string_view::npos) ? std::string_view() : line.substr (first);

        if (trimmed.empty())
        {
            if (!outLines.empty() && outLines.back().kind != FormattedLineKind::Blank)
            {
                outLines.push_back (FormattedLine { FormattedLineKind::Blank });
            }

            isOpen = false;
            continue;
        }

        if (TryExtractImages (trimmed, images, imageRest))
        {
            for (FormattedImage & image : images)
            {
                parsed       = {};
                parsed.kind  = FormattedLineKind::Image;
                parsed.image = std::move (image);
                outLines.push_back (std::move (parsed));
            }

            parsed      = {};
            parsed.kind = FormattedLineKind::Paragraph;
            FormatInline (imageRest, parsed.runs);
            isOpen      = HasVisibleText (parsed.runs);

            if (isOpen)
            {
                outLines.push_back (std::move (parsed));
            }

            continue;
        }

        if (TryParseHeading (line, parsed))
        {
            outLines.push_back (std::move (parsed));
            isOpen = false;
            continue;
        }

        if (TryParseBullet (line, parsed))
        {
            outLines.push_back (std::move (parsed));
            isOpen = true;
            continue;
        }

        if (isOpen)
        {
            AppendContinuation (outLines.back(), trimmed);
            continue;
        }

        // A line of nothing but HTML structure (<table>, <tr>, </td>) draws
        // nothing, and does not join the text around it either.
        parsed      = {};
        parsed.kind = FormattedLineKind::Paragraph;
        FormatInline (trimmed, parsed.runs);
        isOpen      = HasVisibleText (parsed.runs);

        if (isOpen)
        {
            outLines.push_back (std::move (parsed));
        }
    }

    while (!outLines.empty() && outLines.back().kind == FormattedLineKind::Blank)
    {
        outLines.pop_back();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::TryParseHeading
//
//  "#" to "####" at the start of the line, then a space. Deeper headings are
//  not in the subset and stay paragraphs.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesFormatter::TryParseHeading (std::string_view text, FormattedLine & outLine)
{
    static constexpr int  kMaxHeadingLevel = 4;
    size_t                hashes           = text.find_first_not_of ('#');
    int                   level            = (int) hashes;
    bool                  isHeading        = false;



    outLine   = {};
    isHeading = hashes != std::string_view::npos &&
                level >= 1                       &&
                level <= kMaxHeadingLevel        &&
                text[hashes] == ' ';

    if (isHeading)
    {
        outLine.kind         = FormattedLineKind::Heading;
        outLine.headingLevel = level;
        FormatInline (text.substr (hashes + 1), outLine.runs);
    }

    return isHeading;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::TryParseBullet
//
//  "- " or "* " after any indent. Every two columns of indent is one level
//  of nesting.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesFormatter::TryParseBullet (std::string_view text, FormattedLine & outLine)
{
    static constexpr int  kColumnsPerLevel = 2;
    size_t                indent           = text.find_first_not_of (' ');
    bool                  isBullet         = false;



    outLine  = {};
    isBullet = indent != std::string_view::npos              &&
               indent + 1 < text.size()                      &&
               (text[indent] == '-' || text[indent] == '*')  &&
               text[indent + 1] == ' ';

    if (isBullet)
    {
        outLine.kind        = FormattedLineKind::Bullet;
        outLine.indentLevel = (int) indent / kColumnsPerLevel;
        FormatInline (text.substr (indent + 2), outLine.runs);
    }

    return isBullet;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::AppendContinuation
//
//  Joins a wrapped line onto the block it continues, with one space.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesFormatter::AppendContinuation (FormattedLine & line, std::string_view text)
{
    std::vector<FormattedRun>  runs;



    FormatInline (text, runs);
    AppendRun (line.runs, " ", false, false, "");

    for (const FormattedRun & run : runs)
    {
        AppendRun (line.runs, run.text, run.bold, run.code, run.linkUrl);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::FormatInline
//
//  **bold**, `code` and [text](url). A marker without its partner is plain
//  text, so an unmatched ** or ` shows as typed. Inline HTML tags and
//  comments draw nothing in a rendered README, so they are dropped here too;
//  any text between two tags stays.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesFormatter::FormatInline (std::string_view text, std::vector<FormattedRun> & outRuns)
{
    static constexpr std::string_view  kBold = "**";
    std::string       plain;
    std::string_view  label;
    std::string_view  url;
    size_t            i     = 0;
    size_t            close = 0;
    size_t            end   = 0;
    bool              bold  = false;



    outRuns.clear();

    while (i < text.size())
    {
        if (text.substr (i).starts_with (kBold) && (bold || text.find (kBold, i + kBold.size()) != std::string_view::npos))
        {
            AppendRun (outRuns, plain, bold, false, "");
            plain.clear();
            bold = !bold;
            i   += kBold.size();
            continue;
        }

        close = (text[i] == '`') ? text.find ('`', i + 1) : std::string_view::npos;

        if (close != std::string_view::npos)
        {
            AppendRun (outRuns, plain, bold, false, "");
            plain.clear();
            AppendRun (outRuns, text.substr (i + 1, close - i - 1), bold, true, "");
            i = close + 1;
            continue;
        }

        if (text[i] == '<' && TryParseHtmlTag (text, i, end))
        {
            i = end;
            continue;
        }

        if (text[i] == '[' && TryParseLink (text, i, label, url, end))
        {
            AppendRun (outRuns, plain, bold, false, "");
            plain.clear();
            AppendRun (outRuns, label, bold, false, url);
            i = end;
            continue;
        }

        plain += text[i];
        i++;
    }

    AppendRun (outRuns, plain, bold, false, "");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::TryParseHtmlTag
//
//  An HTML comment "<!-- ... -->", or a tag: '<', an optional '/', a letter,
//  then anything but '<' up to the closing '>'. `outEnd` is just past the
//  end. A '<' that starts neither ("a < b", "<3") is ordinary text.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesFormatter::TryParseHtmlTag (std::string_view text, size_t start, size_t & outEnd)
{
    static constexpr std::string_view  kCommentOpen  = "<!--";
    static constexpr std::string_view  kCommentClose = "-->";
    std::string_view  rest     = text.substr (start);
    size_t            nameAt   = 1;
    size_t            close    = std::string_view::npos;
    bool              isTag    = false;



    if (rest.starts_with (kCommentOpen))
    {
        close = rest.find (kCommentClose, kCommentOpen.size());
        isTag = close != std::string_view::npos;

        if (isTag)
        {
            outEnd = start + close + kCommentClose.size();
        }

        return isTag;
    }

    if (rest.size() > nameAt && rest[nameAt] == '/')
    {
        nameAt++;
    }

    if (rest.size() > nameAt && std::isalpha ((unsigned char) rest[nameAt]))
    {
        close = rest.find_first_of ("<>", nameAt);
        isTag = close != std::string_view::npos && rest[close] == '>';
    }

    if (isTag)
    {
        outEnd = start + close + 1;
    }

    return isTag;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::TryParseLink
//
//  "[label](url)" starting at `start`; `outEnd` is just past the ')'.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesFormatter::TryParseLink (
    std::string_view    text,
    size_t              start,
    std::string_view  & outLabel,
    std::string_view  & outUrl,
    size_t            & outEnd)
{
    size_t  close  = text.find (']', start + 1);
    size_t  paren  = std::string_view::npos;
    bool    isLink = false;



    if (close != std::string_view::npos && close + 1 < text.size() && text[close + 1] == '(')
    {
        paren = text.find (')', close + 2);
    }

    isLink = paren != std::string_view::npos && paren > close + 2;

    if (isLink)
    {
        outLabel = text.substr (start + 1, close - start - 1);
        outUrl   = text.substr (close + 2, paren - close - 2);
        outEnd   = paren + 1;
    }

    return isLink;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::AppendRun
//
//  Adds a run, merging it into the last one when the style matches. Empty
//  text adds nothing.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseNotesFormatter::AppendRun (
    std::vector<FormattedRun>  & runs,
    std::string_view             text,
    bool                         bold,
    bool                         code,
    std::string_view             linkUrl)
{
    bool  isMergeable = false;



    if (text.empty())
    {
        return;
    }

    isMergeable = !runs.empty()              &&
                  runs.back().bold == bold   &&
                  runs.back().code == code   &&
                  runs.back().linkUrl == linkUrl;

    if (isMergeable)
    {
        runs.back().text += text;
    }
    else
    {
        runs.push_back (FormattedRun { std::string (text), bold, code, std::string (linkUrl) });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::HasVisibleText
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesFormatter::HasVisibleText (const std::vector<FormattedRun> & runs)
{
    bool  isVisible = false;



    for (const FormattedRun & run : runs)
    {
        isVisible = isVisible || run.text.find_first_not_of (" \t") != std::string::npos;
    }

    return isVisible;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::TryGetAttribute
//
//  One attribute of an HTML tag, quoted with " or ' or bare. The name must
//  follow whitespace, so `alt` does not match inside `data-alt`.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesFormatter::TryGetAttribute (std::string_view tag, std::string_view name, std::string & outValue)
{
    size_t  at      = 0;
    size_t  start   = 0;
    size_t  end     = 0;
    char    quote   = 0;
    bool    isFound = false;



    outValue.clear();

    while (!isFound && (at = tag.find (name, at)) != std::string_view::npos)
    {
        start   = at + name.size();
        isFound = at > 0 && std::isspace ((unsigned char) tag[at - 1]) && start < tag.size() && tag[start] == '=';
        at      = start;
    }

    if (isFound)
    {
        start++;
        quote = (start < tag.size() && (tag[start] == '"' || tag[start] == '\'')) ? tag[start] : 0;
        start = quote != 0 ? start + 1 : start;
        end   = quote != 0 ? tag.find (quote, start) : tag.find_first_of (" \t/>", start);
        end   = (end == std::string_view::npos) ? tag.size() : end;

        outValue = std::string (tag.substr (start, end - start));
    }

    return isFound;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::TryParseImgTag
//
//  "<img src=... alt=... width=...>" at `start`. A tag without a src is not
//  an image. Only a percent width is kept; a pixel width says nothing about
//  how wide the dialog's column is.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesFormatter::TryParseImgTag (std::string_view text, size_t start, FormattedImage & outImage, size_t & outEnd)
{
    std::string_view  rest    = text.substr (start);
    std::string_view  tag;
    std::string       width;
    size_t            close   = 0;
    bool              isImage = false;
    int               percent = 0;



    outImage = {};

    if (rest.size() > 4 && _strnicmp (rest.data(), "<img", 4) == 0 && std::isspace ((unsigned char) rest[4]))
    {
        close = rest.find ('>');
    }
    else
    {
        close = std::string_view::npos;
    }

    if (close != std::string_view::npos)
    {
        tag     = rest.substr (0, close + 1);
        isImage = TryGetAttribute (tag, "src", outImage.src) && !outImage.src.empty();
    }

    if (isImage)
    {
        TryGetAttribute (tag, "alt", outImage.alt);

        if (TryGetAttribute (tag, "width", width) && width.ends_with ('%'))
        {
            std::from_chars (width.data(), width.data() + width.size() - 1, percent);
            outImage.widthPercent = std::clamp (percent, 0, 100);
        }

        outEnd = start + close + 1;
    }

    return isImage;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::TryParseMdImage
//
//  "![alt](src)" or "![alt](src "title")" at `start`.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesFormatter::TryParseMdImage (std::string_view text, size_t start, FormattedImage & outImage, size_t & outEnd)
{
    std::string_view  label;
    std::string_view  url;
    size_t            end     = 0;
    size_t            space   = 0;
    bool              isImage = false;



    outImage = {};

    if (text.substr (start).starts_with ("![") && TryParseLink (text, start + 1, label, url, end))
    {
        space          = url.find (' ');
        outImage.src   = std::string (url.substr (0, space));
        outImage.alt   = std::string (label);
        outEnd         = end;
        isImage        = !outImage.src.empty();
    }

    return isImage;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter::TryExtractImages
//
//  Pulls every image out of one line, in order, and returns the line
//  without them. Both markdown and <img> forms count, and an image wrapped
//  in a link (a badge) counts as the image alone. A <sub> after an image and
//  before the next image or the end of the table cell is that image's
//  caption, and leaves the line with it. False when the line has no image.
//
////////////////////////////////////////////////////////////////////////////////

bool ReleaseNotesFormatter::TryExtractImages (
    std::string_view               line,
    std::vector<FormattedImage>  & outImages,
    std::string                  & outRest)
{
    static constexpr std::string_view  kSubOpen  = "<sub>";
    static constexpr std::string_view  kSubClose = "</sub>";
    static constexpr std::string_view  kCellEnd  = "</td>";
    FormattedImage             image;
    std::vector<FormattedRun>  captionRuns;
    size_t                     i          = 0;
    size_t                     end        = 0;
    size_t                     close      = 0;
    bool                       canCaption = false;



    outImages.clear();
    outRest.clear();

    while (i < line.size())
    {
        if (line[i] == '[' && TryParseMdImage (line, i + 1, image, end) &&
            line.substr (end).starts_with ("](") && (close = line.find (')', end)) != std::string_view::npos)
        {
            outImages.push_back (std::move (image));
            canCaption = true;
            i          = close + 1;
            continue;
        }

        if ((line[i] == '!' && TryParseMdImage (line, i, image, end)) ||
            (line[i] == '<' && TryParseImgTag  (line, i, image, end)))
        {
            outImages.push_back (std::move (image));
            canCaption = true;
            i          = end;
            continue;
        }

        if (canCaption && line.substr (i).starts_with (kSubOpen) &&
            (close = line.find (kSubClose, i)) != std::string_view::npos)
        {
            FormatInline (line.substr (i + kSubOpen.size(), close - i - kSubOpen.size()), captionRuns);

            for (const FormattedRun & run : captionRuns)
            {
                outImages.back().caption += run.text;
            }

            canCaption = false;
            i          = close + kSubClose.size();
            continue;
        }

        if (line.substr (i).starts_with (kCellEnd))
        {
            canCaption = false;
        }

        outRest += line[i];
        i++;
    }

    return !outImages.empty();
}
#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FormattedRun
//
//  A stretch of text with one style. `linkUrl` is empty except for the text
//  of a link.
//
////////////////////////////////////////////////////////////////////////////////

struct FormattedRun
{
    std::string  text;
    bool         bold    = false;
    bool         code    = false;
    std::string  linkUrl;

    bool operator== (const FormattedRun &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FormattedLineKind
//
////////////////////////////////////////////////////////////////////////////////

enum class FormattedLineKind
{
    Heading,
    Bullet,
    Paragraph,
    Blank,
};





////////////////////////////////////////////////////////////////////////////////
//
//  FormattedLine
//
//  One block of the update dialog's body. `headingLevel` is 1-4 for a
//  heading and 0 otherwise; `indentLevel` is a bullet's nesting depth.
//
////////////////////////////////////////////////////////////////////////////////

struct FormattedLine
{
    FormattedLineKind          kind         = FormattedLineKind::Paragraph;
    int                        headingLevel = 0;
    int                        indentLevel  = 0;
    std::vector<FormattedRun>  runs;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesFormatter
//
//  Turns the markdown subset the release notes use into styled lines:
//  headings, nested bullets, paragraphs, bold, code and links. Syntax
//  outside the subset is kept as plain text, never dropped.
//
////////////////////////////////////////////////////////////////////////////////

class ReleaseNotesFormatter
{
public:
    static void  Format             (const std::string           & markdown,
                                     std::vector<FormattedLine>  & outLines);

    static void  FormatInline       (std::string_view              text,
                                     std::vector<FormattedRun>   & outRuns);

private:
    static bool  TryParseHeading    (std::string_view text, FormattedLine & outLine);
    static bool  TryParseBullet     (std::string_view text, FormattedLine & outLine);
    static void  AppendRun          (std::vector<FormattedRun> & runs,
                                     std::string_view            text,
                                     bool                        bold,
                                     bool                        code,
                                     std::string_view            linkUrl);
    static bool  TryParseLink       (std::string_view    text,
                                     size_t              start,
                                     std::string_view  & outLabel,
                                     std::string_view  & outUrl,
                                     size_t            & outEnd);
    static void  AppendContinuation (FormattedLine & line, std::string_view text);
};

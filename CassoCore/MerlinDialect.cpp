#include "Pch.h"

#include "MerlinDialect.h"



//  Merlin's string directives, whose operand is delimiter-quoted rather than
//  whitespace-delimited. The delimiter is whatever character opens the text, so
//  these are the mnemonics whose operand must be scanned by that rule.
static const char * const  s_kpszDelimitedTextDirectives[] =
{
    "ASC",
    "DCI",
    "INV",
    "FLS",
    "STR",
};



//  Introduces a comment when it BEGINS a field. Inside the operand it is data --
//  Merlin's macro-argument separator.
static const char  s_kCommentIntroducer = ';';

//  Introduces a whole-line comment in column 1.
static const char  s_kLineCommentIntroducer = '*';





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::IsFieldSpace
//
//  Tabs separate fields exactly as spaces do, and are never expanded: tab stops
//  change where text appears on a screen and nothing about what it means.
//
////////////////////////////////////////////////////////////////////////////////

bool MerlinDialect::IsFieldSpace (char ch)
{
    return (ch == ' ') || (ch == '\t');
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::TakesDelimitedText
//
////////////////////////////////////////////////////////////////////////////////

bool MerlinDialect::TakesDelimitedText (const std::string & mnemonic)
{
    std::string  upper = Parser::ToUpper (mnemonic);
    bool         found = false;



    for (const char * pszSpelling : s_kpszDelimitedTextDirectives)
    {
        if (upper == pszSpelling)
        {
            found = true;
            break;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::SkipFieldSpace
//
////////////////////////////////////////////////////////////////////////////////

void MerlinDialect::SkipFieldSpace (const std::string & line, size_t & pos)
{
    while ((pos < line.size()) && IsFieldSpace (line[pos]))
    {
        pos++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::ReadPlainField
//
//  One whitespace-delimited field.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinDialect::ReadPlainField (const std::string & line, size_t & pos)
{
    size_t  start = pos;



    while ((pos < line.size()) && !IsFieldSpace (line[pos]))
    {
        pos++;
    }

    return line.substr (start, pos - start);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::ReadOperandField
//
//  The operand, which is whitespace-delimited EXCEPT where a string directive
//  opens delimited text.
//
//  The delimiter is whatever character opens the text -- any character, not a
//  fixed quote set. `ASC !" ASC ""!` on the vendor disk chooses `!` precisely
//  because the text contains quotes, and a scanner hard-coded to `"` would end
//  the operand in the middle of the data.
//
//  Scanning continues past the closing delimiter to the next whitespace, because
//  Merlin allows a trailing byte after the text (`ASC "ABC",8D`), and that byte
//  is part of the operand rather than the beginning of a comment.
//
//  An unterminated delimiter runs to end of line rather than erroring here.
//  Parsing is purely syntactic; deciding that a string was never closed is a
//  diagnostic belonging to the pass that knows what the directive means.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinDialect::ReadOperandField (const std::string & line, size_t & pos, const std::string & mnemonic)
{
    size_t  start     = pos;
    char    delimiter = 0;



    if (!TakesDelimitedText (mnemonic))
    {
        return ReadPlainField (line, pos);
    }

    delimiter = line[pos];
    pos++;

    while ((pos < line.size()) && (line[pos] != delimiter))
    {
        pos++;
    }

    // Step over the closing delimiter when there was one.
    if (pos < line.size())
    {
        pos++;
    }

    while ((pos < line.size()) && !IsFieldSpace (line[pos]))
    {
        pos++;
    }

    return line.substr (start, pos - start);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::ParseLine
//
//  One Merlin source line into its fields.
//
//  Field ORDER carries the meaning, and the only significant column is the
//  first. A line starting in column 0 opens with a label; a line starting with
//  whitespace has none and begins at the opcode.
//
////////////////////////////////////////////////////////////////////////////////

ParsedLine MerlinDialect::ParseLine (const std::string & line, int lineNumber) const
{
    ParsedLine  result = {};
    size_t      pos    = 0;



    result.lineNumber      = lineNumber;
    result.isEmpty         = true;
    result.startsAtColumn0 = (!line.empty() && !IsFieldSpace (line[0]));

    if (line.empty())
    {
        return result;
    }

    // A whole-line comment. The semicolon case is NOT a separate rule: with no
    // label present, column 1 is the first field boundary, so a semicolon there
    // is a semicolon beginning a field.
    if ((line[0] == s_kLineCommentIntroducer) || (line[0] == s_kCommentIntroducer))
    {
        return result;
    }

    if (result.startsAtColumn0)
    {
        result.label = ReadPlainField (line, pos);
    }

    SkipFieldSpace (line, pos);

    if ((pos < line.size()) && (line[pos] != s_kCommentIntroducer))
    {
        result.mnemonic = ReadPlainField (line, pos);

        SkipFieldSpace (line, pos);

        if ((pos < line.size()) && (line[pos] != s_kCommentIntroducer))
        {
            result.operand = ReadOperandField (line, pos, result.mnemonic);
        }
    }

    // Everything from here is the comment field, and is discarded: nothing
    // downstream needs it, and a line that is only a comment must look empty.
    result.isEmpty = result.label.empty() && result.mnemonic.empty();

    return result;
}

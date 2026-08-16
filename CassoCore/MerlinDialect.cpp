#include "Pch.h"

#include "MerlinDialect.h"



//  Merlin's directive vocabulary. Frequencies in the comments are counts across
//  the nine committed vendor sources, so a reader can tell a workhorse from a
//  directive that appears twice.
static constexpr MerlinDirectiveTable::Spelling  s_kMerlinSpellings[] =
{
    //  The string family: ONE operation with six encodings, not six operations.
    //  They differ in high-bit handling, inversion, and terminator convention --
    //  parameters rather than different work. Which encoding applies is resolved
    //  from the spelling later; the token only says "string data".
    { "ASC",  Directive::StringData      },   //  35
    { "DCI",  Directive::StringData      },   // 130 -- the workhorse, and the corpus's sharpest probe
    { "INV",  Directive::StringData      },   //   1
    { "FLS",  Directive::StringData      },
    { "STR",  Directive::StringData      },
    { "REV",  Directive::StringData      },   //   3 -- found in the sources, absent from the spec's list

    //  Data.
    { "DFB",  Directive::Byte            },   //  14
    { "DB",   Directive::Byte            },
    { "DA",   Directive::Word            },   //   8
    { "DW",   Directive::Word            },
    { "DDB",  Directive::WordHighFirst   },   //   0 -- vocabulary, not idiom; see the header
    { "HEX",  Directive::HexData         },   //  13
    { "DS",   Directive::Ds              },   //   2

    //  Location and output.
    { "ORG",  Directive::Org             },   //  16
    { "DSK",  Directive::ObjectFile      },   //   2
    { "END",  Directive::End             },

    //  Inclusion. Two spellings, one operation -- the difference between them is
    //  which filesystem convention the name follows, not what the assembler does.
    { "PUT",  Directive::Include         },   //   1
    { "USE",  Directive::Include         },   //   2

    //  Conditional assembly. CLOCK.S turns one source into two objects with
    //  these, which is why it is the corpus's best single specimen.
    { "DO",   Directive::If              },   //   5
    { "ELSE", Directive::Else            },   //   1
    { "FIN",  Directive::Endif           },   //   5

    //  Macros. `<<<` is the TERMINATOR of a definition, not an invocation --
    //  every macro in the vendor library ends with it.
    { "MAC",  Directive::MacroDef        },   //  18
    { "<<<",  Directive::MacroEnd        },   //  18

    //  Structure.
    { "LUP",  Directive::Loop            },
    { "--^",  Directive::LoopEnd         },
    { "DUM",  Directive::DummySection    },
    { "DEND", Directive::DummySectionEnd },

    //  Assembly-time assertion. LABELS.S depends on it.
    { "ERR",  Directive::ErrorIf         },   //  17

    //  Listing control, which as65 already has a token for.
    { "PAG",  Directive::Page            },   //   1

    //  The CPU selector. Merlin takes its target from here and nowhere else.
    { "XC",   Directive::CpuSelect       },

    //  Outside the supported subset. Present so they are refused BY NAME.
    { "REL",  Directive::Relocatable     },   //   2
    { "ENT",  Directive::EntrySymbol     },   //   7
    { "EXT",  Directive::ExternalSymbol  },   //   3
    { "TYP",  Directive::FileType        },
    { "SAV",  Directive::SaveObject      },   //   2
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
//  MerlinDirectiveTable::FromSpelling
//
////////////////////////////////////////////////////////////////////////////////

Directive MerlinDirectiveTable::FromSpelling (const std::string & word)
{
    Directive  token = Directive::None;



    for (const Spelling & entry : s_kMerlinSpellings)
    {
        if (word == entry.name)
        {
            token = entry.token;
            break;
        }
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDirectiveTable::GetCanonicalName
//
//  The first spelling mapping to the token, which for the string family means
//  ASC rather than the far more common DCI. That is deliberate: the canonical
//  name answers "what operation is this", and every encoding of it is a string.
//
////////////////////////////////////////////////////////////////////////////////

const char * MerlinDirectiveTable::GetCanonicalName (Directive directive)
{
    const char *  name = "";



    for (const Spelling & entry : s_kMerlinSpellings)
    {
        if (entry.token == directive)
        {
            name = entry.name;
            break;
        }
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDirectiveTable::GetAllSpellings
//
////////////////////////////////////////////////////////////////////////////////

std::span<const MerlinDirectiveTable::Spelling> MerlinDirectiveTable::GetAllSpellings()
{
    return std::span<const Spelling> (s_kMerlinSpellings);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDirectiveTable::EncodingModeForSpelling
//
//  Which of the six encodings a string spelling selects.
//
//  A table rather than a chain of comparisons, and separate from the spelling
//  table above because the two answer different questions: that one says "this
//  is string data", this one says "encoded how". Collapsing them would put an
//  encoding column on every row of a table that is mostly not strings.
//
////////////////////////////////////////////////////////////////////////////////

StringEncodingMode MerlinDirectiveTable::EncodingModeForSpelling (const std::string & spelling)
{
    struct ModeRow
    {
        const char *        name;
        StringEncodingMode  mode;
    };

    static constexpr ModeRow  s_kModes[] =
    {
        { "ASC", StringEncodingMode::Plain          },
        { "DCI", StringEncodingMode::DciTerminated  },
        { "INV", StringEncodingMode::Inverse        },
        { "FLS", StringEncodingMode::Flashing       },
        { "STR", StringEncodingMode::LengthPrefixed },
        { "REV", StringEncodingMode::Reversed       },
    };

    StringEncodingMode  mode = StringEncodingMode::Plain;



    for (const ModeRow & row : s_kModes)
    {
        if (spelling == row.name)
        {
            mode = row.mode;
            break;
        }
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinDialect::TakesDelimitedText
//
//  Whether this mnemonic's operand is delimiter-quoted rather than
//  whitespace-delimited.
//
//  Asks the real directive table rather than carrying its own list of string
//  spellings. It did carry one while the table did not exist, which meant the
//  operand scanner -- whose correctness the `!` delimiter rule turns on -- was
//  resting on a private copy that could disagree with the vocabulary. Adding
//  REV to the table now reaches the scanner by construction instead of needing
//  a second edit that could be forgotten.
//
////////////////////////////////////////////////////////////////////////////////

bool MerlinDialect::TakesDelimitedText (const std::string & mnemonic)
{
    return MerlinDirectiveTable::FromSpelling (Parser::ToUpper (mnemonic)) == Directive::StringData;
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

    // Resolve the opcode field against Merlin's vocabulary. A word that is not a
    // directive stays a mnemonic -- it is an instruction or a macro invocation,
    // and telling those two apart needs the macro table rather than the parser.
    result.directiveToken = MerlinDirectiveTable::FromSpelling (Parser::ToUpper (result.mnemonic));
    result.isDirective    = (result.directiveToken != Directive::None);

    if (result.isDirective)
    {
        result.directive    = MerlinDirectiveTable::GetCanonicalName (result.directiveToken);
        result.directiveArg = result.operand;
    }

    // Everything from here is the comment field, and is discarded: nothing
    // downstream needs it, and a line that is only a comment must look empty.
    result.isEmpty = result.label.empty() && result.mnemonic.empty();

    return result;
}

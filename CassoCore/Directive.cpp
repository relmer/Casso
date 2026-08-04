#include "Pch.h"

#include "Directive.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kSpellings
//
//  Every accepted spelling, dotted and bare. The bare forms are as65's; a
//  different dialect supplies a different table and shares everything else.
//
//  A 60-line payload, so it stays a file-scope `static constexpr` under the
//  documented 3+ line exception.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr DirectiveTable::Spelling  s_kSpellings[] =
{
    // Dotted canonical spellings.
    { ".ALIGN",        Directive::Align       },
    { ".BYTE",         Directive::Byte        },
    { ".CMAP",         Directive::Cmap        },
    { ".DD",           Directive::Dd          },
    { ".DS",           Directive::Ds          },
    { ".ELSE",         Directive::Else        },
    { ".END",          Directive::End         },
    { ".ENDIF",        Directive::Endif       },
    { ".ERROR",        Directive::Error       },
    { ".IF",           Directive::If          },
    { ".IFDEF",        Directive::Ifdef       },
    { ".IFNDEF",       Directive::Ifndef      },
    { ".INCLUDE",      Directive::Include     },
    { ".LIST",         Directive::List        },
    { ".MULTINOP",     Directive::MultiNop    },
    { ".NOLIST",       Directive::Nolist      },
    { ".OPT_NOOP",     Directive::OptNoop     },
    { ".ORG",          Directive::Org         },
    { ".PAGE",         Directive::Page        },
    { ".STRUCT",       Directive::Struct      },
    { ".TEXT",         Directive::Text        },
    { ".TITLE",        Directive::Title       },
    { ".WORD",         Directive::Word        },

    // Segment spellings -- long and short forms are the same token.
    { ".SEGMENT_BSS",  Directive::SegmentBss  },
    { ".SEGMENT_CODE", Directive::SegmentCode },
    { ".SEGMENT_DATA", Directive::SegmentData },
    { ".BSS",          Directive::SegmentBss  },
    { ".CODE",         Directive::SegmentCode },
    { ".DATA",         Directive::SegmentData },

    // as65 bare synonyms.
    { "ALIGN",         Directive::Align       },
    { "BYT",           Directive::Byte        },
    { "BYTE",          Directive::Byte        },
    { "DB",            Directive::Byte        },
    { "FCB",           Directive::Byte        },
    { "FCC",           Directive::Byte        },
    { "DW",            Directive::Word        },
    { "FCW",           Directive::Word        },
    { "FDB",           Directive::Word        },
    { "WORD",          Directive::Word        },
    { "DD",            Directive::Dd          },
    { "DS",            Directive::Ds          },
    { "DSB",           Directive::Ds          },
    { "END",           Directive::End         },
    { "ERROR",         Directive::Error       },
    { "ORG",           Directive::Org         },
    { "BSS",           Directive::SegmentBss  },
    { "CODE",          Directive::SegmentCode },
    { "DATA",          Directive::SegmentData },
    { "NOOPT",         Directive::OptNoop     },
    { "OPT",           Directive::OptNoop     },
    { "INCLUDE",       Directive::Include     },
    { "STRUCT",        Directive::Struct      },
    { "CMAP",          Directive::Cmap        },
    // as65 accepts the conditional keywords bare as well. The pre-token
    // parser only listed IFDEF/IFNDEF here and let IF/ELSE/ENDIF fall through
    // to be recognized later by mnemonic; now that recognition reads the
    // token, all five have to resolve from this table.
    { "IF",            Directive::If          },
    { "IFDEF",         Directive::Ifdef       },
    { "IFNDEF",        Directive::Ifndef      },
    { "ELSE",          Directive::Else        },
    { "ENDIF",         Directive::Endif       },
    { "LIST",          Directive::List        },
    { "NOLIST",        Directive::Nolist      },
    { "PAGE",          Directive::Page        },
    { "TITLE",         Directive::Title       },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DirectiveTable::FromSpelling
//
//  Linear scan over 63 rows. It is not the fast option and does not claim to
//  be -- measured against this same table, an unordered_map is ~46x quicker
//  per lookup and a std::string_view row (which skips the strlen this does per
//  row, per call) ~3.6x. It stays linear because assembling is not a hot path:
//  a 1,500-line source takes single-digit milliseconds end to end, of which
//  spelling lookup is a few percent.
//
//  An earlier version of this comment claimed the constant factor beat
//  hashing. It does not; nobody had measured it.
//
////////////////////////////////////////////////////////////////////////////////

Directive DirectiveTable::FromSpelling (const std::string & word)
{
    Directive  token = Directive::None;



    for (const Spelling & entry : s_kSpellings)
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
//  s_kAmbiguousSpellings
//
//  Spellings that cannot live in s_kSpellings because they also name an
//  instruction. as65 writes `rmb <count>` for reserved storage and
//  `rmb <bit>,<zp>` for the Rockwell instruction, so which one it is depends on
//  the operand -- something a flat name->token table cannot express. Listing
//  RMB above would silently turn every Rockwell RMB into a .DS.
//
//  A table of one, kept a table anyway: it is the place a second dialect
//  declares its own ambiguous forms, and it keeps the exception beside the rule
//  it is an exception to instead of scattered across the callers that need it.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr DirectiveTable::Spelling  s_kAmbiguousSpellings[] =
{
    { "RMB", Directive::Ds },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DirectiveTable::FromAmbiguousSpelling
//
////////////////////////////////////////////////////////////////////////////////

Directive DirectiveTable::FromAmbiguousSpelling (const std::string & word)
{
    Directive  token = Directive::None;



    for (const Spelling & entry : s_kAmbiguousSpellings)
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
//  DirectiveTable::FromStorageSpelling
//
////////////////////////////////////////////////////////////////////////////////

Directive DirectiveTable::FromStorageSpelling (const std::string & word)
{
    Directive  token = FromSpelling (word);



    if (token == Directive::None)
    {
        token = FromAmbiguousSpelling (word);
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectiveTable::GetAllSpellings
//
//  Exposes the vocabulary so DirectiveTokenTests can sweep all of it rather
//  than a hand-picked sample, which is what makes a newly added spelling
//  covered without editing a test. A second dialect gets the same sweep over
//  its own table -- the two are never compared to each other.
//
////////////////////////////////////////////////////////////////////////////////

std::span<const DirectiveTable::Spelling> DirectiveTable::GetAllSpellings()
{
    return std::span<const Spelling> (s_kSpellings, std::size (s_kSpellings));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DirectiveTable::GetCanonicalName
//
//  The first dotted spelling that maps to the token. Segment tokens list
//  their long form first, so that is what a listing shows.
//
////////////////////////////////////////////////////////////////////////////////

const char * DirectiveTable::GetCanonicalName (Directive directive)
{
    const char *  name = "";



    for (const Spelling & entry : s_kSpellings)
    {
        if (entry.token == directive && entry.name[0] == '.')
        {
            name = entry.name;
            break;
        }
    }

    return name;
}

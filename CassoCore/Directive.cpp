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
    { "IFDEF",         Directive::Ifdef       },
    { "IFNDEF",        Directive::Ifndef      },
    { "LIST",          Directive::List        },
    { "NOLIST",        Directive::Nolist      },
    { "PAGE",          Directive::Page        },
    { "TITLE",         Directive::Title       },
};




////////////////////////////////////////////////////////////////////////////////
//
//  DirectiveTable::FromSpelling
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
//  DirectiveTable::AllSpellings
//
////////////////////////////////////////////////////////////////////////////////

std::span<const DirectiveTable::Spelling> DirectiveTable::AllSpellings()
{
    return std::span<const Spelling> (s_kSpellings, std::size (s_kSpellings));
}




////////////////////////////////////////////////////////////////////////////////
//
//  DirectiveTable::CanonicalName
//
//  The first dotted spelling that maps to the token. Segment tokens list
//  their long form first, so that is what a listing shows.
//
////////////////////////////////////////////////////////////////////////////////

const char * DirectiveTable::CanonicalName (Directive directive)
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

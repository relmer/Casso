#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftTokenizer
//
//  An Applesoft BASIC listing written as host text, and the tokenized form a
//  guest holds in memory and a disk holds in a file.
//
//  EVERY RULE BELOW WAS MEASURED AGAINST APPLESOFT ITSELF rather than taken
//  from a reference, by typing lines into a booted machine and reading the
//  bytes back out of $0801. The rules are not the obvious ones:
//
//      Spaces outside a string, a REM or a DATA payload are DROPPED, and they
//      are dropped even in the middle of a keyword -- `PR INT` is stored as the
//      one PRINT token. So the stored form of `FOR I = 1 TO 9` carries no
//      spaces at all, and the spacing a listing shows is put there by LIST.
//
//      A REM's text and a DATA statement's payload are stored VERBATIM, spaces
//      included, from the character after the keyword. DATA ends at the first
//      colon outside a quoted string; REM runs to the end of the line and a
//      colon does not end it.
//
//      `?` is the PRINT token.
//
//      `AT` is special-cased against the two keywords it collides with. `ATN`
//      is one token; `A TO` is the letter A followed by TO, because the stored
//      form has no spaces and would otherwise read as `AT` then `O`. Both were
//      confirmed on the machine. `TOTAL` is NOT special-cased and tokenizes as
//      TO followed by TAL, which is Applesoft's own long-standing behavior and
//      is reproduced deliberately.
//
//  ROUND-TRIP LOSS, SETTLED DELIBERATELY AND MEASURED BOTH WAYS.
//
//      tokens -> host text -> tokens    EXACT, for every program Applesoft
//                                       itself can have saved. Verified
//                                       byte-for-byte against the stock DOS 3.3
//                                       master's own HELLO.
//
//                                       Detokenizing accepts two things
//                                       Applesoft's editor never stores -- a
//                                       literal space outside a string, REM or
//                                       DATA, and a literal `?` where the PRINT
//                                       token was meant -- and tokenizing then
//                                       normalizes them exactly as Applesoft
//                                       would if the line were typed in.
//
//      host text -> tokens -> host text NOT the identity, deliberately. Spacing
//                                       outside a string, REM or DATA is
//                                       dropped, `?` comes back as PRINT,
//                                       lowercase outside those three contexts
//                                       is upper-cased, and lines are ordered
//                                       by number. Every one of those is a
//                                       normalization Applesoft performs when
//                                       the line is typed, so the guest would
//                                       do it anyway.
//
//  The direction that matters is the first. Someone extracting a listing asked
//  for a conversion and can see what they got; someone whose saved program comes
//  back subtly reformatted did not ask for anything.
//
//  CONTROL CHARACTERS IN A STRING, A REM OR A DATA PAYLOAD. Applesoft stores
//  them, and real programs have them: a Ctrl-D ahead of a DOS command, and line
//  feeds around a REM's text so that LIST prints blank lines around it.
//
//      A line feed ($0A) is listed as a real line break, as LIST on the Apple
//      prints it. The host lines after it have no line number, and Tokenize
//      joins each one to the line before with a line feed. A blank host line
//      in that position is a line feed too, so a REM followed by a blank line
//      ends in a line feed. Elsewhere a blank line is skipped.
//
//      A line feed whose next host line would start with a digit, and so read
//      back as a new program line, is listed as ^J instead.
//
//      Every other control character is a caret and a letter: $04 is ^D. ^@
//      through ^_ read back as $00 through $1F, except ^@, which fails the
//      line because a zero byte ends a stored line. A literal caret is listed as
//      ^^, so Ctrl-^ ($1E) is listed as the raw byte. A caret followed by any
//      other character, a lowercase letter included, is a literal caret, as is
//      a raw control character in a listing.
//
//  ONE CONSEQUENCE WORTH KNOWING BEFORE AN EDITOR EATS IT: a DATA payload's
//  trailing space is significant, and it is the last character on its line. An
//  editor that strips trailing whitespace on save changes the program.
//
////////////////////////////////////////////////////////////////////////////////

//
//  Which line a refusal is about and what is wrong with it. The line's own text
//  is carried rather than an offset, because "line 250" means nothing to someone
//  looking at a file whose lines are numbered by tens.
//
struct ApplesoftListingError
{
    std::string  sourceLine;
    std::string  reason;
    uint32_t     lineNumber      = 0;
    size_t       sourceLineIndex = 0;   // 1-based, for a line carrying no number
    bool         hasLineNumber   = false;

    //  The lines read before the one that failed, for a caller that shows what
    //  a file cut off in its last line does hold.
    std::string  partialListing;

    //  The number of the last line read whole, which names where a file that
    //  ends between two lines was cut.
    uint32_t     lastLineNumber    = 0;
    bool         hasLastLineNumber = false;
};



class ApplesoftTokenizer
{
public:
    //  Host text listing to the bytes a guest loads at $0801, links included.
    static HRESULT  Tokenize   (const std::string        & hostListing,
                                std::vector<Byte>        & outBytes,
                                ApplesoftListingError    & outError);

    //  The reverse. Refuses anything it cannot render exactly, rather than
    //  guessing -- a listing that came back wrong is worse than one that did
    //  not come back.
    static HRESULT  Detokenize (const std::vector<Byte>  & programBytes,
                                std::string              & outHostListing,
                                ApplesoftListingError    & outError);

    //  What a token byte writes, or nullptr when the byte is not a token.
    static const char *  GetKeyword (Byte token);

    //  One paragraph for the help output, so the claim and the code cannot
    //  drift.
    //
    //  A FUNCTION RATHER THAN A CONSTANT because the sentence names a flag, and
    //  which prefix a flag wears is the reader's choice: someone who asked for
    //  help with `/?` is shown `/basic` throughout, and a paragraph that said
    //  `--basic` in the middle of it would be describing a form they did
    //  not ask for.


    static constexpr Word  kProgramBase = 0x0801;

    //  The first address a program may not reach. Applesoft's own ceiling on a
    //  48K machine, and what makes an over-large listing a refusal.
    static constexpr Word  kProgramCeiling = 0xC000;

    static constexpr Byte  kFirstToken = 0x80;
    static constexpr Byte  kLastToken  = 0xEA;

    static constexpr Byte  kTokenData  = 0x83;
    static constexpr Byte  kTokenRem   = 0xB2;
    static constexpr Byte  kTokenPrint = 0xBA;
    static constexpr Byte  kTokenTo    = 0xC1;
    static constexpr Byte  kTokenAt    = 0xC5;
    static constexpr Byte  kTokenAtn   = 0xE1;

    static constexpr uint32_t  kMaxLineNumber = 63999;

    //  A whole tokenized line, header and terminator included. Applesoft's own
    //  editor cannot produce a longer one, so a program carrying one could not
    //  be edited on the machine it was placed for.
    static constexpr size_t  kMaxTokenizedLine = 255;

private:
    //  What the character stream means where the cursor currently is. The two
    //  verbatim modes are why a tokenizer cannot be a table lookup: the same
    //  byte is a keyword in one and a letter in the next.
    enum class Mode
    {
        Normal,   // spaces dropped, keywords matched, letters upper-cased
        Rem,      // verbatim to the end of the line
        Data,     // verbatim to the first colon outside a quoted string
    };

    //  One source line, parsed and converted, before the links are known.
    //  `continues` indicates the line ends in a REM, a DATA payload or an open
    //  string, so a host line after it with no line number is part of it.
    struct ParsedLine
    {
        std::vector<Byte>  body;
        std::string        text;
        uint32_t           number    = 0;
        size_t             index     = 0;
        bool               continues = false;
    };

    static void     SplitLines      (const std::string        & hostListing,
                                     std::vector<std::string> & outLines);

    static HRESULT  ParseOneLine    (const std::string        & text,
                                     size_t                     index,
                                     ParsedLine               & outParsed,
                                     ApplesoftListingError    & outError);

    static HRESULT  TokenizeBody    (const std::string        & text,
                                     size_t                     from,
                                     std::vector<Byte>        & outBody,
                                     bool                     & outContinues,
                                     std::string              & outReason);

    static HRESULT  EmitProgram     (std::vector<ParsedLine>  & lines,
                                     std::vector<Byte>        & outBytes,
                                     ApplesoftListingError    & outError);

    static HRESULT  RenderOneLine   (const std::vector<Byte>  & body,
                                     uint32_t                   number,
                                     std::string              & outText,
                                     std::string              & outReason);

    //  Copies a quoted run, opening quote through closing quote or end of line,
    //  advancing the cursor past it.
    static bool  TryCopyQuoted      (const std::string  & text,
                                     size_t             & inOutAt,
                                     std::vector<Byte>  & outBody,
                                     bool               & outClosed,
                                     std::string        & outReason);

    //  Copies one character of a string, a REM or a DATA payload, or one ^
    //  escape, advancing the cursor past it.
    static bool  TryCopyVerbatim    (const std::string  & text,
                                     size_t             & inOutAt,
                                     std::vector<Byte>  & outBody,
                                     std::string        & outReason);

    //  Appends the listing form of one byte of a string, a REM or a DATA
    //  payload.
    static bool  TryRenderVerbatim  (const std::vector<Byte>  & body,
                                     size_t                     at,
                                     std::string              & ioText);

    //  Whether the bytes from `at` would read back as a line number.
    static bool  IsLineNumberAt     (const std::vector<Byte>  & body,
                                     size_t                     at);

    //  Matches one form from the cursor, skipping spaces the way Applesoft
    //  does -- which is what makes `PR INT` one token.
    static bool  TryMatchKeyword    (const std::string  & text,
                                     size_t               at,
                                     const char         * keyword,
                                     size_t             & outEnd);

    //  The whole table, in table order, which is the order that decides
    //  `TOTAL` and `ONERR`.
    static bool  TryMatchAnyKeyword (const std::string  & text,
                                     size_t               at,
                                     Byte               & outToken,
                                     size_t             & outEnd);

    static bool  TryParseLineNumber (const std::string  & text,
                                     size_t             & inOutAt,
                                     uint32_t           & outNumber);

    static bool    IsPrintable        (char c);
    static bool    IsStorableVerbatim (char c);
    static bool    IsLowerNumbered    (const ParsedLine & left, const ParsedLine & right);
    static char    ToUpper            (char c);
    static size_t  SkipSpaces         (const std::string & text, size_t at);

    //  What a stored line costs beyond its own body: the link to the next line,
    //  the line number, and the byte that ends it.
    static constexpr size_t  kLinkBytes       = 2;
    static constexpr size_t  kNumberBytes     = 2;
    static constexpr size_t  kTerminatorBytes = 1;

    //  A control character's listing form: ^D is $04, ^@ is $00 and ^_ is
    //  $1F. ^^ is a literal caret.
    static constexpr char  kCaret        = '^';
    static constexpr Byte  kControlBase  = 0x40;
    static constexpr Byte  kLastControl  = 0x1F;
    static constexpr Byte  kHighestAscii = 0x7E;
};

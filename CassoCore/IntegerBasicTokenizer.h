#pragma once

#include "Pch.h"

#include "ApplesoftTokenizer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IntegerBasicTokenizer
//
//  An Integer BASIC listing written as host text, converted to the stored
//  form a type I file contains.
//
//  THE GRAMMAR WAS MEASURED ON THE MACHINE: lines typed into Integer BASIC on
//  an emulated Apple ][ and the stored bytes read back from the program area.
//  Integer BASIC parses a line as it is entered and assigns a token by
//  syntactic position, so this is a parser and not a table lookup.
//
//      Spaces outside a string and a REM are dropped, inside a keyword, a
//      variable and a number too.
//
//      The alternatives of a rule are parsed in order and the first one that
//      parses is final. `IFF=1` is an assignment to IFF because the IF
//      statement fails at the missing THEN. `ENDQ=1` is an error because END
//      parses and the statement then does not end.
//
//      From its second character on, a variable ends ahead of AND, OR, MOD,
//      TO, STEP, THEN and AT. `FOR I=ATOB` is A TO B, and `SCORE=1` is an
//      error.
//
//      A constant is stored as its first typed digit and its value, and the
//      largest value is 32767. A negative number is the unary minus and a
//      constant.
//
//      The immediate commands (RUN, DEL, NEW, CLR, AUTO, MAN, HIMEM:, LOMEM:,
//      LOAD, SAVE, CON) are not statements in a stored line.
//
//  THE LISTING FORM IS IntegerBasicDetokenizer's. One space after REM is
//  dropped, because the listing has one there that the stored line does not.
//  Lines typed on the machine begin a remark with the space typed after REM,
//  and such a line is listed with two.
//
//      tokens -> host text -> tokens    Exact, except that the first-digit
//                                       byte of a constant typed with leading
//                                       zeros is rebuilt from the value, and
//                                       spaces ending a remark are trimmed by
//                                       the listing.
//
//      host text -> tokens -> host text Spacing is normalized, lowercase
//                                       outside a string and a REM is
//                                       upper-cased, and lines are ordered by
//                                       number.
//
////////////////////////////////////////////////////////////////////////////////

class IntegerBasicTokenizer
{
public:
    //  Host text listing to the program bytes of a type I file.
    static HRESULT  Tokenize           (const std::string      & hostListing,
                                        std::vector<Byte>      & outBytes,
                                        ApplesoftListingError  & outError);

    //  The statements of one line, with no line number, to the bytes stored
    //  between the line number and the $01 that ends the line.
    static bool     TokenizeStatements (std::string_view text, std::vector<Byte> & outBody);

    static constexpr uint32_t  kMaxLineNumber = 32767;
    static constexpr uint32_t  kMaxConstant   = 32767;

    //  A whole stored line, counted by its length byte.
    static constexpr size_t  kMaxTokenizedLine = 255;

private:
    struct Scan
    {
        std::string_view   text;
        size_t             at           = 0;
        size_t             furthest     = 0;
        size_t             budget       = 0;
        int                depth        = 0;
        bool               badCharacter = false;
        std::vector<Byte>  out;
    };

    struct Mark
    {
        size_t  at   = 0;
        size_t  size = 0;
    };

    struct ParsedLine
    {
        std::vector<Byte>  body;
        std::string        text;
        uint32_t           number = 0;
        size_t             index  = 0;
    };

    struct WordToken
    {
        const char  * word;
        Byte          token;
    };

    static constexpr int     kMaxDepth    = 64;
    static constexpr size_t  kParseBudget = 20000;

    static constexpr size_t  kLengthBytes     = 1;
    static constexpr size_t  kNumberBytes     = 2;
    static constexpr size_t  kTerminatorBytes = 1;

    static void     SplitLines         (const std::string & hostListing, std::vector<std::string> & outLines);
    static HRESULT  ParseOneLine       (const std::string & text, size_t index, ParsedLine & outParsed, ApplesoftListingError & outError);
    static HRESULT  EmitProgram        (std::vector<ParsedLine> & lines, std::vector<Byte> & outBytes, ApplesoftListingError & outError);
    static void     SetLineError       (const ParsedLine & line, const char * reason, ApplesoftListingError & outError);
    static bool     IsLowerNumbered    (const ParsedLine & left, const ParsedLine & right);
    static bool     ParseLineNumber    (std::string_view text, size_t & ioAt, uint32_t & outNumber);

    static Mark     Save               (const Scan & scan);
    static void     Restore            (Scan & scan, Mark mark);
    static char     Peek               (Scan & scan);
    static bool     Match              (Scan & scan, const char * word);
    static bool     MatchToken         (Scan & scan, const char * word, Byte token);
    static bool     IsLetter           (char c);
    static bool     IsDigit            (char c);
    static bool     IsAtNameStop       (Scan & scan);

    static bool     ParseStatements    (Scan & scan);
    static bool     ParseStatement     (Scan & scan);
    static bool     ParseKeyword       (Scan & scan);
    static bool     ParseRemark        (Scan & scan);
    static bool     ParseOneOperand    (Scan & scan);
    static bool     ParseTwoOperands   (Scan & scan);
    static bool     ParseLine          (Scan & scan, const char * word, Byte first, Byte second, Byte third);
    static bool     ParseFor           (Scan & scan);
    static bool     ParseNext          (Scan & scan);
    static bool     ParseIf            (Scan & scan);
    static bool     ParseDim           (Scan & scan);
    static bool     ParseDimItem       (Scan & scan, Byte stringToken, Byte numericToken);
    static bool     ParseInput         (Scan & scan);
    static bool     ParsePrint         (Scan & scan);
    static bool     ParseList          (Scan & scan);
    static bool     ParseDisplay       (Scan & scan, const char * word, Byte stringToken, Byte numericToken);
    static bool     ParseAssignment    (Scan & scan);

    static bool     ParseExpression    (Scan & scan);
    static bool     ParseOperand       (Scan & scan);
    static bool     ParseUnary         (Scan & scan);
    static bool     ParsePrimary       (Scan & scan);
    static bool     ParseComparison    (Scan & scan);
    static bool     ParseStringCall    (Scan & scan, const char * word, Byte token);
    static bool     ParseScreenCall    (Scan & scan);
    static bool     ParseNumberCall    (Scan & scan);
    static bool     ParseGroup         (Scan & scan, Byte open);
    static bool     ParseVariable      (Scan & scan);
    static bool     ParseStringTarget  (Scan & scan);
    static bool     ParseStringOperand (Scan & scan);
    static bool     ParseString        (Scan & scan);
    static bool     ParseName          (Scan & scan);
    static bool     ParseNumber        (Scan & scan);
};

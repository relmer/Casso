#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  CLexer
//
//  The runs of a line of C to color: keywords, types, numbers, strings and
//  characters, comments and preprocessor lines. Identifiers and operators
//  are left uncolored. A block comment can span lines, so the
//  caller passes whether the line before ended inside one and gets back
//  whether this one does.
//
//  It reads C as cc65 and C99 write it; it does not preprocess or parse.
//
////////////////////////////////////////////////////////////////////////////////

class CLexer
{
public:
    enum class Token { Keyword, Type, Number, String, Comment, Preprocessor };

    struct Run
    {
        int    start  = 0;
        int    length = 0;
        Token  token  = Token::Keyword;

        bool operator== (const Run &) const = default;
    };

    //  The runs of one line, in order and not overlapping. ioIsInComment is
    //  whether a block comment is open at the line's start, and at its end.
    static std::vector<Run>  GetRuns (const std::wstring & line, bool & ioIsInComment);

    //  Whether a file name is a C source or header, by its extension.
    static bool  IsCFile (const std::wstring & name);

    static bool  IsKeyword (const std::wstring & word);
    static bool  IsType    (const std::wstring & word);

private:
    static size_t  ScanQuoted  (const std::wstring & line, size_t at);
    static size_t  ScanNumber  (const std::wstring & line, size_t at);
    static bool    IsWordStart (wchar_t ch);
    static bool    IsWordChar  (wchar_t ch);
};

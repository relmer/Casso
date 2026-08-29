#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  UsageText
//
//  Wrapping for help text, so usage is written as one logical line per item and
//  folded to whatever width the reader's terminal has.
//
//  Authoring the breaks by hand instead means choosing a width for everyone --
//  which is 80 if you want the narrowest reader to be able to read it, and then
//  a wide terminal shows a column of text down its left third.
//
//  THE CONTINUATION INDENT IS THE POINT. A flag row is two columns, and text
//  wrapped back to the left margin reads as a new row rather than as the rest of
//  the one above it:
//
//      -w[<width>]          Wrap listing at <width> columns, 60 to 200
//                           (default: 79, -w alone = 133)
//
//  The column is found rather than passed in, from the last run of two or more
//  spaces on the line: that gap IS the gutter between a flag and its
//  description, and a line with no such gap is prose, which continues at its own
//  indent instead.
//
////////////////////////////////////////////////////////////////////////////////

class UsageText
{
public:
    //  One logical line, folded to `width` columns. A line that fits comes back
    //  as itself, so a caller can print the result unconditionally.
    //
    //  Breaks fall at spaces. A single word longer than the space available is
    //  left over-long rather than cut: a path or a URL broken across two lines
    //  is worse than one that overhangs, because the reader cannot tell whether
    //  the break is part of it.
    static std::vector<std::string>  Wrap (const std::string & line, size_t width);

    //  Where a continuation of this line should start: after its gutter if it
    //  has one, otherwise at its own indent.
    static size_t                    ContinuationIndent (const std::string & line);

    //  The narrowest terminal worth folding to. Below this the gutter alone
    //  eats the line, so the reported width is ignored and the fallback stands.
    static constexpr size_t          kNarrowestTerminal = 40;

    //  What a page has no terminal to ask. A redirected stream has no width,
    //  and guessing a wide one puts long lines into a file somebody will open
    //  in an editor at 80.
    static constexpr size_t          kNoTerminal        = 80;

    //
    //  How wide to fold, from what the environment says and what the console
    //  reports.
    //
    //  SPLIT OUT OF THE PLATFORM CALL SO IT CAN BE TESTED. The decision used to
    //  live entirely inside the executable, next to GetConsoleScreenBufferInfo,
    //  where the test assembly cannot reach it -- so "does the help use the
    //  terminal's width" was a question only a person at a terminal could
    //  answer, and answering it needed a build, a window and an eye.
    //
    //  COLUMNS WINS WHEN IT IS SET, which is the convention every other tool
    //  follows and is what makes the whole path checkable from a script: set it,
    //  run the tool, measure the longest line. It also gives a reader whose
    //  terminal reports the wrong size a way to say so.
    //
    static size_t                    WidthFrom (const char * columnsEnv,
                                                bool         hasConsole,
                                                int          consoleColumns);
};

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
};

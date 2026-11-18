#include "Pch.h"
#include "Render/DxuiSvgPath.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSvgPath::IsCommand
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSvgPath::IsCommand (wchar_t c)
{
    return wcschr (L"MmLlHhVvCcSsQqTtAaZz", c) != nullptr && c != L'\0';
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSvgPath::SkipSeparators
//
//  Whitespace and commas may sit between any two numbers, and neither is
//  required: "1-2" is two numbers.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSvgPath::SkipSeparators (const wchar_t *& p)
{
    while (*p == L' ' || *p == L',' || *p == L'\t' || *p == L'\r' || *p == L'\n')
    {
        p++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSvgPath::ReadNumber
//
//  A number ends where it can no longer continue, so "0.5.5" is 0.5 then .5
//  and "1e-3" is one number, as the SVG grammar reads them.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSvgPath::ReadNumber (const wchar_t *& p, float & outValue)
{
    const wchar_t  * start    = nullptr;
    bool             digits   = false;
    bool             dot      = false;
    wchar_t        * end      = nullptr;
    std::wstring     text;



    SkipSeparators (p);
    start = p;

    if (*p == L'+' || *p == L'-')
    {
        p++;
    }

    while ((*p >= L'0' && *p <= L'9') || (*p == L'.' && !dot))
    {
        dot    = dot || *p == L'.';
        digits = digits || *p != L'.';
        p++;
    }

    if (!digits)
    {
        p = start;
        return false;
    }

    if (*p == L'e' || *p == L'E')
    {
        const wchar_t  * exponent = p + 1;

        if (*exponent == L'+' || *exponent == L'-')
        {
            exponent++;
        }

        if (*exponent >= L'0' && *exponent <= L'9')
        {
            p = exponent;

            while (*p >= L'0' && *p <= L'9')
            {
                p++;
            }
        }
    }

    text.assign (start, p);
    outValue = wcstof (text.c_str(), &end);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSvgPath::ReadFlag
//
//  An arc's two flags are single digits that need no separator: "a1 1 0 01"
//  holds large-arc 0 and sweep 1.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSvgPath::ReadFlag (const wchar_t *& p, bool & outValue)
{
    SkipSeparators (p);

    if (*p != L'0' && *p != L'1')
    {
        return false;
    }

    outValue = *p == L'1';
    p++;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSvgPath::Parse
//
//  One pass, keeping the current point, the start of the sub-shape for a
//  close to return to, and the last control point for S and T to reflect.
//  A command letter may be followed by several coordinate sets; each set
//  repeats the command, except that the sets after a moveto are lines.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSvgPath::Parse (const wchar_t * d, std::vector<DxuiSvgSubpath> & outSubpaths)
{
    const wchar_t   * p         = d;
    wchar_t           command   = L'\0';
    DxuiSvgPoint      current;
    DxuiSvgPoint      lastCubic;
    DxuiSvgPoint      lastQuad;
    wchar_t           previous  = L'\0';
    DxuiSvgSubpath  * open      = nullptr;



    outSubpaths.clear();

    if (d == nullptr)
    {
        return false;
    }

    for (;;)
    {
        bool             relative = false;
        wchar_t          upper    = L'\0';
        DxuiSvgSegment   segment;
        float            a[7]     = {};

        SkipSeparators (p);

        if (*p == L'\0')
        {
            return true;
        }

        if (IsCommand (*p))
        {
            command = *p;
            p++;
        }
        else if (command == L'\0')
        {
            return false;
        }

        relative = iswlower (command) != 0;
        upper    = (wchar_t) towupper (command);

        if (upper == L'Z')
        {
            if (open != nullptr)
            {
                open->closed = true;
                current      = open->start;
            }

            open     = nullptr;
            previous = L'Z';
            command  = L'\0';
            continue;
        }

        //  Anything after a close with no moveto starts where the close ended.
        if (open == nullptr && upper != L'M')
        {
            outSubpaths.push_back (DxuiSvgSubpath {});
            open        = &outSubpaths.back();
            open->start = current;
        }

        switch (upper)
        {
            case L'M':
                if (!ReadNumber (p, a[0]) || !ReadNumber (p, a[1]))
                {
                    return false;
                }

                current = relative ? DxuiSvgPoint { current.x + a[0], current.y + a[1] } : DxuiSvgPoint { a[0], a[1] };

                outSubpaths.push_back (DxuiSvgSubpath {});
                open        = &outSubpaths.back();
                open->start = current;

                //  The coordinate sets after a moveto are lines.
                command = relative ? L'l' : L'L';
                break;

            case L'L':
                if (!ReadNumber (p, a[0]) || !ReadNumber (p, a[1]))
                {
                    return false;
                }

                segment.kind = DxuiSvgSegment::Kind::Line;
                segment.to   = relative ? DxuiSvgPoint { current.x + a[0], current.y + a[1] } : DxuiSvgPoint { a[0], a[1] };
                break;

            case L'H':
                if (!ReadNumber (p, a[0]))
                {
                    return false;
                }

                segment.kind = DxuiSvgSegment::Kind::Line;
                segment.to   = { relative ? current.x + a[0] : a[0], current.y };
                break;

            case L'V':
                if (!ReadNumber (p, a[0]))
                {
                    return false;
                }

                segment.kind = DxuiSvgSegment::Kind::Line;
                segment.to   = { current.x, relative ? current.y + a[0] : a[0] };
                break;

            case L'C':
            case L'S':
            {
                int  first = 0;

                if (upper == L'C')
                {
                    if (!ReadNumber (p, a[0]) || !ReadNumber (p, a[1]))
                    {
                        return false;
                    }

                    segment.c1 = relative ? DxuiSvgPoint { current.x + a[0], current.y + a[1] } : DxuiSvgPoint { a[0], a[1] };
                    first      = 2;
                }
                else
                {
                    //  The first control point mirrors the last cubic's second,
                    //  or is the current point when the last was not a cubic.
                    bool  follows = previous == L'C' || previous == L'S';

                    segment.c1 = follows ? DxuiSvgPoint { 2.0f * current.x - lastCubic.x, 2.0f * current.y - lastCubic.y } : current;
                }

                if (!ReadNumber (p, a[first]) || !ReadNumber (p, a[first + 1]) ||
                    !ReadNumber (p, a[first + 2]) || !ReadNumber (p, a[first + 3]))
                {
                    return false;
                }

                segment.kind = DxuiSvgSegment::Kind::Cubic;
                segment.c2   = relative ? DxuiSvgPoint { current.x + a[first],     current.y + a[first + 1] } : DxuiSvgPoint { a[first],     a[first + 1] };
                segment.to   = relative ? DxuiSvgPoint { current.x + a[first + 2], current.y + a[first + 3] } : DxuiSvgPoint { a[first + 2], a[first + 3] };
                lastCubic    = segment.c2;
                break;
            }

            case L'Q':
            case L'T':
            {
                DxuiSvgPoint  control;

                if (upper == L'Q')
                {
                    if (!ReadNumber (p, a[0]) || !ReadNumber (p, a[1]))
                    {
                        return false;
                    }

                    control = relative ? DxuiSvgPoint { current.x + a[0], current.y + a[1] } : DxuiSvgPoint { a[0], a[1] };
                }
                else
                {
                    bool  follows = previous == L'Q' || previous == L'T';

                    control = follows ? DxuiSvgPoint { 2.0f * current.x - lastQuad.x, 2.0f * current.y - lastQuad.y } : current;
                }

                if (!ReadNumber (p, a[2]) || !ReadNumber (p, a[3]))
                {
                    return false;
                }

                //  A quadratic is the cubic whose controls sit two thirds of
                //  the way from each end toward its one control point.
                segment.kind = DxuiSvgSegment::Kind::Cubic;
                segment.to   = relative ? DxuiSvgPoint { current.x + a[2], current.y + a[3] } : DxuiSvgPoint { a[2], a[3] };
                segment.c1   = { current.x + 2.0f / 3.0f * (control.x - current.x), current.y + 2.0f / 3.0f * (control.y - current.y) };
                segment.c2   = { segment.to.x + 2.0f / 3.0f * (control.x - segment.to.x), segment.to.y + 2.0f / 3.0f * (control.y - segment.to.y) };
                lastQuad     = control;
                break;
            }

            case L'A':
                if (!ReadNumber (p, a[0]) || !ReadNumber (p, a[1]) || !ReadNumber (p, a[2]) ||
                    !ReadFlag (p, segment.largeArc) || !ReadFlag (p, segment.clockwise) ||
                    !ReadNumber (p, a[3]) || !ReadNumber (p, a[4]))
                {
                    return false;
                }

                segment.kind        = DxuiSvgSegment::Kind::Arc;
                segment.radiusX     = fabsf (a[0]);
                segment.radiusY     = fabsf (a[1]);
                segment.rotationDeg = a[2];
                segment.to          = relative ? DxuiSvgPoint { current.x + a[3], current.y + a[4] } : DxuiSvgPoint { a[3], a[4] };
                break;

            default:
                return false;
        }

        if (upper != L'M')
        {
            open->segments.push_back (segment);
            current = segment.to;
        }

        previous = upper;
    }
}

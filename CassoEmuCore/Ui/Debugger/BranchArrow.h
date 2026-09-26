#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  BranchArrow
//
//  The line a disassembly draws from the PC's branch, jump or call to where
//  it goes. It leaves the instruction's mnemonic to the left, turns through a
//  rounded corner toward the target, runs along the left of the mnemonics,
//  and turns again into an arrowhead that points at the target's mnemonic.
//  A target out of view runs the line to that edge of the rows and stops.
//
//  Pure geometry in pixels, so the window only paints what this gives it.
//
////////////////////////////////////////////////////////////////////////////////

class BranchArrow
{
public:
    struct Segment
    {
        float  x0 = 0.0f;
        float  y0 = 0.0f;
        float  x1 = 0.0f;
        float  y1 = 0.0f;
    };

    struct Input
    {
        float                 mnemonicX     = 0.0f;   // left of the mnemonics
        float                 sourceY       = 0.0f;   // middle of the PC's row
        std::optional<float>  targetY;   // middle of the target's row, if in view
        bool                  isTargetBelow = true;
        float                 edgeY         = 0.0f;   // top or bottom of the rows, toward the target
        float                 marginPx      = 3.0f;   // the gap left before a mnemonic
        float                 stubPx        = 12.0f;   // how far the line stands off to the left
        float                 radiusPx      = 4.0f;
        float                 headPx        = 5.0f;
    };

    struct Result
    {
        std::vector<Segment>  segments;
        bool                  hasHead = false;
        float                 head[6] = {};       // tip, then the two back corners
    };

    static Result  Build (const Input & input);

private:
    static constexpr int  kCornerSteps = 6;

    static void  AddCorner (Result & result, float cx, float cy, float radius, float fromDeg, float toDeg);
};

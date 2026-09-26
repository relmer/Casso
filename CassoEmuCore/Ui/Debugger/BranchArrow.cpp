#include "Pch.h"

#include "Ui/Debugger/BranchArrow.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BranchArrow::Build
//
//  Screen y grows downward, so a corner's angles are measured clockwise from
//  the right: 90 is straight down, 270 straight up. A target on the PC's own
//  row, a branch to itself, has no line.
//
////////////////////////////////////////////////////////////////////////////////

BranchArrow::Result BranchArrow::Build (const Input & input)
{
    Result  result;
    float   dir    = input.isTargetBelow ? 1.0f : -1.0f;
    float   end    = input.mnemonicX - input.marginPx;
    float   x      = end - input.stubPx;
    float   r      = input.radiusPx;
    float   stopY  = input.targetY.value_or (input.edgeY);



    if (input.targetY.has_value() && std::fabs (*input.targetY - input.sourceY) < 2.0f * r)
    {
        return result;
    }

    //  Off the PC's mnemonic, then round the corner toward the target.
    result.segments.push_back ({ end, input.sourceY, x + r, input.sourceY });
    AddCorner (result, x + r, input.sourceY + dir * r, r, input.isTargetBelow ? 270.0f : 90.0f, 180.0f);

    if (!input.targetY.has_value())
    {
        result.segments.push_back ({ x, input.sourceY + dir * r, x, stopY });
        return result;
    }

    //  Down (or up) the left of the mnemonics, round into the target's row,
    //  and on to the arrowhead.
    result.segments.push_back ({ x, input.sourceY + dir * r, x, stopY - dir * r });
    AddCorner (result, x + r, stopY - dir * r, r, 180.0f, input.isTargetBelow ? 90.0f : 270.0f);
    result.segments.push_back ({ x + r, stopY, end - input.headPx, stopY });

    result.hasHead = true;
    result.head[0] = end;
    result.head[1] = stopY;
    result.head[2] = end - input.headPx;
    result.head[3] = stopY - input.headPx * 0.8f;
    result.head[4] = end - input.headPx;
    result.head[5] = stopY + input.headPx * 0.8f;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BranchArrow::AddCorner
//
//  A quarter circle as short straight pieces, from one angle to the other.
//
////////////////////////////////////////////////////////////////////////////////

void BranchArrow::AddCorner (Result & result, float cx, float cy, float radius, float fromDeg, float toDeg)
{
    constexpr float  kDegToRad = 3.14159265f / 180.0f;
    float            px        = cx + radius * std::cos (fromDeg * kDegToRad);
    float            py        = cy + radius * std::sin (fromDeg * kDegToRad);



    for (int step = 1; step <= kCornerSteps; step++)
    {
        float  deg = fromDeg + (toDeg - fromDeg) * (float) step / (float) kCornerSteps;
        float  nx  = cx + radius * std::cos (deg * kDegToRad);
        float  ny  = cy + radius * std::sin (deg * kDegToRad);

        result.segments.push_back ({ px, py, nx, ny });
        px = nx;
        py = ny;
    }
}

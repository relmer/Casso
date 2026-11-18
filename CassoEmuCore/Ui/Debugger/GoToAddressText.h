#pragma once

struct DebuggerViewSnapshot;





////////////////////////////////////////////////////////////////////////////////
//
//  GoToAddressText
//
//  The address typed in a code view's Go to box.
//
////////////////////////////////////////////////////////////////////////////////

class GoToAddressText
{
public:
    static bool  TryParseGoTo (const std::wstring & text, const DebuggerViewSnapshot * snapshot, Word & outAddress);
};

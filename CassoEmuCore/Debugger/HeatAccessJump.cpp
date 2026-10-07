#include "Pch.h"

#include "Debugger/HeatAccessJump.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatAccessJump::Plan
//
//  With no access recorded there is nothing to show or go to, and one older
//  than history cannot be found. A seek needs history, and history that
//  still holds the position: the access is gone from it once it is older
//  than the oldest keyframe.
//
////////////////////////////////////////////////////////////////////////////////

HeatAccessPlan HeatAccessJump::Plan (
    const HeatAccessRequest  & request,
    HeatAccessState            state,
    const HeatLastAccess     & access,
    bool                       isRecording,
    uint64_t                   oldestPosition)
{
    HeatAccessPlan  plan;
    std::wstring    where    = HeatMapOptions::DescribeLocation (request.bank, request.address, true);
    std::string     location;
    const char    * kind     = request.isWrite ? "write" : "read";



    for (wchar_t ch : where)
    {
        location += (char) ch;
    }

    if (state == HeatAccessState::None)
    {
        plan.message = std::format ("No {} of {} since the heat map started counting.", kind, location);
        return plan;
    }

    if (state == HeatAccessState::Unknown)
    {
        plan.message = std::format ("The last {} of {} is older than the history kept.", kind, location);
        return plan;
    }

    if (!request.isRewind)
    {
        plan.kind = HeatAccessPlan::Kind::ShowCode;
        plan.pc   = access.GetPc();
        return plan;
    }

    if (!isRecording)
    {
        plan.message = std::format ("The last {} of {} was at cycle {}, and history is not being recorded.", kind, location, GroupDigits (access.cycle));
        return plan;
    }

    if (access.GetPosition() < oldestPosition)
    {
        plan.message = std::format ("The last {} of {}, at cycle {}, is older than the history kept.", kind, location, GroupDigits (access.cycle));
        return plan;
    }

    plan.kind     = HeatAccessPlan::Kind::Seek;
    plan.pc       = access.GetPc();
    plan.position = access.GetPosition() + 1;
    return plan;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatAccessJump::FormatWords
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatAccessJump::FormatWords (const HeatAccessRequest & request)
{
    return std::format ("{} {} {} {:04X}",
                        request.isRewind ? "rewind" : "code",
                        request.isWrite  ? "write"  : "read",
                        HeatMapOptions::GetBankWord (request.bank),
                        request.address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatAccessJump::TryParseWords
//
//  Four words: what to do, which access, the bank and a hex address of one
//  to four digits.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatAccessJump::TryParseWords (
    const std::string    & words,
    HeatAccessRequest    & outRequest)
{
    constexpr size_t    kMaxDigits = 4;
    std::istringstream  stream (words);
    std::string         verb;
    std::string         kind;
    std::string         bank;
    std::string         digits;
    std::string         extra;
    HeatAccessRequest   request;



    stream >> verb >> kind >> bank >> digits;

    if (!stream || (stream >> extra))
    {
        return false;
    }

    if ((verb != "code" && verb != "rewind") || (kind != "write" && kind != "read") || !HeatMapOptions::TryGetBank (bank, request.bank))
    {
        return false;
    }

    if (digits.empty() || digits.size() > kMaxDigits || digits.find_first_not_of ("0123456789abcdefABCDEF") != std::string::npos)
    {
        return false;
    }

    request.isRewind = verb == "rewind";
    request.isWrite  = kind == "write";
    request.address  = (Word) std::stoul (digits, nullptr, 16);

    outRequest = request;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatAccessJump::GroupDigits
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatAccessJump::GroupDigits (uint64_t value)
{
    constexpr size_t  kGroup  = 3;
    std::string       digits  = std::to_string (value);
    std::string       grouped;



    for (size_t i = 0; i < digits.size(); i++)
    {
        if (i > 0 && (digits.size() - i) % kGroup == 0)
        {
            grouped += ',';
        }

        grouped += digits[i];
    }

    return grouped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatAccessJump::Describe
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatAccessJump::Describe (
    bool                    isWrite,
    const HeatAccessInfo  & info)
{
    std::string  text;



    if (info.isTooOld)
    {
        return isWrite ? L"Last written before the history kept" : L"Last read before the history kept";
    }

    if (!info.has)
    {
        return isWrite ? L"Not written since counting started" : L"Not read since counting started";
    }

    text = std::format ("Last {} by ${:04X} ", isWrite ? "written" : "read", info.pc);

    if (!info.label.empty())
    {
        text += info.label + ": ";
    }

    text += info.instruction.empty() ? std::string ("???") : info.instruction;
    text += " at cycle " + GroupDigits (info.cycle);

    return std::wstring (text.begin(), text.end());
}





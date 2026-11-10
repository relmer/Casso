#include "Pch.h"

#include "BasicRenumber.h"
#include "ApplesoftTokenizer.h"
#include "IntegerBasicDetokenizer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicRenumber::RewriteLine
//
//  References are rewritten from the last to the first, so that an Applesoft
//  number of another length does not move the ones ahead of it.
//
////////////////////////////////////////////////////////////////////////////////

void BasicRenumber::RewriteLine (BasicProgram::Dialect                   dialect,
                                 const std::map<uint32_t, uint32_t>    & numbers,
                                 std::vector<Byte>                     & ioBody)
{
    std::vector<BasicProgram::LineReference>  references;
    std::string                               digits;
    size_t                                    index = 0;



    BasicProgram::FindLineReferences (dialect, ioBody, references);

    for (index = references.size(); index > 0; index--)
    {
        const BasicProgram::LineReference       & reference = references[index - 1];
        std::map<uint32_t, uint32_t>::const_iterator  found = numbers.find (reference.target);

        if (found == numbers.end())
        {
            continue;
        }

        digits = std::to_string (found->second);

        if (dialect == BasicProgram::Dialect::Integer)
        {
            ioBody[reference.offset]     = (Byte) (digits[0] | IntegerBasicDetokenizer::kHighBit);
            ioBody[reference.offset + 1] = (Byte) (found->second & 0xFF);
            ioBody[reference.offset + 2] = (Byte) (found->second >> 8);
        }
        else
        {
            ioBody.erase  (ioBody.begin() + reference.offset, ioBody.begin() + reference.offset + reference.length);
            ioBody.insert (ioBody.begin() + reference.offset, digits.begin(), digits.end());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicRenumber::Renumber
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BasicRenumber::Renumber (const std::vector<Byte>  & programBytes,
                                 uint32_t                   start,
                                 uint32_t                   step,
                                 std::vector<Byte>        & outBytes,
                                 std::string              & outReason)
{
    HRESULT                       hr       = S_OK;
    BasicProgram::Program         program;
    std::map<uint32_t, uint32_t>  numbers;
    uint64_t                      number   = start;
    uint32_t                      highest  = 0;
    uint32_t                      longLine = 0;
    bool                          fits     = true;



    outBytes.clear();
    outReason.clear();

    hr = BasicProgram::Parse (programBytes, program, outReason);
    CHR (hr);

    if (step == 0)
    {
        outReason = "the step is zero";
    }

    CBREx (step != 0, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    highest = (program.dialect == BasicProgram::Dialect::Integer) ? IntegerBasicDetokenizer::kMaxLineNumber : ApplesoftTokenizer::kMaxLineNumber;

    for (const BasicProgram::Line & line : program.lines)
    {
        fits = number <= highest;

        if (!fits)
        {
            outReason = "line " + std::to_string (line.number) + " would be numbered past " + std::to_string (highest);
            break;
        }

        numbers.emplace (line.number, (uint32_t) number);
        number += step;
    }

    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    number = start;

    for (BasicProgram::Line & line : program.lines)
    {
        RewriteLine (program.dialect, numbers, line.body);

        line.number = (uint32_t) number;
        number     += step;
    }

    hr = BasicProgram::Emit (program, outBytes, longLine);

    if (FAILED (hr))
    {
        outReason = "line " + std::to_string (longLine) + " is too long once renumbered";
    }

    CHR (hr);

Error:
    return hr;
}

#include "Pch.h"

#include "DiskCommandResult.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandResult::Fail
//
//  Image, file, reason, in that order. A script's user sees the first line and
//  needs to know which disk before anything else; the reason is useless without
//  it when a build places twenty files.
//
////////////////////////////////////////////////////////////////////////////////

void DiskCommandResult::Fail (const std::string  & imagePath,
                              const std::string  & name,
                              const std::string  & sentence)
{
    diagnostics += FormatFailure (imagePath, name, sentence) + "\n";
    exitStatus   = kNoOutput;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandResult::FormatFailure
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskCommandResult::FormatFailure (
    const std::string & imagePath,
    const std::string & fileName,
    const std::string & reason)
{
    std::string  text = imagePath.empty() ? std::string ("(no image)") : imagePath;



    if (!fileName.empty())
    {
        text += ": " + fileName;
    }

    text += ": " + reason;

    return text;
}

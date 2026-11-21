#pragma once

#include "Pch.h"

class IDiskFileIo;
class DiskImageSession;
struct DiskCommandResult;





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskCommandContext
//
//  What the disk tool's command classes access in DiskCommandRunner: the
//  host's files, the open image, the flag prefix the command was typed
//  with, the confirmation prompt and the clock.
//
////////////////////////////////////////////////////////////////////////////////

class IDiskCommandContext
{
public:
    virtual ~IDiskCommandContext() = default;

    virtual IDiskFileIo       & GetFileIo              ()                                                          = 0;
    virtual DiskImageSession  & GetSession             ()                                                          = 0;
    virtual std::string         ApplyPrefix            (const std::string & text) const                            = 0;
    virtual void                ReportMissingParameter (const std::string & parameter, DiskCommandResult & result) const = 0;
    virtual bool                CanConfirm             () const                                                    = 0;
    virtual bool                Confirm                (const std::string & question)                              = 0;
    virtual SYSTEMTIME          GetToday               ()                                                          = 0;
};

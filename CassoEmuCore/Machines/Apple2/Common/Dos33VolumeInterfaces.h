#pragma once

#include "Pch.h"

class FilePath;
struct DeleteOutcome;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos33VolumeInterfaces
//
//  The calls DOS 3.3 volume components make to each other, one interface
//  per group of callers.
//
////////////////////////////////////////////////////////////////////////////////

//  Removes a file into a new buffer. The writer uses it to stage the removal
//  of a file it replaces.
class IDos33FileRemoval
{
public:
    virtual ~IDos33FileRemoval() = default;

    virtual HRESULT  Delete (const FilePath  & path,
                             vector<Byte>    & outBuffer,
                             DeleteOutcome   & outOutcome) const = 0;
};

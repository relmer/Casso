#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFileCommit
//
//  Whole-file reads and atomic whole-file replaces for disk images. A
//  replace writes a sibling temporary, verifies it, and renames it over the
//  target. After a failure at any step the original file is as it was.
//
////////////////////////////////////////////////////////////////////////////////

class DiskFileCommit
{
public:
    static HRESULT  WriteFileAtomically    (const string & path, const vector<Byte> & bytes);
    static HRESULT  ReadFileBytes          (const string & path, vector<Byte> & bytes);

    //  Public so a test can check that a commit left no temporary behind.
    static string   GetCommitTemporaryPath (const string & path, unsigned attempt);
};

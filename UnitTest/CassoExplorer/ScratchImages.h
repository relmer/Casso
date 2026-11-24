#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ScratchImages
//
//  The scratch DOS 3.3 and ProDOS images of Fixtures/CassoExplorer, built in
//  memory with the commands MakeFixtures.ps1 runs. With the pictures, the
//  DOS 3.3 image is the fixture's bytes. The ProDOS image's subdirectory is
//  made by Mkdir, and its entries have no date stamps. Each image is built
//  once.
//
////////////////////////////////////////////////////////////////////////////////

class ScratchImages
{
public:
    static const vector<Byte> &  GetDos33  (bool hasPictures);
    static const vector<Byte> &  GetProDos (bool hasPictures);

private:
    static vector<Byte>  Build       (bool isProDos, bool hasPictures);
    static vector<Byte>  MakePattern (size_t length, int64_t seed);
};

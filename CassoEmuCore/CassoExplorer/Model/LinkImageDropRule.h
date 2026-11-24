#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/TreeModel.h"
#include "Devices/Disk/ImageClassifier.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkImageDrop
//
//  What one disk image dropped on a linked Apple's drive does: copy it as a
//  file, write it to the whole disk, offer both on a menu, or nothing.
//
////////////////////////////////////////////////////////////////////////////////

enum class LinkImageDrop
{
    Files,
    Write,
    Choose,
    None,
};





////////////////////////////////////////////////////////////////////////////////
//
//  LinkImageDropRule
//
//  The drop by the image's size against the drive's. An image the size of
//  the disk is written. A smaller one goes on a blank disk as a write, and
//  onto a formatted volume either way, so a menu offers both. One bigger than
//  the disk does nothing.
//
////////////////////////////////////////////////////////////////////////////////

class LinkImageDropRule
{
public:
    static LinkImageDrop  GetDrop (const ImageClass & image, const LinkTreeEntry::Drive & drive);
};

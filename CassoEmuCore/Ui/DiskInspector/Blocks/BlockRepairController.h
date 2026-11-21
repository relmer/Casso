#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockInspectorInterfaces.h"

class BlockInspectorModel;
class IDriveSpace;
struct RepairPlan;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairController
//
//  Repair of the current volume: the confirmation with every change the
//  volume can have, where the repaired copy goes, and writing it on the
//  worker's thread. The image isn't written.
//
////////////////////////////////////////////////////////////////////////////////

class BlockRepairController : public IBlockRepair
{
public:
    //  The parts of block mode that repair calls.
    struct Parts
    {
        BlockInspectorModel  & model;
        IBlockJobs           & jobs;
        IBlockView           & view;
        IBlockNavigation     & navigation;
        IBlockDialogs        & dialogs;
        IBlockFileInfo       & file;
        IBlockEditNote       & note;
        const IDriveSpace    & drives;
    };

    explicit BlockRepairController (const Parts & parts);

    bool  CanRepair () const override { return GetVolume() >= 0; }
    void  Repair    () override;

private:
    int   GetVolume  () const;
    bool  PickTarget (const std::wstring & why, uint64_t bytes, std::filesystem::path & ioTarget);
    void  QueueCopy  (int volume, const RepairPlan & plan, bool isCard, const std::filesystem::path & target);

    Parts  m_parts;
};

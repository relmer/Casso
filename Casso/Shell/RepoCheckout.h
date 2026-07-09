#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  RepoCheckout
//
//  Pure, filesystem-free helpers for reasoning about which *checkout* of the
//  Casso repo a path belongs to. One machine can hold several checkouts of
//  the same repo at once: the main working tree at <repoRoot>, plus zero or
//  more Claude worktrees at <repoRoot>/.claude/worktrees/<name>. The
//  %LOCALAPPDATA% recent-disks MRU is shared across all of them, so the disk
//  picker has to tell a disk in the *running* checkout apart from one that
//  lives in a sibling checkout (which is noise it should hide).
//
//  Everything here is lexical: no path is touched on disk, so the logic is
//  deterministic and unit-testable without a real repo layout.
//
////////////////////////////////////////////////////////////////////////////////

namespace RepoCheckout
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  WorktreeKeyOf
    //
    //  A Claude worktree checkout lives at <repo>/.claude/worktrees/<name>/...
    //  Returns the ".../.claude/worktrees/<name>" prefix identifying that
    //  worktree, or an empty string if `p` is not inside one.
    //
    ////////////////////////////////////////////////////////////////////////////

    inline std::wstring WorktreeKeyOf (const std::filesystem::path & p)
    {
        std::vector<std::filesystem::path>  comps (p.begin(), p.end());

        for (size_t i = 0; i + 2 < comps.size(); ++i)
        {
            if (_wcsicmp (comps[i].c_str(),     L".claude")   == 0 &&
                _wcsicmp (comps[i + 1].c_str(), L"worktrees") == 0)
            {
                std::filesystem::path  key;

                for (size_t j = 0; j <= i + 2; ++j)
                    key /= comps[j];

                return key.wstring();
            }
        }

        return std::wstring();
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  MainRootOfWorktreeKey
    //
    //  The repo main root implied by a worktree key: strip the trailing
    //  ".claude/worktrees/<name>" (three components). Empty in, empty out.
    //
    ////////////////////////////////////////////////////////////////////////////

    inline std::filesystem::path MainRootOfWorktreeKey (const std::wstring & worktreeKey)
    {
        if (worktreeKey.empty())
        {
            return std::filesystem::path();
        }

        // key == <mainRoot>/.claude/worktrees/<name>; three parents strip
        // <name>, "worktrees", ".claude" to land on <mainRoot>.
        return std::filesystem::path (worktreeKey)
                   .parent_path().parent_path().parent_path();
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  IsUnderOrEqual
    //
    //  True if `child` is `ancestor` itself or lies beneath it, compared
    //  component-by-component and case-insensitively (the host filesystem is
    //  case-insensitive). An empty `ancestor` matches nothing so a missing
    //  main root never swallows unrelated paths.
    //
    ////////////////////////////////////////////////////////////////////////////

    inline bool IsUnderOrEqual (const std::filesystem::path & child,
                                const std::filesystem::path & ancestor)
    {
        std::filesystem::path  c = child.lexically_normal();
        std::filesystem::path  a = ancestor.lexically_normal();

        if (a.empty())
        {
            return false;
        }

        std::filesystem::path::iterator  ci = c.begin();
        std::filesystem::path::iterator  ai = a.begin();

        for (; ai != a.end(); ++ai, ++ci)
        {
            if (ci == c.end())
            {
                return false;                            // child is shorter
            }
            if (_wcsicmp (ci->c_str(), ai->c_str()) != 0)
            {
                return false;                            // component mismatch
            }
        }

        return true;
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  IsForeignCheckoutDisk
    //
    //  Decide whether disk `p` belongs to a DIFFERENT checkout of this repo
    //  than the running build. `runningWorktreeKey` is
    //  WorktreeKeyOf(exeDirectory) — empty when the running build is the main
    //  tree (or an installed layout with no repo around it).
    //
    //  Rules:
    //   * `p` inside a worktree  -> foreign iff it's a different worktree than
    //     the running one. (When running from the main tree, runningWorktreeKey
    //     is empty, so every worktree disk is foreign.)
    //   * `p` NOT inside a worktree, running from a worktree -> foreign iff `p`
    //     lives under the repo main root (a main-tree repo disk); disks outside
    //     the repo (the user's own folders, %LOCALAPPDATA%) always pass.
    //   * `p` NOT inside a worktree, running from the main tree -> never foreign
    //     (main-tree and non-repo disks are both "here").
    //
    ////////////////////////////////////////////////////////////////////////////

    inline bool IsForeignCheckoutDisk (const std::filesystem::path & p,
                                       const std::wstring          & runningWorktreeKey)
    {
        std::wstring  entryKey = WorktreeKeyOf (p);

        if (!entryKey.empty())
        {
            return _wcsicmp (entryKey.c_str(), runningWorktreeKey.c_str()) != 0;
        }

        if (runningWorktreeKey.empty())
        {
            return false;                                // running from main tree
        }

        // Running from a worktree: hide repo disks that live in a sibling
        // checkout — here, the main tree (under the repo root but not under any
        // worktree). Non-repo disks fall outside the main root and pass.
        return IsUnderOrEqual (p, MainRootOfWorktreeKey (runningWorktreeKey));
    }
}

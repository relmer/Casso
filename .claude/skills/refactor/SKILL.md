---
name: "refactor"
description: "Refactor a .cpp file or class to follow docs/class-design.md, planned by Opus and done by Sonnet."
argument-hint: "A .cpp path or a class name, for example CassoExplorerBrowser.cpp or CassoExplorerBrowser"
user-invocable: true
---

Target: $ARGUMENTS

1. Resolve the target to one .cpp file. If it is a class name, find the .cpp
   that defines it (`rg -l "^\w.*\b<Class>::" -g "*.cpp"`, picking the file
   with most of its methods). If it is a bare file name, find its path. If
   more than one file matches, ask which.
2. Launch the `class-design-planner` agent (Agent tool,
   `subagent_type: "class-design-planner"`, Opus) with the resolved path, the
   class name, and the current branch. It reads the code and returns a plan.
3. Show the owner the plan and its open questions, and wait for approval.
   Apply any changes the owner asks for to the plan.
4. Make sure the tree is clean: the executor commits each split, so
   uncommitted work would be swept into its commits. If it isn't clean, ask
   the owner how to proceed.
5. Launch the `class-design-refactor` agent (Agent tool,
   `subagent_type: "class-design-refactor"`, Sonnet) with the path, the
   branch, and the approved plan as written.
6. When it returns, check its commits against the plan, then relay its
   commits, line counts, test results and open questions. Don't push.

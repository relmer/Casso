---
name: "refactor"
description: "Refactor a .cpp file or class to follow docs/class-design.md, through the class-design-refactor agent."
argument-hint: "A .cpp path or a class name, for example CassoExplorerBrowser.cpp or CassoExplorerBrowser"
user-invocable: true
---

Target: $ARGUMENTS

1. Resolve the target to one .cpp file. If it is a class name, find the .cpp
   that defines it (`rg -l "^\w.*\b<Class>::" -g "*.cpp"`, picking the file
   with most of its methods). If it is a bare file name, find its path. If
   more than one file matches, ask which.
2. Launch the `class-design-refactor` agent (Agent tool,
   `subagent_type: "class-design-refactor"`) with the resolved path, the class
   name, and the current branch.
3. When it returns, relay its commits, line counts, test results and open
   questions. Don't push.

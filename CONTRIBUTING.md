# Contributing to Casso

## Commit Messages

This project uses [Conventional Commits](https://www.conventionalcommits.org/). Please format your commit messages as:

```
<type>(<scope>): <description>

[optional body]

[optional footer(s)]
```

### Types

| Type       | Description                                      |
|------------|--------------------------------------------------|
| `feat`     | New feature                                      |
| `fix`      | Bug fix                                          |
| `docs`     | Documentation only                               |
| `style`    | Formatting, no code change                       |
| `refactor` | Code change that neither fixes nor adds feature  |
| `perf`     | Performance improvement                          |
| `test`     | Adding or fixing tests                           |
| `chore`    | Build process, tooling, dependencies             |
| `ci`       | CI/CD changes                                    |
| `build`    | Build system changes                             |

### Examples

```
feat(cpu): implement PHA/PLA stack operations
fix(ops): correct ShiftLeft dispatch to use ASL not ROL
test(adc): add signed overflow edge cases
chore(build): add ARM64 platform configurations
```

## Building

Requires Visual Studio 2026 (v18.x) with "Desktop development with C++" workload.

```powershell
# Build Debug for current architecture
.\scripts\Build.ps1

# Build Release for all platforms
.\scripts\Build.ps1 -Target BuildAllRelease

# Run tests
.\scripts\RunTests.ps1
```

Or use the VS Code tasks (Ctrl+Shift+B).

## Code Style

- See [.github/copilot-instructions.md](.github/copilot-instructions.md) for detailed formatting rules
- Every `.cpp` file must include `"Pch.h"` as its first `#include`
- Use quoted includes (`"header.h"`) for project headers; system headers go in `Pch.h`
- No anonymous namespaces: file-local constants are file-scope `static constexpr` (`s_k` naming); helpers are class `static` members, not free functions
- A closing `}` is followed by a blank line — except a do-while tail, a following `else`, or another closing `}`

## Third-Party Dependencies

Casso vendors a small allowlist of MIT / BSD / Apache / public-domain
third-party sources under `External/` (no package manager, no binary
downloads). The current allowlist and the rules around adding to it
live in [`.specify/memory/constitution.md`](.specify/memory/constitution.md)
under **Approved Third-Party Dependencies**. Adding a new entry is a
constitution amendment (MINOR version bump); don't quietly add
dependencies without going through that process. `scripts/CheckShaderLicenses.ps1`
also runs pre-build to keep GPL / copyleft strings out of `Casso/Shaders/`
outside designated `// ATTRIBUTION:` comment blocks.

## Changelog and README

- **ALWAYS** update [CHANGELOG.md](CHANGELOG.md) when making user-visible changes (`feat`, `fix`, `perf`)
- Add entries under an `[Unreleased]` section at the top; they get moved to a versioned heading at release time
- Follow [Keep a Changelog](https://keepachangelog.com/) categories: Added, Changed, Fixed, Removed
- Update [README.md](README.md) when a change affects documented features, test counts, or the roadmap
- Update the test count in README when adding or removing test methods

## Pull Requests

1. Create a feature branch from `master`
2. Make your changes with conventional commit messages
3. Ensure all tests pass (`.\scripts\RunTests.ps1`)
4. Build must succeed with no errors or warnings
5. Update CHANGELOG.md and README.md if the change is user-visible

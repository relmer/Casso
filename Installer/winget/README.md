# The winget manifest

Three YAML templates and what to do with them. `scripts/NewWingetManifest.ps1`
fills them from a published release; nothing here is edited by hand except the
metadata, and the metadata is the only part that is not derived.

## Why this is an msix manifest and not a portable one

The first submission was a portable zip, and winget's validation refused it on
both architectures.

A portable install drops symlinks into a `Links` folder on `PATH`, and the
validator launches the binaries through those links. Windows resolves a DLL
import against the directory of the **link**, not of its target, so the loader
looked in `Links`, found no `vcruntime140.dll` beside it, fell through to
`System32` and came up empty on a machine without the Visual C++
redistributable. `Casso.exe` and `CassoCli.exe` both exited with
`0xC0000135` / `STATUS_DLL_NOT_FOUND`. Setting `ArchiveBinariesDependOnPath`
changed nothing: it does not move where the loader looks, and the validator
still launched from `Links`.

Reproduced locally in both directions, which is what settled it. Launched from
the folder the build produced, the four CRT DLLs load from that folder.
Launched through a symlink with `CreateProcess`, the same four come from
`System32` instead.

An MSIX has no links. The executables run from the folder they live in, beside
the DLLs the package carries, and the two terminal commands come from app
execution aliases, which are entries in the app-paths list rather than files on
`PATH`.

The zip is still published. It works for direct use, and only the link path was
ever broken.

## The three values that cannot be typed

`InstallerSha256` is the hash of the published download, so it comes from the
published URL rather than a local build.

`SignatureSha256` is the hash of `AppxSignature.p7x` inside the bundle. winget
checks it before installing, to confirm the package was signed by the publisher
the manifest names. It exists only after signing.

`PackageFamilyName` is how Windows and winget identify the package for upgrade
and uninstall. It is the manifest's `Name`, an underscore, and a hash of the
manifest's `Publisher`, so it moves if the signing certificate is ever reissued
under a different subject.

`NewWingetManifest.ps1` reads all three out of the bundle it downloads, and
checks its family-name derivation against the packages already installed on the
machine before it writes anything.

## The first submission

Only the first one is done by hand. After `relmer.Casso` exists upstream, the
release job's *Publish to winget* step keeps it current with `wingetcreate
update`, which reads the same values out of the same bundle on its own.

```powershell
scripts\NewWingetManifest.ps1                      # latest release, or -Tag v1.25.0
winget validate --manifest <the path it prints>
winget install --manifest <the path it prints>     # installs for real; uninstall after
```

Then copy that `manifests/r/relmer/Casso/<version>` folder into a fork of
`microsoft/winget-pkgs` and open a pull request against `master`.

The release the manifest is built from has to be a real one with a signed
bundle attached. A locally packed bundle has no signature to hash, and a
release published before the MSIX existed has no bundle at all.

## If the signing certificate changes

The subject of the Azure Trusted Signing certificate appears in exactly one
place, `Identity/@Publisher` in `Installer/Package.appxmanifest`, and
everything else follows from it. Read the new subject off a signed binary:

```powershell
Get-AuthenticodeSignature Casso.exe | ForEach-Object { $_.SignerCertificate.Subject }
```

Put it there verbatim, and the release job's *Verify the bundle is signed* step
will confirm the two agree on the next release. The package family name changes
with it, which means Windows treats the result as a different application: it
installs alongside the old one rather than upgrading it, and the winget
manifest needs the new family name. That is a consequence worth knowing about
before reissuing a certificate, not after.

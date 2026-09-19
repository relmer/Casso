# The winget manifest

Three YAML templates and what to do with them. `scripts/NewWingetManifest.ps1`
fills them from a published release; nothing here is edited by hand except the
metadata, and the metadata is the only part that is not derived.

## Why msix

An MSIX installs Casso and the DLLs it needs as one unit and runs them from
one folder. A portable install launches through symlinks, where the loader
searches beside the link and never finds the C runtime.

The zip is still published and works for direct use.

## The three values that cannot be typed

`InstallerSha256` is the hash of the published download, so it comes from the
published URL rather than a local build.

`SignatureSha256` is the hash of `AppxSignature.p7x` inside the bundle. winget
checks it before installing, to confirm the package was signed by the publisher
the manifest claims. It exists only after signing.

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

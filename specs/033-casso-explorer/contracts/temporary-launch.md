# Contract: opening an image in a temporary machine (Story 44)

## Command line

```text
Casso.exe --launch <image> [--machine <id>] [--title <text>]
```

- `<id>`: Apple2, Apple2Plus, Apple2e, Apple2eEnhanced (the default),
  Apple2c, Apple2cPlus. Matched without case.
- Casso doesn't accept `--launch` with `--disk1`, `--disk2`, `--hd` or `--smartport`:
  the placement chooses the drive.
- A relaunch after a self-update drops `--launch` and `--machine`.
- Exit status: 0 after the window closes; the bad-command-line status
  for an unknown id or a missing image. A model that can't take the image
  Casso displays the reason and exits without building a machine.

Proposed help lines, for the owner's approval (FR-317):

```text
--launch <image>   Boot this image in a temporary copy of a stock machine,
                   which is never saved.
--machine <name>   Which machine to boot, such as Apple2e. With --launch, which
                   stock machine to copy.
```

## Registry, unpackaged builds

```text
HKCU\Software\Classes\Casso.DiskImage.<ext>\shell\open\command
    = "<Casso.exe>" --launch "%1"
HKCU\Software\Classes\Casso.DiskImage.<ext>\shell\OpenOn
    MUIVerb = Open on
    ExtendedSubCommandsKey = Casso.OpenOn
HKCU\Software\Classes\Casso.OpenOn\shell\<nn><id>
    MUIVerb = <label>
    command = "<Casso.exe>" --launch "%1" --machine <id>
```

`<ext>`: .dsk .do .po .woz .nib .nb2 .2mg .2img .hdv .dc .dc42 .image.
`<nn>` is a two-digit prefix that sorts the rows in model order.

## Package

Each `uap3:FileTypeAssociation` uses `--launch "%1"` for its default and
Open verbs and has one `uap3:Verb` per stock model with
`--launch "%1" --machine <id>`. The block types are added as separate associations.

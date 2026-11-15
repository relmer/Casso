# Contract: ImageClassifier

`CassoEmuCore/Devices/Disk/ImageClassifier.h`. Types are in [../data-model.md](../data-model.md).

```cpp
class ImageClassifier
{
public:
    static constexpr size_t  kHeadBytes = 512;

    //  Pure. `extension` includes the dot and is matched without regard to case.
    //  `head` is the file's first bytes, up to kHeadBytes; a shorter file passes
    //  what it has.
    static ImageClass    ClassifyHeader (std::wstring_view extension, uint64_t fileBytes, std::span<const Byte> head);

    //  One Stat and one ReadAt. A file that cannot be read is Unclassified when
    //  its extension is an image's, NotAnImage otherwise; neither is a failure.
    static ImageClass    Classify       (IDiskFileIo & fileIo, const std::string & path);

    static std::wstring  FormatSize       (const ImageClass & imageClass);
    static std::wstring  FormatFormFactor (const ImageClass & imageClass);   // "5.25-inch", "3.5-inch", "Hard disk", "Tape", or empty
    static bool          IsImageExtension (std::wstring_view extension);
};
```

## Guarantees

- No call reads more than `kHeadBytes` of a file.
- `Classify` never returns a failure for a file's content. A locked, missing or damaged file is a result, not an error, so a listing is never stopped by one file.
- `IsImageExtension` is true for every extension accepted by `DiskImageStore::IsMountableImageExtension`, `BlockImageFile`, `BlockDeviceMedia` and `TapeImageLoader::IsTapeFileExtension`. A test compares the lists.
- A file with an image extension is never `Classified` unless its length and header are consistent with its container. A truncated `.dsk` is `Unclassified`.
- The same bytes always produce the same result; there is no dependence on the folder, the clock or preferences.

## Not covered

- Whether the file system inside the image is intact, or which file system it is.
- Compressed or archived images (`.gz`, `.zip`, `.sdk`). They are `NotAnImage` here; the emulator does not mount them directly.

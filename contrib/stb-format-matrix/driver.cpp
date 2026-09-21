#include <cstdio>
#include <cstring>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringIterable.h>
#include <Corrade/Containers/StringView.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/PluginManager/PluginMetadata.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/Format.h>
#include <Corrade/Utility/Move.h>
#include <Corrade/Utility/Path.h>

#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImageConverter.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/ImageData.h>

using namespace Corrade;
using namespace Magnum;
using namespace Containers::Literals;

namespace {

/* Same order as matrix.cmake, which is stb's own loader dispatch order in
   stbi__load_main(). Bit i of a mask means format i is stripped, so the first
   enabled format that accepts a file is the one that decodes it. */
constexpr Containers::StringView ImporterFormats[]{
    "JPEG"_s, "PNG"_s, "BMP"_s, "GIF"_s, "PSD"_s, "PIC"_s, "PNM"_s, "HDR"_s, "TGA"_s
};
constexpr std::size_t GifIndex = 3;

constexpr Containers::StringView ConverterFormats[]{
    "BMP"_s, "JPEG"_s, "HDR"_s, "PNG"_s, "TGA"_s
};
constexpr Containers::StringView ConverterAliases[]{
    "StbBmpImageConverter"_s, "StbJpegImageConverter"_s, "StbHdrImageConverter"_s,
    "StbPngImageConverter"_s, "StbTgaImageConverter"_s
};
constexpr Containers::StringView ConverterExtensions[]{
    "bmp"_s, "jpg"_s, "hdr"_s, "png"_s, "tga"_s
};
/* JPEG is the only lossy one, so its round-trip compares size and format only */
constexpr std::size_t JpegConverterIndex = 1;
constexpr std::size_t HdrConverterIndex = 2;

struct Variant {
    Containers::String plugin, id, dir, library;
    unsigned mask;
    Containers::Array<Containers::String> enabled, provides;
};

struct Decoded {
    bool opened = false, ok = false;
    unsigned frames = 0;
    Vector2i size;
    PixelFormat format{};
    Containers::Array<char> data;
};

Containers::Array<char> copyOf(Containers::ArrayView<const char> in) {
    Containers::Array<char> out{NoInit, in.size()};
    std::memcpy(out.data(), in.data(), in.size());
    return out;
}

bool sameBytes(Containers::ArrayView<const char> a, Containers::ArrayView<const char> b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size()) == 0;
}

bool same(const Decoded& a, const Decoded& b) {
    if(a.ok != b.ok || a.frames != b.frames)
        return false;
    if(!a.ok)
        return true;
    return a.size == b.size && a.format == b.format && sameBytes(a.data, b.data);
}

Decoded decode(Trade::AbstractImporter& importer, Containers::StringView path) {
    Decoded out;
    Error silenceError{nullptr};
    Warning silenceWarning{nullptr};
    if(!importer.openFile(path))
        return out;
    out.opened = true;
    out.frames = importer.image2DCount();
    if(Containers::Optional<Trade::ImageData2D> image = importer.image2D(0)) {
        out.ok = true;
        out.size = image->size();
        if(!image->isCompressed())
            out.format = image->format();
        out.data = copyOf(image->data());
    }
    importer.close();
    return out;
}

bool contains(Containers::ArrayView<const Containers::String> haystack, Containers::StringView needle) {
    for(const Containers::String& s: haystack)
        if(s == needle)
            return true;
    return false;
}

Containers::Array<Containers::String> splitCsv(Containers::StringView in) {
    Containers::Array<Containers::String> out;
    for(Containers::StringView piece: in.split(','))
        if(piece)
            arrayAppend(out, InPlaceInit, piece);
    return out;
}

int failures = 0;

void fail(Containers::StringView what) {
    Error{} << "FAIL" << what;
    ++failures;
}

const Variant* byMask(Containers::ArrayView<const Variant> variants, unsigned mask) {
    for(const Variant& v: variants)
        if(v.mask == mask)
            return &v;
    return nullptr;
}

/* The converter strips by having the linker drop unreferenced statics, so it
   regresses with no visible change in behaviour */
void checkSizes(Containers::StringView what, Containers::ArrayView<const Variant> variants,
    Containers::ArrayView<const Containers::StringView> formats)
{
    const Variant* allOn = byMask(variants, 0);
    Containers::Optional<std::size_t> base = allOn ? Utility::Path::size(allOn->library) : Containers::NullOpt;
    if(!base) {
        fail("cannot size the all-formats-on "_s + what);
        return;
    }
    for(std::size_t i = 0; i != formats.size(); ++i) {
        const Variant* off = byMask(variants, 1u << i);
        Containers::Optional<std::size_t> size = off ? Utility::Path::size(off->library) : Containers::NullOpt;
        if(!size) {
            fail("cannot size "_s + what + " without "_s + formats[i]);
            continue;
        }
        if(*size >= *base)
            fail(what + " without "_s + formats[i] + " is not smaller than with every format on"_s);
        else
            Debug{} << " " << what << "no-"_s + formats[i] << Utility::format("{}", std::ptrdiff_t(*size) - std::ptrdiff_t(*base));
    }
}

}

int main(int argc, char** argv) {
    if(argc < 4) {
        std::fprintf(stderr, "usage: %s <variants.tsv> <images-dir> <tmp-dir>\n", argv[0]);
        return 2;
    }
    const Containers::StringView tsvPath = argv[1];
    const Containers::String imagesDir = argv[2];
    const Containers::String tmpDir = argv[3];
    if(!Utility::Path::make(tmpDir)) {
        Error{} << "cannot create" << tmpDir;
        return 2;
    }

    Containers::Optional<Containers::String> tsv = Utility::Path::readString(tsvPath);
    if(!tsv) {
        Error{} << "cannot read" << tsvPath;
        return 2;
    }
    Containers::Array<Variant> importerVariants, converterVariants;
    std::size_t rows = 0;
    for(Containers::StringView line: tsv->split('\n')) {
        /* file(WRITE) opens in text mode on Windows, so the rows arrive CRLF */
        if(line.hasSuffix("\r"_s))
            line = line.exceptSuffix(1);
        if(!line)
            continue;
        ++rows;
        const Containers::Array<Containers::StringView> f = line.split('\t');
        if(f.size() != 7) {
            Error{} << "malformed row" << line;
            return 2;
        }
        Variant v;
        v.plugin = f[0];
        v.id = f[1];
        v.mask = unsigned(std::strtoul(Containers::String{f[2]}.data(), nullptr, 10));
        v.dir = f[3];
        v.enabled = splitCsv(f[4]);
        v.provides = splitCsv(f[5]);
        v.library = f[6];
        /* Not arrayAppend(cond ? a : b, ...) -- clang 23.1.0-rc3 at -O2 and above
           reads both sizes back one append short here, and the missing element
           reappears on the first call into a plugin DLL */
        if(v.plugin == "StbImageImporter"_s)
            arrayAppend(importerVariants, InPlaceInit, Utility::move(v));
        else
            arrayAppend(converterVariants, InPlaceInit, Utility::move(v));
    }
    if(rows != importerVariants.size() + converterVariants.size()) {
        Error{} << "parsed" << importerVariants.size() << "importer and"
            << converterVariants.size() << "converter variants out of" << rows << "rows";
        return 2;
    }
    Debug{} << "variants:" << rows << "rows ->" << importerVariants.size() << "importer,"
        << converterVariants.size() << "converter";

    const Variant* reference = nullptr;
    for(const Variant& v: importerVariants)
        if(v.mask == 0)
            reference = &v;
    if(!reference) {
        Error{} << "no all-formats-on importer variant";
        return 2;
    }

    Containers::Array<Containers::String> samples;
    if(Containers::Optional<Containers::Array<Containers::String>> list = Utility::Path::list(imagesDir,
        Utility::Path::ListFlag::SkipDirectories|Utility::Path::ListFlag::SortAscending))
    {
        for(Containers::String& name: *list)
            arrayAppend(samples, InPlaceInit, Utility::Path::join(imagesDir, name));
    }
    if(samples.isEmpty()) {
        Error{} << "no samples in" << imagesDir;
        return 2;
    }

    Debug{} << "";
    Debug{} << "phase 0: reference decode, variant" << reference->id;

    PluginManager::Manager<Trade::AbstractImporter> referenceManager{reference->dir};
    Containers::Pointer<Trade::AbstractImporter> referenceImporter =
        referenceManager.loadAndInstantiate("StbImageImporter");
    if(!referenceImporter) {
        Error{} << "cannot load the reference importer from" << reference->dir;
        return 2;
    }

    Containers::Array<Decoded> referenceDecoded;
    for(const Containers::String& path: samples) {
        Decoded d = decode(*referenceImporter, path);
        if(!d.ok)
            fail(Utility::Path::filename(path) + " does not decode with all formats on"_s);
        else
            Debug{} << " " << Utility::Path::filename(path) << "frames" << d.frames << d.size << d.format;
        arrayAppend(referenceDecoded, InPlaceInit, Utility::move(d));
    }
    if(failures) {
        Error{} << "phase 0 failed, the samples are wrong -- stopping";
        return 1;
    }

    Debug{} << "";
    Debug{} << "phase 1: one loader at a time";

    const std::size_t formatCount = Containers::arraySize(ImporterFormats);
    Containers::Array<Decoded> probe{ValueInit, formatCount*samples.size()};
    for(std::size_t i = 0; i != formatCount; ++i) {
        const unsigned singleton = ((1u << formatCount) - 1) ^ (1u << i);
        const Variant* v = nullptr;
        for(const Variant& candidate: importerVariants)
            if(candidate.mask == singleton)
                v = &candidate;
        if(!v) {
            fail("no singleton variant for "_s + ImporterFormats[i]);
            continue;
        }

        PluginManager::Manager<Trade::AbstractImporter> manager{v->dir};
        Containers::Pointer<Trade::AbstractImporter> importer = manager.loadAndInstantiate("StbImageImporter");
        if(!importer) {
            fail("cannot load the "_s + ImporterFormats[i] + "-only importer"_s);
            continue;
        }

        Containers::String accepted;
        for(std::size_t s = 0; s != samples.size(); ++s) {
            probe[i*samples.size() + s] = decode(*importer, samples[s]);
            if(probe[i*samples.size() + s].ok)
                accepted = accepted + " "_s + Utility::Path::filename(samples[s]);
        }
        Debug{} << " " << ImporterFormats[i] << "accepts:" << (accepted ? Containers::StringView{accepted} : " (nothing)"_s);
    }
    if(failures)
        return 1;

    Debug{} << "";
    Debug{} << "phase 2:" << importerVariants.size() << "importer configs";

    for(const Variant& v: importerVariants) {
        for(std::size_t i = 0; i != formatCount; ++i) {
            const bool stripped = (v.mask >> i) & 1;
            if(contains(v.enabled, ImporterFormats[i]) == stripped)
                fail("importer "_s + v.id + " table disagrees with the mask on "_s + ImporterFormats[i]);
        }

        PluginManager::Manager<Trade::AbstractImporter> manager{v.dir};

        const PluginManager::PluginMetadata* metadata = manager.metadata("StbImageImporter");
        if(!metadata) {
            fail("importer "_s + v.id + " has no metadata"_s);
            continue;
        }
        std::size_t advertised = 0;
        for(Containers::StringView p: metadata->provides()) {
            ++advertised;
            if(!contains(v.provides, p))
                fail("importer "_s + v.id + " advertises unexpected "_s + p);
            if(manager.loadState(p) == PluginManager::LoadState::NotFound)
                fail("importer "_s + v.id + " alias "_s + p + " is advertised but not found"_s);
        }
        if(advertised != v.provides.size())
            fail("importer "_s + v.id + " advertises the wrong alias count"_s);

        Containers::Pointer<Trade::AbstractImporter> importer = manager.loadAndInstantiate("StbImageImporter");
        if(!importer) {
            fail("importer "_s + v.id + " does not load"_s);
            continue;
        }

        for(std::size_t s = 0; s != samples.size(); ++s) {
            /* The winner is the first enabled loader that phase 1 showed
               accepts this file. TGA is tried last because its test
               false-positives, so this cannot be shortcut to "decodes iff its
               own format is enabled". */
            Decoded expected;
            for(std::size_t i = 0; i != formatCount; ++i) {
                if((v.mask >> i) & 1)
                    continue;
                if(probe[i*samples.size() + s].ok) {
                    expected.ok = true;
                    expected.size = probe[i*samples.size() + s].size;
                    expected.format = probe[i*samples.size() + s].format;
                    expected.data = copyOf(probe[i*samples.size() + s].data);
                    break;
                }
            }
            /* image2DCount comes from the GIF probe in doOpenData, which runs
               before any loader is picked */
            expected.frames = ((v.mask >> GifIndex) & 1) ? 1 : probe[GifIndex*samples.size() + s].frames;

            const Decoded actual = decode(*importer, samples[s]);
            if(!actual.opened)
                fail("importer "_s + v.id + " failed to open "_s + Utility::Path::filename(samples[s]));
            if(!same(expected, actual)) {
                fail("importer "_s + v.id + " "_s + Utility::Path::filename(samples[s]) +
                    ": expected ok="_s + (expected.ok ? "1"_s : "0"_s) + " frames="_s +
                    Utility::format("{}", expected.frames) + ", got ok="_s +
                    (actual.ok ? "1"_s : "0"_s) + " frames="_s + Utility::format("{}", actual.frames) +
                    (expected.ok && actual.ok && !sameBytes(expected.data, actual.data) ? " (pixels differ)"_s : ""_s));
            }
        }
    }

    Debug{} << "";
    Debug{} << "converter:" << converterVariants.size() << "configs";

    const Decoded* rgbSample = nullptr;
    for(std::size_t s = 0; s != samples.size(); ++s)
        if(Utility::Path::filename(samples[s]) == "rgb.png"_s)
            rgbSample = &referenceDecoded[s];
    if(!rgbSample || rgbSample->format != PixelFormat::RGB8Unorm) {
        Error{} << "rgb.png is missing or not RGB8Unorm";
        return 1;
    }
    const ImageView2D rgbInput{PixelStorage{}.setAlignment(1),
        PixelFormat::RGB8Unorm, rgbSample->size, rgbSample->data};

    /* Powers of two survive the RGBE encoding stb writes for HDR bit-exactly,
       so this round-trip can compare as strictly as the 8-bit ones */
    Float hdrPixels[4*4*3];
    for(std::size_t i = 0; i != Containers::arraySize(hdrPixels); ++i)
        hdrPixels[i] = Float(1 << (i % 5))*0.25f;
    const ImageView2D hdrInput{PixelStorage{}.setAlignment(1), PixelFormat::RGB32F, {4, 4},
        Containers::arrayCast<const char>(Containers::arrayView(hdrPixels))};

    for(const Variant& v: converterVariants) {
        PluginManager::Manager<Trade::AbstractImageConverter> manager{v.dir};

        const PluginManager::PluginMetadata* metadata = manager.metadata("StbImageConverter");
        if(!metadata) {
            fail("converter "_s + v.id + " has no metadata"_s);
            continue;
        }
        std::size_t advertised = 0;
        for(Containers::StringView p: metadata->provides()) {
            ++advertised;
            if(!contains(v.provides, p))
                fail("converter "_s + v.id + " advertises unexpected "_s + p);
        }
        if(advertised != v.provides.size())
            fail("converter "_s + v.id + " advertises the wrong alias count"_s);

        for(std::size_t i = 0; i != Containers::arraySize(ConverterFormats); ++i) {
            const bool enabled = contains(v.enabled, ConverterFormats[i]);
            const ImageView2D& input = i == HdrConverterIndex ? hdrInput : rgbInput;

            const bool found = manager.loadState(ConverterAliases[i]) != PluginManager::LoadState::NotFound;
            if(found != enabled)
                fail("converter "_s + v.id + " alias "_s + ConverterAliases[i] +
                    (enabled ? " is missing"_s : " should not exist"_s));

            if(enabled) {
                Containers::Pointer<Trade::AbstractImageConverter> converter =
                    manager.loadAndInstantiate(ConverterAliases[i]);
                if(!converter) {
                    fail("converter "_s + v.id + " cannot instantiate "_s + ConverterAliases[i]);
                } else {
                    Containers::Optional<Containers::Array<char>> out;
                    {
                        Warning silence{nullptr};
                        out = converter->convertToData(input);
                    }
                    if(!out) {
                        fail("converter "_s + v.id + " "_s + ConverterFormats[i] + " produced nothing"_s);
                    } else {
                        Containers::String problem;
                        {
                            Error silenceError{nullptr};
                            Warning silenceWarning{nullptr};
                            Containers::Optional<Trade::ImageData2D> back;
                            if(referenceImporter->openData(*out))
                                back = referenceImporter->image2D(0);
                            referenceImporter->close();
                            if(!back)
                                problem = "output does not read back"_s;
                            else if(back->size() != input.size())
                                problem = "round-trip changed the size"_s;
                            else if(i != JpegConverterIndex && !sameBytes(back->data(), input.data()))
                                problem = "round-trip changed the pixels"_s;
                        }
                        if(problem)
                            fail("converter "_s + v.id + " "_s + ConverterFormats[i] + " "_s + problem);
                    }
                }
            }

            Containers::Pointer<Trade::AbstractImageConverter> plain =
                manager.loadAndInstantiate("StbImageConverter");
            if(!plain) {
                fail("converter "_s + v.id + " does not load under its own name"_s);
                continue;
            }
            const Containers::String outFile = Utility::Path::join(tmpDir,
                "out-"_s + v.id + "."_s + ConverterExtensions[i]);
            Containers::String message;
            bool wrote;
            {
                Error redirect{&message};
                Warning silence{nullptr};
                wrote = plain->convertToFile(input, outFile);
            }
            if(wrote != enabled)
                fail("converter "_s + v.id + " extension ."_s + ConverterExtensions[i] +
                    (enabled ? " should have written"_s : " should have refused"_s));
            if(!enabled && !message)
                fail("converter "_s + v.id + " extension ."_s + ConverterExtensions[i] +
                    " failed without saying why"_s);
        }
    }

    Debug{} << "";
    Debug{} << "sizes: one format off vs all on";
    checkSizes("importer"_s, importerVariants, ImporterFormats);
    checkSizes("converter"_s, converterVariants, ConverterFormats);

    Debug{} << "";
    if(failures) {
        Error{} << failures << "failures";
        return 1;
    }
    Debug{} << "all" << (importerVariants.size() + converterVariants.size()) << "configs pass";
    return 0;
}

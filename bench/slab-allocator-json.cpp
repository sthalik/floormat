#include "compat/slab-allocator.inl"
#include "compat/slab-allocator-std.hpp"
#include "compat/never-destroyed.hpp"
#include "compat/assert.hpp"
#include <cstdlib>
#include <map>
#include <random>
#include <string>
#include <vector>
#include <cr/Array.h>
#include <cr/Optional.h>
#include <cr/Path.h>
#include <cr/String.h>
#include <nlohmann/json.hpp>
#include <benchmark/benchmark.h>

namespace floormat {

namespace {

struct json_tag;

constexpr slab::tunables json_tunables = {.buffer_source = slab::buffer_source::slabs};
constexpr slab::tunables json_tunables_with_superpages_from_arena_base = {
    .bytes_until_superpages = 0, .buffer_source = slab::buffer_source::slabs};
constexpr slab::tunables json_tunables_without_superpages = {.bytes_until_superpages = slab::tunables::no_superpages,
                                                            .buffer_source = slab::buffer_source::slabs};
constexpr slab::tunables json_tunables_without_pool_per_type = {.buffer_source = slab::buffer_source::slabs,
                                                               .has_pool_per_type = false};

template<slab::tunables Tunables>
constinit never_destroyed<slab::memory_resource<json_tag, Tunables>> json_slab_resource;

using std_json = nlohmann::json;

template<slab::tunables Tunables>
struct slab_json_types
{
    template<typename T> using allocator = slab::global_std_allocator<T, &json_slab_resource<Tunables>.value>;
    using json = nlohmann::basic_json<std::map, std::vector, std::string, bool, std_json::number_integer_t,
                                      std_json::number_unsigned_t, std_json::number_float_t, allocator>;
};

using slab_json = slab_json_types<json_tunables>::json;
using slab_json_with_superpages_from_arena_base = slab_json_types<json_tunables_with_superpages_from_arena_base>::json;
using slab_json_without_superpages = slab_json_types<json_tunables_without_superpages>::json;
using slab_json_without_pool_per_type = slab_json_types<json_tunables_without_pool_per_type>::json;

#ifdef __clang__
#pragma clang diagnostic push
// Clang 23.1.5 treats emplace_back() as invalidating the vector parameter itself.
// https://github.com/llvm/llvm-project/issues/222537
#if __has_warning("-Wlifetime-safety-invalidation")
#pragma clang diagnostic ignored "-Wlifetime-safety-invalidation"
#endif
#endif
void append_json_files(std::vector<std::string>& texts, StringView dir)
{
    const auto names = Path::list(dir, Path::ListFlag::SkipDotAndDotDot | Path::ListFlag::SortAscending);
    fm_assert(names);
    for (const String& name : *names)
    {
        const String path = Path::join(dir, name);
        if (Path::isDirectory(path))
            append_json_files(texts, path);
        else if (name.hasSuffix(".json"_s))
        {
            const auto text = Path::readString(path);
            fm_assert(text);
            texts.emplace_back(text->data(), text->size());
        }
    }
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

std::vector<std::string> read_floormat_json_files()
{
    std::vector<std::string> texts;
    if (Path::exists("scenery/scenery.json"_s))
        for (StringView dir : {"anim"_s, "ground"_s, "scenery"_s, "vobj"_s, "walls"_s})
            append_json_files(texts, dir);
    return texts;
}

const std::vector<std::string>& floormat_json_files()
{
    static const std::vector<std::string> texts = read_floormat_json_files();
    return texts;
}

std::vector<std::string> make_generated_document()
{
    std::mt19937_64 rng{0x9e3779b97f4a7c15};
    std_json frames = std_json::array();
    for (u32 i = 0; i < 50000; i++)
    {
        std_json frame = std_json::object();
        frame["name"] = "frame-" + std::to_string(i);
        frame["ground"] = {(u32)(rng() % 256), (u32)(rng() % 256)};
        frame["offset"] = {(i32)(rng() % 64) - 32, (i32)(rng() % 64) - 32};
        frame["size"] = {(u32)(rng() % 128), (u32)(rng() % 192)};
        frame["scale"] = (double)(rng() % 1000) / 1000;
        frame["tags"] = {"walk", "north", "loop"};
        frames.push_back(std::move(frame));
    }
    return {std_json{{"frames", std::move(frames)}}.dump()};
}

const std::vector<std::string>& generated_document()
{
    static const std::vector<std::string> texts = make_generated_document();
    return texts;
}

std::vector<std::string> read_external_documents()
{
    std::vector<std::string> texts;
    const char* const dir = std::getenv("FLOORMAT_JSON_BENCH_DIR");
    if (!dir || !*dir)
        return texts;
    const auto names = Path::list(dir, Path::ListFlag::SkipDirectories | Path::ListFlag::SkipDotAndDotDot |
                                           Path::ListFlag::SortAscending);
    fm_assert(names);
    for (const String& name : *names)
    {
        const auto text = Path::readString(Path::join(dir, name));
        fm_assert(text);
        if (name.hasSuffix(".jsonl"_s))
        {
            for (StringView line : text->splitWithoutEmptyParts('\n'))
                texts.emplace_back(line.data(), line.size());
        }
        else if (name.hasSuffix(".json"_s))
            texts.emplace_back(text->data(), text->size());
    }
    return texts;
}

const std::vector<std::string>& external_documents()
{
    static const std::vector<std::string> texts = read_external_documents();
    return texts;
}

using json_texts = const std::vector<std::string>& (*)();

usize total_bytes(const std::vector<std::string>& texts)
{
    usize bytes = 0;
    for (const std::string& text : texts)
        bytes += text.size();
    return bytes;
}

template<typename Json>
std::vector<Json> parse_all(const std::vector<std::string>& texts)
{
    std::vector<Json> values;
    values.reserve(texts.size());
    for (const std::string& text : texts)
        values.push_back(Json::parse(text));
    return values;
}

template<typename Json, json_texts Texts>
void Json_Parse(benchmark::State& state)
{
    const std::vector<std::string>& texts = Texts();
    if (texts.empty())
        state.SkipWithError("JSON files not installed");
    else
    {
        for (auto _ : state)
            for (const std::string& text : texts)
            {
                Json value = Json::parse(text);
                benchmark::DoNotOptimize(value);
            }
        state.SetBytesProcessed(state.iterations() * (i64)total_bytes(texts));
    }
}

template<typename Json, json_texts Texts>
void Json_Dump(benchmark::State& state)
{
    const std::vector<std::string>& texts = Texts();
    if (texts.empty())
        state.SkipWithError("JSON files not installed");
    else
    {
        const std::vector<Json> values = parse_all<Json>(texts);
        for (auto _ : state)
            for (const Json& value : values)
            {
                std::string text = value.dump();
                benchmark::DoNotOptimize(text);
            }
        state.SetBytesProcessed(state.iterations() * (i64)total_bytes(texts));
    }
}

template<typename Json, json_texts Texts>
void Json_Copy(benchmark::State& state)
{
    const std::vector<std::string>& texts = Texts();
    if (texts.empty())
        state.SkipWithError("JSON files not installed");
    else
    {
        const std::vector<Json> values = parse_all<Json>(texts);
        for (auto _ : state)
            for (const Json& value : values)
            {
                Json copy = value;
                benchmark::DoNotOptimize(copy);
            }
        state.SetBytesProcessed(state.iterations() * (i64)total_bytes(texts));
    }
}

template<typename Json, json_texts Texts>
void register_json_cases(const std::string& suffix)
{
    benchmark::RegisterBenchmark("Json_Parse/" + suffix, &Json_Parse<Json, Texts>)->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("Json_Dump/" + suffix, &Json_Dump<Json, Texts>)->Unit(benchmark::kMicrosecond);
    benchmark::RegisterBenchmark("Json_Copy/" + suffix, &Json_Copy<Json, Texts>)->Unit(benchmark::kMicrosecond);
}

template<typename Json>
void register_json_cases_on_each_document_set(const std::string& suffix)
{
    register_json_cases<Json, &floormat_json_files>("floormat_files/" + suffix);
    register_json_cases<Json, &generated_document>("generated/" + suffix);
    register_json_cases<Json, &external_documents>("external/" + suffix);
}

[[maybe_unused]] const int registered = [] {
    register_json_cases<std_json, &floormat_json_files>("floormat_files/std_allocator");
    register_json_cases<slab_json, &floormat_json_files>("floormat_files/single_threaded");
    register_json_cases<std_json, &generated_document>("generated/std_allocator");
    register_json_cases<slab_json, &generated_document>("generated/single_threaded");
    register_json_cases<std_json, &external_documents>("external/std_allocator");
    register_json_cases<slab_json, &external_documents>("external/single_threaded");
    register_json_cases_on_each_document_set<slab_json_with_superpages_from_arena_base>(
        "single_threaded/superpages:from_arena_base");
    register_json_cases_on_each_document_set<slab_json_without_superpages>("single_threaded/superpages:off");
    register_json_cases_on_each_document_set<slab_json_without_pool_per_type>("single_threaded/pool_per_type:off");
    return 0;
}();

} // namespace

} // namespace floormat

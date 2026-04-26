#pragma once
#include "compat/safe-ptr.hpp"
#include <cr/Format.h>
#include <cr/StringView.h>
#include <mg/Range.h>

struct ImDrawList;

namespace floormat::imgui {

namespace detail { struct text_painter_state; }

class text_painter_pool;

class text_painter
{
    detail::text_painter_state* _;

    explicit text_painter(detail::text_painter_state* s) noexcept : _{s} {}
    friend class text_painter_pool;

    uint32_t format_grow(uint32_t need);
    char* format_at(uint32_t offset) noexcept;
    void format_finalize(uint32_t color, uint32_t start, uint32_t length);

public:
    text_painter() = delete;
    ~text_painter();
    text_painter(text_painter&&) noexcept;
    text_painter& operator=(text_painter&&) noexcept;
    text_painter(const text_painter&) = delete;
    text_painter& operator=(const text_painter&) = delete;

    struct slot { uint32_t span_index; };

    text_painter& text(uint32_t color, StringView s);
    text_painter& rect(uint32_t color, float width, float vert_pad = 0);
    slot gap(float px);

    template<class... Args>
    text_painter& format(uint32_t color, const char* fmt, const Args&... args)
    {
        using Containers::MutableStringView;
        const auto need = uint32_t(Utility::formatInto(MutableStringView{}, fmt, args...));
        const auto start = format_grow(need);
        Utility::formatInto(MutableStringView{format_at(start), need + 1}, fmt, args...);
        format_finalize(color, start, need);
        return *this;
    }

    text_painter& with_background(uint32_t color, Vector2 padding = {});

    Vector2 size() const noexcept;
    Vector2 total_size() const noexcept;
    Math::Range2D<float> bounds() const noexcept;
    Math::Range2D<float> bounds_of(slot s) const noexcept;
    Math::Range2D<float> render(ImDrawList& draw, Vector2 anchor) const;
};

class text_painter_pool
{
    safe_ptr<detail::text_painter_state> _;

public:
    text_painter_pool();
    ~text_painter_pool();
    text_painter_pool(text_painter_pool&&) noexcept;
    text_painter_pool& operator=(text_painter_pool&&) noexcept;
    text_painter_pool(const text_painter_pool&) = delete;
    text_painter_pool& operator=(const text_painter_pool&) = delete;

    text_painter make(float font_size);
};

} // namespace floormat::imgui

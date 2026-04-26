#include "imgui-text.hpp"
#include "compat/assert.hpp"
#include "compat/defs.hpp"
#include <cr/GrowableArray.h>
#include <imgui.h>

namespace floormat::imgui {

namespace {

enum class kind : uint8_t { text, rect, gap };

struct span
{
    uint32_t start;
    uint32_t length;
    float width;
    float vert_pad;
    uint32_t color;
    kind k;
};

constexpr uint32_t initial_buf_reserve = 256;
constexpr uint32_t initial_spans_reserve = 16;

} // namespace

namespace detail {

struct text_painter_state
{
    Array<char> buf;
    Array<span> spans;
    float font_size = 0;
    float total_width = 0;

    bool has_bg = false;
    bool in_use = false;
    uint32_t bg_color = 0;
    Vector2 bg_padding{0};
    Vector2 last_anchor{0};

    text_painter_state()
    {
        arrayReserve(buf, initial_buf_reserve);
        arrayReserve(spans, initial_spans_reserve);
    }

    void reset(float fs) noexcept
    {
        arrayClear(buf);
        arrayClear(spans);
        font_size = fs;
        total_width = 0;
        has_bg = false;
        bg_color = 0;
        bg_padding = Vector2{0};
        last_anchor = Vector2{0};
    }

    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(text_painter_state);
};

} // namespace detail

text_painter_pool::text_painter_pool() : _{InPlaceInit} {}
text_painter_pool::~text_painter_pool() = default;
text_painter_pool::text_painter_pool(text_painter_pool&&) noexcept = default;
text_painter_pool& text_painter_pool::operator=(text_painter_pool&&) noexcept = default;

text_painter text_painter_pool::make(float font_size)
{
    if (_->in_use)
        fm_abort("text_painter_pool::make: previous painter still alive");
    _->reset(font_size);
    _->in_use = true;
    return text_painter{&*_};
}

text_painter::~text_painter()
{
    if (_)
        _->in_use = false;
}

text_painter::text_painter(text_painter&& other) noexcept :
    _{other._}
{
    other._ = nullptr;
}

text_painter& text_painter::operator=(text_painter&& other) noexcept
{
    if (this != &other)
    {
        if (_)
            _->in_use = false;
        _ = other._;
        other._ = nullptr;
    }
    return *this;
}

text_painter& text_painter::text(uint32_t color, StringView s)
{
    const auto sz = ImGui::CalcTextSize(s.begin(), s.end());
    const auto start = uint32_t(_->buf.size());
    arrayAppend(_->buf, ArrayView<const char>{s.data(), s.size()});
    arrayAppend(_->buf, '\0');
    arrayAppend(_->spans, span{ start, uint32_t(s.size()), sz.x, 0.f, color, kind::text });
    _->total_width += sz.x;
    return *this;
}

text_painter& text_painter::rect(uint32_t color, float width, float vert_pad)
{
    arrayAppend(_->spans, span{ 0, 0, width, vert_pad, color, kind::rect });
    _->total_width += width;
    return *this;
}

text_painter::slot text_painter::gap(float px)
{
    const auto idx = uint32_t(_->spans.size());
    arrayAppend(_->spans, span{ 0, 0, px, 0.f, 0, kind::gap });
    _->total_width += px;
    return slot{ idx };
}

uint32_t text_painter::format_grow(uint32_t need)
{
    const auto start = uint32_t(_->buf.size());
    arrayAppend(_->buf, NoInit, need + 1);
    return start;
}

char* text_painter::format_at(uint32_t offset) noexcept
{
    return _->buf.data() + offset;
}

void text_painter::format_finalize(uint32_t color, uint32_t start, uint32_t length)
{
    const char* p = _->buf.data() + start;
    const auto sz = ImGui::CalcTextSize(p, p + length);
    arrayAppend(_->spans, span{ start, length, sz.x, 0.f, color, kind::text });
    _->total_width += sz.x;
}

text_painter& text_painter::with_background(uint32_t color, Vector2 padding)
{
    _->has_bg = true;
    _->bg_color = color;
    _->bg_padding = padding;
    return *this;
}

Vector2 text_painter::size() const noexcept
{
    return { _->total_width, _->font_size };
}

Vector2 text_painter::total_size() const noexcept
{
    const Vector2 inner{ _->total_width, _->font_size };
    return _->has_bg ? inner + _->bg_padding * 2.f : inner;
}

Math::Range2D<float> text_painter::bounds() const noexcept
{
    const Vector2 inner_min = _->last_anchor;
    const Vector2 inner_max = _->last_anchor + Vector2{_->total_width, _->font_size};
    return _->has_bg
        ? Math::Range2D<float>{ inner_min - _->bg_padding, inner_max + _->bg_padding }
        : Math::Range2D<float>{ inner_min, inner_max };
}

Math::Range2D<float> text_painter::bounds_of(slot s) const noexcept
{
    fm_assert(s.span_index < _->spans.size());
    float x = 0;
    for (uint32_t i = 0; i < s.span_index; ++i)
        x += _->spans[i].width;
    const auto& sp = _->spans[s.span_index];
    const auto a = _->last_anchor;
    return Math::Range2D<float>{
        { a.x() + x,            a.y() },
        { a.x() + x + sp.width, a.y() + _->font_size }
    };
}

Math::Range2D<float> text_painter::render(ImDrawList& draw, Vector2 anchor) const
{
    _->last_anchor = anchor;
    const auto& s = *_;
    const char* const text_base = s.buf.data();

    if (s.has_bg)
    {
        const Vector2 bg_min = anchor - s.bg_padding;
        const Vector2 bg_max = anchor + Vector2{s.total_width, s.font_size} + s.bg_padding;
        draw.AddRectFilled({bg_min.x(), bg_min.y()}, {bg_max.x(), bg_max.y()}, s.bg_color);
    }

    float x = anchor.x();
    for (const auto& sp : s.spans)
    {
        switch (sp.k)
        {
        case kind::text: {
            const char* p = text_base + sp.start;
            draw.AddText({x, anchor.y()}, sp.color, p, p + sp.length);
            break;
        }
        case kind::rect:
            draw.AddRectFilled(
                {x,             anchor.y() + sp.vert_pad},
                {x + sp.width,  anchor.y() + s.font_size - sp.vert_pad},
                sp.color);
            break;
        case kind::gap:
            break;
        }
        x += sp.width;
    }

    return bounds();
}

} // namespace floormat::imgui

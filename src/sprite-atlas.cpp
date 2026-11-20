#include "sprite-atlas.hpp"
#include "sprite-atlas-impl.hpp"
#include "sprite-constants.hpp"
#include "quads.hpp"
#include "compat/assert.hpp"
#include "compat/exception.hpp"
#include "compat/enumerate.hpp"
#include "compat/gl-error.hpp"
#include "compat/limits.hpp"
#include <cstring>
#include <cstdio>
#include <bit>
#include <cr/GrowableArray.h>
#include <cr/Path.h>
#include <cr/StringView.h>
#include <cr/StructuredBindings.h>
#include <mg/BatchFunctions.h>
#include <mg/Buffer.h>
#include <mg/BufferImage.h>
#include <mg/Texture.h>
#include <mg/TextureArray.h>
#include <mg/TextureFormat.h>
#include <mg/Color.h>
#include <mg/Context.h>
#include <mg/Extensions.h>
#include <mg/Framebuffer.h>
#include <mg/ImageView.h>
#include <mg/Image.h>
#include <mg/PixelFormat.h>
#include <mg/AbstractImageConverter.h>

namespace floormat::SpriteAtlas {

namespace {

struct framebuffer_saver final
{
    framebuffer_saver();
    ~framebuffer_saver();

private:
    GLint draw_fb = 0, read_fb = 0, viewport[4] = {};
};

framebuffer_saver::framebuffer_saver()
{
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fb);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fb);
    glGetIntegerv(GL_VIEWPORT, viewport);
}

framebuffer_saver::~framebuffer_saver()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)draw_fb);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)read_fb);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    GL::Context::current().resetState(GL::Context::State::Framebuffers);
}

Shelf* find_shelf_with_space(Atlas& atlas, u32 w, u32 h)
{
    for (auto* sh : atlas.height_classes[height_class_index(h)].shelves)
        if (atlas.layer_width - sh->next_x >= w)
            return sh;

    return nullptr;
}

struct TallerShelf
{
    Shelf* shelf = nullptr;
    u32 loss = 0;
};

TallerShelf find_taller_shelf_with_space(Atlas& atlas, u32 w, u32 h)
{
    const u32 own_class = height_class_index(h);
    if (!atlas.height_classes[own_class].shelves.isEmpty())
        return {};
    // A smaller limit wastes more area in an atlas of few sprites.
    // Allowing any taller class wastes more in an atlas of many.
    for (u32 i = own_class + 1; i < height_classes.size() && height_classes[i] - h <= h / 2; i++)
        for (auto* sh : atlas.height_classes[i].shelves)
            if (atlas.layer_width - sh->next_x >= w)
                return { sh, w * (height_classes[i] - h) };

    return {};
}

Vector2ui rect_size(const Sprite& sprite)
{
    const auto size = Vector2ui{sprite.width, sprite.height};
    return !sprite.is_rotated ? size : size.flipped();
}

// Writes the sprite's pixels to dst tightly packed, in the rect's orientation.
void copy_to_rect(char* dst, const Sprite& sprite, const ImageView2D& pixels)
{
    const auto orig_w = sprite.width, orig_h = sprite.height;
    fm_assert((u32)pixels.size().x() == orig_w);
    fm_assert((u32)pixels.size().y() == orig_h);

    const auto px_size = (usize)pixels.pixelSize();
    const auto src_view = pixels.pixels();
    const usize src_row_stride = (usize)src_view.stride()[0];
    const auto* src_base = static_cast<const char*>(src_view.data());
    fm_assert(src_base);

    if (!sprite.is_rotated)
    {
        const usize row = (usize)orig_w * px_size;
        for (u32 y = 0; y < orig_h; y++)
            std::memcpy(dst + (usize)y * row, src_base + (usize)y * src_row_stride, row);
    }
    else
    {
        // Rotate 90° CCW in GL bottom-up memory. Derivation (both views
        // bu-oriented): dst_bu[y_r][x_r] = src_bu[orig_h-1-x_r][y_r].
        // Rect dims are (rect_w=orig_h, rect_h=orig_w).
        const u32 rect_w = orig_h;
        const u32 rect_h = orig_w;
        const usize dst_row_stride = (usize)rect_w * px_size;
        for (u32 y_r = 0; y_r < rect_h; y_r++)
        {
            char* dst_row = dst + (usize)y_r * dst_row_stride;
            for (u32 x_r = 0; x_r < rect_w; x_r++)
            {
                const char* src_px = src_base + (usize)(rect_w - 1 - x_r) * src_row_stride + (usize)y_r * px_size;
                std::memcpy(dst_row + (usize)x_r * px_size, src_px, px_size);
            }
        }
    }
}

void upload_rect(Atlas& atlas, const Sprite& sprite, PixelFormat format, ArrayView<const char> pixels)
{
    const auto size = rect_size(sprite);
    PixelStorage storage;
    storage.setAlignment(1);
    const ImageView3D view3d{storage, format, {(Int)size.x(), (Int)size.y(), 1}, pixels};
    atlas.texture.setSubImage(0, {(Int)sprite.x, (Int)sprite.y, (Int)sprite.layer}, view3d);
}

void clear_layers_to_magenta(GL::Texture2DArray& texture, Vector2i layer_size, u32 first, u32 end)
{
    const framebuffer_saver saver;
    GL::Framebuffer fb{{{}, layer_size}};
    for (auto i = first; i < end; i++)
    {
        fb.attachTextureLayer(GL::Framebuffer::ColorAttachment{0}, texture, 0, (Int)i);
        fb.clearColor(0, Color4{1.f, 0.f, 1.f, 1.f});
    }
}

// Returns a texture with no GL object if allocating the sparse storage fails.
GL::Texture2DArray make_sparse_texture(Vector2i layer_size, u32 layer_count, Int page_size_index)
{
    GL::Texture2DArray texture;
    GLint old_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &old_texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture.id());
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_VIRTUAL_PAGE_SIZE_INDEX_ARB, page_size_index);
    glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)old_texture);
    if (!fm_warn_glcall(texture.setStorage(1, GL::TextureFormat::RGBA8, {layer_size, (Int)layer_count})).second())
        texture = GL::Texture2DArray{NoCreate};
    return texture;
}

void commit_sparse_texture_layers(GL::Texture2DArray& texture, Vector2i layer_size, u32 first, u32 end)
{
    GLint old_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &old_texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture.id());
    fm_assert(fm_check_glcall(glTexPageCommitmentARB(GL_TEXTURE_2D_ARRAY, 0, 0, 0, (GLint)first,
                                                     layer_size.x(), layer_size.y(), (GLsizei)(end - first),
                                                     GL_TRUE)));
    glBindTexture(GL_TEXTURE_2D_ARRAY, (GLuint)old_texture);
}

Array<GL::Buffer> read_layers_into_pixel_buffers(GL::Texture2DArray& texture, Vector2i layer_size, u32 layer_count,
                                                 u32 layers_per_buffer, bool buffer_storage_supported)
{
    const auto layer_bytes = (usize)layer_size.product() * 4;
    const u32 buffer_count = (layer_count + layers_per_buffer - 1) / layers_per_buffer;
    Array<GL::Buffer> buffers{DirectInit, buffer_count, NoCreate};
    for (u32 j = 0; j < buffer_count; j++)
    {
        const u32 num_layers = Math::min(layers_per_buffer, layer_count - j * layers_per_buffer);
        const usize buffer_bytes = layer_bytes * num_layers;
        buffers[j] = GL::Buffer{GL::Buffer::TargetHint::PixelPack};
        const bool is_allocated = buffer_storage_supported
            ? fm_warn_glcall(buffers[j].setStorage(buffer_bytes, GL::Buffer::StorageFlag::ClientStorage)).second()
            : fm_warn_glcall(buffers[j].setData({nullptr, buffer_bytes}, GL::BufferUsage::StreamRead)).second();
        if (!is_allocated)
        {
            buffers = {};
            break;
        }
    }
    if (!buffers.isEmpty())
    {
        const with_gl_errors scope{gl_error_policy::error};
        const framebuffer_saver saver;
        GL::Framebuffer fb{{{}, layer_size}};
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        for (u32 i = 0; i < layer_count; i++)
        {
            fb.attachTextureLayer(GL::Framebuffer::ColorAttachment{0}, texture, 0, (Int)i);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.id());
            glBindBuffer(GL_PIXEL_PACK_BUFFER, buffers[i / layers_per_buffer].id());
            glReadPixels(0, 0, layer_size.x(), layer_size.y(), GL_RGBA, GL_UNSIGNED_BYTE,
                         reinterpret_cast<void*>(uptr{i % layers_per_buffer} * layer_bytes));
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        GL::Context::current().resetState(GL::Context::State::Buffers | GL::Context::State::PixelStorage);
    }
    return buffers;
}

} // namespace

Atlas::Limits get_texture_limits()
{
    using namespace GL::Extensions;
    fm_assert(GL::Context::hasCurrent());
    const auto& ctx = GL::Context::current();
    const Vector3i max = GL::Texture2DArray::maxSize();
    fm_assert(max.x() > 0 && max.z() > 0);
    GLint sparse_texture_size = 0, sparse_array_texture_layer_count = 0;
    if (ctx.isExtensionSupported<ARB::sparse_texture>() && ctx.isExtensionSupported<ARB::texture_storage>() &&
        ctx.isExtensionSupported<ARB::internalformat_query>())
    {
        glGetIntegerv(GL_MAX_SPARSE_TEXTURE_SIZE_ARB, &sparse_texture_size);
        glGetIntegerv(GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB, &sparse_array_texture_layer_count);
    }
    return {
        .size = std::bit_floor((u32)max.x()),
        .layer_count = (u32)max.z(),
        .sparse_texture_size = (u32)Math::max(sparse_texture_size, 0),
        .sparse_array_texture_layer_count = (u32)Math::max(sparse_array_texture_layer_count, 0),
        .buffer_storage_supported = ctx.isExtensionSupported<ARB::buffer_storage>(),
    };
}

Optional<Int> find_virtual_page_size_index(const Atlas& atlas)
{
    constexpr GLint max_count = 16;
    GLint count = 0, xs[max_count], ys[max_count], zs[max_count];
    glGetInternalformativ(GL_TEXTURE_2D_ARRAY, GL_RGBA8, GL_NUM_VIRTUAL_PAGE_SIZES_ARB, 1, &count);
    count = Math::clamp(count, 0, max_count);
    glGetInternalformativ(GL_TEXTURE_2D_ARRAY, GL_RGBA8, GL_VIRTUAL_PAGE_SIZE_X_ARB, count, xs);
    glGetInternalformativ(GL_TEXTURE_2D_ARRAY, GL_RGBA8, GL_VIRTUAL_PAGE_SIZE_Y_ARB, count, ys);
    glGetInternalformativ(GL_TEXTURE_2D_ARRAY, GL_RGBA8, GL_VIRTUAL_PAGE_SIZE_Z_ARB, count, zs);
    for (Int i = 0; i < count; i++)
        if (xs[i] > 0 && ys[i] > 0 && zs[i] == 1 &&
            atlas.layer_width % (u32)xs[i] == 0 && atlas.layer_height % (u32)ys[i] == 0)
            return i;
    return {};
}

bool can_make_sparse_texture(const Atlas& atlas, u32 layer_count)
{
    fm_debug_assert(atlas.limits.size > 0);
    const auto sparse_texture_layer_count =
        Math::min(atlas.limits.layer_count, atlas.limits.sparse_array_texture_layer_count);
    return layer_count > 0 && layer_count <= sparse_texture_layer_count &&
           Math::max(atlas.layer_width, atlas.layer_height) <= atlas.limits.sparse_texture_size &&
           find_virtual_page_size_index(atlas);
}

void Atlas::Limits::ensure_filled()
{
    if (!size)
        *this = get_texture_limits();
}

u32 get_next_layer_count(u32 layer_count, const Atlas& A)
{
    // Growth policy: double while VRAM footprint is modest, switch to 1.25x
    // once the atlas crosses 1 GiB
    const auto cur_bytes = (u64)layer_count * (u64)A.layer_width * (u64)A.layer_height * 4;
    constexpr u64 gibibyte = 1 << 30;

    if (layer_count == 0)
    {
        return 1;
    }
    else if (cur_bytes >= gibibyte)
    {
        auto new_size = layer_count + (layer_count + 3) / 4;
        fm_assert(new_size > layer_count);
        return new_size;
    }
    else
    {
        return layer_count << 1;
    }
}

void grow_texture(Atlas& atlas, u32 new_layer_count)
{
    fm_assert(atlas.layer_width > 0 && atlas.layer_height > 0);
    fm_assert(new_layer_count > atlas.texture_layer_count && new_layer_count >= atlas.layers.size());

    const auto layer_size = Vector2i{(Int)atlas.layer_width, (Int)atlas.layer_height};
    const auto layer_bytes = (usize)atlas.layer_width * atlas.layer_height * 4;
    atlas.limits.ensure_filled();

    if (atlas.layer_copy == layer_copy_mode::sparse_texture && new_layer_count <= atlas.texture_layer_capacity)
    {
        const with_gl_errors scope{gl_error_policy::error};
        commit_sparse_texture_layers(atlas.texture, layer_size, atlas.texture_layer_count, new_layer_count);
        clear_layers_to_magenta(atlas.texture, layer_size, atlas.texture_layer_count, new_layer_count);
    }
    else
    {
        if (atlas.layer_copy == layer_copy_mode::sparse_texture && !can_make_sparse_texture(atlas, new_layer_count))
            atlas.layer_copy = layer_copy_mode::fallback;

        const auto layers_per_pixel_buffer =
            (u32)Math::max(usize{1}, atlas.limits.max_pixel_buffer_bytes / layer_bytes);
        Array<GL::Buffer> used_layer_pixel_buffers;
        if (atlas.layer_copy == layer_copy_mode::pixel_buffer_readback && atlas.used_layers > 0)
        {
            used_layer_pixel_buffers = read_layers_into_pixel_buffers(atlas.texture, layer_size, atlas.used_layers,
                                                                      layers_per_pixel_buffer,
                                                                      atlas.limits.buffer_storage_supported);
            if (!used_layer_pixel_buffers.isEmpty())
                atlas.texture = GL::Texture2DArray{NoCreate};
            else
                atlas.layer_copy = layer_copy_mode::readback;
        }

        Array<char> used_layer_pixels;
        if (atlas.layer_copy == layer_copy_mode::readback && atlas.used_layers > 0)
        {
            used_layer_pixels = Array<char>{NoInit, layer_bytes * atlas.used_layers};
            for (u32 i = 0; i < atlas.used_layers; i++)
                read_layer_pixels(atlas.texture, (Int)i, {{}, layer_size},
                                  MutableImageView2D{PixelFormat::RGBA8Unorm, layer_size,
                                                     used_layer_pixels.sliceSize(i * layer_bytes, layer_bytes)});
            atlas.texture = GL::Texture2DArray{NoCreate};
        }

        // Texture storage is immutable, so growing the atlas requires a new texture.
        GL::Texture2DArray new_texture{NoCreate};
        auto new_capacity = new_layer_count;
        {
            const with_gl_errors scope{gl_error_policy::error};
            if (atlas.layer_copy == layer_copy_mode::sparse_texture)
            {
                const auto sparse_texture_layer_count =
                    Math::min(atlas.limits.layer_count, atlas.limits.sparse_array_texture_layer_count);
                new_texture = make_sparse_texture(layer_size, sparse_texture_layer_count,
                                                  *find_virtual_page_size_index(atlas));
                if (new_texture.id())
                {
                    commit_sparse_texture_layers(new_texture, layer_size, 0, new_layer_count);
                    new_capacity = sparse_texture_layer_count;
                }
                else
                    atlas.layer_copy = layer_copy_mode::fallback;
            }
            if (!new_texture.id())
            {
                new_texture = GL::Texture2DArray{};
                fm_assert(fm_check_glcall(new_texture.setStorage(1, GL::TextureFormat::RGBA8,
                                                                 {layer_size, (Int)new_layer_count})).second());
            }
            new_texture.setMagnificationFilter(GL::SamplerFilter::Nearest)
                       .setMinificationFilter(GL::SamplerFilter::Linear)
                       .setWrapping(GL::SamplerWrapping::ClampToEdge);

            clear_layers_to_magenta(new_texture, layer_size, atlas.used_layers, new_layer_count);

            if (!used_layer_pixel_buffers.isEmpty())
            {
                for (u32 j = 0; j < (u32)used_layer_pixel_buffers.size(); j++)
                {
                    const u32 first = j * layers_per_pixel_buffer;
                    const u32 num_layers = Math::min(layers_per_pixel_buffer, atlas.used_layers - first);
                    GL::BufferImage3D image{PixelFormat::RGBA8Unorm, {layer_size, (Int)num_layers},
                                            move(used_layer_pixel_buffers[j]), layer_bytes * num_layers};
                    new_texture.setSubImage(0, {0, 0, (Int)first}, image);
                }
            }
            else if (!used_layer_pixels.isEmpty())
            {
                for (u32 i = 0; i < atlas.used_layers; i++)
                    new_texture.setSubImage(0, {0, 0, (Int)i},
                                            ImageView3D{PixelFormat::RGBA8Unorm, {layer_size, 1},
                                                        used_layer_pixels.sliceSize(i * layer_bytes, layer_bytes)});
            }
            else if (atlas.used_layers > 0)
            {
                const framebuffer_saver saver;
                const auto rect = Range2Di{{}, layer_size};
                constexpr auto attachment = GL::Framebuffer::ColorAttachment{0};
                GL::Framebuffer src_fb{rect}, dst_fb{rect};
                for (Int i = 0; i < (Int)atlas.used_layers; i++)
                {
                    src_fb.attachTextureLayer(attachment, atlas.texture, 0, i);
                    dst_fb.attachTextureLayer(attachment, new_texture, 0, i);
                    GL::Framebuffer::blit(src_fb, dst_fb, rect, GL::FramebufferBlit::Color);
                }
            }
        }

        atlas.texture = move(new_texture);
        atlas.texture_layer_capacity = new_capacity;
        atlas.texture_gen++;
    }
    atlas.texture_layer_count = new_layer_count;
}

void ensure_texture(Atlas& atlas)
{
    if (atlas.texture_layer_count >= atlas.layers.size())
        return;

    atlas.limits.ensure_filled();
    fm_assert(Math::max(atlas.layer_width, atlas.layer_height) <= atlas.limits.size);
    if (atlas.layers.size() > atlas.limits.layer_count) [[unlikely]]
        fm_abort("ensure_texture: %u layers > max %u",
                 (unsigned)atlas.layers.size(), (unsigned)atlas.limits.layer_count);

    const auto num_layers = (u32)atlas.layers.size();
    auto layer_count = atlas.texture_layer_count;
    // Growing sparse storage copies no layers, so it commits only the layers needed.
    if (atlas.layer_copy == layer_copy_mode::sparse_texture &&
        (num_layers <= atlas.texture_layer_capacity || can_make_sparse_texture(atlas, num_layers)))
        layer_count = num_layers;
    else
        while (layer_count < num_layers)
            layer_count = get_next_layer_count(layer_count, atlas);
    grow_texture(atlas, Math::min(layer_count, atlas.limits.layer_count));
}

void free_atlas(Atlas& atlas)
{
    atlas.texture = GL::Texture2DArray{NoCreate};
    atlas.layers = {};
    for (auto& hc : atlas.height_classes)
        arrayClear(hc.shelves);
    atlas.pending = {};
    atlas.staging = {};
    atlas.texture_layer_count = 0;
    atlas.texture_layer_capacity = 0;
    atlas.used_layers = 0;
}

Shelf* alloc_new_shelf(Atlas& atlas, u32 height)
{
    fm_assert(height > 0 && height <= max_sprite_xy);

    const u32 hc_idx = height_class_index(height);
    const u32 q_height = height_classes[hc_idx];
    fm_assert(q_height <= atlas.layer_height);
    HeightClass& hc = atlas.height_classes[hc_idx];

    // scan existing layers for one with vertical room for q_height.
    auto layer_idx = (u32)-1;
    for (const auto& [i, layer] : enumerate(atlas.layers))
        if (atlas.layer_height - layer.next_y >= q_height)
        {
            layer_idx = i;
            break;
        }

    if (layer_idx == (u32)-1)
    {
        // No layer has room — must create a new Layer.
        arrayReserve(atlas.layers, 16);
        arrayAppend(atlas.layers, InPlaceInit);
        layer_idx = (u32)(atlas.layers.size() - 1);
    }

    // place the new Shelf at the layer's vertical watermark and advance it.
    Layer& L = atlas.layers[layer_idx];
    arrayReserve(L.shelves, 16);
    Shelf* shelf_ptr = &*arrayAppend(L.shelves, InPlaceInit, InPlaceInit, Shelf {
        .y = L.next_y,
        .next_x = 0,
        .layer = layer_idx,
    });
    L.next_y += q_height;
    arrayReserve(hc.shelves, 16);
    arrayAppend(hc.shelves, shelf_ptr);

    return shelf_ptr;
}

Sprite* alloc_sprite(Atlas& atlas, u32 w, u32 h, bool allow_rotate)
{
    fm_soft_assert(w > 0 && h > 0);
    const auto max_allowed_size = Math::min({atlas.layer_width, atlas.layer_height, max_sprite_xy});
    if (Math::max(w, h) > max_allowed_size) [[unlikely]]
        fm_throw("alloc_sprite: {{{},{}}} > min(L_w:{}, L_h:{}, M:{})",
                 w, h, atlas.layer_width, atlas.layer_height, max_sprite_xy);

    const u32 unrotated_loss = w * (quantize_height(h) - h), rotated_loss = h * (quantize_height(w) - w);
    bool is_rotated = allow_rotate && rotated_loss < unrotated_loss;
    Shelf* shelf = find_shelf_with_space(atlas, is_rotated ? h : w, is_rotated ? w : h);
    if (!shelf && allow_rotate)
    {
        shelf = find_shelf_with_space(atlas, is_rotated ? w : h, is_rotated ? h : w);
        if (shelf)
            is_rotated = !is_rotated;
    }
    if (!shelf)
    {
        auto taller = find_taller_shelf_with_space(atlas, is_rotated ? h : w, is_rotated ? w : h);
        if (allow_rotate)
        {
            const auto other = find_taller_shelf_with_space(atlas, is_rotated ? w : h, is_rotated ? h : w);
            if (other.shelf && (!taller.shelf || other.loss < taller.loss))
            {
                taller = other;
                is_rotated = !is_rotated;
            }
        }
        shelf = taller.shelf;
    }
    if (!shelf)
    {
        is_rotated = allow_rotate && h > w;
        shelf = alloc_new_shelf(atlas, is_rotated ? w : h);
    }

    const u32 rect_w = is_rotated ? h : w;
    const u32 x = shelf->next_x;
    shelf->next_x = x + rect_w;

    arrayReserve(shelf->sprites, 16);
    auto& sp = arrayAppend(shelf->sprites, InPlaceInit, InPlaceInit, Sprite {
        .x = x,
        .y = shelf->y,
        .layer = shelf->layer,
        .width = w,
        .height = h,
        .is_rotated = is_rotated,
    });
    return &*sp;
}

void stage_sprite(Atlas& atlas, const Sprite& sprite, const ImageView2D& pixels)
{
    const auto size = rect_size(sprite);
    const auto offset = atlas.staging.size();
    const auto bytes = (usize)size.x() * size.y() * pixels.pixelSize();
    fm_assert(offset + bytes <= limits<u32>::max);
    arrayReserve(atlas.staging, 1 << 20);
    copy_to_rect(arrayAppend(atlas.staging, NoInit, bytes).data(), sprite, pixels);
    arrayReserve(atlas.pending, 64);
    arrayAppend(atlas.pending, PendingSprite{ &sprite, pixels.format(), (u32)offset });
}

void flush(Atlas& atlas)
{
    ensure_texture(atlas);
    {
        const with_gl_errors scope{gl_error_policy::error};
        for (auto i = 0u; i < (u32)atlas.pending.size(); i++)
        {
            const auto& p = atlas.pending[i];
            const auto end = i + 1 < atlas.pending.size() ? (usize)atlas.pending[i + 1].staging_offset
                                                           : atlas.staging.size();
            upload_rect(atlas, *p.sprite, p.format, atlas.staging.slice(p.staging_offset, end));
        }
    }
    // Keeps the capacity stage_sprite() reserved, so the next add() doesn't allocate it again.
    arrayClear(atlas.pending);
    arrayClear(atlas.staging);
    atlas.used_layers = (u32)atlas.layers.size();
}

std::array<Vector3, 4> texcoords_for_subrect(const Atlas& atlas, const Sprite& sprite,
                                             Vector2ui sub_offset, Vector2ui sub_size,
                                             bool mirror)
{
    using vec2ull = Math::Vector2<u64>;
    fm_assert(sub_size.min() > 0);
    fm_assert(vec2ull(sub_offset) + vec2ull(sub_size) <= vec2ull{sprite.width, sprite.height});

    const auto layer = (float)sprite.layer;
    const auto atlas_w = (float)atlas.layer_width, atlas_h = (float)atlas.layer_height;

    float u0, u1, v0, v1;
    if (!sprite.is_rotated)
    {
        const float left      = (float)sprite.x + (float)sub_offset.x();
        const float right     = left + (float)sub_size.x();
        const float top_gl    = (float)sprite.y + (float)sprite.height - (float)sub_offset.y();
        const float bottom_gl = top_gl - (float)sub_size.y();
        u0 = (left + 0.5f) / atlas_w;
        u1 = (right - 0.5f) / atlas_w;
        v0 = (bottom_gl + 0.5f) / atlas_h; // low v (GL bottom-up)
        v1 = (top_gl - 0.5f) / atlas_h;    // high v
    }
    else
    {
        const float atlas_u_lo = (float)sprite.x + (float)sub_offset.y();
        const float atlas_u_hi = atlas_u_lo + (float)sub_size.y();
        const float atlas_v_lo = (float)sprite.y + (float)sub_offset.x();
        const float atlas_v_hi = atlas_v_lo + (float)sub_size.x();
        u0 = (atlas_u_lo + 0.5f) / atlas_w;
        u1 = (atlas_u_hi - 0.5f) / atlas_w;
        v0 = (atlas_v_lo + 0.5f) / atlas_h;
        v1 = (atlas_v_hi - 0.5f) / atlas_h;
    }

    // Vertex order per Quads::quad convention: {0=BR, 1=TR, 2=BL, 3=TL}.
    Vector2 corners[4] = {
        {u1, v0}, // 0: BR
        {u1, v1}, // 1: TR
        {u0, v0}, // 2: BL
        {u0, v1}, // 3: TL
    };

    // Mirror and rotation permutations over vertex order:
    // - normal:        {0,1,2,3}
    // - rotated only:  {1,3,0,2}  — CCW rotation of vertex→corner mapping
    // - mirrored only: {2,3,0,1}  — horizontal flip (vertex i <-> i XOR 2)
    // - both:          {0,2,1,3}  — mirror applied on top of rotated
    constexpr struct {
        u8 a:2, b:2, c:2, d:2;
    } perm[4] = {
        {0, 1, 2, 3},
        {1, 3, 0, 2},
        {2, 3, 0, 1},
        {0, 2, 1, 3},
    };
    const auto [a, b, c, d] = perm[(u32)mirror << 1 | (u32)sprite.is_rotated];

    return {{
        {corners[a], layer},
        {corners[b], layer},
        {corners[c], layer},
        {corners[d], layer},
    }};
}

std::array<Vector3, 4> texcoords_for_sprite(const Atlas& atlas, const Sprite& sprite, bool mirror)
{
    return texcoords_for_subrect(atlas, sprite, {0, 0}, {sprite.width, sprite.height}, mirror);
}

void read_layer_pixels(GL::Texture2DArray& texture, Int layer, Range2Di rect, const MutableImageView2D& out)
{
    fm_assert(out.size() == rect.size());
    const with_gl_errors scope{gl_error_policy::error};
    // NVIDIA's first glGetTextureSubImage on a texture is slower than a framebuffer read. On a sparse texture it
    // takes time proportional to the texture's virtual size.
    const framebuffer_saver saver;
    GL::Framebuffer fb{rect};
    fb.attachTextureLayer(GL::Framebuffer::ColorAttachment{0}, texture, 0, layer);
    fb.read(rect, out);
}

Image2D read_layer_pixels(GL::Texture2DArray& texture, Int layer, Range2Di rect)
{
    Image2D ret{PixelFormat::RGBA8Unorm, rect.size(), Array<char>{NoInit, (usize)rect.size().product() * 4}};
    read_layer_pixels(texture, layer, rect, ret);
    return ret;
}

GL::Texture2D copy_rect(Atlas& atlas, const Sprite& sprite)
{
    flush(atlas);
    const with_gl_errors scope{gl_error_policy::error};
    const auto size = Vector2i(rect_size(sprite));
    const auto layer_size = Vector2i{(Int)atlas.layer_width, (Int)atlas.layer_height};
    GL::Texture2D ret;
    fm_assert(fm_check_glcall(ret.setStorage(1, GL::TextureFormat::RGBA8, size)).second());
    ret.setWrapping(GL::SamplerWrapping::ClampToEdge)
       .setMagnificationFilter(GL::SamplerFilter::Nearest)
       .setMinificationFilter(GL::SamplerFilter::Nearest);
    {
        const framebuffer_saver saver;
        constexpr auto attachment = GL::Framebuffer::ColorAttachment{0};
        GL::Framebuffer src_fb{{{}, layer_size}}, dst_fb{{{}, size}};
        src_fb.attachTextureLayer(attachment, atlas.texture, 0, (Int)sprite.layer);
        dst_fb.attachTexture(attachment, ret, 0);
        GL::Framebuffer::blit(src_fb, dst_fb, Range2Di::fromSize({(Int)sprite.x, (Int)sprite.y}, size), {{}, size},
                              GL::FramebufferBlit::Color, GL::FramebufferBlitFilter::Nearest);
    }
    return ret;
}

void dump_atlas(Atlas& atlas, StringView out_filename)
{
    fm_assert(!atlas.layers.isEmpty());
    flush(atlas);

    // stb’s buffers grow by 2m + 1, so before an `int32_t` overflow the capacity is about 1.5 GiB.
    constexpr Int pixels_per_chunk = 1 << 28;
    const auto split = Path::splitExtension(out_filename);
    const auto base = split.first();
    const auto ext = split.second();
    const auto wʹ = (i64)atlas.layer_width, hʹ = (i64)atlas.layer_height;
    fm_assert(wʹ * hʹ <= pixels_per_chunk);
    const auto w = (Int)wʹ, h = (Int)hʹ;
    const auto n = (Int)atlas.used_layers;
    const auto layers_per_chunk = pixels_per_chunk / (w * h);

    PluginManager::Manager<Trade::AbstractImageConverter> mgr;
    auto conv = mgr.loadAndInstantiate("AnyImageConverter");
    fm_assert(conv);

    const Int n_chunks = (n + layers_per_chunk - 1) / layers_per_chunk;
    const auto layer_bytes = (usize)w * (usize)h * 4;
    Array<char> chunk{NoInit, layer_bytes * (usize)Math::min(layers_per_chunk, n)};
    for (Int c = 0; c < n_chunks; c++)
    {
        const Int first = c * layers_per_chunk;
        const Int count = Math::min(layers_per_chunk, n - first);

        for (Int i = 0; i < count; i++)
            read_layer_pixels(atlas.texture, first + i, {{}, {w, h}},
                              MutableImageView2D{PixelFormat::RGBA8Unorm, {w, h},
                                                 chunk.slice((usize)i * layer_bytes, (usize)(i + 1) * layer_bytes)});
        ImageView2D stitched{PixelFormat::RGBA8Unorm, {w, count * h}, chunk.prefix((usize)count * layer_bytes)};

        char path_buf[1024];
        const int written = std::snprintf(path_buf, sizeof(path_buf),
                                          "%.*s.%03d%.*s",
                                          (int)base.size(), base.data(), c,
                                          (int)ext.size(), ext.data());
        fm_assert(written > 0 && (usize)written < sizeof(path_buf));
        auto dst = StringView{path_buf, (usize)written, StringViewFlag::NullTerminated};
        if (!conv->convertToFile(stitched, dst))
            fm_throw("can't write atlas chunk {} to '{}'", c, dst);
    }
}

void dump_sprite(Atlas& atlas, const Sprite& sprite, StringView out_filename)
{
    // sprite.width/height always describe the ORIGINAL dims. When the packer
    // stores the sprite rotated, the atlas rect is transposed
    // (rect_w = orig_h, rect_h = orig_w); read the rect and un-rotate
    // before writing so the output matches what the caller expects.
    const auto orig_w = sprite.width;
    const auto orig_h = sprite.height;
    const bool rotated = sprite.is_rotated;
    const auto rect_w = rotated ? orig_h : orig_w;
    const auto rect_h = rotated ? orig_w : orig_h;
    const Int layer = (Int)sprite.layer;

    flush(atlas);
    auto img = read_layer_pixels(atlas.texture, layer, Range2Di::fromSize({(Int)sprite.x, (Int)sprite.y}, {(Int)rect_w, (Int)rect_h}));

    const Vector2i dst_size{(Int)orig_w, (Int)orig_h};
    Array<u32> unrotated;
    ArrayView<const char> pixels = img.data();
    if (rotated)
    {
        // Invert copy_to_rect's 90° CCW.
        unrotated = Array<u32>{NoInit, (usize)orig_w * (usize)orig_h};
        const auto* rect_pixels = reinterpret_cast<const u32*>(img.data().data());
        for (u32 y_o = 0; y_o < orig_h; y_o++)
        {
            u32* dst_row = unrotated.data() + (usize)y_o * (usize)orig_w;
            const u32 rect_col = orig_h - 1 - y_o;
            for (u32 x_o = 0; x_o < orig_w; x_o++)
                dst_row[x_o] = rect_pixels[(usize)x_o * (usize)rect_w + (usize)rect_col];
        }
        pixels = {reinterpret_cast<const char*>(unrotated.data()),
                  unrotated.size() * sizeof(u32)};
    }

    ImageView2D view{img.format(), dst_size, pixels};

    PluginManager::Manager<Trade::AbstractImageConverter> mgr;
    auto conv = mgr.loadAndInstantiate("AnyImageConverter");
    fm_assert(conv);
    if (!conv->convertToFile(view, out_filename))
        fm_throw("can't write sprite dump to '{}'", out_filename);
}

} // namespace floormat::SpriteAtlas

namespace floormat {

using namespace floormat::SpriteAtlas;

sprite::sprite(const Sprite* s) : _s{s} { fm_assert(_s); }

u32 sprite::x()      const { return _s->x; }
u32 sprite::y()      const { return _s->y; }
u32 sprite::layer()  const { return _s->layer; }
u32 sprite::width()  const { return _s->width; }
u32 sprite::height() const { return _s->height; }
bool sprite::is_rotated() const { return _s->is_rotated; }
const Sprite* sprite::raw() const { return _s; }

sprite_atlas::sprite_atlas()
    : _atlas{InPlaceInit}
{
    // the first add() sets the layer size
}

sprite_atlas::sprite_atlas(u32 layer_width, u32 layer_height)
    : _atlas{InPlaceInit}
{
    _atlas->layer_width = layer_width;
    _atlas->layer_height = layer_height;
    fm_assert(!layer_height || layer_height >= max_sprite_xy || quantize_height(layer_height) == layer_height);
}

sprite_atlas::~sprite_atlas() noexcept = default;
sprite_atlas::sprite_atlas(sprite_atlas&&) noexcept = default;
sprite_atlas& sprite_atlas::operator=(sprite_atlas&&) noexcept = default;

u32 sprite_atlas::layer_width()  const { return _atlas->layer_width; }
u32 sprite_atlas::layer_height() const { return _atlas->layer_height; }
u32 sprite_atlas::layer_count()  const { return (u32)_atlas->layers.size(); }
u32 sprite_atlas::texture_gen()  const { return _atlas->texture_gen; }
void sprite_atlas::set_layer_copy(layer_copy_mode mode) { _atlas->layer_copy = mode; }
Atlas* sprite_atlas::raw() { return _atlas.get(); }
const Atlas* sprite_atlas::raw() const { return _atlas.get(); }

sprite sprite_atlas::add(const ImageView2D& pixels, bool allow_rotate)
{
    const auto format = pixels.format();
    fm_soft_assert(format == PixelFormat::RGBA8Unorm || format == PixelFormat::RGB8Unorm);
    auto image_size = Vector2ui(pixels.size());

    if (Math::min(_atlas->layer_width, _atlas->layer_height) == 0)
    {
        constexpr auto max_layer_size = Vector2ui{max_layer_width, max_layer_height};
        static_assert(max_layer_size >= Vector2ui{max_sprite_xy});
        auto layer_size = max_layer_size;
        if (GL::Context::hasCurrent()) [[likely]]
        {
            _atlas->limits.ensure_filled();
            layer_size = Math::min(layer_size, _atlas->limits.size);
        }
        if (!_atlas->layer_width)
            _atlas->layer_width  = layer_size.x();
        if (!_atlas->layer_height)
            _atlas->layer_height = layer_size.y();
    }

    auto* s = alloc_sprite(*_atlas, image_size.x(), image_size.y(), allow_rotate);
    stage_sprite(*_atlas, *s, pixels);
    return sprite{s};
}

std::array<Vector3, 4> sprite_atlas::texcoords_for(sprite s, bool mirror) const
{
    return texcoords_for_sprite(*_atlas, *s.raw(), mirror);
}

std::array<Vector3, 4> sprite_atlas::texcoords_for(sprite s, Vector2ui sub_offset, Vector2ui sub_size, bool mirror) const
{
    return texcoords_for_subrect(*_atlas, *s.raw(), sub_offset, sub_size, mirror);
}

GL::AbstractTexture& sprite_atlas::texture()
{
    if (!_atlas->pending.isEmpty() || _atlas->texture_layer_count < _atlas->layers.size()) [[unlikely]]
        flush(*_atlas);
    return _atlas->texture;
}

void sprite_atlas::reset()
{
    free_atlas(*_atlas);
}

void sprite_atlas::dump(StringView out_filename)
{
    dump_atlas(*_atlas, out_filename);
}

void sprite_atlas::dump_sprite(sprite s, StringView out_filename)
{
    SpriteAtlas::dump_sprite(*_atlas, *s.raw(), out_filename);
}

} // namespace floormat

#!/bin/sh
#
# Runs floormat's GL tests and editor scenes on Mesa (zink by default) with every extension outside core GL 3.3
# disabled. --all also runs once per extension that a Magnum or floormat code path depends on. Run with --help.

set -o pipefail
set -eu

case "${OS:-}" in
    Windows_NT) export PATH="$PATH:/usr/bin" ;;
    *) echo "fatal: Windows only, it runs the mesa-dist-win DLLs" >&2; exit 3 ;;
esac

self="$(basename -- "$0")"
cd -- "$(dirname -- "$0")"

mesa_version=26.2.6
# 26.2.6's MSVC package has no OpenGL DLLs, a known issue in its release notes.
mesa_archive="mesa3d-${mesa_version}-release-mingw.7z"
mesa_sha256=da0f71d9181b672c1918f3c342f11118c687f6059a46b16502845160733cc388
mesa_url="https://github.com/pal1000/mesa-dist-win/releases/download/${mesa_version}/${mesa_archive}"
mesa_dir="build/mesa-dist-win-${mesa_version}"

. contrib/gl33-proxy/gl-xml.sh

# Core in 3.3 but too old for Magnum's extension table.
pre_gl30_core="GL_EXT_texture3D GL_EXT_bgra GL_EXT_packed_pixels GL_EXT_draw_range_elements
GL_SGIS_texture_edge_clamp GL_SGIS_texture_lod GL_ARB_texture_compression GL_ARB_texture_cube_map
GL_ARB_multisample GL_ARB_multitexture GL_ARB_texture_border_clamp GL_SGIS_generate_mipmap GL_ARB_depth_texture
GL_ARB_shadow GL_EXT_multi_draw_arrays GL_ARB_point_parameters GL_EXT_blend_func_separate GL_EXT_stencil_wrap
GL_EXT_texture_lod_bias GL_ARB_texture_mirrored_repeat GL_EXT_blend_color GL_EXT_blend_minmax GL_EXT_blend_subtract
GL_ARB_vertex_buffer_object GL_ARB_occlusion_query GL_EXT_shadow_funcs GL_ARB_shader_objects GL_ARB_vertex_shader
GL_ARB_fragment_shader GL_ARB_shading_language_100 GL_ARB_draw_buffers GL_ARB_texture_non_power_of_two
GL_ARB_point_sprite GL_EXT_blend_equation_separate GL_EXT_stencil_two_side GL_ARB_pixel_buffer_object
GL_EXT_pixel_buffer_object GL_EXT_texture_sRGB GL_ARB_texture_compression_rgtc GL_EXT_framebuffer_object
GL_EXT_framebuffer_blit GL_EXT_framebuffer_multisample GL_EXT_packed_depth_stencil GL_EXT_framebuffer_sRGB
GL_EXT_draw_instanced GL_EXT_texture_buffer_object GL_EXT_provoking_vertex GL_EXT_vertex_array_bgra GL_NV_depth_clamp
GL_EXT_texture_swizzle GL_EXT_timer_query"

# Mesa 26.2.6 gives these the enable flag of a core 3.3 extension, so its override can't disable them alone.
mesa_core_aliases="GL_ATI_blend_equation_separate GL_ATI_texture_float GL_MESA_texture_signed_rgba
GL_ARB_texture_storage_multisample"

editor_scenes=all

# Each gets a run that keeps all of its extensions. The editor's palette uses texture views only with both.
extension_combinations="GL_ARB_texture_storage+GL_ARB_texture_view"

# Each line is an extension, then the commands and enums that the specs, unlike gl.xml, remove without it.
# The DSA spec defines glVertexArrayVertexBuffers through glVertexArrayVertexBuffer, so it's removed with that one.
spec_dependencies="GL_ARB_transform_feedback2 glCreateTransformFeedbacks glGetTransformFeedbackiv
GL_ARB_transform_feedback2 glGetTransformFeedbacki_v glGetTransformFeedbacki64_v glTextureBuffer
GL_ARB_vertex_attrib_64bit glVertexArrayAttribLFormat
GL_ARB_separate_shader_objects glCreateProgramPipelines GL_PROGRAM_PIPELINE
GL_ARB_texture_storage glTextureStorage1D glTextureStorage2D glTextureStorage3D
GL_ARB_texture_storage_multisample glTextureStorage2DMultisample glTextureStorage3DMultisample
GL_ARB_vertex_attrib_binding glVertexArrayVertexBuffer glVertexArrayAttribFormat glVertexArrayAttribIFormat
GL_ARB_vertex_attrib_binding glVertexArrayAttribLFormat glVertexArrayAttribBinding glVertexArrayBindingDivisor
GL_ARB_vertex_attrib_binding glBindVertexBuffers glVertexArrayVertexBuffers
GL_ARB_invalidate_subdata glInvalidateNamedFramebufferData glInvalidateNamedFramebufferSubData
GL_ARB_texture_buffer_range glTextureBufferRange
GL_ARB_clear_buffer_object glClearNamedBufferData glClearNamedBufferSubData
GL_ARB_clear_texture glClearNamedBufferData glClearNamedBufferSubData
GL_ARB_framebuffer_no_attachments glNamedFramebufferParameteri glGetNamedFramebufferParameteriv
GL_ARB_buffer_storage glNamedBufferStorage
GL_ARB_multi_bind glVertexArrayVertexBuffers
GL_ARB_query_buffer_object glGetQueryBufferObjectiv glGetQueryBufferObjectuiv glGetQueryBufferObjecti64v
GL_ARB_query_buffer_object glGetQueryBufferObjectui64v
GL_ARB_gpu_shader_fp64 glProgramUniform1d glProgramUniform1dv glProgramUniform2d glProgramUniform2dv
GL_ARB_gpu_shader_fp64 glProgramUniform3d glProgramUniform3dv glProgramUniform4d glProgramUniform4dv
GL_ARB_gpu_shader_fp64 glProgramUniformMatrix2dv glProgramUniformMatrix2x3dv glProgramUniformMatrix2x4dv
GL_ARB_gpu_shader_fp64 glProgramUniformMatrix3dv glProgramUniformMatrix3x2dv glProgramUniformMatrix3x4dv
GL_ARB_gpu_shader_fp64 glProgramUniformMatrix4dv glProgramUniformMatrix4x2dv glProgramUniformMatrix4x3dv"

driver=zink
build_dir=build/clang-asan
only=
all=
editor_only=

usage() {
    echo "usage: ${self} [--zink|--llvmpipe|--softpipe] [--only=none|GL_X[+GL_Y...]|--all] [--editor] [build-dir]" \
        "[-- editor-args...]" >&2
    echo "       runs Mesa zink on the GPU's Vulkan driver by default, or llvmpipe or softpipe on the CPU" >&2
    echo "       build-dir defaults to build/clang-asan and must be installed already" >&2
    echo "       --only=none, the default, runs with every extension outside core 3.3 disabled," >&2
    echo "       --only=GL_X+GL_Y with all but those" >&2
    echo "       --all runs none, then GL_X for each extension that changes a code path," >&2
    echo "       then each of: $extension_combinations" >&2
    echo "       --editor opens floormat-editor's window instead of running floormat-test and the editor's scenes" >&2
    echo "       editor-args go to floormat-editor after the script's own, which they override" >&2
    echo "       queried GL limits are the minimums of GL 3.3 and of the kept extensions; a call past one aborts" >&2
    exit 64
}

while test $# -gt 0; do
    case "$1" in
        --zink) driver=zink ;;
        --llvmpipe) driver=llvmpipe ;;
        --softpipe) driver=softpipe ;;
        --only=?*) only="${1#--only=}" ;;
        --all) all=1 ;;
        --editor) editor_only=1 ;;
        --) shift; break ;;
        -*) usage ;;
        *) build_dir="${1%/}" ;;
    esac
    shift
done
if test -n "$all" && { test -n "$only" || test -n "$editor_only"; }; then
    usage
fi
only="${only:-none}"

if ! test -f "$build_dir/install/bin/floormat-test.exe"; then
    echo "fatal: $build_dir/install/bin/floormat-test.exe not found, run ninja install first" >&2
    exit 1
fi
if test -e "$build_dir/install/bin/opengl32.dll"; then
    echo "fatal: $build_dir/install/bin/opengl32.dll would load instead of Mesa's" >&2
    exit 1
fi

runner=
if command -v nojit >/dev/null 2>&1; then
    runner=nojit
fi

download_mesa() {
    if test -f "$mesa_dir/x64/opengl32.dll" && test -f "$mesa_dir/x64/libgallium_wgl.dll"; then
        return 0
    fi
    mkdir -p -- "$mesa_dir"
    if ! test -f "$mesa_dir/$mesa_archive"; then
        echo "downloading $mesa_url" >&2
        curl -fsSL -o "$mesa_dir/$mesa_archive.part" -- "$mesa_url"
        mv -f -- "$mesa_dir/$mesa_archive.part" "$mesa_dir/$mesa_archive"
    fi
    if ! echo "$mesa_sha256 *$mesa_dir/$mesa_archive" | sha256sum -c --status; then
        echo "fatal: $mesa_dir/$mesa_archive doesn't match its SHA-256" >&2
        exit 1
    fi
    7z x -y -bso0 -bsp0 -o"$mesa_dir" -- "$mesa_dir/$mesa_archive" x64/opengl32.dll x64/libgallium_wgl.dll
}

download_mesa
download_gl_xml

run_dir="build/gl33-$(basename -- "$build_dir")"
rm -rf -- "$run_dir"
mkdir -p -- "$run_dir/logs" "$run_dir/allowed" "$run_dir/dlls"

export GALLIUM_DRIVER="$driver" LP_NUM_THREADS="$(nproc)" MESA_DEBUG=1
export MESA_GL_VERSION_OVERRIDE=3.3FC MESA_GLSL_VERSION_OVERRIDE=330
unset MESA_EXTENSION_OVERRIDE MAGNUM_DISABLE_EXTENSIONS FLOORMAT_GL33_ALLOWED_FUNCTIONS FLOORMAT_GL33_ALLOWED_ENUMS

# Runs the build's $2 with the DLLs in directory $1 loaded instead of System32's.
with_dll_dir() {
    local dir="$1" exe="$2"
    shift 2
    $runner contrib/gl33-proxy/with-dll-dir.exe "$(cygpath -aw "$dir")" "$(cygpath -aw "$build_dir/install/bin/$exe")" \
        "$@"
}

magnum_gl_info() {
    with_dll_dir "$mesa_dir/x64" magnum-gl-info.exe "$@"
}

# Reads Magnum's log on stdin and prints the extensions it lists under "Using optional features".
extensions_magnum_uses() {
    awk '/^Using optional features:/ { f = 1; next } f && /^ +GL_/ { print $1; next } { f = 0 }'
}

# The words of $1 that aren't words of $2, one per line.
set_difference() {
    printf '%s\n' $1 | awk -v set="$2" 'BEGIN { n = split(set, s); for (i = 1; i <= n; i++) skip[s[i]] = 1 }
                                         NF && !($1 in skip)' | sort -u
}

# The words of $1 that are words of $2, one per line.
set_intersection() {
    printf '%s\n' $1 | awk -v set="$2" 'BEGIN { n = split(set, s); for (i = 1; i <= n; i++) keep[s[i]] = 1 }
                                         NF && ($1 in keep)' | sort -u
}

# Prints the gl_functions_required_by or gl_enums_required_by lines on stdin, except those whose name needs
# an extension that the arguments lack, per $spec_dependencies.
drop_names_with_missing_dependencies() {
    dependencies="$spec_dependencies" awk -v kept="$*" '
        BEGIN { n = split(kept, k, " "); for (i = 1; i <= n; i++) have[k[i]] = 1
                n = split(ENVIRON["dependencies"], lines, "\n")
                for (i = 1; i <= n; i++) { m = split(lines[i], w, " ")
                                           if (!(w[1] in have)) for (j = 2; j <= m; j++) drop[w[j]] = 1 } }
        !($1 in drop)'
}

# Reads gl33-proxy's lines on stdin and appends the gl.xml names of the enum value each one reports.
append_enum_names() {
    awk 'NR == FNR { if (match($0, /<enum value="[^"]*"/)) { v = strtonum(substr($0, RSTART + 13, RLENGTH - 14))
                                                           match($0, /name="[^"]*"/)
                                                           names[v] = names[v] " " substr($0, RSTART + 6, RLENGTH - 7) }
                     next }
         match($0, / = 0x[0-9A-F]+,/) { print $0 " (" substr(names[strtonum(substr($0, RSTART + 3, RLENGTH - 4))], 2) ")" }' \
        "$gl_xml" -
}

info="$(magnum_gl_info --extension-strings)" || true
renderer="$(printf '%s\n' "$info" | sed -n 's/^Renderer: //p')"
if test "${renderer%% *}" != "$driver"; then
    echo "fatal: GALLIUM_DRIVER=$driver gave renderer '$renderer'" >&2
    exit 1
fi
advertised="$(printf '%s\n' "$info" | awk '/^Extension strings:/ { f = 1; next } f && /^ +GL_/ { print $1 }' | sort -u)"
used_by_magnum="$(printf '%s\n' "$info" | extensions_magnum_uses | sort -u)"

core="$(sed -nE 's/^ *_extension\( *[0-9]+, *([A-Z0-9]+), *([A-Za-z0-9_]+), *(\/\*[^*]*\*\/ *)?[A-Z0-9]+, *GL3[0-3]0\).*/GL_\1_\2/p' \
    external/magnum/src/Magnum/GL/Extensions.h) $pre_gl30_core"

magnum_disable="$(set_difference "$advertised" "$core")"
mesa_disable="$(set_difference "$magnum_disable" "$mesa_core_aliases")"

# Mesa can't disable an extension it implements unconditionally, and says so on stdout.
cannot_disable="$(MESA_EXTENSION_OVERRIDE="$(printf -- '-%s ' $mesa_disable)" magnum_gl_info 2>&1 |
    sed -nE "s/^Warning: extension '(GL_[A-Za-z0-9_]+)' cannot be disabled.*/\1/p" | sort -u)"
mesa_disable="$(set_difference "$mesa_disable" "$cannot_disable")"

leftover="$(MESA_EXTENSION_OVERRIDE="$(printf -- '-%s ' $mesa_disable)" \
    MAGNUM_DISABLE_EXTENSIONS="$(echo $magnum_disable)" magnum_gl_info | extensions_magnum_uses)"
leftover="$(set_difference "$leftover" "$core")"
if test -n "$leftover"; then
    echo "fatal: Magnum still uses extensions outside core 3.3:" $leftover >&2
    exit 1
fi

echo "$mesa_disable" > "$run_dir/mesa-disabled.txt"
echo "$magnum_disable" > "$run_dir/magnum-disabled.txt"

# Mesa drops what it can't disable from the extension string only, so contrib/gl33-proxy hides every function outside
# core 3.3 from wglGetProcAddress and reports the enums outside core 3.3 that core functions get.
cp -f -- contrib/gl33-proxy/opengl32.dll "$run_dir/dlls/"
cp -f -- "$mesa_dir/x64/opengl32.dll" "$run_dir/dlls/opengl32-mesa.dll"
ln -f -- "$mesa_dir/x64/libgallium_wgl.dll" "$run_dir/dlls/"
gl_functions_required_by $core_versions > "$run_dir/allowed/core.txt"
gl_enums_required_by $core_versions > "$run_dir/allowed/core-enums.txt"

checked_by_floormat="$(git grep -hoE 'isExtensionSupported<GL::Extensions::[A-Z0-9]+::[A-Za-z0-9_]+>' -- \
    '*.cpp' '*.hpp' '*.inl' | sed -E 's/.*::([A-Z0-9]+)::([A-Za-z0-9_]+)>/GL_\1_\2/')"
code_paths="$(set_intersection "$used_by_magnum $checked_by_floormat" "$magnum_disable")"

echo "renderer: $renderer"
echo "disabled: $(echo "$magnum_disable" | wc -l) of $(echo "$advertised" | wc -l) extensions"
echo "left enabled by Mesa, the proxy hides their functions and reports their enums given to core functions:" \
    $cannot_disable \
    $(set_intersection "$mesa_core_aliases" "$magnum_disable")
echo "code paths:" $code_paths

failed=0

# Runs floormat-test and the editor with every extension outside core 3.3 disabled, except those $1 joins with +.
# The other arguments go to the editor.
run_tests_with() {
    local name="$1" keep= test_status=0 editor_status=0 errors enums proxy=
    local test_log="$run_dir/logs/$name-test.log" editor_log="$run_dir/logs/$name-editor.log"
    shift
    if test "$name" != none; then
        keep="$(echo "$name" | tr + ' ')"
    fi

    gl_functions_required_by $keep | drop_names_with_missing_dependencies $keep |
        sort -u - "$run_dir/allowed/core.txt" > "$run_dir/allowed/$name.txt"
    export FLOORMAT_GL33_ALLOWED_FUNCTIONS="$(cygpath -m "$PWD/$run_dir/allowed/$name.txt")"
    gl_enums_required_by $keep | drop_names_with_missing_dependencies $keep |
        sort -u - "$run_dir/allowed/core-enums.txt" > "$run_dir/allowed/$name-enums.txt"
    export FLOORMAT_GL33_ALLOWED_ENUMS="$(cygpath -m "$PWD/$run_dir/allowed/$name-enums.txt")"
    export MESA_EXTENSION_OVERRIDE="$(printf -- '-%s ' $(set_difference "$mesa_disable" "$keep"))"
    export MAGNUM_DISABLE_EXTENSIONS="$(echo $(set_difference "$magnum_disable" "$keep"))"
    if test -n "$editor_only"; then
        with_dll_dir "$run_dir/dlls" floormat-editor.exe "$@" 2>&1 | tee "$editor_log" || editor_status=$?
        set -- "$editor_log"
    else
        MAGNUM_LOG=verbose with_dll_dir "$run_dir/dlls" floormat-test.exe --gl-only > "$test_log" 2>&1 ||
            test_status=$?
        FLOORMAT_PGO_DRIVER_SHORTEN_SCENES=1 with_dll_dir "$run_dir/dlls" floormat-editor.exe --window=minimized \
            --vsync=off --scenes="$editor_scenes" "$@" > "$editor_log" 2>&1 || editor_status=$?
        set -- "$test_log" "$editor_log"
    fi
    unset FLOORMAT_GL33_ALLOWED_FUNCTIONS FLOORMAT_GL33_ALLOWED_ENUMS MESA_EXTENSION_OVERRIDE MAGNUM_DISABLE_EXTENSIONS

    errors="$(cat "$@" | grep -c '^Mesa: error: ' || true)"
    enums="$(cat "$@" | grep -c '^gl33-proxy: gl[A-Za-z0-9_]* got ' || true)"
    for log; do
        if ! grep -q '^gl33-proxy: [0-9]* GL functions allowed by ' "$log"; then
            proxy=", gl33-proxy not loaded"
        fi
    done
    if test -n "$editor_only"; then
        echo "$name: floormat-editor exit $editor_status, $errors Mesa errors, $enums enums reported by" \
            "gl33-proxy$proxy"
    else
        echo "$name: floormat-test exit $test_status, floormat-editor exit $editor_status, $errors Mesa errors," \
            "$enums enums reported by gl33-proxy$proxy"
    fi
    grep -ho 'gl33-proxy: gl[A-Za-z0-9_]* was called.*' "$@" || true
    { grep -ho 'gl33-proxy: gl[A-Za-z0-9_]* got .*' "$@" || true; } | sort -u | append_enum_names
    if test "$test_status" != 0 || test "$editor_status" != 0 || test "$errors" != 0 || test "$enums" != 0 ||
       test -n "$proxy"; then
        failed=1
    fi
}

if test -n "$all"; then
    run_tests_with none "$@"
    for ext in $code_paths $extension_combinations; do
        run_tests_with "$ext" "$@"
    done
else
    grep -hoE 'gl33-proxy: gl[A-Za-z0-9_]* (passed|linked) .*' "$@" || true
    run_tests_with "$only" "$@"
fi

echo "logs: $run_dir/logs"
exit $failed

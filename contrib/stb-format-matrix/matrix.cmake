# Builds one plugin DLL per subset of compiled-in formats. Bit i of the mask
# means format i is *stripped*, so mask 0 is everything on.
#
# Driven through the generated configure.h and .conf, never through -DSTBI_NO_*
# on the command line -- the command-line route bypasses the mechanism the
# patch adds and would pass even if configure.h.cmake were broken.

set(MATRIX_TSV "")

# The converter's Test configure.h reads the importer's variables too, so both
# plugins get cleared each iteration, not just the one being swept
set(ALL_FORMATS BMP GIF HDR JPEG PIC PNG PNM PSD TGA)

function(fm_matrix plugin upper formats)
    list(LENGTH formats n)
    math(EXPR last "(1 << ${n}) - 1")

    set(tsv "")
    foreach(mask RANGE 0 ${last})
        # A leftover from the previous iteration would produce a variant that
        # does not match its own name, as configure_file reads current scope
        foreach(f IN LISTS ALL_FORMATS)
            unset(MAGNUM_STBIMAGEIMPORTER_NO_${f})
            unset(MAGNUM_STBIMAGECONVERTER_NO_${f})
        endforeach()
        foreach(f IN LISTS formats)
            set(_MAGNUM_${upper}_NO_${f} "")
        endforeach()

        set(i 0)
        set(enabled "")
        set(stripped 0)
        foreach(f IN LISTS formats)
            math(EXPR bit "(${mask} >> ${i}) & 1")
            if(bit)
                set(MAGNUM_${upper}_NO_${f} ON)
                set(_MAGNUM_${upper}_NO_${f} "#")
                math(EXPR stripped "${stripped} + 1")
            else()
                list(APPEND enabled ${f})
            endif()
            math(EXPR i "${i} + 1")
        endforeach()

        # Every format off leaves the converter with an empty Format enum and
        # an empty error-message list, which it rejects with #error
        if(stripped EQUAL n AND "${plugin}" STREQUAL "StbImageConverter")
            continue()
        endif()

        string(LENGTH "${last}" width)
        string(LENGTH "${mask}" have)
        set(id "${mask}")
        while(have LESS width)
            set(id "0${id}")
            math(EXPR have "${have} + 1")
        endwhile()

        set(dir ${CMAKE_BINARY_DIR}/variants/${plugin}/${id})
        set(gen ${CMAKE_BINARY_DIR}/gen/${plugin}/${id})

        configure_file(${MP}/src/MagnumPlugins/${plugin}/configure.h.cmake
                       ${gen}/MagnumPlugins/${plugin}/configure.h)
        configure_file(${MP}/src/MagnumPlugins/${plugin}/${plugin}.conf
                       ${dir}/${plugin}.conf NEWLINE_STYLE LF)

        set(target ${plugin}_${id})
        add_library(${target} MODULE ${MP}/src/MagnumPlugins/${plugin}/${plugin}.cpp)
        set_target_properties(${target} PROPERTIES
            PREFIX "" OUTPUT_NAME ${plugin}
            # CMake would otherwise define StbImageImporter_042_EXPORTS, and the
            # header's #ifdef StbImageImporter_EXPORTS would take the dllimport
            # branch while the class is being defined in that same TU
            DEFINE_SYMBOL ${plugin}_EXPORTS
            LIBRARY_OUTPUT_DIRECTORY ${dir}
            RUNTIME_OUTPUT_DIRECTORY ${dir})
        # CORRADE_PLUGIN_REGISTER expands to nothing without this, giving a DLL
        # with no exports that fails at load
        target_compile_definitions(${target} PRIVATE CORRADE_DYNAMIC_PLUGIN)
        target_compile_options(${target} PRIVATE -O1)
        target_include_directories(${target} BEFORE PRIVATE ${gen})
        target_include_directories(${target} SYSTEM PRIVATE ${MP}/src/external/stb)
        target_link_libraries(${target} PRIVATE Magnum::Trade)

        # Through the upstream Test/configure.h.cmake rather than a header of our
        # own, so the macros the guards read are the ones a real build produces
        set(testgen ${CMAKE_BINARY_DIR}/gen/${plugin}Test/${id})
        set(PNGIMPORTER_TEST_DIR ${MP}/src/MagnumPlugins/PngImporter/Test)
        set(JPEGIMPORTER_TEST_DIR ${MP}/src/MagnumPlugins/JpegImporter/Test)
        set(STBIMAGEIMPORTER_TEST_DIR ${MP}/src/MagnumPlugins/StbImageImporter/Test)
        set(STBIMAGECONVERTER_TEST_OUTPUT_DIR ${CMAKE_BINARY_DIR}/testout/${id})
        set(plugin_file ${dir}/${plugin}${CMAKE_SHARED_MODULE_SUFFIX})
        if("${plugin}" STREQUAL "StbImageImporter")
            set(STBIMAGEIMPORTER_PLUGIN_FILENAME ${plugin_file})
            set(test_deps ${target})
        else()
            set(STBIMAGECONVERTER_PLUGIN_FILENAME ${plugin_file})
            # Read back through an importer with everything left in, so a failure
            # is always the converter's
            set(STBIMAGEIMPORTER_PLUGIN_FILENAME
                ${CMAKE_BINARY_DIR}/variants/StbImageImporter/000/StbImageImporter${CMAKE_SHARED_MODULE_SUFFIX})
            set(test_deps ${target} StbImageImporter_000)
        endif()

        configure_file(${MP}/src/MagnumPlugins/${plugin}/Test/configure.h.cmake
                       ${testgen}/configure.h)

        add_executable(${target}_test ${MP}/src/MagnumPlugins/${plugin}/Test/${plugin}Test.cpp)
        target_include_directories(${target}_test PRIVATE ${testgen})
        target_compile_options(${target}_test PRIVATE -O1)
        target_link_libraries(${target}_test PRIVATE
            Corrade::TestSuite Magnum::DebugTools Magnum::Trade)
        add_dependencies(${target}_test ${test_deps})

        file(STRINGS ${dir}/${plugin}.conf lines REGEX "^provides=")
        set(provides "")
        foreach(line IN LISTS lines)
            string(REGEX REPLACE "^provides=" "" line "${line}")
            list(APPEND provides ${line})
        endforeach()

        string(REPLACE ";" "," enabled_csv "${enabled}")
        string(REPLACE ";" "," provides_csv "${provides}")
        string(APPEND tsv "${plugin}\t${id}\t${mask}\t${dir}\t${enabled_csv}\t${provides_csv}\t${dir}/${plugin}${CMAKE_SHARED_MODULE_SUFFIX}\n")
    endforeach()

    set(MATRIX_TSV "${MATRIX_TSV}${tsv}" PARENT_SCOPE)
endfunction()

fm_matrix(StbImageImporter STBIMAGEIMPORTER
    "JPEG;PNG;BMP;GIF;PSD;PIC;PNM;HDR;TGA")
fm_matrix(StbImageConverter STBIMAGECONVERTER
    "BMP;JPEG;HDR;PNG;TGA")

file(WRITE ${CMAKE_BINARY_DIR}/variants.tsv "${MATRIX_TSV}")

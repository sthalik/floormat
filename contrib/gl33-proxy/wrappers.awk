# Reads the core function names, the names opengl32.dll exports, then gl.xml. Writes a wrapper for each core
# function taking an enum, which passes its enums to check_enum() before calling the function.

function strip(s) {
    gsub(/<[^>]*>/, "", s)
    gsub(/^ +| +$/, "", s)
    return s
}

function attribute(tag, key) {
    if (!match(tag, " " key "=\"[^\"]*\""))
        return ""
    return substr(tag, RSTART + length(key) + 3, RLENGTH - length(key) - 4)
}

function emit(   i, checks, types, params, args) {
    for (i = 0; i < param_count; i++) {
        # Mesa checks a texture unit against its own limit, and units past GL_TEXTURE31 have no enum.
        if (type[i] == "GLenum" && group[i] != "TextureUnit")
            checks = checks sprintf("    check_enum(\"%s\", \"%s\", p%d);\n", name, param[i], i)
        else if (type[i] == "GLint" && group[i] == "InternalFormat")
            checks = checks sprintf("    check_enum(\"%s\", \"%s\", (GLenum)p%d);\n", name, param[i], i)
        else if (type[i] == "const GLenum *" && length_of[i] in index_of)
            checks = checks sprintf("    for (GLsizei i = 0; p%d && i < p%d; i++)\n        check_enum(\"%s\", \"%s\", p%d[i]);\n",
                                    i, index_of[length_of[i]], name, param[i], i)
        types = types (i ? ", " : "") type[i]
        params = params (i ? ", " : "") type[i] " p" i
        args = args (i ? ", " : "") "p" i
    }
    if (checks == "")
        return
    printf "static %s (WINAPI *real_%s)(%s);\n", result, name, types
    printf "%s WINAPI wrap_%s(%s)\n{\n", result, name, params
    if (name in exported)
        printf "    ensure_init();\n"
    printf "%s    %sreal_%s(%s);\n}\n\n", checks, result == "void" ? "" : "return ", name, args
    table = table sprintf("    { \"%s\", (PROC)(void*)wrap_%s, (void*)&real_%s },\n", name, name, name)
    if (name in exported)
        print name > exported_wrappers
}

FILENAME == ARGV[1] { core[$1] = 1; next }
FILENAME == ARGV[2] { exported[$1] = 1; next }

/<command[ >]/ && !/<command name=/ { in_command = 1; param_count = 0; delete index_of; next }

in_command && /<proto[ >]/ {
    s = $0; sub(/.*<proto[^>]*>/, "", s); sub(/<\/proto>.*/, "", s)
    name = s; sub(/.*<name>/, "", name); sub(/<\/name>.*/, "", name)
    result = s; sub(/<name>.*/, "", result); result = strip(result)
    next
}

in_command && /<param[ >]/ {
    tag = $0; sub(/^[^<]*<param/, "", tag); sub(/>.*/, "", tag)
    s = $0; sub(/.*<param[^>]*>/, "", s); sub(/<\/param>.*/, "", s)
    param[param_count] = s; sub(/.*<name>/, "", param[param_count]); sub(/<\/name>.*/, "", param[param_count])
    type[param_count] = s; sub(/<name>.*/, "", type[param_count]); type[param_count] = strip(type[param_count])
    group[param_count] = attribute(tag, "group")
    length_of[param_count] = attribute(tag, "len")
    index_of[param[param_count]] = param_count
    param_count++
    next
}

in_command && /<\/command>/ {
    in_command = 0
    if (name in core)
        emit()
}

END { printf "static struct wrapper wrappers[] = {\n%s};\n", table }

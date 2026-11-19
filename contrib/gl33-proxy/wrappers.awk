# Reads the core function names, the names opengl32.dll exports, the hook names, then gl.xml. Wraps each core
# function taking an enum, and each function with a hook or taking a vertex attribute index or a mip level.

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

function emit(   i, is_core, checks, types, params, args, call) {
    is_core = name in core
    for (i = 0; i < param_count; i++) {
        # Mesa checks a texture unit against its own limit, and units past GL_TEXTURE31 have no enum.
        if (is_core && type[i] == "GLenum" && group[i] != "TextureUnit")
            checks = checks sprintf("    check_enum(\"%s\", \"%s\", p%d);\n", name, param[i], i)
        else if (is_core && type[i] == "GLint" && group[i] == "InternalFormat")
            checks = checks sprintf("    check_enum(\"%s\", \"%s\", (GLenum)p%d);\n", name, param[i], i)
        else if (is_core && type[i] == "const GLenum *" && length_of[i] in index_of)
            checks = checks sprintf("    for (GLsizei i = 0; p%d && i < p%d; i++)\n        check_enum(\"%s\", \"%s\", p%d[i]);\n",
                                    i, index_of[length_of[i]], name, param[i], i)
        types = types (i ? ", " : "") type[i]
        params = params (i ? ", " : "") type[i] " p" i
        args = args (i ? ", " : "") "p" i
    }
    if (name ~ /^gl(Get)?VertexAttrib|^glVertexArrayAttrib|^gl(Enable|Disable)Vertex(AttribArray|ArrayAttrib)$/ ||
        name ~ /^glBindAttribLocation$|^glGetVertexArrayIndexed/)
        for (i = 0; i < param_count && name !~ /[A-Z][A-Z]+$/; i++)
            if (param[i] == "index" || param[i] == "attribindex")
                checks = checks sprintf("    check_below(\"%s\", \"%s\", p%d, GL_MAX_VERTEX_ATTRIBS);\n",
                                        name, param[i], i)
    if ("level" in index_of && name !~ /[A-Z][A-Z]+$/) {
        if ("textarget" in index_of)
            checks = checks sprintf("    if (p%d)\n        check_level(\"%s\", p%d, p%d);\n", index_of["texture"],
                                    name, index_of["textarget"], index_of["level"])
        else if ("texture" in index_of)
            checks = checks sprintf("    if (p%d)\n        check_level(\"%s\", texture_target(p%d), p%d);\n",
                                    index_of["texture"], name, index_of["texture"], index_of["level"])
        else if ("target" in index_of)
            checks = checks sprintf("    check_level(\"%s\", p%d, p%d);\n", name, index_of["target"],
                                    index_of["level"])
    }
    if ("bindingindex" in index_of && name !~ /[A-Z][A-Z]+$/)
        checks = checks sprintf("    check_below(\"%s\", \"bindingindex\", p%d, GL_MAX_VERTEX_ATTRIB_BINDINGS);\n",
                                name, index_of["bindingindex"])
    if ("relativeoffset" in index_of && name !~ /[A-Z][A-Z]+$/)
        checks = checks sprintf("    check_at_most(\"%s\", \"relativeoffset\", p%d, " \
                                "GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET);\n", name, index_of["relativeoffset"])
    if (checks == "" && !(name in pre) && !(name in post))
        return
    if (param_count == 0)
        types = params = "void"
    printf "static %s (WINAPI *real_%s)(%s);\n", result, name, types
    printf "%s WINAPI wrap_%s(%s)\n{\n", result, name, params
    if (name in exported)
        printf "    ensure_init();\n"
    printf "%s", checks
    if (name in pre)
        printf "    pre_%s(%s);\n", name, args
    call = sprintf("real_%s(%s)", name, args)
    if (!(name in post))
        printf "    %s%s;\n", result == "void" ? "" : "return ", call
    else if (result == "void")
        printf "    %s;\n    post_%s(%s);\n", call, name, args
    else
        printf "    %s result = %s;\n    post_%s(%s);\n    return result;\n", result, call, name, args
    printf "}\n\n"
    table = table sprintf("    { \"%s\", (PROC)(void*)wrap_%s, (void*)&real_%s },\n", name, name, name)
    wrapped[name] = 1
    if (name in exported)
        print name > exported_wrappers
}

FILENAME == ARGV[1] { core[$1] = 1; next }
FILENAME == ARGV[2] { exported[$1] = 1; next }
FILENAME == ARGV[3] { if (sub(/^pre_/, "")) pre[$0] = 1; else if (sub(/^post_/, "")) post[$0] = 1; next }

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
    emit()
}

END {
    printf "static struct wrapper wrappers[] = {\n%s};\n", table
    for (name in pre)
        if (!(name in wrapped))
            missing = missing " pre_" name
    for (name in post)
        if (!(name in wrapped))
            missing = missing " post_" name
    if (missing != "") {
        print "fatal: gl.xml has no function for the hooks" missing > "/dev/stderr"
        exit 1
    }
}

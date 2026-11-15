# Sourced from the repository root by run-gl33.sh and build.sh.

gl_xml_commit=1cdd228e34966dd6b95bd203e9f84faba0f371a1
gl_xml_sha256=b9ca2cfa5c676e901c20d34af3407f1687cde0f1336a5ff7a8974d04c7494ad3
gl_xml_url="https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/${gl_xml_commit}/xml/gl.xml"
gl_xml="build/opengl-registry-$(echo "$gl_xml_commit" | cut -c1-8)/gl.xml"

core_versions="GL_VERSION_1_0 GL_VERSION_1_1 GL_VERSION_1_2 GL_VERSION_1_3 GL_VERSION_1_4 GL_VERSION_1_5
GL_VERSION_2_0 GL_VERSION_2_1 GL_VERSION_3_0 GL_VERSION_3_1 GL_VERSION_3_2 GL_VERSION_3_3"

download_gl_xml() {
    if ! test -f "$gl_xml"; then
        mkdir -p -- "$(dirname -- "$gl_xml")"
        echo "downloading $gl_xml_url" >&2
        curl -fsSL -o "$gl_xml.part" -- "$gl_xml_url"
        mv -f -- "$gl_xml.part" "$gl_xml"
    fi
    if ! echo "$gl_xml_sha256 *$gl_xml" | sha256sum -c --status; then
        echo "fatal: $gl_xml doesn't match its SHA-256" >&2
        exit 1
    fi
}

# The names of the $1 elements that the gl.xml features or extensions named by $2 require in the core profile,
# one per line. An enum's line also has its value.
gl_required_by() {
    awk -v element="$1" -v names="$2" 'BEGIN { n = split(names, a, " "); for (i = 1; i <= n; i++) want[a[i]] = 1 }
        /<enum value="/ && !/api="gles/ { match($0, /value="[^"]*"/); v = substr($0, RSTART + 7, RLENGTH - 8)
                                          match($0, /name="[^"]*"/); value[substr($0, RSTART + 6, RLENGTH - 7)] = v }
        /<(feature|extension) / { match($0, /name="[^"]*"/); on = (substr($0, RSTART + 6, RLENGTH - 7) in want) }
        /<\/(feature|extension)>/ { on = 0 }
        /<(require|remove)[ >]/ && !/\/>/ { core = on && !/api="gles/ && !/profile="compatibility"/
                                            req = core && /<require/; rem = core && /<remove/ }
        /<\/(require|remove)>/ { req = rem = 0 }
        (req || rem) && $0 ~ "<" element " name=\"" { match($0, /name="[^"]*"/)
                                                      name = substr($0, RSTART + 6, RLENGTH - 7)
                                                      if (req) required[name] = 1; else delete required[name] }
        END { for (name in required) if (element == "enum") print name, value[name]; else print name }' \
        "$gl_xml" | sort -u
}

# The GL functions that the gl.xml features or extensions named by the arguments add, one per line.
gl_functions_required_by() {
    gl_required_by command "$*"
}

# The enums that the gl.xml features or extensions named by the arguments add, one "name value" line each.
gl_enums_required_by() {
    gl_required_by enum "$*"
}

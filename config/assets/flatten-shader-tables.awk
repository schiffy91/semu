# Flattens constant two-dimensional tables in a slang shader: `const T name[R][C] = { {..}, {..} };`
# becomes `const T name[R*C] = { .. };` and every `name[a][b]` after it becomes `name[(a)*C+(b)]`.
# Arrays of arrays need GLSL 4.3; macOS stops at OpenGL 4.1, where the CRT mask code would not compile.
# The picture is the same: only the storage of the table changes. Run as: gawk -f this FILE > OUT
BEGIN { RS = "^$" }

function lookups(text,    name, pattern, result, found, inside, parts) {  # name[a][b] -> name[(a)*C+(b)] for every table seen so far
    for (name in columns) {
        pattern = "\\<" name "\\[[^][]+\\]\\[[^][]+\\]"
        result = ""
        while (match(text, pattern)) {
            found = substr(text, RSTART, RLENGTH)
            inside = substr(found, length(name) + 2, length(found) - length(name) - 2)
            split(inside, parts, "\\]\\[")
            result = result substr(text, 1, RSTART - 1) name "[(" parts[1] ")*" columns[name] "+(" parts[2] ")]"
            text = substr(text, RSTART + RLENGTH)
        }
        text = result text
    }
    return text
}

{
    rest = $0
    output = ""
    declaration = "const[ \t]+[A-Za-z0-9_]+[ \t]+[A-Za-z0-9_]+\\[[0-9]+\\]\\[[0-9]+\\][ \t]*=[ \t]*\\{"
    while (match(rest, declaration)) {
        start = RSTART; span = RLENGTH  # lookups() runs match() too
        output = output lookups(substr(rest, 1, start - 1))
        head = substr(rest, start, span)
        rest = substr(rest, start + span)
        depth = 1
        for (position = 1; position <= length(rest) && depth > 0; position++) {  # the initializer's closing brace
            character = substr(rest, position, 1)
            if (character == "{") { depth++ } else if (character == "}") { depth-- }
        }
        body = substr(rest, 1, position - 2)
        rest = substr(rest, position)
        gsub(/[{}]/, "", body)
        gsub(/,[ \t\n]*,/, ",", body)
        sub(/,[ \t\n]*$/, "", body)
        match(head, /\[[0-9]+\]\[[0-9]+\]/)
        sizes = substr(head, RSTART + 1, RLENGTH - 2)
        split(sizes, dimension, "\\]\\[")
        match(head, /[A-Za-z0-9_]+[ \t]*\[/)
        name = substr(head, RSTART, RLENGTH - 1)
        sub(/[ \t]+$/, "", name)
        columns[name] = dimension[2]
        sub(/\[[0-9]+\]\[[0-9]+\]/, "[" dimension[1] * dimension[2] "]", head)
        output = output head body "}"
    }
    printf "%s", output lookups(rest)
}

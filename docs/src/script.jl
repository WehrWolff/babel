import Markdown

const BABEL_VERSION_MAJOR = @ccall "../../build/libconfig.so".expose_babel_version_major()::Cint
const BABEL_VERSION_MINOR = @ccall "../../build/libconfig.so".expose_babel_version_minor()::Cint
const BABEL_VERSION_PATCH = @ccall "../../build/libconfig.so".expose_babel_version_patch()::Cint
const BABEL_VERSION_STRING = unsafe_string(@ccall "../../build/libconfig.so".expose_babel_version_string()::Cstring)
const BABEL_VERSION_SPECIFIER = unsafe_string(@ccall "../../build/libconfig.so".expose_babel_version_specifier()::Cstring)
const BABEL_FULL_VERSION = unsafe_string(@ccall "../../build/libconfig.so".expose_babel_full_version()::Cstring)

io = IOBuffer()
release = isempty(BABEL_VERSION_SPECIFIER)
v = "$(BABEL_VERSION_MAJOR).$(BABEL_VERSION_MINOR)"
print(io, """
    # Babel $(v) Documentation

    Welcome to the documentation for Babel $(v).

    """)
if !release
    print(io,"""
        !!! warning "Work in progress!"
            This documentation is for an unreleased, in-development, version of Babel.
        """)
end

# print(io, "Please read the [release notes](NEWS.md) to see what has changed since the last release.")

file = "TheBabelProgrammingLanguage.pdf"
path = "$(file)"

print(io,"""
    !!! note
        The documentation is also available in PDF format: [$file]($path).
    """)

res = Markdown.parse(String(take!(io)))
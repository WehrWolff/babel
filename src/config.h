#ifndef BABEL_CONFIG_H
#define BABEL_CONFIG_H

#define BABEL_VERSION_MAJOR 0
#define BABEL_VERSION_MINOR 0
#define BABEL_VERSION_PATCH 0
#define BABEL_VERSION_STRING "0.0.0"

#define BABEL_VERSION_SPECIFIER "pre-alpha"
#define BABEL_FULL_VERSION "0.0.0-pre-alpha"

int expose_babel_version_major();
int expose_babel_version_minor();
int expose_babel_version_patch();
const char* expose_babel_version_string();
const char* expose_babel_version_specifier();
const char* expose_babel_full_version();

#endif

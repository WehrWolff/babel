#include "config.h"

int expose_babel_version_major() { return BABEL_VERSION_MAJOR; }
int expose_babel_version_minor() { return BABEL_VERSION_MINOR; }
int expose_babel_version_patch() { return BABEL_VERSION_PATCH; }
const char* expose_babel_version_string() { return BABEL_VERSION_STRING; }
const char* expose_babel_version_specifier() { return BABEL_VERSION_SPECIFIER; }
const char* expose_babel_full_version() { return BABEL_FULL_VERSION; }
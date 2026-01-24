#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putd - putchar that takes a digit (int) and returns 0.
extern "C" DLLEXPORT int putd(int X) {
    fputc((char)X, stderr);
    return 0;
}

/// printd - fprintf that takes a digit (int) prints it as "%d\n", returning 0.
extern "C" DLLEXPORT int printd(int X) {
    fprintf(stderr, "%d\n", X);
    return 0;
}

extern "C" DLLEXPORT int inputInt(const char* cstr) {
    int out;
    printf("%s", cstr);
    scanf("%d", &out);
    return out;
}

/* Binding header for the dynamic-linker API.
 *
 * btrc resolves C declarations through a declared native binding whose header
 * must be a file in this package, and RTLD_* are macros rather than
 * declarations, so they are named in the manifest's symbol list. */
#define _GNU_SOURCE 1
#include <dlfcn.h>

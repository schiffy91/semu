/* Binding header for librashader's ABI-2 OpenGL C API.
 *
 * btrc resolves C declarations through a declared native binding whose header
 * must be a file in this package, so the runtime selection macro lives here
 * rather than in the .btrc that consumes it. */
#define LIBRA_RUNTIME_OPENGL 1
#include <librashader/librashader.h>

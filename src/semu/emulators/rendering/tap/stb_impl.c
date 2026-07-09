/* stb_impl.c — the single translation unit that instantiates stb_image.
 * The btrc tap declares stbi_load/stbi_image_free as externs; link this object
 * to satisfy them (matches how the C tap #define'd STB_IMAGE_IMPLEMENTATION). */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

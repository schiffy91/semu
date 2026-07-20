{ writeText }:

# Emulator-facing direct OpenGL renderer ABI. Emulators link this API and call
# it at their two owned render boundaries; there is no runtime symbol lookup.
writeText "semu_renderer.h" ''
  #ifndef SEMU_RENDERER_H
  #define SEMU_RENDERER_H

  #include <stdint.h>

  #ifdef __cplusplus
  extern "C" {
  #endif

  #define SEMU_RENDER_ABI_GL 2u
  #define SEMU_RENDER_MAX_SURFACES 2
  #define SEMU_RENDER_ORIGIN_BOTTOM_LEFT 0
  #define SEMU_RENDER_ORIGIN_TOP_LEFT 1
  #define SEMU_RENDER_LAYOUT_AUTO 0u
  #define SEMU_RENDER_LAYOUT_DEFAULT 1u
  #define SEMU_RENDER_LAYOUT_VARIANT_B 2u
  #define SEMU_RENDER_LAYOUT_VARIANT_C 3u
  #define SEMU_RENDER_LAYOUT_VARIANT_D 4u
  #define SEMU_RENDER_PASSTHROUGH 0
  #define SEMU_RENDER_APPLIED 1
  #define SEMU_RENDER_INVALID_FRAME (-1)
  #define SEMU_RENDER_NO_CONTEXT (-2)
  #define SEMU_RENDER_GL_UNAVAILABLE (-3)
  #define SEMU_RENDER_PIPELINE_FAILED (-4)
  #define SEMU_RENDER_ASSET_MISSING (-5)
  #define SEMU_RENDER_POINTER_OUTSIDE 0
  #define SEMU_RENDER_POINTER_MAPPED 1
  #define SEMU_RENDER_POINTER_INVALID (-1)
  #define SEMU_RENDER_POINTER_UNAVAILABLE (-2)
  #define SEMU_RENDER_POINTER_STALE (-3)

  typedef void* (*SemuRenderGetProc)(const char* name);
  typedef void* (*SemuRenderCurrentContext)(void);

  typedef struct SemuRenderSurfaceGl {
      int x;
      int y;
      int width;
      int height;
      int native_width;
      int native_height;
      int rotation;
      int origin;
  } SemuRenderSurfaceGl;

  typedef struct SemuRenderPointerSurface {
      int x;
      int y;
      int width;
      int height;
      int native_width;
      int native_height;
      int rotation;
      int origin;
  } SemuRenderPointerSurface;

  typedef struct SemuRenderPointerMap {
      unsigned int abi;
      unsigned int struct_size;
      uint64_t frame_id;
      int framebuffer_width;
      int framebuffer_height;
      int surface_count;
      SemuRenderPointerSurface surfaces[SEMU_RENDER_MAX_SURFACES];
  } SemuRenderPointerMap;

  typedef struct SemuRenderPointerQuery {
      unsigned int abi;
      unsigned int struct_size;
      uint64_t frame_id;
      float x;
      float y;
      int viewport_width;
      int viewport_height;
      int origin;
      int surface_index;
      int clamp;
  } SemuRenderPointerQuery;

  typedef struct SemuRenderPointerResult {
      unsigned int abi;
      unsigned int struct_size;
      uint64_t frame_id;
      int surface_index;
      int native_width;
      int native_height;
      int rotation;
      int origin;
      int clamped;
      float normalized_x;
      float normalized_y;
      float native_x;
      float native_y;
  } SemuRenderPointerResult;

  typedef int (*SemuRenderMapPointer)(SemuRenderPointerMap* map,
      SemuRenderPointerQuery* query, SemuRenderPointerResult* result);

  typedef struct SemuRenderFrameGl {
      unsigned int abi;
      unsigned int struct_size;
      uint64_t frame_id;
      unsigned int framebuffer;
      unsigned int color_buffer;
      int framebuffer_width;
      int framebuffer_height;
      float presentation_aspect;
      unsigned int layout_variant;
      int surface_count;
      SemuRenderSurfaceGl surfaces[SEMU_RENDER_MAX_SURFACES];
      SemuRenderGetProc get_proc;
      SemuRenderCurrentContext current_context;
      SemuRenderPointerMap pointer_map;
      SemuRenderMapPointer map_pointer;
  } SemuRenderFrameGl;

  int semu_render_game_gl(SemuRenderFrameGl* frame);
  int semu_render_post_ui_gl(SemuRenderFrameGl* frame);

  #ifdef __cplusplus
  }
  #endif
  #endif
''

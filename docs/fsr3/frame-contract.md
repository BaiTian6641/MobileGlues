# MobileGlues FSR3 frame contract (ABI v1)

Status: **frozen for S2**. Changes require a new ABI version and a coordinated backend/adapter
update. This is an in-process native contract owned by MobileGlues. It is not a GL extension.

## 1. Invariants

- One `mg_fsr_session_t` belongs to one graphics session (share group + provider generation).
  It is not shared across EGL contexts or surfaces.
- Submission is synchronous on the render thread. GPU work may complete asynchronously.
- Resource names are MobileGlues frontend GL names from the session's share group, never raw
  Vulkan handles or unrelated application texture IDs.
- Every dispatch identifies the frame and resource generation. Stale or foreign names are
  rejected; they are not silently interpreted.
- `Off` performs no session creation, allocation, copy, jitter change, or pass.
- A failed backend compile/link/framebuffer/capability check makes the mode unavailable and
  reports the failure. Application `ignoreError` settings never override internal results.
- The adapter composites HUD/UI at display resolution after reconstruction. The contract's
  output is the scene image before native UI.

## 2. Structures

```c
#define MG_FSR_ABI_VERSION 1u

typedef enum mg_fsr_mode_t {
    MG_FSR_MODE_OFF = 0,
    MG_FSR_MODE_FSR1_SPATIAL = 1,
    MG_FSR_MODE_FSR3_TEMPORAL = 2,
} mg_fsr_mode_t;

typedef enum mg_fsr_color_space_t {
    MG_FSR_COLOR_LDR_PERCEPTUAL = 0,
    MG_FSR_COLOR_LINEAR_HDR = 1,
} mg_fsr_color_space_t;

typedef enum mg_fsr_resource_ns_t {
    MG_FSR_NS_NONE = 0,
    MG_FSR_NS_GL_FRONTEND = 1,
} mg_fsr_resource_ns_t;

typedef struct mg_fsr_resource_t {
    uint64_t handle;          /* frontend GL name in the declared namespace */
    uint32_t namespace_id;    /* mg_fsr_resource_ns_t */
    uint32_t gl_target;       /* GL_TEXTURE_2D, GL_TEXTURE_2D_ARRAY, ... */
    uint32_t mip;             /* first sampled level */
    uint32_t layer;           /* first sampled layer */
    uint32_t format;          /* logical GL internal format, not a substitute */
    uint32_t reserved;
    uint64_t generation;      /* resource generation at producer handoff */
} mg_fsr_resource_t;

typedef struct mg_fsr_extent_t {
    uint32_t x, y, width, height; /* origin is top-left for active rectangles */
} mg_fsr_extent_t;

typedef struct mg_fsr_dispatch_t {
    uint32_t abi_version;
    uint32_t struct_size;

    uint64_t frame_id;
    uint64_t session_id;
    uint64_t context_id;
    uint64_t surface_id;
    uint64_t resource_generation;

    mg_fsr_resource_t color;       /* required, render-resolution scene color */
    mg_fsr_resource_t depth;       /* required for temporal reconstruction */
    mg_fsr_resource_t motion;      /* optional; required for moving geometry */
    mg_fsr_resource_t exposure;    /* optional; 1x1 or 3x1 as declared */
    mg_fsr_resource_t reactive;    /* optional */
    mg_fsr_resource_t transparency;/* optional composition/alpha input */
    mg_fsr_resource_t output;      /* optional; 0 = backend-owned output */

    mg_fsr_extent_t color_active;
    mg_fsr_extent_t output_active;

    uint32_t render_width, render_height;
    uint32_t output_width, output_height;

    uint32_t color_space;          /* mg_fsr_color_space_t */
    uint32_t alpha_premultiplied;  /* 1 = premultiplied */
    uint32_t output_alpha_mode;    /* 0 = opaque scene output, 1 = preserve */
    float pre_exposure;            /* positive; 1.0 if exposure not supplied */

    uint32_t depth_reversed;
    uint32_t depth_infinite_far;
    uint32_t depth_valid;
    float camera_near;
    float camera_far;
    float camera_fov_y_radians;

    float jitter_x;                /* actual jitter applied, render pixels */
    float jitter_y;
    float motion_scale_x;          /* current-to-previous, pixel units */
    float motion_scale_y;
    uint32_t motion_units;         /* 0 = render pixels, 1 = normalized UV */
    uint32_t motion_valid;
    uint32_t motion_includes_jitter;
    uint32_t motion_confidence;    /* 0 none, 1 camera-only, 2 mixed, 3 full */
    uint32_t depth_includes_jitter;

    float frame_delta_seconds;     /* clamped by caller; > 0 */
    uint32_t reset_history;
    uint32_t reset_reason;         /* enum in session header */
    uint32_t render_stage_id;      /* producer/accepted stage identity */
    uint32_t flags;                /* MG_FSR_FLAG_* */

    uint64_t reserved[8];
} mg_fsr_dispatch_t;

typedef struct mg_fsr_caps_t {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t mode_mask;            /* bit per mg_fsr_mode_t */
    uint32_t gles_compute;
    uint32_t fp16;
    uint32_t max_workgroup[3];
    uint32_t max_image_units;
    uint32_t max_texture_units;
    char backend_name[64];
    char failure_reason[256];
} mg_fsr_caps_t;
```

## 3. Field semantics

### Identity and lifetime

- `session_id` is returned by `mg_fsr_session_create`. `context_id`/`surface_id` are the
  MobileGlues identities current at dispatch. A different context or surface requires a new
  session or an explicit reset; it is never silently retargeted.
- `frame_id` increases monotonically per session. `resource_generation` changes on allocation,
  resize, texture recreation, or reuse of a name. A different generation invalidates history.
- A zero handle is invalid for `color`, `depth`, `motion`, `exposure`, `reactive`, and
  `transparency`. `output.handle == 0` means the backend-owned output is used and returned by
  `mg_fsr_get_output`.
- The producer must finish writing the referenced resources before dispatch. Synchronous
  submission covers the command-stream handoff; any future cross-context path needs a complete
  fence contract and a new ABI revision.

### Dimensions and extents

- `render_*` is the actual scene work extent, not the surface extent.
- `output_*` is the display/presentation extent. FSR3 reconstruction writes this size.
- `color_active`/`output_active` are the valid rectangles; `0,0,render_width,render_height` is
  the default. Allocated extents may be larger. No pass may infer the active rectangle from a
  texture's full allocation.
- The render scale is `render_width/output_width`; it is explicit, never a global viewport
  side effect. Shadow maps, LUTs, volumes, and fixed-size effect resources are not implied by
  this field.

### Color and exposure

- LDR is perceptual/sRGB-decoded as declared by `color_space`; HDR is linear.
- `pre_exposure` is the positive exposure already applied to color before this dispatch.
  `exposure` is the exposure texture when a profile supplies one; otherwise `pre_exposure`
  alone is used.
- Premultiplied/straight alpha is explicit. The output alpha policy is explicit; UI
  composition is outside this dispatch.

### Depth

- `depth_reversed` and `depth_infinite_far` describe the projection. `camera_near`,
  `camera_far`, and vertical FOV use world units and radians.
- Depth must be the sampleable scene depth matching color. Jitter inclusion is explicit.

### Motion and jitter

- Motion is **current-to-previous**: an object that moves two render pixels to the right has
  `motion_x == -2` at its current location, in render-pixel units when `motion_units == 0`.
- Normalized UV units are allowed only with `motion_units == 1`; the backend converts using
  the declared render extents, never by guessing.
- `jitter_*` is the actual offset applied to the scene camera this frame, in render pixels.
  `motion_includes_jitter` states whether motion vectors already contain it.
- One jitter owner only. The adapter and pack profile must not both jitter the scene.
- Camera-only reconstruction is `motion_confidence == 1` and is experimental. Full support
  requires independently moving objects and deformation coverage; those are separate gates.

### Reset

`reset_history` must be set on context/surface change, render-scale change, shader reload,
world/dimension change, teleport, camera cut, FOV/perspective jump, algorithm switch, resource
generation change, and any invalid history. `reset_reason` is diagnostic only; correctness
must not depend on parsing it.

## 4. API

```c
void* mg_fsr_create_session(void);                       /* opaque, current context */
void  mg_fsr_destroy_session(void* session);
int   mg_fsr_query_caps(void* session, mg_fsr_caps_t* caps);
int   mg_fsr_dispatch(void* session, const mg_fsr_dispatch_t* desc);
int   mg_fsr_get_output(void* session, mg_fsr_resource_t* output);
int   mg_fsr_invalidate(void* session, uint32_t reason);
const char* mg_fsr_last_error(void* session);
```

All functions return 0 on success and a stable negative error code on failure. `last_error`
returns an explanatory string until the next call. `dispatch` validates version, struct size,
session identity, resource names, generations, extents, and capabilities before any GPU work.
A rejected dispatch leaves history and output untouched.

## 5. Dispatch boundary

The adapter submits after scene rendering and required effect preparation, but before HUD/UI
composition. For a pack profile the exact boundary is recorded in integration metadata. FSR3
replaces the final image TAA resolve; effect denoisers/histories remain unless separately
audited. The output is then finished by the profile's display-stage effects and the HUD is
composited at native resolution.

## 6. Backend ownership

The backend owns intermediates, history resources, pipelines, and temporary copies. The
adapter does not manufacture FSR scratch images. The backend must not read back to CPU or
issue device-wide waits in the normal frame path. Output may be backend-owned; if the adapter
supplies one, its format, extent, and generation are validated.

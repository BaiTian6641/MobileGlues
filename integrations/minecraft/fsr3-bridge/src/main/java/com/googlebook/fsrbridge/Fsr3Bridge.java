package com.googlebook.fsrbridge;

import net.irisshaders.iris.gl.texture.DepthBufferFormat;
import net.irisshaders.iris.targets.DepthTexture;

/**
 * Optional Minecraft/Iris connector. It owns no renderer logic: after Iris has
 * finished its final pass, the native MobileGlues library resolves the current
 * main framebuffer's color/depth attachments and runs the in-tree FSR3
 * backend. The bridge is only a semantic boundary, not a second upscaler.
 *
 * The class deliberately exposes no Minecraft types, so it can be built against
 * the Iris artifact alone.
 */
public final class Fsr3Bridge {
    private static final boolean LOADED;
    private static volatile boolean warned;

    static {
        LOADED = loadNative();
        if (LOADED) {
            try {
                NativeBridge.bridgeMode();
            } catch (Throwable t) {
                mark("bridge mode negotiation unavailable: " + t);
            }
        }
        mark("Fsr3Bridge loaded=" + LOADED);
    }

    private Fsr3Bridge() {
    }

    private static String loadedCorePath() {
        try {
            for (String line : java.nio.file.Files.readAllLines(java.nio.file.Paths.get("/proc/self/maps"))) {
                int slash = line.indexOf('/');
                if (slash < 0) continue;
                String path = line.substring(slash).trim();
                if (path.endsWith("/libmobileglues.so") || path.contains("/libmobileglues.so")) {
                    int space = path.indexOf(' ');
                    if (space > 0) path = path.substring(0, space);
                    return path;
                }
            }
        } catch (Throwable ignored) {
        }
        return null;
    }

    private static boolean loadNative() {
        String path = System.getProperty("mg.fsr3.library");
        if (path == null || path.isEmpty()) {
            path = loadedCorePath();
        }
        if (path == null) {
            try {
                Class<?> activityThread = Class.forName("android.app.ActivityThread", false, ClassLoader.getSystemClassLoader());
                Object app = activityThread.getMethod("currentApplication").invoke(null);
                if (app != null) {
                    Object pm = app.getClass().getMethod("getPackageManager").invoke(app);
                    Object info = pm.getClass().getMethod("getApplicationInfo", String.class, int.class)
                        .invoke(pm, "com.fcl.plugin.mobileglues", 0);
                    java.lang.reflect.Field dir = info.getClass().getField("nativeLibraryDir");
                    path = String.valueOf(dir.get(info)) + "/libmobileglues.so";
                }
            } catch (Throwable t) {
                warn("FCL native-library resolution failed: " + t);
            }
        }
        if (path == null) {
            warn("no mapped MobileGlues library path");
            return false;
        }
        mark("helper init " + path);
        return NativeBridge.load(path);
    }

    private static boolean cameraWarning;
    private static volatile boolean sceneLogDone;

    /**
     * Capture Iris' current camera matrices for camera-only reprojection.
     * Reflection keeps this adapter independent of Minecraft/JOML at build time;
     * if the class or matrix accessor is absent, the native side keeps the
     * synthetic zero-motion path.
     */
    public static void captureCamera() {
        if (!LOADED) return;
        try {
            Class<?> stateClass = Class.forName("net.irisshaders.iris.uniforms.CapturedRenderingState");
            Object state = stateClass.getField("INSTANCE").get(null);
            Object modelView = stateClass.getMethod("getGbufferModelView").invoke(state);
            Object projection = stateClass.getMethod("getGbufferProjection").invoke(state);
            Class<?> cameraClass = Class.forName("net.irisshaders.iris.uniforms.CameraUniforms");
            Object position = cameraClass.getMethod("getUnshiftedCameraPosition").invoke(null);
            float[] mv = new float[16];
            float[] pr = new float[16];
            double[] pos = new double[3];
            copyMatrix(modelView, mv);
            copyMatrix(projection, pr);
            pos[0] = position.getClass().getField("x").getDouble(position);
            pos[1] = position.getClass().getField("y").getDouble(position);
            pos[2] = position.getClass().getField("z").getDouble(position);
            NativeBridge.camera(mv, pr, pos);
        } catch (Throwable t) {
            if (!cameraWarning) {
                cameraWarning = true;
                warn("camera matrices unavailable; using zero motion: " + t);
            }
            try {
                NativeBridge.invalidateCamera();
            } catch (Throwable ignored) {
            }
        }
    }

    private static void copyMatrix(Object matrix, float[] out) throws Exception {
        if (matrix == null) throw new IllegalStateException("null matrix");
        matrix.getClass().getMethod("get", float[].class, int.class).invoke(matrix, out, 0);
    }

    public static void mark(String message) {
        System.err.println("[mgfsr3bridge] " + message);
        try {
            java.nio.file.Path p = java.nio.file.Paths.get(System.getProperty("user.dir", "."), "mgfsr3.log");
            java.nio.file.Files.writeString(p, message + "\n",
                java.nio.file.StandardOpenOption.CREATE, java.nio.file.StandardOpenOption.APPEND);
        } catch (Throwable ignored) {
        }
    }

    private static void warn(String message) {
        if (warned) return;
        warned = true;
        mark("WARN " + message);
    }

    public static boolean available() {
        if (!LOADED) return false;
        try {
            boolean available = NativeBridge.available();
            if (available && !warned) {
                warned = true;
                mark("nativeAvailable=true");
            }
            return available;
        } catch (Throwable t) {
            warn("nativeAvailable failed: " + t);
            return false;
        }
    }

    /**
     * Scale an Iris scene target dimension. Returns the input unchanged unless
     * FSR3 is enabled and a non-native quality preset is selected. Minecraft
     * only: this is the optional adapter boundary, not a generic renderer hook.
     */
    private static volatile boolean inFinalPass;

    public static void beginFinalPass() { inFinalPass = true; }
    public static void endFinalPass() { inFinalPass = false; }

    /** World/composite programs get the scene extent; the final pass gets native. */
    public static int uniformWidth(int width) {
        return inFinalPass ? width : sceneWidth(width);
    }

    public static int uniformHeight(int height) {
        return inFinalPass ? height : sceneHeight(height);
    }

    private static DepthTexture managedDepth;
    private static int managedWidth;
    private static int managedHeight;
    private static int managedFormat;
    private static int managedDepthFbo;

    /**
     * Experiment B: give the scaled Iris color targets a matching scene depth.
     * Minecraft's main depth remains native for the final/HUD surface; this
     * texture is owned by the bridge and is not the noHand snapshot.
     */
    public static synchronized int sceneDepthTexture(int original, int width, int height) {
        if (!LOADED || width <= 0 || height <= 0) return original;
        try {
            if (NativeBridge.scale() <= 1.001f) return original;
            int format = queryTextureFormat(original);
            if (format == 0) {
                warn("scene depth format unavailable; keeping external depth");
                return original;
            }
            if (managedDepth != null && managedWidth == width && managedHeight == height
                    && managedFormat == format) {
                return managedDepth.getTextureId();
            }
            DepthBufferFormat depthFormat = DepthBufferFormat.fromGlEnumOrDefault(format);
            DepthTexture next = new DepthTexture("mgfsr3-scene-depth", width, height, depthFormat);
            int nextId = next.getTextureId();
            int nextFbo = createDepthFbo(nextId);
            DepthTexture previous = managedDepth;
            int previousFbo = managedDepthFbo;
            managedDepth = next;
            managedDepthFbo = nextFbo;
            managedWidth = width;
            managedHeight = height;
            managedFormat = format;
            if (previous != null) {
                try {
                    previous.destroy();
                } catch (Throwable ignored) {
                }
            }
            if (previousFbo != 0) deleteFbo(previousFbo);
            mark("scene depth " + width + "x" + height + " id=" + nextId + " format=" + format);
            return nextId;
        } catch (Throwable t) {
            warn("scene depth allocation failed: " + t);
            return original;
        }
    }

    public static synchronized void destroySceneDepth() {
        if (managedDepth == null) return;
        try {
            managedDepth.destroy();
        } catch (Throwable ignored) {
        }
        managedDepth = null;
        managedWidth = managedHeight = managedFormat = 0;
        if (managedDepthFbo != 0) {
            deleteFbo(managedDepthFbo);
            managedDepthFbo = 0;
        }
    }

    private static int createDepthFbo(int texture) {
        try {
            Class<?> gl30 = Class.forName("org.lwjgl.opengl.GL30");
            int fbo = (Integer) gl30.getMethod("glGenFramebuffers").invoke(null);
            gl30.getMethod("glBindFramebuffer", int.class, int.class).invoke(null, 0x8D40, fbo);
            gl30.getMethod("glFramebufferTexture2D", int.class, int.class, int.class, int.class, int.class)
                    .invoke(null, 0x8D40, 0x8D00, 0x0DE1, texture, 0);
            int status = (Integer) gl30.getMethod("glCheckFramebufferStatus", int.class)
                    .invoke(null, 0x8D40);
            gl30.getMethod("glBindFramebuffer", int.class, int.class).invoke(null, 0x8D40, 0);
            if (status != 0x8CD5) {
                warn("scene depth FBO incomplete: " + Integer.toHexString(status));
                deleteFbo(fbo);
                return 0;
            }
            return fbo;
        } catch (Throwable t) {
            warn("scene depth FBO creation failed: " + t);
            return 0;
        }
    }

    private static void deleteFbo(int fbo) {
        try {
            Class<?> gl30 = Class.forName("org.lwjgl.opengl.GL30");
            gl30.getMethod("glDeleteFramebuffers", int.class).invoke(null, fbo);
        } catch (Throwable ignored) {
        }
    }

    public static void clearSceneDepth() {
        if (managedDepthFbo == 0) return;
        try {
            Class<?> gl11 = Class.forName("org.lwjgl.opengl.GL11");
            Class<?> gl30 = Class.forName("org.lwjgl.opengl.GL30");
            int[] previousFbo = new int[1];
            gl11.getMethod("glGetIntegerv", int.class, int[].class)
                    .invoke(null, 0x8CA6, previousFbo);
            int[] scissor = new int[1];
            gl11.getMethod("glGetIntegerv", int.class, int[].class)
                    .invoke(null, 0x0C11, scissor);
            int[] depthMask = new int[1];
            gl11.getMethod("glGetIntegerv", int.class, int[].class)
                    .invoke(null, 0x0B72, depthMask);
            float[] clearDepth = new float[1];
            gl11.getMethod("glGetFloatv", int.class, float[].class)
                    .invoke(null, 0x0B73, clearDepth);
            gl30.getMethod("glBindFramebuffer", int.class, int.class).invoke(null, 0x8D40, managedDepthFbo);
            if (scissor[0] != 0) gl11.getMethod("glDisable", int.class).invoke(null, 0x0C11);
            gl11.getMethod("glDepthMask", boolean.class).invoke(null, true);
            gl11.getMethod("glClearDepth", double.class).invoke(null, 1.0d);
            gl11.getMethod("glClear", int.class).invoke(null, 0x0100);
            gl11.getMethod("glClearDepth", double.class).invoke(null, (double) clearDepth[0]);
            gl11.getMethod("glDepthMask", boolean.class).invoke(null, depthMask[0] != 0);
            if (scissor[0] != 0) gl11.getMethod("glEnable", int.class).invoke(null, 0x0C11);
            gl30.getMethod("glBindFramebuffer", int.class, int.class)
                    .invoke(null, 0x8D40, previousFbo[0]);
        } catch (Throwable t) {
            warn("scene depth clear failed: " + t);
        }
    }

    private static int queryTextureFormat(int texture) {
        if (texture == 0) return 0;
        try {
            Class<?> gl11 = Class.forName("org.lwjgl.opengl.GL11");
            int[] binding = new int[1];
            gl11.getMethod("glGetIntegerv", int.class, int[].class)
                    .invoke(null, 0x8069, binding); // GL_TEXTURE_BINDING_2D
            gl11.getMethod("glBindTexture", int.class, int.class).invoke(null, 0x0DE1, texture);
            int[] format = new int[1];
            gl11.getMethod("glGetTexLevelParameteriv", int.class, int.class, int.class, int[].class)
                    .invoke(null, 0x0DE1, 0, 0x1003, format); // GL_TEXTURE_INTERNAL_FORMAT
            gl11.getMethod("glBindTexture", int.class, int.class).invoke(null, 0x0DE1, binding[0]);
            return format[0];
        } catch (Throwable t) {
            return 0;
        }
    }

    public static int sceneWidth(int width) {
        if (width <= 0 || !LOADED) return width;
        try {
            float scale = NativeBridge.scale();
            if (scale <= 1.001f) return width;
            return Math.max(64, Math.round(width / scale));
        } catch (Throwable t) {
            warn("scale failed: " + t);
            return width;
        }
    }

    /** Height is kept independent so odd window dimensions do not accumulate rounding. */
    public static int sceneHeight(int height) {
        if (height <= 0 || !LOADED) return height;
        try {
            float scale = NativeBridge.scale();
            if (scale <= 1.001f) return height;
            return Math.max(64, Math.round(height / scale));
        } catch (Throwable t) {
            return height;
        }
    }

    /** Forget camera/history inputs after a world, shader or pipeline change. */
    public static void invalidateCamera() {
        if (!LOADED) return;
        try {
            NativeBridge.invalidateCamera();
        } catch (Throwable t) {
            warn("invalidate camera failed: " + t);
        }
    }

    /** Set the viewport back to the native EGL surface before Iris' final pass. */
    public static void resetNativeViewport() {
        if (!LOADED) return;
        try {
            NativeBridge.viewport();
        } catch (Throwable t) {
            warn("native viewport failed: " + t);
        }
    }

    /** Called on the render thread immediately after Iris' final pass. */
    public static void submit() {
        if (!LOADED) return;
        try {
            int rc = NativeBridge.submit();
            if (rc != 0) mark("submit rc=" + rc);
        } catch (Throwable t) {
            warn("nativeSubmit failed: " + t);
        }
    }
}

package com.googlebook.fsrbridge;

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
            float[] pos = new float[3];
            copyMatrix(modelView, mv);
            copyMatrix(projection, pr);
            pos[0] = (float) position.getClass().getField("x").getDouble(position);
            pos[1] = (float) position.getClass().getField("y").getDouble(position);
            pos[2] = (float) position.getClass().getField("z").getDouble(position);
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
    public static int sceneWidth(int width) {
        if (width <= 0 || !available()) {
            logScene("width " + width + " unchanged (adapter unavailable)");
            return width;
        }
        try {
            float scale = NativeBridge.scale();
            if (scale <= 1.001f) {
                logScene("width " + width + " unchanged (scale=" + scale + ")");
                return width;
            }
            int scaled = Math.max(64, Math.round(width / scale));
            logScene("width " + width + " -> " + scaled + " (scale=" + scale + ")");
            return scaled;
        } catch (Throwable t) {
            warn("scale failed: " + t);
            return width;
        }
    }

    /** Height is kept independent so odd window dimensions do not accumulate rounding. */
    public static int sceneHeight(int height) {
        if (height <= 0 || !available()) {
            logScene("height " + height + " unchanged (adapter unavailable)");
            return height;
        }
        try {
            float scale = NativeBridge.scale();
            if (scale <= 1.001f) {
                logScene("height " + height + " unchanged (scale=" + scale + ")");
                return height;
            }
            int scaled = Math.max(64, Math.round(height / scale));
            logScene("height " + height + " -> " + scaled + " (scale=" + scale + ")");
            return scaled;
        } catch (Throwable t) {
            warn("scale failed: " + t);
            return height;
        }
    }

    private static void logScene(String message) {
        if (sceneLogDone) return;
        sceneLogDone = true;
        try {
            Class<?> log = Class.forName("android.util.Log");
            log.getMethod("w", String.class, String.class).invoke(null, "MGFSR3", message);
        } catch (Throwable ignored) {
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
            mark("submit rc=" + rc);
        } catch (Throwable t) {
            warn("nativeSubmit failed: " + t);
        }
    }
}

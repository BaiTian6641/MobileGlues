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

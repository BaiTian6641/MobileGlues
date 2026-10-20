package com.googlebook.fsrbridge;

/** Tiny helper that reuses the core already mapped by the renderer plugin. */
public final class NativeBridge {
    private static boolean loaded;

    private NativeBridge() {
    }

    static synchronized boolean load(String corePath) {
        if (loaded) return true;
        try {
            java.io.File helper = extractHelper();
            if (helper == null) return false;
            System.load(helper.getAbsolutePath());
            loaded = init(corePath);
            return loaded;
        } catch (Throwable t) {
            Fsr3Bridge.mark("helper load failed: " + t);
            return false;
        }
    }

    private static java.io.File extractHelper() {
        String name = "/native/arm64-v8a/libmgfsrhelper.so";
        try (java.io.InputStream in = NativeBridge.class.getResourceAsStream(name)) {
            if (in == null) return null;
            java.io.File file = java.io.File.createTempFile("mgfsrhelper", ".so");
            java.nio.file.Files.copy(in, file.toPath(), java.nio.file.StandardCopyOption.REPLACE_EXISTING);
            file.setReadable(true, true);
            file.setWritable(false, false);
            file.setExecutable(true, true);
            return file;
        } catch (Throwable t) {
            Fsr3Bridge.mark("helper extraction failed: " + t);
            return null;
        }
    }

    static native boolean init(String corePath);

    static native boolean available();

    static native float scale();

    static native void viewport();

    static native void camera(float[] modelView, float[] projection, double[] cameraPosition);

    static native void invalidateCamera();

    static native void bridgeMode();

    static native int submit();

    public static native void finalScope(boolean active);

    public static native void setSceneDepth(int texture);

    static native String trace(int op, int role, int frame, int event);
}

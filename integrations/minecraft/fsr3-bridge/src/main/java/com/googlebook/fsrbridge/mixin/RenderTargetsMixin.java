package com.googlebook.fsrbridge.mixin;

import com.googlebook.fsrbridge.DiagnosticTrace;
import com.googlebook.fsrbridge.Fsr3Bridge;
import net.irisshaders.iris.targets.RenderTargets;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.ModifyVariable;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Optional Minecraft/Iris scene adapter. The constructor and the per-frame
 * resize path are the only places where Iris allocates the main scene targets,
 * so scaling here keeps the color, alt and depth textures coherent. The final
 * pass and HUD remain native.
 */
@Mixin(RenderTargets.class)
public abstract class RenderTargetsMixin {
    private static int mgfsr3$argWidth;
    private static int mgfsr3$argHeight;

    static {
        Fsr3Bridge.mark("RenderTargetsMixin loaded");
    }

    @ModifyVariable(method = "<init>", at = @At("HEAD"), argsOnly = true, ordinal = 0)
    private static int mgfsr3$constructorWidth(int width) {
        mgfsr3$argWidth = width;
        return Fsr3Bridge.sceneWidth(width);
    }

    @ModifyVariable(method = "<init>", at = @At("HEAD"), argsOnly = true, ordinal = 1)
    private static int mgfsr3$constructorHeight(int height) {
        mgfsr3$argHeight = height;
        return Fsr3Bridge.sceneHeight(height);
    }

    @ModifyVariable(method = "<init>", at = @At("HEAD"), argsOnly = true, ordinal = 2)
    private static int mgfsr3$constructorDepth(int depthTexture) {
        return Fsr3Bridge.sceneDepthTexture(depthTexture, Fsr3Bridge.sceneWidth(mgfsr3$argWidth),
                Fsr3Bridge.sceneHeight(mgfsr3$argHeight));
    }

    // Iris 1.8.14 signature is (depthBufferVersion, depthTextureId, width, height, ...).
    // The first two ints are not dimensions; touching them corrupts resize state.
    @ModifyVariable(method = "resizeIfNeeded", at = @At("HEAD"), argsOnly = true, ordinal = 2)
    private int mgfsr3$resizeWidth(int width) {
        mgfsr3$argWidth = width;
        return Fsr3Bridge.sceneWidth(width);
    }

    @ModifyVariable(method = "resizeIfNeeded", at = @At("HEAD"), argsOnly = true, ordinal = 3)
    private int mgfsr3$resizeHeight(int height) {
        mgfsr3$argHeight = height;
        return Fsr3Bridge.sceneHeight(height);
    }

    @ModifyVariable(method = "resizeIfNeeded", at = @At("HEAD"), argsOnly = true, ordinal = 1)
    private int mgfsr3$resizeDepth(int depthTexture) {
        return Fsr3Bridge.sceneDepthTexture(depthTexture, Fsr3Bridge.sceneWidth(mgfsr3$argWidth),
                Fsr3Bridge.sceneHeight(mgfsr3$argHeight));
    }

    @Inject(method = "<init>", at = @At("RETURN"), require = 0)
    private void mgfsr3$traceAllocation(CallbackInfo ci) {
        DiagnosticTrace.irisTargets(this, "ALLOCATION");
    }

    @Inject(method = "resizeIfNeeded", at = @At("RETURN"), require = 0)
    private void mgfsr3$traceResize(CallbackInfoReturnable<Boolean> cir) {
        DiagnosticTrace.irisTargets(this, "RESIZE");
    }

    @Inject(method = "copyPreHandDepth", at = @At("HEAD"), require = 0)
    private void mgfsr3$tracePreHandEntry(CallbackInfo ci) {
        DiagnosticTrace.copyEntry("PRE_HAND_COPY", this);
    }

    @Inject(method = "copyPreHandDepth", at = @At("RETURN"), require = 0)
    private void mgfsr3$tracePreHandExit(CallbackInfo ci) {
        DiagnosticTrace.copyExit("PRE_HAND_COPY");
    }

    @Inject(method = "copyPreTranslucentDepth", at = @At("HEAD"), require = 0)
    private void mgfsr3$tracePreTranslucentEntry(CallbackInfo ci) {
        DiagnosticTrace.copyEntry("PRE_TRANSLUCENT_COPY", this);
    }

    @Inject(method = "copyPreTranslucentDepth", at = @At("RETURN"), require = 0)
    private void mgfsr3$tracePreTranslucentExit(CallbackInfo ci) {
        DiagnosticTrace.copyExit("PRE_TRANSLUCENT_COPY");
    }
}

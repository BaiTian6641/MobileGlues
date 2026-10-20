package com.googlebook.fsrbridge.mixin;

import com.googlebook.fsrbridge.Fsr3Bridge;
import net.irisshaders.iris.pipeline.FinalPassRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * The main framebuffer is current at the end of Iris' final pass. Calling the
 * bridge here replaces/upgrades the scene before vanilla composes the HUD, so
 * UI stays at display resolution. The pack's final pass still runs; replacing
 * its TAA is a later profile step, not silently disabled here.
 */
@Mixin(FinalPassRenderer.class)
public abstract class FinalPassRendererMixin {
    static {
        com.googlebook.fsrbridge.Fsr3Bridge.mark("FinalPassRendererMixin loaded");
    }

    @Inject(method = "renderFinalPass", at = @At("HEAD"))
    private void mgfsr3$nativeViewport(CallbackInfo ci) {
        if (Fsr3Bridge.available()) {
            Fsr3Bridge.captureCamera();
            Fsr3Bridge.resetNativeViewport();
        }
    }

    @Inject(method = "renderFinalPass", at = @At("RETURN"))
    private void mgfsr3$submitBeforeHud(CallbackInfo ci) {
        com.googlebook.fsrbridge.Fsr3Bridge.mark("final pass hook");
        if (Fsr3Bridge.available()) {
            Fsr3Bridge.submit();
        }
    }
}

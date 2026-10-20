package com.googlebook.fsrbridge.mixin;

import com.googlebook.fsrbridge.DiagnosticTrace;
import com.googlebook.fsrbridge.Fsr3Bridge;
import com.googlebook.fsrbridge.NativeBridge;
import net.irisshaders.iris.gl.framebuffer.GlFramebuffer;
import net.irisshaders.iris.pipeline.FinalPassRenderer;
import org.spongepowered.asm.mixin.Final;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.Shadow;
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
    @Shadow @Final private GlFramebuffer colorHolder;

    static {
        com.googlebook.fsrbridge.Fsr3Bridge.mark("FinalPassRendererMixin loaded");
    }

    @Inject(method = "renderFinalPass", at = @At("HEAD"))
    private void mgfsr3$nativeViewport(CallbackInfo ci) {
        if (Fsr3Bridge.available()) {
            DiagnosticTrace.startRole("FINAL_DRAW", true);
            DiagnosticTrace.snapshot("FINAL_ENTRY", null);
            Fsr3Bridge.beginFinalPass();
            NativeBridge.finalScope(true);
            NativeBridge.setSceneDepth(Fsr3Bridge.sceneDepthTextureId());
            Fsr3Bridge.captureCamera();
        }
    }

    /**
     * Iris binds its color-only final holder to the native main color. Replace
     * that attachment with the bridge's scene-sized final target after the
     * bind, so the final shader runs before FSR3 instead of after it.
     */
    @Inject(method = "renderFinalPass",
            at = @At(value = "INVOKE",
                    target = "Lnet/irisshaders/iris/gl/framebuffer/GlFramebuffer;bind()V",
                    ordinal = 0, shift = At.Shift.AFTER))
    private void mgfsr3$redirectFinalColor(CallbackInfo ci) {
        if (!Fsr3Bridge.available()) return;
        int width = Fsr3Bridge.sceneWidth(Fsr3Bridge.mainWidth());
        int height = Fsr3Bridge.sceneHeight(Fsr3Bridge.mainHeight());
        int color = Fsr3Bridge.sceneFinalColorTexture(width, height);
        if (color == 0) return;
        colorHolder.addColorAttachment(0, color);
        NativeBridge.setSceneColor(color);
        Fsr3Bridge.setViewport(width, height);
        Fsr3Bridge.setPreFinalActive(true);
    }

    @Inject(method = "renderFinalPass", at = @At("RETURN"))
    private void mgfsr3$submitBeforeHud(CallbackInfo ci) {
        if (Fsr3Bridge.available()) {
            DiagnosticTrace.endRole("FINAL_DRAW_RETURN", null);
            if (Fsr3Bridge.preFinalActive()) {
                Fsr3Bridge.bindMainWrite();
                Fsr3Bridge.setViewport(Fsr3Bridge.mainWidth(), Fsr3Bridge.mainHeight());
                Fsr3Bridge.submit();
                NativeBridge.setSceneColor(0);
                colorHolder.addColorAttachment(0, Fsr3Bridge.mainColorTextureId());
                Fsr3Bridge.setPreFinalActive(false);
            }
            DiagnosticTrace.plain("FINAL_POST_SUBMIT", null);
            NativeBridge.finalScope(false);
            NativeBridge.setSceneDepth(0);
            Fsr3Bridge.endFinalPass();
        }
    }
}

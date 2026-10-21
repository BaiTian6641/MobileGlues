package com.googlebook.fsrbridge.mixin;

import com.googlebook.fsrbridge.DiagnosticTrace;
import com.googlebook.fsrbridge.Fsr3Bridge;
import com.googlebook.fsrbridge.NativeBridge;
import net.irisshaders.iris.gl.framebuffer.GlFramebuffer;
import net.irisshaders.iris.pipeline.FinalPassRenderer;
import net.irisshaders.iris.shaderpack.loading.ProgramId;
import net.irisshaders.iris.shaderpack.programs.ProgramSet;
import net.irisshaders.iris.targets.RenderTarget;
import net.irisshaders.iris.targets.RenderTargets;
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
    @Shadow @Final private GlFramebuffer baseline;
    @Shadow @Final private RenderTargets renderTargets;

    static {
        com.googlebook.fsrbridge.Fsr3Bridge.mark("FinalPassRendererMixin loaded");
    }

    @Inject(method = "renderFinalPass", at = @At("HEAD"))
    private void mgfsr3$nativeViewport(CallbackInfo ci) {
        RenderTarget target = renderTargets.get(0);
        if (target != null) baseline.addColorAttachment(0, target.getMainTexture());
        if (Fsr3Bridge.available()) {
            DiagnosticTrace.startRole("FINAL_DRAW", true);
            DiagnosticTrace.snapshot("FINAL_ENTRY", null);
            Fsr3Bridge.beginFinalPass();
            NativeBridge.finalScope(true);
            NativeBridge.setSceneDepth(Fsr3Bridge.sceneDepthTextureId());
            Fsr3Bridge.captureCamera();
            Fsr3Bridge.resetNativeViewport();
        }
    }

    @Inject(method = "renderFinalPass", at = @At("RETURN"))
    private void mgfsr3$submitBeforeHud(CallbackInfo ci) {
        if (Fsr3Bridge.available()) {
            DiagnosticTrace.endRole("FINAL_DRAW_RETURN", null);
            Fsr3Bridge.submit();
            DiagnosticTrace.plain("FINAL_POST_SUBMIT", null);
            NativeBridge.finalScope(false);
            NativeBridge.setSceneDepth(0);
            Fsr3Bridge.endFinalPass();
        }
    }
}

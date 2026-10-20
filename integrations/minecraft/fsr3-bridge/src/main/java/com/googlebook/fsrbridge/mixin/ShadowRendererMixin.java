package com.googlebook.fsrbridge.mixin;

import com.googlebook.fsrbridge.DiagnosticTrace;
import net.irisshaders.iris.shadows.ShadowRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/** Trace-only shadow pass boundary; absent packs never invoke it. */
@Mixin(ShadowRenderer.class)
public abstract class ShadowRendererMixin {
    static {
        DiagnosticTrace.hook("ShadowRenderer");
    }

    @Inject(method = "renderShadows", at = @At("HEAD"), require = 0)
    private void mgfsr3$shadowHead(CallbackInfo ci) {
        DiagnosticTrace.startRole("SHADOW", true);
    }

    @Inject(method = "renderShadows", at = @At("RETURN"), require = 0)
    private void mgfsr3$shadowReturn(CallbackInfo ci) {
        DiagnosticTrace.endRole("SHADOW_RETURN", null);
    }

    @Inject(method = "setupShadowViewport", at = @At("HEAD"), require = 0)
    private void mgfsr3$shadowViewport(CallbackInfo ci) {
        DiagnosticTrace.plain("SHADOW_VIEWPORT", null);
    }
}

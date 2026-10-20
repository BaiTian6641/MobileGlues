package com.googlebook.fsrbridge.mixin;

import com.googlebook.fsrbridge.DiagnosticTrace;
import net.irisshaders.iris.pathways.HandRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Trace-only hand boundaries. The role observes the current framebuffer and
 * the first/last actual draw; target routing is untouched. Optional so a
 * different Iris signature does not disable the adapter.
 */
@Mixin(HandRenderer.class)
public abstract class HandRendererMixin {
    static {
        DiagnosticTrace.hook("HandRenderer");
    }

    @Inject(method = "renderSolid", at = @At("HEAD"), require = 0)
    private void mgfsr3$solidHead(CallbackInfo ci) {
        DiagnosticTrace.startRole("HAND_SOLID", true);
    }

    @Inject(method = "renderSolid", at = @At("RETURN"), require = 0)
    private void mgfsr3$solidReturn(CallbackInfo ci) {
        DiagnosticTrace.endRole("HAND_SOLID_RETURN", null);
    }

    @Inject(method = "renderTranslucent", at = @At("HEAD"), require = 0)
    private void mgfsr3$translucentHead(CallbackInfo ci) {
        DiagnosticTrace.startRole("HAND_TRANSLUCENT", true);
    }

    @Inject(method = "renderTranslucent", at = @At("RETURN"), require = 0)
    private void mgfsr3$translucentReturn(CallbackInfo ci) {
        DiagnosticTrace.endRole("HAND_TRANSLUCENT_RETURN", null);
    }
}

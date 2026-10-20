package com.googlebook.fsrbridge.mixin;

import com.googlebook.fsrbridge.DiagnosticTrace;
import net.irisshaders.iris.pipeline.CompositeRenderer;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/** Trace-only composite/light pass boundary; optional across Iris versions. */
@Mixin(CompositeRenderer.class)
public abstract class CompositeRendererMixin {
    static {
        DiagnosticTrace.hook("CompositeRenderer");
    }

    @Inject(method = "renderAll", at = @At("HEAD"), require = 0)
    private void mgfsr3$compositeHead(CallbackInfo ci) {
        DiagnosticTrace.startRole("COMPOSITE", true);
    }

    @Inject(method = "renderAll", at = @At("RETURN"), require = 0)
    private void mgfsr3$compositeReturn(CallbackInfo ci) {
        DiagnosticTrace.endRole("COMPOSITE_RETURN", null);
    }
}

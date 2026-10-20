package com.googlebook.fsrbridge.mixin;

import com.googlebook.fsrbridge.Fsr3Bridge;
import net.irisshaders.iris.uniforms.ViewportUniforms;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/** Report the reduced scene extent to world/composite programs, but native
 * extent to the final pass and UI. */
@Mixin(ViewportUniforms.class)
public abstract class ViewportUniformsMixin {
    static {
        Fsr3Bridge.mark("ViewportUniformsMixin loaded");
    }

    @Inject(method = "lambda$addViewportUniforms$0", at = @At("RETURN"), cancellable = true)
    private static void mgfsr3$height(CallbackInfoReturnable<Integer> cir) {
        cir.setReturnValue(Fsr3Bridge.sceneHeight(cir.getReturnValue()));
    }

    @Inject(method = "lambda$addViewportUniforms$1", at = @At("RETURN"), cancellable = true)
    private static void mgfsr3$width(CallbackInfoReturnable<Integer> cir) {
        cir.setReturnValue(Fsr3Bridge.sceneWidth(cir.getReturnValue()));
    }
}

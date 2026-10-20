package com.googlebook.fsrbridge.mixin;

import com.googlebook.fsrbridge.Fsr3Bridge;
import net.irisshaders.iris.pipeline.IrisRenderingPipeline;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/** Pipeline replacement marks a new world/shader generation. */
@Mixin(IrisRenderingPipeline.class)
public abstract class IrisRenderingPipelineMixin {
    static {
        Fsr3Bridge.mark("IrisRenderingPipelineMixin loaded");
    }

    @Inject(method = "<init>", at = @At("RETURN"))
    private void mgfsr3$newPipeline(CallbackInfo ci) {
        Fsr3Bridge.invalidateCamera();
    }

    @Inject(method = "destroy", at = @At("HEAD"))
    private void mgfsr3$destroyPipeline(CallbackInfo ci) {
        Fsr3Bridge.invalidateCamera();
    }
}

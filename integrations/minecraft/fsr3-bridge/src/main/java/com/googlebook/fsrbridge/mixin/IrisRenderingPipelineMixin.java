package com.googlebook.fsrbridge.mixin;

import com.googlebook.fsrbridge.DiagnosticTrace;
import com.googlebook.fsrbridge.Fsr3Bridge;
import net.irisshaders.iris.pipeline.IrisRenderingPipeline;
import net.irisshaders.iris.pipeline.WorldRenderingPhase;
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
        Fsr3Bridge.destroySceneDepth();
        DiagnosticTrace.finish();
    }

    @Inject(method = "beginLevelRendering", at = @At("HEAD"), require = 0)
    private void mgfsr3$traceLevel(CallbackInfo ci) {
        DiagnosticTrace.beginFrame("LEVEL");
    }

    @Inject(method = "beginLevelRendering", at = @At("RETURN"), require = 0)
    private void mgfsr3$clearSceneDepth(CallbackInfo ci) {
        Fsr3Bridge.clearSceneDepth();
    }

    @Inject(method = "setPhase", at = @At("HEAD"), require = 0)
    private void mgfsr3$tracePhase(WorldRenderingPhase phase, CallbackInfo ci) {
        DiagnosticTrace.phase(phase.name());
    }

    @Inject(method = "beginHand", at = @At("HEAD"), require = 0)
    private void mgfsr3$traceBeginHand(CallbackInfo ci) {
        DiagnosticTrace.plain("BEGIN_HAND", null);
    }

    @Inject(method = "beginTranslucents", at = @At("HEAD"), require = 0)
    private void mgfsr3$traceBeginTranslucents(CallbackInfo ci) {
        DiagnosticTrace.plain("BEGIN_TRANSLUCENTS", null);
    }

    @Inject(method = "finalizeGameRendering", at = @At("HEAD"), require = 0)
    private void mgfsr3$tracePreHud(CallbackInfo ci) {
        DiagnosticTrace.plain("PRE_HUD", null);
    }
}

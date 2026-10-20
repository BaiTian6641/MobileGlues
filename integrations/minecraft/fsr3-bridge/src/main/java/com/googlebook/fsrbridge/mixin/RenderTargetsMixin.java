package com.googlebook.fsrbridge.mixin;

import com.googlebook.fsrbridge.Fsr3Bridge;
import net.irisshaders.iris.targets.RenderTargets;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.ModifyVariable;

/**
 * Optional Minecraft/Iris scene adapter. The constructor and the per-frame
 * resize path are the only places where Iris allocates the main scene targets,
 * so scaling here keeps the color, alt and depth textures coherent. The final
 * pass and HUD remain native.
 */
@Mixin(RenderTargets.class)
public abstract class RenderTargetsMixin {
    static {
        Fsr3Bridge.mark("RenderTargetsMixin loaded");
    }

    @ModifyVariable(method = "<init>", at = @At("HEAD"), argsOnly = true, ordinal = 0)
    private int mgfsr3$constructorWidth(int width) {
        return Fsr3Bridge.sceneWidth(width);
    }

    @ModifyVariable(method = "<init>", at = @At("HEAD"), argsOnly = true, ordinal = 1)
    private int mgfsr3$constructorHeight(int height) {
        return Fsr3Bridge.sceneHeight(height);
    }

    // Iris 1.8.14 signature is (depthBufferVersion, depthTextureId, width, height, ...).
    // The first two ints are not dimensions; touching them corrupts resize state.
    @ModifyVariable(method = "resizeIfNeeded", at = @At("HEAD"), argsOnly = true, ordinal = 2)
    private int mgfsr3$resizeWidth(int width) {
        return Fsr3Bridge.sceneWidth(width);
    }

    @ModifyVariable(method = "resizeIfNeeded", at = @At("HEAD"), argsOnly = true, ordinal = 3)
    private int mgfsr3$resizeHeight(int height) {
        return Fsr3Bridge.sceneHeight(height);
    }
}

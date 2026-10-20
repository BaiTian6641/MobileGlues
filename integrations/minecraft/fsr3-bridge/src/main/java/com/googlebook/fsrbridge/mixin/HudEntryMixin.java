package com.googlebook.fsrbridge.mixin;

import com.googlebook.fsrbridge.DiagnosticTrace;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * Optional, version-scoped HUD boundary for the 1.21.1 NeoForge diagnostic
 * target. Other Minecraft versions simply skip it (require=0); PRE_HUD remains
 * the cross-version Iris boundary. No Minecraft type is referenced at compile
 * time.
 */
@Mixin(targets = "net.minecraft.client.gui.Gui", remap = false)
public abstract class HudEntryMixin {
    static {
        DiagnosticTrace.hook("HudEntry");
    }

    @Inject(method = "render(Lnet/minecraft/client/gui/GuiGraphics;Lnet/minecraft/client/DeltaTracker;)V",
            at = @At("HEAD"), require = 0, remap = false)
    private void mgfsr3$hudEntry(CallbackInfo ci) {
        DiagnosticTrace.hudEntry();
    }
}

#!/bin/sh
# Host-only descriptor check for the Experiment A diagnostic mixins against the
# exact Iris delivery JAR. Minecraft classes are intentionally not required:
# the HUD hook is version-scoped and optional (require=0).
set -eu
IRIS_JAR="${1:-/tmp/iris-1.8.14.jar}"
[ -f "$IRIS_JAR" ] || { echo "missing Iris JAR: $IRIS_JAR" >&2; exit 1; }
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

fail=0
check() { # class method-or-field needle
    javap -classpath "$IRIS_JAR" -p "$1" > "$TMP/$1.txt" 2>/dev/null || {
        echo "MISSING CLASS $1"; fail=1; return; }
    if ! grep -Fq "$3" "$TMP/$1.txt"; then
        echo "MISSING $1 :: $3"; fail=1
    fi
}
check net.irisshaders.iris.targets.RenderTargets method 'copyPreHandDepth()'
check net.irisshaders.iris.targets.RenderTargets method 'copyPreTranslucentDepth()'
check net.irisshaders.iris.targets.RenderTargets method 'resizeIfNeeded('
check net.irisshaders.iris.targets.RenderTargets field 'handDepthDirty'
check net.irisshaders.iris.targets.RenderTargets field 'translucentDepthDirty'
check net.irisshaders.iris.targets.RenderTargets field 'copyStrategy'
check net.irisshaders.iris.pathways.HandRenderer method 'renderSolid('
check net.irisshaders.iris.pathways.HandRenderer method 'renderTranslucent('
check net.irisshaders.iris.pipeline.IrisRenderingPipeline method 'beginLevelRendering()'
check net.irisshaders.iris.pipeline.IrisRenderingPipeline method 'beginHand()'
check net.irisshaders.iris.pipeline.IrisRenderingPipeline method 'beginTranslucents()'
check net.irisshaders.iris.pipeline.IrisRenderingPipeline method 'setPhase('
check net.irisshaders.iris.pipeline.IrisRenderingPipeline method 'finalizeGameRendering()'
check net.irisshaders.iris.pipeline.FinalPassRenderer method 'renderFinalPass()'
check net.irisshaders.iris.shadows.ShadowRenderer method 'renderShadows('
check net.irisshaders.iris.shadows.ShadowRenderer method 'setupShadowViewport()'
check net.irisshaders.iris.pipeline.CompositeRenderer method 'renderAll()'
check net.irisshaders.iris.uniforms.ViewportUniforms method 'lambda$addViewportUniforms$0'
check net.irisshaders.iris.uniforms.ViewportUniforms method 'lambda$addViewportUniforms$1'

echo "Iris JAR: $IRIS_JAR"
echo "HUD hook: unverified here (optional Minecraft class, require=0)"
if [ "$fail" -ne 0 ]; then exit 1; fi
echo "descriptor checks: PASS"

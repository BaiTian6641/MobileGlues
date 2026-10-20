// MobileGlues - upscaling/fsr3/fsr3_shaders.h
// Generated from AMD FidelityFX SDK v1.1.4, FSR3.1.4, commit
// c6efa6bf7f2027b3ec94f28578bb5965eabb9e55. The adapted ESSL is a
// derivative of AMD's MIT-licensed shader source; the MIT notice and
// source manifest are in docs/fsr3/audits/. Do not edit these strings by
// hand: regenerate with tools/fsr3_spike/dispatch/make_shaders.sh.
// SPDX-License-Identifier: MIT
#pragma once

namespace fsr3_shaders {

// prepare_inputs_essl320.glsl sha256 a624b575604835ed99cc82bd65c43bcdc952db74437ffd4029c8417b20669960
inline constexpr const char* kPrepareInputs = R"FSR3(#version 320 es
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

struct BilinearSamplingData
{
    ivec2 iOffsets[4];
    float fWeights[4];
    ivec2 iBasePos;
};

struct DepthExtents
{
    float fNearest;
    ivec2 fNearestCoord;
    float fFarthest;
};

const ivec2 _528[9] = ivec2[](ivec2(0), ivec2(1, 0), ivec2(0, 1), ivec2(0, -1), ivec2(-1, 0), ivec2(-1, 1), ivec2(1), ivec2(-1), ivec2(1, -1));

layout(binding = 8, std140) uniform cbFSR3UPSCALER_t
{
    ivec2 iRenderSize;
    ivec2 iPreviousFrameRenderSize;
    ivec2 iUpscaleSize;
    ivec2 iPreviousFrameUpscaleSize;
    ivec2 iMaxRenderSize;
    ivec2 iMaxUpscaleSize;
    vec4 fDeviceToViewDepth;
    vec2 fJitter;
    vec2 fPreviousFrameJitter;
    vec2 fMotionVectorScale;
    vec2 fDownscaleFactor;
    vec2 fMotionVectorJitterCancellation;
    float fTanHalfFOV;
    float fJitterSequenceLength;
    float fDeltaTime;
    float fDeltaPreExposure;
    float fViewSpaceToMetersFactor;
    float fFrameIndex;
    float fVelocityFactor;
    float fReactivenessScale;
    float fShadingChangeScale;
    float fAccumulationAddedPerFrame;
    float fMinDisocclusionAccumulation;
} cbFSR3Upscaler;

layout(binding = 6, rgba16f) uniform writeonly highp image2D rw_farthest_depth;
layout(binding = 7, rgba16f) uniform writeonly highp image2D rw_current_luma;
layout(binding = 5, r32ui) uniform highp uimage2D rw_reconstructed_previous_nearest_depth;
layout(binding = 4, r32f) uniform writeonly highp image2D rw_dilated_depth;
layout(binding = 3, rgba16f) uniform writeonly highp image2D rw_dilated_motion_vectors;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_depthSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_motion_vectorsSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_color_jitteredSPIRV_Cross_DummySampler;

float LoadInputDepth(ivec2 iPxPos)
{
    return texelFetch(SPIRV_Cross_Combinedr_input_depthSPIRV_Cross_DummySampler, iPxPos, 0).x;
}

ivec2 RenderSize()
{
    return cbFSR3Upscaler.iRenderSize;
}

bool IsOnScreen(ivec2 pos, ivec2 size)
{
    return all(lessThan(uvec2(pos), uvec2(size)));
}

float ffxMax(float x, float y)
{
    return max(x, y);
}

DepthExtents FindDepthExtents(ivec2 iPxPos)
{
    int iSampleIndex = 0;
    iSampleIndex = 0;
    float depth[9];
    for (; iSampleIndex < 9; iSampleIndex++)
    {
        ivec2 iPos = iPxPos + _528[iSampleIndex];
        ivec2 param = iPos;
        depth[iSampleIndex] = LoadInputDepth(param);
    }
    DepthExtents extents;
    extents.fNearestCoord = iPxPos;
    extents.fNearest = depth[0];
    extents.fFarthest = depth[0];
    iSampleIndex = 1;
    for (; iSampleIndex < 9; iSampleIndex++)
    {
        ivec2 iPos_1 = iPxPos + _528[iSampleIndex];
        ivec2 param_1 = iPos_1;
        ivec2 param_2 = RenderSize();
        if (IsOnScreen(param_1, param_2))
        {
            float fNdDepth = depth[iSampleIndex];
            if (fNdDepth < extents.fNearest)
            {
                float param_3 = extents.fFarthest;
                float param_4 = fNdDepth;
                extents.fFarthest = ffxMax(param_3, param_4);
                extents.fNearestCoord = iPos_1;
                extents.fNearest = fNdDepth;
            }
        }
    }
    return extents;
}

vec2 Jitter()
{
    return cbFSR3Upscaler.fJitter;
}

ivec2 UpscaleSize()
{
    return cbFSR3Upscaler.iUpscaleSize;
}

ivec2 ComputeHrPosFromLrPos(ivec2 iPxLrPos)
{
    vec2 fSrcJitteredPos = (vec2(iPxLrPos) + vec2(0.5)) - Jitter();
    vec2 fLrPosInHr = (fSrcJitteredPos / vec2(RenderSize())) * vec2(UpscaleSize());
    ivec2 iPxHrPos = ivec2(floor(fLrPosInHr));
    return iPxHrPos;
}

vec2 MotionVectorScale()
{
    return cbFSR3Upscaler.fMotionVectorScale;
}

vec2 LoadInputMotionVector(ivec2 iPxDilatedMotionVectorPos)
{
    vec2 fSrcMotionVector = texelFetch(SPIRV_Cross_Combinedr_input_motion_vectorsSPIRV_Cross_DummySampler, iPxDilatedMotionVectorPos, 0).xy;
    vec2 fUvMotionVector = fSrcMotionVector * MotionVectorScale();
    return fUvMotionVector;
}

vec2 DilateMotionVector(ivec2 iPxPos, DepthExtents depthExtents)
{
    ivec2 param = iPxPos;
    ivec2 iSamplePos = ComputeHrPosFromLrPos(param);
    ivec2 param_1 = depthExtents.fNearestCoord;
    ivec2 iMotionVectorPos = ComputeHrPosFromLrPos(param_1);
    ivec2 param_2 = iMotionVectorPos;
    vec2 fDilatedMotionVector = LoadInputMotionVector(param_2);
    return fDilatedMotionVector;
}

vec4 DeviceToViewSpaceTransformFactors()
{
    return cbFSR3Upscaler.fDeviceToViewDepth;
}

float GetViewSpaceDepth(float fDeviceDepth)
{
    vec4 fDeviceToViewDepth = DeviceToViewSpaceTransformFactors();
    return fDeviceToViewDepth.y / (fDeviceDepth - fDeviceToViewDepth.x);
}

float ViewSpaceToMetersFactor()
{
    return cbFSR3Upscaler.fViewSpaceToMetersFactor;
}

float GetViewSpaceDepthInMeters(float fDeviceDepth)
{
    float param = fDeviceDepth;
    return GetViewSpaceDepth(param) * ViewSpaceToMetersFactor();
}

float ffxMin(float x, float y)
{
    return min(x, y);
}

float ffxSaturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

float ffxLerp(float x, float y, float t)
{
    return mix(x, y, t);
}

float ReconstructedDepthMvPxThreshold(float fNearestDepthInMeters)
{
    float param = fNearestDepthInMeters / 100.0;
    float param_1 = 0.25;
    float param_2 = 0.75;
    float param_3 = ffxSaturate(param);
    return ffxLerp(param_1, param_2, param_3);
}

float Get4KVelocity(vec2 fMotionVector)
{
    return length(fMotionVector * vec2(3840.0, 2160.0));
}

vec2 ffxFract(vec2 x)
{
    return fract(x);
}

BilinearSamplingData GetBilinearSamplingData(vec2 fUv, ivec2 iSize)
{
    vec2 fPxSample = (fUv * vec2(iSize)) - vec2(0.5);
    BilinearSamplingData data;
    data.iBasePos = ivec2(floor(fPxSample));
    vec2 param = fPxSample;
    vec2 fPxFrac = ffxFract(param);
    data.iOffsets[0] = ivec2(0);
    data.iOffsets[1] = ivec2(1, 0);
    data.iOffsets[2] = ivec2(0, 1);
    data.iOffsets[3] = ivec2(1);
    data.fWeights[0] = (1.0 - fPxFrac.x) * (1.0 - fPxFrac.y);
    data.fWeights[1] = fPxFrac.x * (1.0 - fPxFrac.y);
    data.fWeights[2] = (1.0 - fPxFrac.x) * fPxFrac.y;
    data.fWeights[3] = fPxFrac.x * fPxFrac.y;
    return data;
}

void StoreReconstructedDepth(ivec2 iPxSample, float fDepth)
{
    uint uDepth = floatBitsToUint(fDepth);
    uint _276 = imageAtomicMin(rw_reconstructed_previous_nearest_depth, iPxSample, uDepth);
}

void ReconstructPrevDepth(ivec2 iPxPos, float fDepth, inout vec2 fMotionVector)
{
    float param = fDepth;
    float param_1 = GetViewSpaceDepthInMeters(param);
    float param_2 = 65504.0;
    float fNearestDepthInMeters = ffxMin(param_1, param_2);
    float param_3 = fNearestDepthInMeters;
    float fReconstructedDeptMvThreshold = ReconstructedDepthMvPxThreshold(param_3);
    vec2 param_4 = fMotionVector;
    fMotionVector *= float(Get4KVelocity(param_4) > fReconstructedDeptMvThreshold);
    vec2 fUv = (vec2(iPxPos) + vec2(0.5)) / vec2(RenderSize());
    vec2 fReprojectedUv = fUv + fMotionVector;
    vec2 param_5 = fReprojectedUv;
    ivec2 param_6 = RenderSize();
    BilinearSamplingData bilinearInfo = GetBilinearSamplingData(param_5, param_6);
    for (int iSampleIndex = 0; iSampleIndex < 4; iSampleIndex++)
    {
        ivec2 iOffset = bilinearInfo.iOffsets[iSampleIndex];
        float fWeight = bilinearInfo.fWeights[iSampleIndex];
        if (fWeight > 0.0006099999882280826568603515625)
        {
            ivec2 iStorePos = bilinearInfo.iBasePos + iOffset;
            ivec2 param_7 = iStorePos;
            ivec2 param_8 = RenderSize();
            if (IsOnScreen(param_7, param_8))
            {
                ivec2 param_9 = iStorePos;
                float param_10 = fDepth;
                StoreReconstructedDepth(param_9, param_10);
            }
        }
    }
}

void StoreDilatedMotionVector(ivec2 iPxPos, vec2 fMotionVector)
{
    imageStore(rw_dilated_motion_vectors, iPxPos, vec4(fMotionVector, 0.0, 0.0));
}

void StoreDilatedDepth(ivec2 iPxPos, float fDepth)
{
    imageStore(rw_dilated_depth, iPxPos, vec4(fDepth, 0.0, 0.0, 0.0));
}

void StoreFarthestDepth(ivec2 iPxPos, float fDepth)
{
    imageStore(rw_farthest_depth, iPxPos, vec4(fDepth, 0.0, 0.0, 0.0));
}

vec3 LoadInputColor(ivec2 iPxPos)
{
    return texelFetch(SPIRV_Cross_Combinedr_input_color_jitteredSPIRV_Cross_DummySampler, iPxPos, 0).xyz;
}

vec3 ffxMax(vec3 x, vec3 y)
{
    return max(x, y);
}

float RGBToLuma(vec3 fLinearRgb)
{
    return dot(fLinearRgb, vec3(0.2125999927520751953125, 0.715200006961822509765625, 0.072200000286102294921875));
}

float GetCurrentFrameLuma(ivec2 iPxPos)
{
    ivec2 param = iPxPos;
    vec3 param_1 = vec3(0.0);
    vec3 param_2 = LoadInputColor(param);
    vec3 fRgb = ffxMax(param_1, param_2);
    vec3 param_3 = fRgb;
    float fLuma = RGBToLuma(param_3);
    return fLuma;
}

void StoreCurrentLuma(ivec2 iPxPos, float fLuma)
{
    imageStore(rw_current_luma, iPxPos, vec4(fLuma, 0.0, 0.0, 0.0));
}

void PrepareInputs(ivec2 iPxPos)
{
    ivec2 param = iPxPos;
    DepthExtents depthExtents = FindDepthExtents(param);
    ivec2 param_1 = iPxPos;
    vec2 fDilatedMotionVector = DilateMotionVector(param_1, depthExtents);
    ivec2 param_2 = iPxPos;
    float param_3 = depthExtents.fNearest;
    vec2 param_4 = fDilatedMotionVector;
    ReconstructPrevDepth(param_2, param_3, param_4);
    ivec2 param_5 = iPxPos;
    vec2 param_6 = fDilatedMotionVector;
    StoreDilatedMotionVector(param_5, param_6);
    ivec2 param_7 = iPxPos;
    float param_8 = depthExtents.fNearest;
    StoreDilatedDepth(param_7, param_8);
    float param_9 = depthExtents.fFarthest;
    float param_10 = GetViewSpaceDepthInMeters(param_9);
    float param_11 = 65504.0;
    float fFarthestDepthInMeters = ffxMin(param_10, param_11);
    ivec2 param_12 = iPxPos;
    float param_13 = fFarthestDepthInMeters;
    StoreFarthestDepth(param_12, param_13);
    ivec2 param_14 = iPxPos;
    float fLuma = GetCurrentFrameLuma(param_14);
    ivec2 param_15 = iPxPos;
    float param_16 = fLuma;
    StoreCurrentLuma(param_15, param_16);
}

void main()
{
    ivec2 param = ivec2(gl_GlobalInvocationID.xy);
    PrepareInputs(param);
}

)FSR3";

// luma_pyramid_essl320.glsl sha256 25c6aebaae4835eff3bcc3362c5bb73ab20018b0a3dd9cf4358cc36862c03c56
inline constexpr const char* kLumaPyramid = R"FSR3(#version 320 es
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(binding = 11, std140) uniform cbFSR3UPSCALER_t
{
    ivec2 iRenderSize;
    ivec2 iPreviousFrameRenderSize;
    ivec2 iUpscaleSize;
    ivec2 iPreviousFrameUpscaleSize;
    ivec2 iMaxRenderSize;
    ivec2 iMaxUpscaleSize;
    vec4 fDeviceToViewDepth;
    vec2 fJitter;
    vec2 fPreviousFrameJitter;
    vec2 fMotionVectorScale;
    vec2 fDownscaleFactor;
    vec2 fMotionVectorJitterCancellation;
    float fTanHalfFOV;
    float fJitterSequenceLength;
    float fDeltaTime;
    float fDeltaPreExposure;
    float fViewSpaceToMetersFactor;
    float fFrameIndex;
    float fVelocityFactor;
    float fReactivenessScale;
    float fShadingChangeScale;
    float fAccumulationAddedPerFrame;
    float fMinDisocclusionAccumulation;
} cbFSR3Upscaler;

layout(binding = 12, std140) uniform cbSPD_t
{
    uint mips;
    uint numWorkGroups;
    uvec2 workGroupOffset;
    uvec2 renderSize;
} cbSPD;

layout(binding = 10, rgba16f) uniform writeonly highp image2D rw_farthest_depth_mip1;
layout(binding = 3, rgba32f) uniform coherent readonly highp image2D r_rw_frame_info;
layout(binding = 22, rgba32f) uniform coherent writeonly highp image2D rw_frame_info;
layout(binding = 4, rgba16f) uniform coherent readonly highp image2D r_rw_spd_mip0;
layout(binding = 5, rgba16f) uniform coherent readonly highp image2D r_rw_spd_mip1;
layout(binding = 6, rgba16f) uniform coherent readonly highp image2D r_rw_spd_mip2;
layout(binding = 7, rgba16f) uniform coherent readonly highp image2D r_rw_spd_mip3;
layout(binding = 8, rgba16f) uniform coherent readonly highp image2D r_rw_spd_mip4;
layout(binding = 9, rgba16f) uniform coherent readonly highp image2D r_rw_spd_mip5;
layout(binding = 24, rgba16f) uniform coherent writeonly highp image2D rw_spd_mip0;
layout(binding = 25, rgba16f) uniform coherent writeonly highp image2D rw_spd_mip1;
layout(binding = 26, rgba16f) uniform coherent writeonly highp image2D rw_spd_mip2;
layout(binding = 27, rgba16f) uniform coherent writeonly highp image2D rw_spd_mip3;
layout(binding = 28, rgba16f) uniform coherent writeonly highp image2D rw_spd_mip4;
layout(binding = 29, rgba16f) uniform coherent writeonly highp image2D rw_spd_mip5;
layout(binding = 2, r32ui) uniform coherent highp uimage2D rw_spd_global_atomic;
uniform highp sampler2D SPIRV_Cross_Combinedr_current_lumaSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_farthest_depthSPIRV_Cross_DummySampler;

shared uint spdCounter;
shared float spdIntermediateR[16][16];
shared float spdIntermediateG[16][16];
shared float spdIntermediateB[16][16];
shared float spdIntermediateA[16][16];

uint MipCount()
{
    return cbSPD.mips;
}

uint NumWorkGroups()
{
    return cbSPD.numWorkGroups;
}

uvec2 WorkGroupOffset()
{
    return cbSPD.workGroupOffset;
}

uint ffxBitfieldExtract(uint src, uint off, uint bits)
{
    return bitfieldExtract(src, int(off), int(bits));
}

uint ffxBitfieldInsertMask(uint src, uint ins, uint bits)
{
    return bitfieldInsert(src, ins, 0, int(bits));
}

uvec2 ffxRemapForWaveReduction(uint a)
{
    uint param = a;
    uint param_1 = 2u;
    uint param_2 = 3u;
    uint param_3 = ffxBitfieldExtract(param, param_1, param_2);
    uint param_4 = a;
    uint param_5 = 1u;
    uint param_6 = a;
    uint param_7 = 3u;
    uint param_8 = 3u;
    uint param_9 = a;
    uint param_10 = 1u;
    uint param_11 = 2u;
    uint param_12 = ffxBitfieldExtract(param_6, param_7, param_8);
    uint param_13 = ffxBitfieldExtract(param_9, param_10, param_11);
    uint param_14 = 2u;
    return uvec2(ffxBitfieldInsertMask(param_3, param_4, param_5), ffxBitfieldInsertMask(param_12, param_13, param_14));
}

ivec2 RenderSize()
{
    return cbFSR3Upscaler.iRenderSize;
}

int ffxMin(int x, int y)
{
    return min(x, y);
}

int ffxMax(int x, int y)
{
    return max(x, y);
}

ivec2 ClampLoad(ivec2 iPxSample, ivec2 iPxOffset, ivec2 iTextureSize)
{
    ivec2 result = iPxSample + iPxOffset;
    int param = result.x;
    int param_1 = iTextureSize.x - 1;
    int param_2 = 0;
    int param_3 = ffxMin(param, param_1);
    result.x = ffxMax(param_2, param_3);
    int param_4 = result.y;
    int param_5 = iTextureSize.y - 1;
    int param_6 = 0;
    int param_7 = ffxMin(param_4, param_5);
    result.y = ffxMax(param_6, param_7);
    return result;
}

float LoadCurrentLuma(ivec2 iPxPos)
{
    return texelFetch(SPIRV_Cross_Combinedr_current_lumaSPIRV_Cross_DummySampler, iPxPos, 0).x;
}

float ffxMax(float x, float y)
{
    return max(x, y);
}

float LoadFarthestDepth(ivec2 iPxPos)
{
    return texelFetch(SPIRV_Cross_Combinedr_farthest_depthSPIRV_Cross_DummySampler, iPxPos, 0).x;
}

vec4 SpdLoadSourceImage(vec2 iPxPos, uint slice)
{
    ivec2 param = ivec2(iPxPos);
    ivec2 param_1 = ivec2(0);
    ivec2 param_2 = RenderSize();
    ivec2 iPxSamplePos = ClampLoad(param, param_1, param_2);
    ivec2 param_3 = iPxSamplePos;
    float fLuma = LoadCurrentLuma(param_3);
    float param_4 = 6.099999882280826568603515625e-05;
    float param_5 = log(fLuma);
    float fLogLuma = ffxMax(param_4, param_5);
    ivec2 param_6 = iPxSamplePos;
    float fFarthestDepthInMeters = LoadFarthestDepth(param_6);
    vec4 fOutput = vec4(0.0);
    fOutput.x = fLogLuma;
    fOutput.y = fLuma;
    fOutput.z = fFarthestDepthInMeters;
    return fOutput;
}

vec4 SpdReduce4(vec4 v0, vec4 v1, vec4 v2, vec4 v3)
{
    return (((v0 + v1) + v2) + v3) * 0.25;
}

vec4 SpdReduceLoadSourceImage4(uvec2 i0, uvec2 i1, uvec2 i2, uvec2 i3, uint slice)
{
    vec2 param = vec2(ivec2(i0));
    uint param_1 = slice;
    vec4 v0 = SpdLoadSourceImage(param, param_1);
    vec2 param_2 = vec2(ivec2(i1));
    uint param_3 = slice;
    vec4 v1 = SpdLoadSourceImage(param_2, param_3);
    vec2 param_4 = vec2(ivec2(i2));
    uint param_5 = slice;
    vec4 v2 = SpdLoadSourceImage(param_4, param_5);
    vec2 param_6 = vec2(ivec2(i3));
    uint param_7 = slice;
    vec4 v3 = SpdLoadSourceImage(param_6, param_7);
    vec4 param_8 = v0;
    vec4 param_9 = v1;
    vec4 param_10 = v2;
    vec4 param_11 = v3;
    return SpdReduce4(param_8, param_9, param_10, param_11);
}

vec4 SpdReduceLoadSourceImage(uvec2 base, uint slice)
{
    uvec2 param = base + uvec2(0u);
    uvec2 param_1 = base + uvec2(0u, 1u);
    uvec2 param_2 = base + uvec2(1u, 0u);
    uvec2 param_3 = base + uvec2(1u);
    uint param_4 = slice;
    return SpdReduceLoadSourceImage4(param, param_1, param_2, param_3, param_4);
}

void StorePyramid(ivec2 iPxPos, vec2 outValue, uint index)
{
    if (index == 0u)
    {
        imageStore(rw_spd_mip0, iPxPos, vec4(outValue, 0.0, 0.0));
    }
    if (index == 1u)
    {
        imageStore(rw_spd_mip1, iPxPos, vec4(outValue, 0.0, 0.0));
    }
    if (index == 2u)
    {
        imageStore(rw_spd_mip2, iPxPos, vec4(outValue, 0.0, 0.0));
    }
    if (index == 3u)
    {
        imageStore(rw_spd_mip3, iPxPos, vec4(outValue, 0.0, 0.0));
    }
    if (index == 4u)
    {
        imageStore(rw_spd_mip4, iPxPos, vec4(outValue, 0.0, 0.0));
    }
    if (index == 5u)
    {
        imageStore(rw_spd_mip5, iPxPos, vec4(outValue, 0.0, 0.0));
    }
}

void StoreFarthestDepthMip1(ivec2 iPxPos, float fDepth)
{
    imageStore(rw_farthest_depth_mip1, iPxPos, vec4(fDepth, 0.0, 0.0, 0.0));
}

vec4 LoadFrameInfo()
{
    return imageLoad(r_rw_frame_info, ivec2(0));
}

float DeltaTime()
{
    return cbFSR3Upscaler.fDeltaTime;
}

float ffxPow(float x, float y)
{
    return pow(x, y);
}

float ComputeAutoExposureFromLavg(inout float Lavg)
{
    Lavg = exp(Lavg);
    float ExposureISO100 = log2((Lavg * 100.0) / 12.5);
    float param = 2.0;
    float param_1 = ExposureISO100;
    float Lmax = 1.2000000476837158203125 * ffxPow(param, param_1);
    return 1.0 / Lmax;
}

void StoreFrameInfo(vec4 fInfo)
{
    imageStore(rw_frame_info, ivec2(0), fInfo);
}

void SpdStore(ivec2 pix, vec4 outValue, uint index, uint slice)
{
    if (index == 5u)
    {
        ivec2 param = pix;
        vec2 param_1 = outValue.xy;
        uint param_2 = index;
        StorePyramid(param, param_1, param_2);
    }
    else
    {
        if (index == 0u)
        {
            ivec2 param_3 = pix;
            float param_4 = outValue.z;
            StoreFarthestDepthMip1(param_3, param_4);
        }
    }
    if (index == (MipCount() - 1u))
    {
        if (all(equal(pix, ivec2(0))))
        {
            vec4 frameInfo = LoadFrameInfo();
            float fSceneAvgLuma = outValue.y;
            float fPrevLogLuma = frameInfo.y;
            float fLogLuma = outValue.x;
            if (fPrevLogLuma < 10000.0)
            {
                fLogLuma = fPrevLogLuma + ((fLogLuma - fPrevLogLuma) * (1.0 - exp(-DeltaTime())));
                float param_5 = 0.0;
                float param_6 = fLogLuma;
                fLogLuma = ffxMax(param_5, param_6);
            }
            float param_7 = fLogLuma;
            float _744 = ComputeAutoExposureFromLavg(param_7);
            frameInfo.x = _744;
            frameInfo.y = fLogLuma;
            frameInfo.z = fSceneAvgLuma;
            vec4 param_8 = frameInfo;
            StoreFrameInfo(param_8);
        }
    }
}

void SpdStoreIntermediate(uint x, uint y, vec4 value)
{
    spdIntermediateR[x][y] = value.x;
    spdIntermediateG[x][y] = value.y;
    spdIntermediateB[x][y] = value.z;
    spdIntermediateA[x][y] = value.w;
}

void ffxSpdWorkgroupShuffleBarrier()
{
    groupMemoryBarrier();
    barrier();
}

vec4 SpdLoadIntermediate(uint x, uint y)
{
    return vec4(spdIntermediateR[x][y], spdIntermediateG[x][y], spdIntermediateB[x][y], spdIntermediateA[x][y]);
}

vec4 SpdReduceIntermediate(uvec2 i0, uvec2 i1, uvec2 i2, uvec2 i3)
{
    uint param = i0.x;
    uint param_1 = i0.y;
    vec4 v0 = SpdLoadIntermediate(param, param_1);
    uint param_2 = i1.x;
    uint param_3 = i1.y;
    vec4 v1 = SpdLoadIntermediate(param_2, param_3);
    uint param_4 = i2.x;
    uint param_5 = i2.y;
    vec4 v2 = SpdLoadIntermediate(param_4, param_5);
    uint param_6 = i3.x;
    uint param_7 = i3.y;
    vec4 v3 = SpdLoadIntermediate(param_6, param_7);
    vec4 param_8 = v0;
    vec4 param_9 = v1;
    vec4 param_10 = v2;
    vec4 param_11 = v3;
    return SpdReduce4(param_8, param_9, param_10, param_11);
}

void SpdDownsampleMips_0_1_LDS(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
    ivec2 tex = ivec2(workGroupID * uvec2(64u)) + ivec2(int(x * 2u), int(y * 2u));
    ivec2 pix = ivec2(workGroupID * uvec2(32u)) + ivec2(int(x), int(y));
    uvec2 param = uvec2(tex);
    uint param_1 = slice;
    vec4 v[4];
    v[0] = SpdReduceLoadSourceImage(param, param_1);
    ivec2 param_2 = pix;
    vec4 param_3 = v[0];
    uint param_4 = 0u;
    uint param_5 = slice;
    SpdStore(param_2, param_3, param_4, param_5);
    tex = ivec2(workGroupID * uvec2(64u)) + ivec2(int((x * 2u) + 32u), int(y * 2u));
    pix = ivec2(workGroupID * uvec2(32u)) + ivec2(int(x + 16u), int(y));
    uvec2 param_6 = uvec2(tex);
    uint param_7 = slice;
    v[1] = SpdReduceLoadSourceImage(param_6, param_7);
    ivec2 param_8 = pix;
    vec4 param_9 = v[1];
    uint param_10 = 0u;
    uint param_11 = slice;
    SpdStore(param_8, param_9, param_10, param_11);
    tex = ivec2(workGroupID * uvec2(64u)) + ivec2(int(x * 2u), int((y * 2u) + 32u));
    pix = ivec2(workGroupID * uvec2(32u)) + ivec2(int(x), int(y + 16u));
    uvec2 param_12 = uvec2(tex);
    uint param_13 = slice;
    v[2] = SpdReduceLoadSourceImage(param_12, param_13);
    ivec2 param_14 = pix;
    vec4 param_15 = v[2];
    uint param_16 = 0u;
    uint param_17 = slice;
    SpdStore(param_14, param_15, param_16, param_17);
    tex = ivec2(workGroupID * uvec2(64u)) + ivec2(int((x * 2u) + 32u), int((y * 2u) + 32u));
    pix = ivec2(workGroupID * uvec2(32u)) + ivec2(int(x + 16u), int(y + 16u));
    uvec2 param_18 = uvec2(tex);
    uint param_19 = slice;
    v[3] = SpdReduceLoadSourceImage(param_18, param_19);
    ivec2 param_20 = pix;
    vec4 param_21 = v[3];
    uint param_22 = 0u;
    uint param_23 = slice;
    SpdStore(param_20, param_21, param_22, param_23);
    if (mip <= 1u)
    {
        return;
    }
    for (uint i = 0u; i < 4u; i++)
    {
        uint param_24 = x;
        uint param_25 = y;
        vec4 param_26 = v[i];
        SpdStoreIntermediate(param_24, param_25, param_26);
        ffxSpdWorkgroupShuffleBarrier();
        if (localInvocationIndex < 64u)
        {
            uvec2 param_27 = uvec2((x * 2u) + 0u, (y * 2u) + 0u);
            uvec2 param_28 = uvec2((x * 2u) + 1u, (y * 2u) + 0u);
            uvec2 param_29 = uvec2((x * 2u) + 0u, (y * 2u) + 1u);
            uvec2 param_30 = uvec2((x * 2u) + 1u, (y * 2u) + 1u);
            v[i] = SpdReduceIntermediate(param_27, param_28, param_29, param_30);
            ivec2 param_31 = ivec2(workGroupID * uvec2(16u)) + ivec2(int(x + ((i % 2u) * 8u)), int(y + ((i / 2u) * 8u)));
            vec4 param_32 = v[i];
            uint param_33 = 1u;
            uint param_34 = slice;
            SpdStore(param_31, param_32, param_33, param_34);
        }
        ffxSpdWorkgroupShuffleBarrier();
    }
    if (localInvocationIndex < 64u)
    {
        uint param_35 = x + 0u;
        uint param_36 = y + 0u;
        vec4 param_37 = v[0];
        SpdStoreIntermediate(param_35, param_36, param_37);
        uint param_38 = x + 8u;
        uint param_39 = y + 0u;
        vec4 param_40 = v[1];
        SpdStoreIntermediate(param_38, param_39, param_40);
        uint param_41 = x + 0u;
        uint param_42 = y + 8u;
        vec4 param_43 = v[2];
        SpdStoreIntermediate(param_41, param_42, param_43);
        uint param_44 = x + 8u;
        uint param_45 = y + 8u;
        vec4 param_46 = v[3];
        SpdStoreIntermediate(param_44, param_45, param_46);
    }
}

void SpdDownsampleMips_0_1(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
    uint param = x;
    uint param_1 = y;
    uvec2 param_2 = workGroupID;
    uint param_3 = localInvocationIndex;
    uint param_4 = mip;
    uint param_5 = slice;
    SpdDownsampleMips_0_1_LDS(param, param_1, param_2, param_3, param_4, param_5);
}

void SpdDownsampleMip_2(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
    if (localInvocationIndex < 64u)
    {
        uvec2 param = uvec2((x * 2u) + 0u, (y * 2u) + 0u);
        uvec2 param_1 = uvec2((x * 2u) + 1u, (y * 2u) + 0u);
        uvec2 param_2 = uvec2((x * 2u) + 0u, (y * 2u) + 1u);
        uvec2 param_3 = uvec2((x * 2u) + 1u, (y * 2u) + 1u);
        vec4 v = SpdReduceIntermediate(param, param_1, param_2, param_3);
        ivec2 param_4 = ivec2(workGroupID * uvec2(8u)) + ivec2(int(x), int(y));
        vec4 param_5 = v;
        uint param_6 = mip;
        uint param_7 = slice;
        SpdStore(param_4, param_5, param_6, param_7);
        uint param_8 = (x * 2u) + (y % 2u);
        uint param_9 = y * 2u;
        vec4 param_10 = v;
        SpdStoreIntermediate(param_8, param_9, param_10);
    }
}

void SpdDownsampleMip_3(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
    if (localInvocationIndex < 16u)
    {
        uvec2 param = uvec2(((x * 4u) + 0u) + 0u, (y * 4u) + 0u);
        uvec2 param_1 = uvec2(((x * 4u) + 2u) + 0u, (y * 4u) + 0u);
        uvec2 param_2 = uvec2(((x * 4u) + 0u) + 1u, (y * 4u) + 2u);
        uvec2 param_3 = uvec2(((x * 4u) + 2u) + 1u, (y * 4u) + 2u);
        vec4 v = SpdReduceIntermediate(param, param_1, param_2, param_3);
        ivec2 param_4 = ivec2(workGroupID * uvec2(4u)) + ivec2(int(x), int(y));
        vec4 param_5 = v;
        uint param_6 = mip;
        uint param_7 = slice;
        SpdStore(param_4, param_5, param_6, param_7);
        uint param_8 = (x * 4u) + y;
        uint param_9 = y * 4u;
        vec4 param_10 = v;
        SpdStoreIntermediate(param_8, param_9, param_10);
    }
}

void SpdDownsampleMip_4(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
    if (localInvocationIndex < 4u)
    {
        uvec2 param = uvec2((((x * 8u) + 0u) + 0u) + (y * 2u), (y * 8u) + 0u);
        uvec2 param_1 = uvec2((((x * 8u) + 4u) + 0u) + (y * 2u), (y * 8u) + 0u);
        uvec2 param_2 = uvec2((((x * 8u) + 0u) + 1u) + (y * 2u), (y * 8u) + 4u);
        uvec2 param_3 = uvec2((((x * 8u) + 4u) + 1u) + (y * 2u), (y * 8u) + 4u);
        vec4 v = SpdReduceIntermediate(param, param_1, param_2, param_3);
        ivec2 param_4 = ivec2(workGroupID * uvec2(2u)) + ivec2(int(x), int(y));
        vec4 param_5 = v;
        uint param_6 = mip;
        uint param_7 = slice;
        SpdStore(param_4, param_5, param_6, param_7);
        uint param_8 = x + (y * 2u);
        uint param_9 = 0u;
        vec4 param_10 = v;
        SpdStoreIntermediate(param_8, param_9, param_10);
    }
}

void SpdDownsampleMip_5(uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
    if (localInvocationIndex < 1u)
    {
        uvec2 param = uvec2(0u);
        uvec2 param_1 = uvec2(1u, 0u);
        uvec2 param_2 = uvec2(2u, 0u);
        uvec2 param_3 = uvec2(3u, 0u);
        vec4 v = SpdReduceIntermediate(param, param_1, param_2, param_3);
        ivec2 param_4 = ivec2(workGroupID);
        vec4 param_5 = v;
        uint param_6 = mip;
        uint param_7 = slice;
        SpdStore(param_4, param_5, param_6, param_7);
    }
}

void SpdDownsampleNextFour(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint baseMip, uint mips, uint slice)
{
    if (mips <= baseMip)
    {
        return;
    }
    ffxSpdWorkgroupShuffleBarrier();
    uint param = x;
    uint param_1 = y;
    uvec2 param_2 = workGroupID;
    uint param_3 = localInvocationIndex;
    uint param_4 = baseMip;
    uint param_5 = slice;
    SpdDownsampleMip_2(param, param_1, param_2, param_3, param_4, param_5);
    if (mips <= (baseMip + 1u))
    {
        return;
    }
    ffxSpdWorkgroupShuffleBarrier();
    uint param_6 = x;
    uint param_7 = y;
    uvec2 param_8 = workGroupID;
    uint param_9 = localInvocationIndex;
    uint param_10 = baseMip + 1u;
    uint param_11 = slice;
    SpdDownsampleMip_3(param_6, param_7, param_8, param_9, param_10, param_11);
    if (mips <= (baseMip + 2u))
    {
        return;
    }
    ffxSpdWorkgroupShuffleBarrier();
    uint param_12 = x;
    uint param_13 = y;
    uvec2 param_14 = workGroupID;
    uint param_15 = localInvocationIndex;
    uint param_16 = baseMip + 2u;
    uint param_17 = slice;
    SpdDownsampleMip_4(param_12, param_13, param_14, param_15, param_16, param_17);
    if (mips <= (baseMip + 3u))
    {
        return;
    }
    ffxSpdWorkgroupShuffleBarrier();
    uvec2 param_18 = workGroupID;
    uint param_19 = localInvocationIndex;
    uint param_20 = baseMip + 3u;
    uint param_21 = slice;
    SpdDownsampleMip_5(param_18, param_19, param_20, param_21);
}

void SPD_IncreaseAtomicCounter(out uint spdCounter_1)
{
    uint _556 = imageAtomicAdd(rw_spd_global_atomic, ivec2(0), 1u);
    spdCounter_1 = _556;
}

void SpdIncreaseAtomicCounter(uint slice)
{
    uint param = spdCounter;
    SPD_IncreaseAtomicCounter(param);
    spdCounter = param;
}

uint SpdGetAtomicCounter()
{
    return spdCounter;
}

bool SpdExitWorkgroup(uint numWorkGroups, uint localInvocationIndex, uint slice)
{
    if (localInvocationIndex == 0u)
    {
        uint param = slice;
        SpdIncreaseAtomicCounter(param);
    }
    ffxSpdWorkgroupShuffleBarrier();
    return SpdGetAtomicCounter() != (numWorkGroups - 1u);
}

void SPD_ResetAtomicCounter()
{
    imageStore(rw_spd_global_atomic, ivec2(0), uvec4(0u));
}

void SpdResetAtomicCounter(uint slice)
{
    SPD_ResetAtomicCounter();
}

vec2 RWLoadPyramid(ivec2 iPxPos, uint index)
{
    if (index == 0u)
    {
        return imageLoad(r_rw_spd_mip0, iPxPos).xy;
    }
    if (index == 1u)
    {
        return imageLoad(r_rw_spd_mip1, iPxPos).xy;
    }
    if (index == 2u)
    {
        return imageLoad(r_rw_spd_mip2, iPxPos).xy;
    }
    if (index == 3u)
    {
        return imageLoad(r_rw_spd_mip3, iPxPos).xy;
    }
    if (index == 4u)
    {
        return imageLoad(r_rw_spd_mip4, iPxPos).xy;
    }
    if (index == 5u)
    {
        return imageLoad(r_rw_spd_mip5, iPxPos).xy;
    }
    return vec2(0.0);
}

vec4 SpdLoad(ivec2 tex, uint slice)
{
    ivec2 param = tex;
    uint param_1 = 5u;
    return vec4(RWLoadPyramid(param, param_1), 0.0, 0.0);
}

vec4 SpdReduceLoad4(uvec2 i0, uvec2 i1, uvec2 i2, uvec2 i3, uint slice)
{
    ivec2 param = ivec2(i0);
    uint param_1 = slice;
    vec4 v0 = SpdLoad(param, param_1);
    ivec2 param_2 = ivec2(i1);
    uint param_3 = slice;
    vec4 v1 = SpdLoad(param_2, param_3);
    ivec2 param_4 = ivec2(i2);
    uint param_5 = slice;
    vec4 v2 = SpdLoad(param_4, param_5);
    ivec2 param_6 = ivec2(i3);
    uint param_7 = slice;
    vec4 v3 = SpdLoad(param_6, param_7);
    vec4 param_8 = v0;
    vec4 param_9 = v1;
    vec4 param_10 = v2;
    vec4 param_11 = v3;
    return SpdReduce4(param_8, param_9, param_10, param_11);
}

vec4 SpdReduceLoad4(uvec2 base, uint slice)
{
    uvec2 param = base + uvec2(0u);
    uvec2 param_1 = base + uvec2(0u, 1u);
    uvec2 param_2 = base + uvec2(1u, 0u);
    uvec2 param_3 = base + uvec2(1u);
    uint param_4 = slice;
    return SpdReduceLoad4(param, param_1, param_2, param_3, param_4);
}

void SpdDownsampleMips_6_7(uint x, uint y, uint mips, uint slice)
{
    ivec2 tex = ivec2(int((x * 4u) + 0u), int((y * 4u) + 0u));
    ivec2 pix = ivec2(int((x * 2u) + 0u), int((y * 2u) + 0u));
    uvec2 param = uvec2(tex);
    uint param_1 = slice;
    vec4 v0 = SpdReduceLoad4(param, param_1);
    ivec2 param_2 = pix;
    vec4 param_3 = v0;
    uint param_4 = 6u;
    uint param_5 = slice;
    SpdStore(param_2, param_3, param_4, param_5);
    tex = ivec2(int((x * 4u) + 2u), int((y * 4u) + 0u));
    pix = ivec2(int((x * 2u) + 1u), int((y * 2u) + 0u));
    uvec2 param_6 = uvec2(tex);
    uint param_7 = slice;
    vec4 v1 = SpdReduceLoad4(param_6, param_7);
    ivec2 param_8 = pix;
    vec4 param_9 = v1;
    uint param_10 = 6u;
    uint param_11 = slice;
    SpdStore(param_8, param_9, param_10, param_11);
    tex = ivec2(int((x * 4u) + 0u), int((y * 4u) + 2u));
    pix = ivec2(int((x * 2u) + 0u), int((y * 2u) + 1u));
    uvec2 param_12 = uvec2(tex);
    uint param_13 = slice;
    vec4 v2 = SpdReduceLoad4(param_12, param_13);
    ivec2 param_14 = pix;
    vec4 param_15 = v2;
    uint param_16 = 6u;
    uint param_17 = slice;
    SpdStore(param_14, param_15, param_16, param_17);
    tex = ivec2(int((x * 4u) + 2u), int((y * 4u) + 2u));
    pix = ivec2(int((x * 2u) + 1u), int((y * 2u) + 1u));
    uvec2 param_18 = uvec2(tex);
    uint param_19 = slice;
    vec4 v3 = SpdReduceLoad4(param_18, param_19);
    ivec2 param_20 = pix;
    vec4 param_21 = v3;
    uint param_22 = 6u;
    uint param_23 = slice;
    SpdStore(param_20, param_21, param_22, param_23);
    if (mips <= 7u)
    {
        return;
    }
    vec4 param_24 = v0;
    vec4 param_25 = v1;
    vec4 param_26 = v2;
    vec4 param_27 = v3;
    vec4 v = SpdReduce4(param_24, param_25, param_26, param_27);
    ivec2 param_28 = ivec2(int(x), int(y));
    vec4 param_29 = v;
    uint param_30 = 7u;
    uint param_31 = slice;
    SpdStore(param_28, param_29, param_30, param_31);
    uint param_32 = x;
    uint param_33 = y;
    vec4 param_34 = v;
    SpdStoreIntermediate(param_32, param_33, param_34);
}

void SpdDownsample(uvec2 workGroupID, uint localInvocationIndex, uint mips, uint numWorkGroups, uint slice)
{
    uint param = localInvocationIndex % 64u;
    uvec2 sub_xy = ffxRemapForWaveReduction(param);
    uint x = sub_xy.x + (8u * ((localInvocationIndex >> uint(6)) % 2u));
    uint y = sub_xy.y + (8u * (localInvocationIndex >> uint(7)));
    uint param_1 = x;
    uint param_2 = y;
    uvec2 param_3 = workGroupID;
    uint param_4 = localInvocationIndex;
    uint param_5 = mips;
    uint param_6 = slice;
    SpdDownsampleMips_0_1(param_1, param_2, param_3, param_4, param_5, param_6);
    uint param_7 = x;
    uint param_8 = y;
    uvec2 param_9 = workGroupID;
    uint param_10 = localInvocationIndex;
    uint param_11 = 2u;
    uint param_12 = mips;
    uint param_13 = slice;
    SpdDownsampleNextFour(param_7, param_8, param_9, param_10, param_11, param_12, param_13);
    if (mips <= 6u)
    {
        return;
    }
    uint param_14 = numWorkGroups;
    uint param_15 = localInvocationIndex;
    uint param_16 = slice;
    bool _1852 = SpdExitWorkgroup(param_14, param_15, param_16);
    if (_1852)
    {
        return;
    }
    uint param_17 = slice;
    SpdResetAtomicCounter(param_17);
    uint param_18 = x;
    uint param_19 = y;
    uint param_20 = mips;
    uint param_21 = slice;
    SpdDownsampleMips_6_7(param_18, param_19, param_20, param_21);
    uint param_22 = x;
    uint param_23 = y;
    uvec2 param_24 = uvec2(0u);
    uint param_25 = localInvocationIndex;
    uint param_26 = 8u;
    uint param_27 = mips;
    uint param_28 = slice;
    SpdDownsampleNextFour(param_22, param_23, param_24, param_25, param_26, param_27, param_28);
}

void SpdDownsample(uvec2 workGroupID, uint localInvocationIndex, uint mips, uint numWorkGroups, uint slice, uvec2 workGroupOffset)
{
    uvec2 param = workGroupID + workGroupOffset;
    uint param_1 = localInvocationIndex;
    uint param_2 = mips;
    uint param_3 = numWorkGroups;
    uint param_4 = slice;
    SpdDownsample(param, param_1, param_2, param_3, param_4);
}

void ComputeAutoExposure(uvec3 WorkGroupId, uint LocalThreadIndex)
{
    uvec2 param = WorkGroupId.xy;
    uint param_1 = LocalThreadIndex;
    uint param_2 = MipCount();
    uint param_3 = NumWorkGroups();
    uint param_4 = WorkGroupId.z;
    uvec2 param_5 = WorkGroupOffset();
    SpdDownsample(param, param_1, param_2, param_3, param_4, param_5);
}

void main()
{
    uvec3 param = gl_WorkGroupID;
    uint param_1 = gl_LocalInvocationIndex;
    ComputeAutoExposure(param, param_1);
}

)FSR3";

// shading_change_pyramid_essl320.glsl sha256 3bfd9cd89b747ce735cd4e222d78c63db1cfe509ecdf977492e930ebb5b3f130
inline constexpr const char* kShadingChangePyramid = R"FSR3(#version 320 es
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct SampleSet
{
    float fSamples[5];
};

struct PreviousLumaBilinearSamplesData
{
    SampleSet fSet;
    bool bIsExistingSample;
};

const ivec2 _1000[5] = ivec2[](ivec2(0), ivec2(-1, 0), ivec2(1, 0), ivec2(0, -1), ivec2(0, 1));

layout(binding = 11, std140) uniform cbFSR3UPSCALER_t
{
    ivec2 iRenderSize;
    ivec2 iPreviousFrameRenderSize;
    ivec2 iUpscaleSize;
    ivec2 iPreviousFrameUpscaleSize;
    ivec2 iMaxRenderSize;
    ivec2 iMaxUpscaleSize;
    vec4 fDeviceToViewDepth;
    vec2 fJitter;
    vec2 fPreviousFrameJitter;
    vec2 fMotionVectorScale;
    vec2 fDownscaleFactor;
    vec2 fMotionVectorJitterCancellation;
    float fTanHalfFOV;
    float fJitterSequenceLength;
    float fDeltaTime;
    float fDeltaPreExposure;
    float fViewSpaceToMetersFactor;
    float fFrameIndex;
    float fVelocityFactor;
    float fReactivenessScale;
    float fShadingChangeScale;
    float fAccumulationAddedPerFrame;
    float fMinDisocclusionAccumulation;
} cbFSR3Upscaler;

layout(binding = 12, std140) uniform cbSPD_t
{
    uint mips;
    uint numWorkGroups;
    uvec2 workGroupOffset;
    uvec2 renderSize;
} cbSPD;

layout(binding = 5, rgba16f) uniform coherent readonly highp image2D r_rw_spd_mip0;
layout(binding = 6, rgba16f) uniform coherent readonly highp image2D r_rw_spd_mip1;
layout(binding = 7, rgba16f) uniform coherent readonly highp image2D r_rw_spd_mip2;
layout(binding = 8, rgba16f) uniform coherent readonly highp image2D r_rw_spd_mip3;
layout(binding = 9, rgba16f) uniform coherent readonly highp image2D r_rw_spd_mip4;
layout(binding = 10, rgba16f) uniform coherent readonly highp image2D r_rw_spd_mip5;
layout(binding = 24, rgba16f) uniform coherent writeonly highp image2D rw_spd_mip0;
layout(binding = 25, rgba16f) uniform coherent writeonly highp image2D rw_spd_mip1;
layout(binding = 26, rgba16f) uniform coherent writeonly highp image2D rw_spd_mip2;
layout(binding = 27, rgba16f) uniform coherent writeonly highp image2D rw_spd_mip3;
layout(binding = 28, rgba16f) uniform coherent writeonly highp image2D rw_spd_mip4;
layout(binding = 29, rgba16f) uniform coherent writeonly highp image2D rw_spd_mip5;
layout(binding = 4, r32ui) uniform coherent highp uimage2D rw_spd_global_atomic;
uniform highp sampler2D SPIRV_Cross_Combinedr_dilated_motion_vectorsSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_current_lumaSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_previous_lumaSPIRV_Cross_DummySampler;

shared uint spdCounter;
shared float spdIntermediateR[16][16];
shared float spdIntermediateG[16][16];
shared float spdIntermediateB[16][16];
shared float spdIntermediateA[16][16];

uint MipCount()
{
    return cbSPD.mips;
}

uint NumWorkGroups()
{
    return cbSPD.numWorkGroups;
}

uvec2 WorkGroupOffset()
{
    return cbSPD.workGroupOffset;
}

uint ffxBitfieldExtract(uint src, uint off, uint bits)
{
    return bitfieldExtract(src, int(off), int(bits));
}

uint ffxBitfieldInsertMask(uint src, uint ins, uint bits)
{
    return bitfieldInsert(src, ins, 0, int(bits));
}

uvec2 ffxRemapForWaveReduction(uint a)
{
    uint param = a;
    uint param_1 = 2u;
    uint param_2 = 3u;
    uint param_3 = ffxBitfieldExtract(param, param_1, param_2);
    uint param_4 = a;
    uint param_5 = 1u;
    uint param_6 = a;
    uint param_7 = 3u;
    uint param_8 = 3u;
    uint param_9 = a;
    uint param_10 = 1u;
    uint param_11 = 2u;
    uint param_12 = ffxBitfieldExtract(param_6, param_7, param_8);
    uint param_13 = ffxBitfieldExtract(param_9, param_10, param_11);
    uint param_14 = 2u;
    return uvec2(ffxBitfieldInsertMask(param_3, param_4, param_5), ffxBitfieldInsertMask(param_12, param_13, param_14));
}

ivec2 RenderSize()
{
    return cbFSR3Upscaler.iRenderSize;
}

int ffxMin(int x, int y)
{
    return min(x, y);
}

int ffxMax(int x, int y)
{
    return max(x, y);
}

ivec2 ClampLoad(ivec2 iPxSample, ivec2 iPxOffset, ivec2 iTextureSize)
{
    ivec2 result = iPxSample + iPxOffset;
    int param = result.x;
    int param_1 = iTextureSize.x - 1;
    int param_2 = 0;
    int param_3 = ffxMin(param, param_1);
    result.x = ffxMax(param_2, param_3);
    int param_4 = result.y;
    int param_5 = iTextureSize.y - 1;
    int param_6 = 0;
    int param_7 = ffxMin(param_4, param_5);
    result.y = ffxMax(param_6, param_7);
    return result;
}

vec2 LoadDilatedMotionVector(ivec2 iPxInput)
{
    return texelFetch(SPIRV_Cross_Combinedr_dilated_motion_vectorsSPIRV_Cross_DummySampler, iPxInput, 0).xy;
}

vec2 Jitter()
{
    return cbFSR3Upscaler.fJitter;
}

float LoadCurrentLuma(ivec2 iPxPos)
{
    return texelFetch(SPIRV_Cross_Combinedr_current_lumaSPIRV_Cross_DummySampler, iPxPos, 0).x;
}

float Exposure()
{
    float exposure = texelFetch(SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler, ivec2(0), 0).x;
    if (exposure == 0.0)
    {
        exposure = 1.0;
    }
    return exposure;
}

float ffxPow(float x, float y)
{
    return pow(x, y);
}

float ffxMax(float x, float y)
{
    return max(x, y);
}

SampleSet GetCurrentLumaBilinearSamples(vec2 fUv)
{
    vec2 fUvJittered = fUv + (Jitter() / vec2(RenderSize()));
    ivec2 iBasePos = ivec2(floor(fUvJittered * vec2(RenderSize())));
    SampleSet fSet;
    for (int iSampleIndex = 0; iSampleIndex < 5; iSampleIndex++)
    {
        ivec2 param = iBasePos;
        ivec2 param_1 = _1000[iSampleIndex];
        ivec2 param_2 = RenderSize();
        ivec2 iSamplePos = ClampLoad(param, param_1, param_2);
        ivec2 param_3 = iSamplePos;
        fSet.fSamples[iSampleIndex] = LoadCurrentLuma(param_3) * Exposure();
        float param_4 = fSet.fSamples[iSampleIndex];
        float param_5 = 1.0;
        fSet.fSamples[iSampleIndex] = ffxPow(param_4, param_5);
        float param_6 = fSet.fSamples[iSampleIndex];
        float param_7 = 6.099999882280826568603515625e-05;
        fSet.fSamples[iSampleIndex] = ffxMax(param_6, param_7);
    }
    return fSet;
}

vec2 PreviousFrameJitter()
{
    return cbFSR3Upscaler.fPreviousFrameJitter;
}

ivec2 PreviousFrameRenderSize()
{
    return cbFSR3Upscaler.iPreviousFrameRenderSize;
}

bool IsUvInside(vec2 fUv)
{
    bool _644 = fUv.x >= 0.0;
    bool _650;
    if (_644)
    {
        _650 = fUv.x <= 1.0;
    }
    else
    {
        _650 = _644;
    }
    bool _662;
    if (_650)
    {
        bool _655 = fUv.y >= 0.0;
        bool _661;
        if (_655)
        {
            _661 = fUv.y <= 1.0;
        }
        else
        {
            _661 = _655;
        }
        _662 = _661;
    }
    else
    {
        _662 = _650;
    }
    return _662;
}

float LoadPreviousLuma(ivec2 iPxPos)
{
    return texelFetch(SPIRV_Cross_Combinedr_previous_lumaSPIRV_Cross_DummySampler, iPxPos, 0).x;
}

float DeltaPreExposure()
{
    return cbFSR3Upscaler.fDeltaPreExposure;
}

PreviousLumaBilinearSamplesData GetPreviousLumaBilinearSamples(vec2 fUv, vec2 fMotionVector)
{
    vec2 fUvJittered = fUv + (PreviousFrameJitter() / vec2(PreviousFrameRenderSize()));
    vec2 fReprojectedUv = fUvJittered + fMotionVector;
    vec2 param = fReprojectedUv;
    PreviousLumaBilinearSamplesData data;
    data.bIsExistingSample = IsUvInside(param);
    if (data.bIsExistingSample)
    {
        ivec2 iBasePos = ivec2(floor(fReprojectedUv * vec2(PreviousFrameRenderSize())));
        for (int iSampleIndex = 0; iSampleIndex < 5; iSampleIndex++)
        {
            ivec2 param_1 = iBasePos;
            ivec2 param_2 = _1000[iSampleIndex];
            ivec2 param_3 = PreviousFrameRenderSize();
            ivec2 iSamplePos = ClampLoad(param_1, param_2, param_3);
            ivec2 param_4 = iSamplePos;
            data.fSet.fSamples[iSampleIndex] = (LoadPreviousLuma(param_4) * DeltaPreExposure()) * Exposure();
            float param_5 = data.fSet.fSamples[iSampleIndex];
            float param_6 = 1.0;
            data.fSet.fSamples[iSampleIndex] = ffxPow(param_5, param_6);
            float param_7 = data.fSet.fSamples[iSampleIndex];
            float param_8 = 6.099999882280826568603515625e-05;
            data.fSet.fSamples[iSampleIndex] = ffxMax(param_7, param_8);
        }
    }
    return data;
}

float ffxMin(float x, float y)
{
    return min(x, y);
}

void SortSet(inout SampleSet fSet)
{
    float param = fSet.fSamples[0];
    float param_1 = fSet.fSamples[3];
    float fTmp = ffxMin(param, param_1);
    float param_2 = fSet.fSamples[0];
    float param_3 = fSet.fSamples[3];
    fSet.fSamples[3] = ffxMax(param_2, param_3);
    fSet.fSamples[0] = fTmp;
    float param_4 = fSet.fSamples[1];
    float param_5 = fSet.fSamples[4];
    float fTmp_1 = ffxMin(param_4, param_5);
    float param_6 = fSet.fSamples[1];
    float param_7 = fSet.fSamples[4];
    fSet.fSamples[4] = ffxMax(param_6, param_7);
    fSet.fSamples[1] = fTmp_1;
    float param_8 = fSet.fSamples[0];
    float param_9 = fSet.fSamples[2];
    float fTmp_2 = ffxMin(param_8, param_9);
    float param_10 = fSet.fSamples[0];
    float param_11 = fSet.fSamples[2];
    fSet.fSamples[2] = ffxMax(param_10, param_11);
    fSet.fSamples[0] = fTmp_2;
    float param_12 = fSet.fSamples[1];
    float param_13 = fSet.fSamples[3];
    float fTmp_3 = ffxMin(param_12, param_13);
    float param_14 = fSet.fSamples[1];
    float param_15 = fSet.fSamples[3];
    fSet.fSamples[3] = ffxMax(param_14, param_15);
    fSet.fSamples[1] = fTmp_3;
    float param_16 = fSet.fSamples[0];
    float param_17 = fSet.fSamples[1];
    float fTmp_4 = ffxMin(param_16, param_17);
    float param_18 = fSet.fSamples[0];
    float param_19 = fSet.fSamples[1];
    fSet.fSamples[1] = ffxMax(param_18, param_19);
    fSet.fSamples[0] = fTmp_4;
    float param_20 = fSet.fSamples[2];
    float param_21 = fSet.fSamples[4];
    float fTmp_5 = ffxMin(param_20, param_21);
    float param_22 = fSet.fSamples[2];
    float param_23 = fSet.fSamples[4];
    fSet.fSamples[4] = ffxMax(param_22, param_23);
    fSet.fSamples[2] = fTmp_5;
    float param_24 = fSet.fSamples[1];
    float param_25 = fSet.fSamples[2];
    float fTmp_6 = ffxMin(param_24, param_25);
    float param_26 = fSet.fSamples[1];
    float param_27 = fSet.fSamples[2];
    fSet.fSamples[2] = ffxMax(param_26, param_27);
    fSet.fSamples[1] = fTmp_6;
    float param_28 = fSet.fSamples[3];
    float param_29 = fSet.fSamples[4];
    float fTmp_7 = ffxMin(param_28, param_29);
    float param_30 = fSet.fSamples[3];
    float param_31 = fSet.fSamples[4];
    fSet.fSamples[4] = ffxMax(param_30, param_31);
    fSet.fSamples[3] = fTmp_7;
    float param_32 = fSet.fSamples[2];
    float param_33 = fSet.fSamples[3];
    float fTmp_8 = ffxMin(param_32, param_33);
    float param_34 = fSet.fSamples[2];
    float param_35 = fSet.fSamples[3];
    fSet.fSamples[3] = ffxMax(param_34, param_35);
    fSet.fSamples[2] = fTmp_8;
}

float MinDividedByMax(float v0, float v1)
{
    float param = v0;
    float param_1 = v1;
    float m = ffxMax(param, param_1);
    float _630;
    if (m != 0.0)
    {
        float param_2 = v0;
        float param_3 = v1;
        _630 = ffxMin(param_2, param_3) / m;
    }
    else
    {
        _630 = 0.0;
    }
    return _630;
}

float ComputeMinimumDifference(ivec2 iPxPos, inout SampleSet fSet0, inout SampleSet fSet1)
{
    float fMinDiff = 65503.0;
    int a = 0;
    int b = 0;
    SampleSet param = fSet0;
    SortSet(param);
    fSet0 = param;
    SampleSet param_1 = fSet1;
    SortSet(param_1);
    fSet1 = param_1;
    float param_2 = fSet0.fSamples[4];
    float param_3 = fSet1.fSamples[4];
    float fMax = ffxMin(param_2, param_3);
    if (fMax > 1.1754943508222875079687365372222e-38)
    {
        for (int i = 0; (i < 5) && (fMinDiff < 65504.0); i++)
        {
            float fDiff = fSet0.fSamples[a] - fSet1.fSamples[b];
            if (abs(fDiff) > 6.099999882280826568603515625e-05)
            {
                fDiff = sign(fDiff) * (1.0 - MinDividedByMax(fSet0.fSamples[a], fSet1.fSamples[b]));
                fMinDiff = (abs(fDiff) < abs(fMinDiff)) ? fDiff : fMinDiff;
                a += int(fSet0.fSamples[a] < fSet1.fSamples[b]);
                b += int(fSet0.fSamples[a] >= fSet1.fSamples[b]);
            }
            else
            {
                fMinDiff = 65504.0;
            }
        }
    }
    return fMinDiff * float(fMinDiff < 65503.0);
}

float ComputeDiff(ivec2 iPxPos, vec2 fUv, vec2 fMotionVector)
{
    float fMinDiff = 0.0;
    vec2 param = fUv;
    SampleSet fCurrentSamples = GetCurrentLumaBilinearSamples(param);
    vec2 param_1 = fUv;
    vec2 param_2 = fMotionVector;
    PreviousLumaBilinearSamplesData previousData = GetPreviousLumaBilinearSamples(param_1, param_2);
    if (previousData.bIsExistingSample)
    {
        ivec2 param_3 = iPxPos;
        SampleSet param_4 = fCurrentSamples;
        SampleSet param_5 = previousData.fSet;
        float _1141 = ComputeMinimumDifference(param_3, param_4, param_5);
        fMinDiff = _1141;
    }
    return fMinDiff;
}

vec4 SpdLoadSourceImage(vec2 iPxPos, uint slice)
{
    ivec2 param = ivec2(iPxPos);
    ivec2 param_1 = ivec2(0);
    ivec2 param_2 = RenderSize();
    ivec2 iPxSamplePos = ClampLoad(param, param_1, param_2);
    ivec2 param_3 = iPxSamplePos;
    vec2 fDilatedMotionVector = LoadDilatedMotionVector(param_3);
    vec2 fUv = (vec2(iPxSamplePos) + vec2(0.5)) / vec2(RenderSize());
    ivec2 param_4 = iPxSamplePos;
    vec2 param_5 = fUv;
    vec2 param_6 = fDilatedMotionVector;
    float fScaledAndSignedLumaDiff = ComputeDiff(param_4, param_5, param_6);
    vec4 fOutput = vec4(0.0);
    fOutput.x = fScaledAndSignedLumaDiff;
    float _1180;
    if (fScaledAndSignedLumaDiff != 0.0)
    {
        _1180 = sign(fScaledAndSignedLumaDiff);
    }
    else
    {
        _1180 = 0.0;
    }
    fOutput.y = _1180;
    fOutput.z = 1.0;
    return fOutput;
}

vec4 SpdReduce4(vec4 v0, vec4 v1, vec4 v2, vec4 v3)
{
    return (((v0 + v1) + v2) + v3) * 0.25;
}

vec4 SpdReduceLoadSourceImage4(uvec2 i0, uvec2 i1, uvec2 i2, uvec2 i3, uint slice)
{
    vec2 param = vec2(ivec2(i0));
    uint param_1 = slice;
    vec4 v0 = SpdLoadSourceImage(param, param_1);
    vec2 param_2 = vec2(ivec2(i1));
    uint param_3 = slice;
    vec4 v1 = SpdLoadSourceImage(param_2, param_3);
    vec2 param_4 = vec2(ivec2(i2));
    uint param_5 = slice;
    vec4 v2 = SpdLoadSourceImage(param_4, param_5);
    vec2 param_6 = vec2(ivec2(i3));
    uint param_7 = slice;
    vec4 v3 = SpdLoadSourceImage(param_6, param_7);
    vec4 param_8 = v0;
    vec4 param_9 = v1;
    vec4 param_10 = v2;
    vec4 param_11 = v3;
    return SpdReduce4(param_8, param_9, param_10, param_11);
}

vec4 SpdReduceLoadSourceImage(uvec2 base, uint slice)
{
    uvec2 param = base + uvec2(0u);
    uvec2 param_1 = base + uvec2(0u, 1u);
    uvec2 param_2 = base + uvec2(1u, 0u);
    uvec2 param_3 = base + uvec2(1u);
    uint param_4 = slice;
    return SpdReduceLoadSourceImage4(param, param_1, param_2, param_3, param_4);
}

void StorePyramid(ivec2 iPxPos, vec2 outValue, uint index)
{
    if (index == 0u)
    {
        imageStore(rw_spd_mip0, iPxPos, vec4(outValue, 0.0, 0.0));
    }
    if (index == 1u)
    {
        imageStore(rw_spd_mip1, iPxPos, vec4(outValue, 0.0, 0.0));
    }
    if (index == 2u)
    {
        imageStore(rw_spd_mip2, iPxPos, vec4(outValue, 0.0, 0.0));
    }
    if (index == 3u)
    {
        imageStore(rw_spd_mip3, iPxPos, vec4(outValue, 0.0, 0.0));
    }
    if (index == 4u)
    {
        imageStore(rw_spd_mip4, iPxPos, vec4(outValue, 0.0, 0.0));
    }
    if (index == 5u)
    {
        imageStore(rw_spd_mip5, iPxPos, vec4(outValue, 0.0, 0.0));
    }
}

void SpdStore(ivec2 pix, vec4 outValue, uint index, uint slice)
{
    if (index >= 0u)
    {
        ivec2 param = pix;
        vec2 param_1 = outValue.xy;
        uint param_2 = index;
        StorePyramid(param, param_1, param_2);
    }
}

void SpdStoreIntermediate(uint x, uint y, vec4 value)
{
    spdIntermediateR[x][y] = value.x;
    spdIntermediateG[x][y] = value.y;
    spdIntermediateB[x][y] = value.z;
    spdIntermediateA[x][y] = value.w;
}

void ffxSpdWorkgroupShuffleBarrier()
{
    groupMemoryBarrier();
    barrier();
}

vec4 SpdLoadIntermediate(uint x, uint y)
{
    return vec4(spdIntermediateR[x][y], spdIntermediateG[x][y], spdIntermediateB[x][y], spdIntermediateA[x][y]);
}

vec4 SpdReduceIntermediate(uvec2 i0, uvec2 i1, uvec2 i2, uvec2 i3)
{
    uint param = i0.x;
    uint param_1 = i0.y;
    vec4 v0 = SpdLoadIntermediate(param, param_1);
    uint param_2 = i1.x;
    uint param_3 = i1.y;
    vec4 v1 = SpdLoadIntermediate(param_2, param_3);
    uint param_4 = i2.x;
    uint param_5 = i2.y;
    vec4 v2 = SpdLoadIntermediate(param_4, param_5);
    uint param_6 = i3.x;
    uint param_7 = i3.y;
    vec4 v3 = SpdLoadIntermediate(param_6, param_7);
    vec4 param_8 = v0;
    vec4 param_9 = v1;
    vec4 param_10 = v2;
    vec4 param_11 = v3;
    return SpdReduce4(param_8, param_9, param_10, param_11);
}

void SpdDownsampleMips_0_1_LDS(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
    ivec2 tex = ivec2(workGroupID * uvec2(64u)) + ivec2(int(x * 2u), int(y * 2u));
    ivec2 pix = ivec2(workGroupID * uvec2(32u)) + ivec2(int(x), int(y));
    uvec2 param = uvec2(tex);
    uint param_1 = slice;
    vec4 v[4];
    v[0] = SpdReduceLoadSourceImage(param, param_1);
    ivec2 param_2 = pix;
    vec4 param_3 = v[0];
    uint param_4 = 0u;
    uint param_5 = slice;
    SpdStore(param_2, param_3, param_4, param_5);
    tex = ivec2(workGroupID * uvec2(64u)) + ivec2(int((x * 2u) + 32u), int(y * 2u));
    pix = ivec2(workGroupID * uvec2(32u)) + ivec2(int(x + 16u), int(y));
    uvec2 param_6 = uvec2(tex);
    uint param_7 = slice;
    v[1] = SpdReduceLoadSourceImage(param_6, param_7);
    ivec2 param_8 = pix;
    vec4 param_9 = v[1];
    uint param_10 = 0u;
    uint param_11 = slice;
    SpdStore(param_8, param_9, param_10, param_11);
    tex = ivec2(workGroupID * uvec2(64u)) + ivec2(int(x * 2u), int((y * 2u) + 32u));
    pix = ivec2(workGroupID * uvec2(32u)) + ivec2(int(x), int(y + 16u));
    uvec2 param_12 = uvec2(tex);
    uint param_13 = slice;
    v[2] = SpdReduceLoadSourceImage(param_12, param_13);
    ivec2 param_14 = pix;
    vec4 param_15 = v[2];
    uint param_16 = 0u;
    uint param_17 = slice;
    SpdStore(param_14, param_15, param_16, param_17);
    tex = ivec2(workGroupID * uvec2(64u)) + ivec2(int((x * 2u) + 32u), int((y * 2u) + 32u));
    pix = ivec2(workGroupID * uvec2(32u)) + ivec2(int(x + 16u), int(y + 16u));
    uvec2 param_18 = uvec2(tex);
    uint param_19 = slice;
    v[3] = SpdReduceLoadSourceImage(param_18, param_19);
    ivec2 param_20 = pix;
    vec4 param_21 = v[3];
    uint param_22 = 0u;
    uint param_23 = slice;
    SpdStore(param_20, param_21, param_22, param_23);
    if (mip <= 1u)
    {
        return;
    }
    for (uint i = 0u; i < 4u; i++)
    {
        uint param_24 = x;
        uint param_25 = y;
        vec4 param_26 = v[i];
        SpdStoreIntermediate(param_24, param_25, param_26);
        ffxSpdWorkgroupShuffleBarrier();
        if (localInvocationIndex < 64u)
        {
            uvec2 param_27 = uvec2((x * 2u) + 0u, (y * 2u) + 0u);
            uvec2 param_28 = uvec2((x * 2u) + 1u, (y * 2u) + 0u);
            uvec2 param_29 = uvec2((x * 2u) + 0u, (y * 2u) + 1u);
            uvec2 param_30 = uvec2((x * 2u) + 1u, (y * 2u) + 1u);
            v[i] = SpdReduceIntermediate(param_27, param_28, param_29, param_30);
            ivec2 param_31 = ivec2(workGroupID * uvec2(16u)) + ivec2(int(x + ((i % 2u) * 8u)), int(y + ((i / 2u) * 8u)));
            vec4 param_32 = v[i];
            uint param_33 = 1u;
            uint param_34 = slice;
            SpdStore(param_31, param_32, param_33, param_34);
        }
        ffxSpdWorkgroupShuffleBarrier();
    }
    if (localInvocationIndex < 64u)
    {
        uint param_35 = x + 0u;
        uint param_36 = y + 0u;
        vec4 param_37 = v[0];
        SpdStoreIntermediate(param_35, param_36, param_37);
        uint param_38 = x + 8u;
        uint param_39 = y + 0u;
        vec4 param_40 = v[1];
        SpdStoreIntermediate(param_38, param_39, param_40);
        uint param_41 = x + 0u;
        uint param_42 = y + 8u;
        vec4 param_43 = v[2];
        SpdStoreIntermediate(param_41, param_42, param_43);
        uint param_44 = x + 8u;
        uint param_45 = y + 8u;
        vec4 param_46 = v[3];
        SpdStoreIntermediate(param_44, param_45, param_46);
    }
}

void SpdDownsampleMips_0_1(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
    uint param = x;
    uint param_1 = y;
    uvec2 param_2 = workGroupID;
    uint param_3 = localInvocationIndex;
    uint param_4 = mip;
    uint param_5 = slice;
    SpdDownsampleMips_0_1_LDS(param, param_1, param_2, param_3, param_4, param_5);
}

void SpdDownsampleMip_2(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
    if (localInvocationIndex < 64u)
    {
        uvec2 param = uvec2((x * 2u) + 0u, (y * 2u) + 0u);
        uvec2 param_1 = uvec2((x * 2u) + 1u, (y * 2u) + 0u);
        uvec2 param_2 = uvec2((x * 2u) + 0u, (y * 2u) + 1u);
        uvec2 param_3 = uvec2((x * 2u) + 1u, (y * 2u) + 1u);
        vec4 v = SpdReduceIntermediate(param, param_1, param_2, param_3);
        ivec2 param_4 = ivec2(workGroupID * uvec2(8u)) + ivec2(int(x), int(y));
        vec4 param_5 = v;
        uint param_6 = mip;
        uint param_7 = slice;
        SpdStore(param_4, param_5, param_6, param_7);
        uint param_8 = (x * 2u) + (y % 2u);
        uint param_9 = y * 2u;
        vec4 param_10 = v;
        SpdStoreIntermediate(param_8, param_9, param_10);
    }
}

void SpdDownsampleMip_3(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
    if (localInvocationIndex < 16u)
    {
        uvec2 param = uvec2(((x * 4u) + 0u) + 0u, (y * 4u) + 0u);
        uvec2 param_1 = uvec2(((x * 4u) + 2u) + 0u, (y * 4u) + 0u);
        uvec2 param_2 = uvec2(((x * 4u) + 0u) + 1u, (y * 4u) + 2u);
        uvec2 param_3 = uvec2(((x * 4u) + 2u) + 1u, (y * 4u) + 2u);
        vec4 v = SpdReduceIntermediate(param, param_1, param_2, param_3);
        ivec2 param_4 = ivec2(workGroupID * uvec2(4u)) + ivec2(int(x), int(y));
        vec4 param_5 = v;
        uint param_6 = mip;
        uint param_7 = slice;
        SpdStore(param_4, param_5, param_6, param_7);
        uint param_8 = (x * 4u) + y;
        uint param_9 = y * 4u;
        vec4 param_10 = v;
        SpdStoreIntermediate(param_8, param_9, param_10);
    }
}

void SpdDownsampleMip_4(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
    if (localInvocationIndex < 4u)
    {
        uvec2 param = uvec2((((x * 8u) + 0u) + 0u) + (y * 2u), (y * 8u) + 0u);
        uvec2 param_1 = uvec2((((x * 8u) + 4u) + 0u) + (y * 2u), (y * 8u) + 0u);
        uvec2 param_2 = uvec2((((x * 8u) + 0u) + 1u) + (y * 2u), (y * 8u) + 4u);
        uvec2 param_3 = uvec2((((x * 8u) + 4u) + 1u) + (y * 2u), (y * 8u) + 4u);
        vec4 v = SpdReduceIntermediate(param, param_1, param_2, param_3);
        ivec2 param_4 = ivec2(workGroupID * uvec2(2u)) + ivec2(int(x), int(y));
        vec4 param_5 = v;
        uint param_6 = mip;
        uint param_7 = slice;
        SpdStore(param_4, param_5, param_6, param_7);
        uint param_8 = x + (y * 2u);
        uint param_9 = 0u;
        vec4 param_10 = v;
        SpdStoreIntermediate(param_8, param_9, param_10);
    }
}

void SpdDownsampleMip_5(uvec2 workGroupID, uint localInvocationIndex, uint mip, uint slice)
{
    if (localInvocationIndex < 1u)
    {
        uvec2 param = uvec2(0u);
        uvec2 param_1 = uvec2(1u, 0u);
        uvec2 param_2 = uvec2(2u, 0u);
        uvec2 param_3 = uvec2(3u, 0u);
        vec4 v = SpdReduceIntermediate(param, param_1, param_2, param_3);
        ivec2 param_4 = ivec2(workGroupID);
        vec4 param_5 = v;
        uint param_6 = mip;
        uint param_7 = slice;
        SpdStore(param_4, param_5, param_6, param_7);
    }
}

void SpdDownsampleNextFour(uint x, uint y, uvec2 workGroupID, uint localInvocationIndex, uint baseMip, uint mips, uint slice)
{
    if (mips <= baseMip)
    {
        return;
    }
    ffxSpdWorkgroupShuffleBarrier();
    uint param = x;
    uint param_1 = y;
    uvec2 param_2 = workGroupID;
    uint param_3 = localInvocationIndex;
    uint param_4 = baseMip;
    uint param_5 = slice;
    SpdDownsampleMip_2(param, param_1, param_2, param_3, param_4, param_5);
    if (mips <= (baseMip + 1u))
    {
        return;
    }
    ffxSpdWorkgroupShuffleBarrier();
    uint param_6 = x;
    uint param_7 = y;
    uvec2 param_8 = workGroupID;
    uint param_9 = localInvocationIndex;
    uint param_10 = baseMip + 1u;
    uint param_11 = slice;
    SpdDownsampleMip_3(param_6, param_7, param_8, param_9, param_10, param_11);
    if (mips <= (baseMip + 2u))
    {
        return;
    }
    ffxSpdWorkgroupShuffleBarrier();
    uint param_12 = x;
    uint param_13 = y;
    uvec2 param_14 = workGroupID;
    uint param_15 = localInvocationIndex;
    uint param_16 = baseMip + 2u;
    uint param_17 = slice;
    SpdDownsampleMip_4(param_12, param_13, param_14, param_15, param_16, param_17);
    if (mips <= (baseMip + 3u))
    {
        return;
    }
    ffxSpdWorkgroupShuffleBarrier();
    uvec2 param_18 = workGroupID;
    uint param_19 = localInvocationIndex;
    uint param_20 = baseMip + 3u;
    uint param_21 = slice;
    SpdDownsampleMip_5(param_18, param_19, param_20, param_21);
}

void SPD_IncreaseAtomicCounter(out uint spdCounter_1)
{
    uint _620 = imageAtomicAdd(rw_spd_global_atomic, ivec2(0), 1u);
    spdCounter_1 = _620;
}

void SpdIncreaseAtomicCounter(uint slice)
{
    uint param = spdCounter;
    SPD_IncreaseAtomicCounter(param);
    spdCounter = param;
}

uint SpdGetAtomicCounter()
{
    return spdCounter;
}

bool SpdExitWorkgroup(uint numWorkGroups, uint localInvocationIndex, uint slice)
{
    if (localInvocationIndex == 0u)
    {
        uint param = slice;
        SpdIncreaseAtomicCounter(param);
    }
    ffxSpdWorkgroupShuffleBarrier();
    return SpdGetAtomicCounter() != (numWorkGroups - 1u);
}

void SPD_ResetAtomicCounter()
{
    imageStore(rw_spd_global_atomic, ivec2(0), uvec4(0u));
}

void SpdResetAtomicCounter(uint slice)
{
    SPD_ResetAtomicCounter();
}

vec2 RWLoadPyramid(ivec2 iPxPos, uint index)
{
    if (index == 0u)
    {
        return imageLoad(r_rw_spd_mip0, iPxPos).xy;
    }
    if (index == 1u)
    {
        return imageLoad(r_rw_spd_mip1, iPxPos).xy;
    }
    if (index == 2u)
    {
        return imageLoad(r_rw_spd_mip2, iPxPos).xy;
    }
    if (index == 3u)
    {
        return imageLoad(r_rw_spd_mip3, iPxPos).xy;
    }
    if (index == 4u)
    {
        return imageLoad(r_rw_spd_mip4, iPxPos).xy;
    }
    if (index == 5u)
    {
        return imageLoad(r_rw_spd_mip5, iPxPos).xy;
    }
    return vec2(0.0);
}

vec4 SpdLoad(ivec2 tex, uint slice)
{
    ivec2 param = tex;
    uint param_1 = 5u;
    return vec4(RWLoadPyramid(param, param_1), 0.0, 0.0);
}

vec4 SpdReduceLoad4(uvec2 i0, uvec2 i1, uvec2 i2, uvec2 i3, uint slice)
{
    ivec2 param = ivec2(i0);
    uint param_1 = slice;
    vec4 v0 = SpdLoad(param, param_1);
    ivec2 param_2 = ivec2(i1);
    uint param_3 = slice;
    vec4 v1 = SpdLoad(param_2, param_3);
    ivec2 param_4 = ivec2(i2);
    uint param_5 = slice;
    vec4 v2 = SpdLoad(param_4, param_5);
    ivec2 param_6 = ivec2(i3);
    uint param_7 = slice;
    vec4 v3 = SpdLoad(param_6, param_7);
    vec4 param_8 = v0;
    vec4 param_9 = v1;
    vec4 param_10 = v2;
    vec4 param_11 = v3;
    return SpdReduce4(param_8, param_9, param_10, param_11);
}

vec4 SpdReduceLoad4(uvec2 base, uint slice)
{
    uvec2 param = base + uvec2(0u);
    uvec2 param_1 = base + uvec2(0u, 1u);
    uvec2 param_2 = base + uvec2(1u, 0u);
    uvec2 param_3 = base + uvec2(1u);
    uint param_4 = slice;
    return SpdReduceLoad4(param, param_1, param_2, param_3, param_4);
}

void SpdDownsampleMips_6_7(uint x, uint y, uint mips, uint slice)
{
    ivec2 tex = ivec2(int((x * 4u) + 0u), int((y * 4u) + 0u));
    ivec2 pix = ivec2(int((x * 2u) + 0u), int((y * 2u) + 0u));
    uvec2 param = uvec2(tex);
    uint param_1 = slice;
    vec4 v0 = SpdReduceLoad4(param, param_1);
    ivec2 param_2 = pix;
    vec4 param_3 = v0;
    uint param_4 = 6u;
    uint param_5 = slice;
    SpdStore(param_2, param_3, param_4, param_5);
    tex = ivec2(int((x * 4u) + 2u), int((y * 4u) + 0u));
    pix = ivec2(int((x * 2u) + 1u), int((y * 2u) + 0u));
    uvec2 param_6 = uvec2(tex);
    uint param_7 = slice;
    vec4 v1 = SpdReduceLoad4(param_6, param_7);
    ivec2 param_8 = pix;
    vec4 param_9 = v1;
    uint param_10 = 6u;
    uint param_11 = slice;
    SpdStore(param_8, param_9, param_10, param_11);
    tex = ivec2(int((x * 4u) + 0u), int((y * 4u) + 2u));
    pix = ivec2(int((x * 2u) + 0u), int((y * 2u) + 1u));
    uvec2 param_12 = uvec2(tex);
    uint param_13 = slice;
    vec4 v2 = SpdReduceLoad4(param_12, param_13);
    ivec2 param_14 = pix;
    vec4 param_15 = v2;
    uint param_16 = 6u;
    uint param_17 = slice;
    SpdStore(param_14, param_15, param_16, param_17);
    tex = ivec2(int((x * 4u) + 2u), int((y * 4u) + 2u));
    pix = ivec2(int((x * 2u) + 1u), int((y * 2u) + 1u));
    uvec2 param_18 = uvec2(tex);
    uint param_19 = slice;
    vec4 v3 = SpdReduceLoad4(param_18, param_19);
    ivec2 param_20 = pix;
    vec4 param_21 = v3;
    uint param_22 = 6u;
    uint param_23 = slice;
    SpdStore(param_20, param_21, param_22, param_23);
    if (mips <= 7u)
    {
        return;
    }
    vec4 param_24 = v0;
    vec4 param_25 = v1;
    vec4 param_26 = v2;
    vec4 param_27 = v3;
    vec4 v = SpdReduce4(param_24, param_25, param_26, param_27);
    ivec2 param_28 = ivec2(int(x), int(y));
    vec4 param_29 = v;
    uint param_30 = 7u;
    uint param_31 = slice;
    SpdStore(param_28, param_29, param_30, param_31);
    uint param_32 = x;
    uint param_33 = y;
    vec4 param_34 = v;
    SpdStoreIntermediate(param_32, param_33, param_34);
}

void SpdDownsample(uvec2 workGroupID, uint localInvocationIndex, uint mips, uint numWorkGroups, uint slice)
{
    uint param = localInvocationIndex % 64u;
    uvec2 sub_xy = ffxRemapForWaveReduction(param);
    uint x = sub_xy.x + (8u * ((localInvocationIndex >> uint(6)) % 2u));
    uint y = sub_xy.y + (8u * (localInvocationIndex >> uint(7)));
    uint param_1 = x;
    uint param_2 = y;
    uvec2 param_3 = workGroupID;
    uint param_4 = localInvocationIndex;
    uint param_5 = mips;
    uint param_6 = slice;
    SpdDownsampleMips_0_1(param_1, param_2, param_3, param_4, param_5, param_6);
    uint param_7 = x;
    uint param_8 = y;
    uvec2 param_9 = workGroupID;
    uint param_10 = localInvocationIndex;
    uint param_11 = 2u;
    uint param_12 = mips;
    uint param_13 = slice;
    SpdDownsampleNextFour(param_7, param_8, param_9, param_10, param_11, param_12, param_13);
    if (mips <= 6u)
    {
        return;
    }
    uint param_14 = numWorkGroups;
    uint param_15 = localInvocationIndex;
    uint param_16 = slice;
    bool _2321 = SpdExitWorkgroup(param_14, param_15, param_16);
    if (_2321)
    {
        return;
    }
    uint param_17 = slice;
    SpdResetAtomicCounter(param_17);
    uint param_18 = x;
    uint param_19 = y;
    uint param_20 = mips;
    uint param_21 = slice;
    SpdDownsampleMips_6_7(param_18, param_19, param_20, param_21);
    uint param_22 = x;
    uint param_23 = y;
    uvec2 param_24 = uvec2(0u);
    uint param_25 = localInvocationIndex;
    uint param_26 = 8u;
    uint param_27 = mips;
    uint param_28 = slice;
    SpdDownsampleNextFour(param_22, param_23, param_24, param_25, param_26, param_27, param_28);
}

void SpdDownsample(uvec2 workGroupID, uint localInvocationIndex, uint mips, uint numWorkGroups, uint slice, uvec2 workGroupOffset)
{
    uvec2 param = workGroupID + workGroupOffset;
    uint param_1 = localInvocationIndex;
    uint param_2 = mips;
    uint param_3 = numWorkGroups;
    uint param_4 = slice;
    SpdDownsample(param, param_1, param_2, param_3, param_4);
}

void ComputeShadingChangePyramid(uvec3 WorkGroupId, uint LocalThreadIndex)
{
    uvec2 param = WorkGroupId.xy;
    uint param_1 = LocalThreadIndex;
    uint param_2 = MipCount();
    uint param_3 = NumWorkGroups();
    uint param_4 = WorkGroupId.z;
    uvec2 param_5 = WorkGroupOffset();
    SpdDownsample(param, param_1, param_2, param_3, param_4, param_5);
}

void main()
{
    uvec3 param = gl_WorkGroupID;
    uint param_1 = gl_LocalInvocationIndex;
    ComputeShadingChangePyramid(param, param_1);
}

)FSR3";

// shading_change_essl320.glsl sha256 0835f5e00e38652d7f4470dbe1f8acccc015f09b9b5375bbc03e08594e25a052
inline constexpr const char* kShadingChange = R"FSR3(#version 320 es
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

struct ShadingChangeLumaInfo
{
    float fSamples[3];
};

layout(binding = 2, std140) uniform cbFSR3UPSCALER_t
{
    ivec2 iRenderSize;
    ivec2 iPreviousFrameRenderSize;
    ivec2 iUpscaleSize;
    ivec2 iPreviousFrameUpscaleSize;
    ivec2 iMaxRenderSize;
    ivec2 iMaxUpscaleSize;
    vec4 fDeviceToViewDepth;
    vec2 fJitter;
    vec2 fPreviousFrameJitter;
    vec2 fMotionVectorScale;
    vec2 fDownscaleFactor;
    vec2 fMotionVectorJitterCancellation;
    float fTanHalfFOV;
    float fJitterSequenceLength;
    float fDeltaTime;
    float fDeltaPreExposure;
    float fViewSpaceToMetersFactor;
    float fFrameIndex;
    float fVelocityFactor;
    float fReactivenessScale;
    float fShadingChangeScale;
    float fAccumulationAddedPerFrame;
    float fMinDisocclusionAccumulation;
} cbFSR3Upscaler;

layout(binding = 1, rgba8) uniform writeonly highp image2D rw_shading_change;
uniform highp sampler2D SPIRV_Cross_Combinedr_spd_mipsSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_spd_mipss_LinearClamp;

ivec2 RenderSize()
{
    return cbFSR3Upscaler.iRenderSize;
}

ivec2 ShadingChangeRenderSize()
{
    return ivec2(vec2(RenderSize()) * 0.5);
}

bool IsOnScreen(ivec2 pos, ivec2 size)
{
    return all(lessThan(uvec2(pos), uvec2(size)));
}

vec2 Jitter()
{
    return cbFSR3Upscaler.fJitter;
}

ivec2 GetSPDMipDimensions(uint uMipLevel)
{
    return textureSize(SPIRV_Cross_Combinedr_spd_mipsSPIRV_Cross_DummySampler, int(uMipLevel));
}

vec2 ffxMin(vec2 x, vec2 y)
{
    return min(x, y);
}

vec2 ffxMax(vec2 x, vec2 y)
{
    return max(x, y);
}

vec2 ClampUv(vec2 fUv, ivec2 iTextureSize, ivec2 iResourceSize)
{
    vec2 fSampleLocation = fUv * vec2(iTextureSize);
    vec2 param = fSampleLocation;
    vec2 param_1 = vec2(iTextureSize) - vec2(0.5);
    vec2 param_2 = vec2(0.5);
    vec2 param_3 = ffxMin(param, param_1);
    vec2 fClampedLocation = ffxMax(param_2, param_3);
    vec2 fClampedUv = fClampedLocation / vec2(iResourceSize);
    return fClampedUv;
}

vec2 SampleSPDMipLevel(vec2 fUV, uint mipLevel)
{
    return textureLod(SPIRV_Cross_Combinedr_spd_mipss_LinearClamp, fUV, float(mipLevel)).xy;
}

ShadingChangeLumaInfo ComputeShadingChangeLuma(ivec2 iPxPos, vec2 fUv, ivec2 iCurrentSize)
{
    uint param = 0u;
    vec2 param_1 = fUv;
    ivec2 param_2 = ShadingChangeRenderSize();
    ivec2 param_3 = GetSPDMipDimensions(param);
    vec2 fMipUv = ClampUv(param_1, param_2, param_3);
    ShadingChangeLumaInfo info;
    for (int iMipLevel = 0; iMipLevel < 3; iMipLevel++)
    {
        vec2 param_4 = fMipUv;
        uint param_5 = uint(iMipLevel);
        vec2 fSample = SampleSPDMipLevel(param_4, param_5);
        info.fSamples[iMipLevel] = abs(fSample.x * fSample.y);
    }
    return info;
}

float ffxMax(float x, float y)
{
    return max(x, y);
}

float ffxSaturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

void StoreShadingChange(ivec2 iPxPos, float fShadingChange)
{
    imageStore(rw_shading_change, iPxPos, vec4(fShadingChange, 0.0, 0.0, 0.0));
}

void ShadingChange(ivec2 iPxPos)
{
    ivec2 param = iPxPos;
    ivec2 param_1 = ShadingChangeRenderSize();
    if (IsOnScreen(param, param_1))
    {
        vec2 fUv = (vec2(iPxPos) + vec2(0.5)) / vec2(ShadingChangeRenderSize());
        vec2 fUvJittered = fUv + (Jitter() / vec2(RenderSize()));
        ivec2 param_2 = iPxPos;
        vec2 param_3 = fUvJittered;
        ShadingChangeLumaInfo info = ComputeShadingChangeLuma(param_2, param_3, ShadingChangeRenderSize());
        float fShadingChange = 0.0;
        for (int iMipLevel = 0; iMipLevel < 3; iMipLevel++)
        {
            if (info.fSamples[iMipLevel] > 0.0)
            {
                float param_4 = fShadingChange;
                float param_5 = info.fSamples[iMipLevel];
                fShadingChange = ffxMax(param_4, param_5) * 1.0;
            }
        }
        float param_6 = fShadingChange;
        ivec2 param_7 = iPxPos;
        float param_8 = ffxSaturate(param_6);
        StoreShadingChange(param_7, param_8);
    }
}

void main()
{
    ivec2 param = ivec2(gl_GlobalInvocationID.xy);
    ShadingChange(param);
}

)FSR3";

// prepare_reactivity_essl320.glsl sha256 821a384220c16449a89eff8a4d97463c91c1cfbe1e16d2a0263dc4552076374f
inline constexpr const char* kPrepareReactivity = R"FSR3(#version 320 es
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

struct BilinearSamplingData
{
    ivec2 iOffsets[4];
    float fWeights[4];
    ivec2 iBasePos;
};

const ivec2 _946[9] = ivec2[](ivec2(0), ivec2(-1), ivec2(0, -1), ivec2(1, -1), ivec2(-1, 0), ivec2(1, 0), ivec2(-1, 1), ivec2(0, 1), ivec2(1));
const uint _1071[4] = uint[](23u, 45u, 209u, 417u);

layout(binding = 12, std140) uniform cbFSR3UPSCALER_t
{
    ivec2 iRenderSize;
    ivec2 iPreviousFrameRenderSize;
    ivec2 iUpscaleSize;
    ivec2 iPreviousFrameUpscaleSize;
    ivec2 iMaxRenderSize;
    ivec2 iMaxUpscaleSize;
    vec4 fDeviceToViewDepth;
    vec2 fJitter;
    vec2 fPreviousFrameJitter;
    vec2 fMotionVectorScale;
    vec2 fDownscaleFactor;
    vec2 fMotionVectorJitterCancellation;
    float fTanHalfFOV;
    float fJitterSequenceLength;
    float fDeltaTime;
    float fDeltaPreExposure;
    float fViewSpaceToMetersFactor;
    float fFrameIndex;
    float fVelocityFactor;
    float fReactivenessScale;
    float fShadingChangeScale;
    float fAccumulationAddedPerFrame;
    float fMinDisocclusionAccumulation;
} cbFSR3Upscaler;

layout(binding = 11, rgba8) uniform writeonly highp image2D rw_accumulation;
layout(binding = 23, rgba8) uniform coherent writeonly highp image2D rw_new_locks;
layout(binding = 9, rgba8) uniform writeonly highp image2D rw_dilated_reactive_masks;
layout(binding = 10, rgba8) uniform coherent readonly highp image2D r_rw_new_locks;
uniform highp sampler2D SPIRV_Cross_Combinedr_dilated_motion_vectorsSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_dilated_depthSPIRV_Cross_DummySampler;
uniform highp usampler2D SPIRV_Cross_Combinedr_reconstructed_previous_nearest_depthSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_reactive_maskSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_shading_changes_LinearClamp;
uniform highp sampler2D SPIRV_Cross_Combinedr_transparency_and_composition_maskSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_transparency_and_composition_masks_LinearClamp;
uniform highp sampler2D SPIRV_Cross_Combinedr_accumulations_LinearClamp;
uniform highp sampler2D SPIRV_Cross_Combinedr_current_lumaSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler;

ivec2 RenderSize()
{
    return cbFSR3Upscaler.iRenderSize;
}

vec2 LoadDilatedMotionVector(ivec2 iPxInput)
{
    return texelFetch(SPIRV_Cross_Combinedr_dilated_motion_vectorsSPIRV_Cross_DummySampler, iPxInput, 0).xy;
}

float Get4KVelocity(vec2 fMotionVector)
{
    return length(fMotionVector * vec2(3840.0, 2160.0));
}

float LoadDilatedDepth(ivec2 iPxInput)
{
    return texelFetch(SPIRV_Cross_Combinedr_dilated_depthSPIRV_Cross_DummySampler, iPxInput, 0).x;
}

vec4 DeviceToViewSpaceTransformFactors()
{
    return cbFSR3Upscaler.fDeviceToViewDepth;
}

float GetViewSpaceDepth(float fDeviceDepth)
{
    vec4 fDeviceToViewDepth = DeviceToViewSpaceTransformFactors();
    return fDeviceToViewDepth.y / (fDeviceDepth - fDeviceToViewDepth.x);
}

float ViewSpaceToMetersFactor()
{
    return cbFSR3Upscaler.fViewSpaceToMetersFactor;
}

float GetViewSpaceDepthInMeters(float fDeviceDepth)
{
    float param = fDeviceDepth;
    return GetViewSpaceDepth(param) * ViewSpaceToMetersFactor();
}

float ffxMin(float x, float y)
{
    return min(x, y);
}

float ffxSaturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

float ffxLerp(float x, float y, float t)
{
    return mix(x, y, t);
}

float ReconstructedDepthMvPxThreshold(float fNearestDepthInMeters)
{
    float param = fNearestDepthInMeters / 100.0;
    float param_1 = 0.25;
    float param_2 = 0.75;
    float param_3 = ffxSaturate(param);
    return ffxLerp(param_1, param_2, param_3);
}

vec2 ffxFract(vec2 x)
{
    return fract(x);
}

BilinearSamplingData GetBilinearSamplingData(vec2 fUv, ivec2 iSize)
{
    vec2 fPxSample = (fUv * vec2(iSize)) - vec2(0.5);
    BilinearSamplingData data;
    data.iBasePos = ivec2(floor(fPxSample));
    vec2 param = fPxSample;
    vec2 fPxFrac = ffxFract(param);
    data.iOffsets[0] = ivec2(0);
    data.iOffsets[1] = ivec2(1, 0);
    data.iOffsets[2] = ivec2(0, 1);
    data.iOffsets[3] = ivec2(1);
    data.fWeights[0] = (1.0 - fPxFrac.x) * (1.0 - fPxFrac.y);
    data.fWeights[1] = fPxFrac.x * (1.0 - fPxFrac.y);
    data.fWeights[2] = (1.0 - fPxFrac.x) * fPxFrac.y;
    data.fWeights[3] = fPxFrac.x * fPxFrac.y;
    return data;
}

int ffxMin(int x, int y)
{
    return min(x, y);
}

int ffxMax(int x, int y)
{
    return max(x, y);
}

ivec2 ClampLoad(ivec2 iPxSample, ivec2 iPxOffset, ivec2 iTextureSize)
{
    ivec2 result = iPxSample + iPxOffset;
    int param = result.x;
    int param_1 = iTextureSize.x - 1;
    int param_2 = 0;
    int param_3 = ffxMin(param, param_1);
    result.x = ffxMax(param_2, param_3);
    int param_4 = result.y;
    int param_5 = iTextureSize.y - 1;
    int param_6 = 0;
    int param_7 = ffxMin(param_4, param_5);
    result.y = ffxMax(param_6, param_7);
    return result;
}

bool IsOnScreen(ivec2 pos, ivec2 size)
{
    return all(lessThan(uvec2(pos), uvec2(size)));
}

float LoadReconstructedPrevDepth(ivec2 iPxPos)
{
    return uintBitsToFloat(texelFetch(SPIRV_Cross_Combinedr_reconstructed_previous_nearest_depthSPIRV_Cross_DummySampler, iPxPos, 0).x);
}

float ffxMax(float x, float y)
{
    return max(x, y);
}

float ComputeDisocclusions(vec2 fUv, inout vec2 fMotionVector, float fCurrentDepthViewSpace)
{
    float param = fCurrentDepthViewSpace * ViewSpaceToMetersFactor();
    float param_1 = 65504.0;
    float fNearestDepthInMeters = ffxMin(param, param_1);
    float param_2 = fNearestDepthInMeters;
    float fReconstructedDeptMvThreshold = ReconstructedDepthMvPxThreshold(param_2);
    vec2 param_3 = fMotionVector;
    fMotionVector *= float(Get4KVelocity(param_3) > fReconstructedDeptMvThreshold);
    vec2 fReprojectedUv = fUv + fMotionVector;
    vec2 param_4 = fReprojectedUv;
    ivec2 param_5 = RenderSize();
    BilinearSamplingData bilinearInfo = GetBilinearSamplingData(param_4, param_5);
    float fDisocclusion = 0.0;
    float fWeightSum = 0.0;
    bool bPotentialDisocclusion = true;
    for (int iSampleIndex = 0; (iSampleIndex < 4) && bPotentialDisocclusion; iSampleIndex++)
    {
        ivec2 iOffset = bilinearInfo.iOffsets[iSampleIndex];
        ivec2 param_6 = bilinearInfo.iBasePos;
        ivec2 param_7 = iOffset;
        ivec2 param_8 = RenderSize();
        ivec2 iSamplePos = ClampLoad(param_6, param_7, param_8);
        ivec2 param_9 = iSamplePos;
        ivec2 param_10 = RenderSize();
        if (IsOnScreen(param_9, param_10))
        {
            float fWeight = bilinearInfo.fWeights[iSampleIndex];
            if (fWeight > 0.0006099999882280826568603515625)
            {
                ivec2 param_11 = iSamplePos;
                float param_12 = LoadReconstructedPrevDepth(param_11);
                float fPrevNearestDepthViewSpace = GetViewSpaceDepth(param_12);
                float fDepthDifference = fCurrentDepthViewSpace - fPrevNearestDepthViewSpace;
                bPotentialDisocclusion = bPotentialDisocclusion && (fDepthDifference > 1.1754943508222875079687365372222e-38);
                if (bPotentialDisocclusion)
                {
                    float fHalfViewportWidth = length(vec2(RenderSize()) * 0.5);
                    float param_13 = fCurrentDepthViewSpace;
                    float param_14 = fPrevNearestDepthViewSpace;
                    float fDepthThreshold = ffxMax(param_13, param_14);
                    float fRequiredDepthSeparation = (1.3699999726668465882539749145508e-05 * fHalfViewportWidth) * fDepthThreshold;
                    float param_15 = fRequiredDepthSeparation / fDepthDifference;
                    fDisocclusion += (ffxSaturate(param_15) * fWeight);
                    fWeightSum += fWeight;
                }
            }
        }
    }
    float _795;
    if (bPotentialDisocclusion && (fWeightSum > 0.0))
    {
        float param_16 = 1.0 - (fDisocclusion / fWeightSum);
        _795 = ffxSaturate(param_16);
    }
    else
    {
        _795 = 0.0;
    }
    fDisocclusion = _795;
    return fDisocclusion;
}

float LoadReactiveMask(ivec2 iPxPos)
{
    return texelFetch(SPIRV_Cross_Combinedr_reactive_maskSPIRV_Cross_DummySampler, iPxPos, 0).x * cbFSR3Upscaler.fReactivenessScale;
}

float DilateReactiveMasks(ivec2 iPxPos, vec2 fUv)
{
    float fDilatedReactiveMasks = 0.0;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            ivec2 param = iPxPos;
            ivec2 param_1 = ivec2(x, y);
            ivec2 param_2 = RenderSize();
            ivec2 sampleCoord = ClampLoad(param, param_1, param_2);
            ivec2 param_3 = sampleCoord;
            float param_4 = fDilatedReactiveMasks;
            float param_5 = LoadReactiveMask(param_3);
            fDilatedReactiveMasks = ffxMax(param_4, param_5);
        }
    }
    return fDilatedReactiveMasks;
}

vec2 Jitter()
{
    return cbFSR3Upscaler.fJitter;
}

ivec2 ShadingChangeRenderSize()
{
    return ivec2(vec2(RenderSize()) * 0.5);
}

ivec2 MaxRenderSize()
{
    return cbFSR3Upscaler.iMaxRenderSize;
}

ivec2 ShadingChangeMaxRenderSize()
{
    return ivec2(vec2(MaxRenderSize()) * 0.5);
}

vec2 ffxMin(vec2 x, vec2 y)
{
    return min(x, y);
}

vec2 ffxMax(vec2 x, vec2 y)
{
    return max(x, y);
}

vec2 ClampUv(vec2 fUv, ivec2 iTextureSize, ivec2 iResourceSize)
{
    vec2 fSampleLocation = fUv * vec2(iTextureSize);
    vec2 param = fSampleLocation;
    vec2 param_1 = vec2(iTextureSize) - vec2(0.5);
    vec2 param_2 = vec2(0.5);
    vec2 param_3 = ffxMin(param, param_1);
    vec2 fClampedLocation = ffxMax(param_2, param_3);
    vec2 fClampedUv = fClampedLocation / vec2(iResourceSize);
    return fClampedUv;
}

float SampleShadingChange(vec2 fUV)
{
    return textureLod(SPIRV_Cross_Combinedr_shading_changes_LinearClamp, fUV, 0.0).x * cbFSR3Upscaler.fShadingChangeScale;
}

float ComputeShadingChange(vec2 fUv)
{
    vec2 param = fUv - (Jitter() / vec2(RenderSize()));
    ivec2 param_1 = ShadingChangeRenderSize();
    ivec2 param_2 = ShadingChangeMaxRenderSize();
    vec2 fShadingChangeUv = ClampUv(param, param_1, param_2);
    vec2 param_3 = fShadingChangeUv;
    float param_4 = SampleShadingChange(param_3);
    float fShadingChange = ffxSaturate(param_4);
    return fShadingChange;
}

float MinDividedByMax(float v0, float v1)
{
    float param = v0;
    float param_1 = v1;
    float m = ffxMax(param, param_1);
    float _471;
    if (m != 0.0)
    {
        float param_2 = v0;
        float param_3 = v1;
        _471 = ffxMin(param_2, param_3) / m;
    }
    else
    {
        _471 = 0.0;
    }
    return _471;
}

float ComputeMotionDivergence(vec2 fUv, vec2 fMotionVector, float fCurrentDepthSample)
{
    ivec2 iPxReprojectedPos = ivec2((fUv + fMotionVector) * vec2(RenderSize()));
    ivec2 param = iPxReprojectedPos;
    float fReprojectedDepth = LoadDilatedDepth(param);
    ivec2 param_1 = iPxReprojectedPos;
    vec2 fReprojectedMotionVector = LoadDilatedMotionVector(param_1);
    vec2 param_2 = fReprojectedMotionVector;
    float fReprojectedVelocity = Get4KVelocity(param_2);
    vec2 param_3 = fMotionVector;
    float f4KVelocity = Get4KVelocity(param_3);
    float fMaxLen = max(length(fMotionVector), length(fReprojectedMotionVector));
    float param_4 = fReprojectedDepth;
    float fNucleusDepthInMeters = GetViewSpaceDepthInMeters(param_4);
    float param_5 = fCurrentDepthSample;
    float fCurrentDepthInMeters = GetViewSpaceDepthInMeters(param_5);
    float fDistanceFactor = MinDividedByMax(fNucleusDepthInMeters, fCurrentDepthInMeters);
    float param_6 = f4KVelocity / 10.0;
    float fVelocityFactor = ffxSaturate(param_6);
    float param_7 = fReprojectedVelocity / f4KVelocity;
    float fMotionVectorFieldConfidence = ((1.0 - ffxSaturate(param_7)) * fDistanceFactor) * fVelocityFactor;
    return fMotionVectorFieldConfidence;
}

ivec2 GetTransparencyAndCompositionMaskResourceDimensions()
{
    return textureSize(SPIRV_Cross_Combinedr_transparency_and_composition_maskSPIRV_Cross_DummySampler, 0);
}

float SampleTransparencyAndCompositionMask(vec2 fUV)
{
    return textureLod(SPIRV_Cross_Combinedr_transparency_and_composition_masks_LinearClamp, fUV, 0.0).x;
}

float DilateTransparencyAndCompositionMasks(ivec2 iPxPos, vec2 fUv)
{
    vec2 param = fUv;
    ivec2 param_1 = RenderSize();
    ivec2 param_2 = GetTransparencyAndCompositionMaskResourceDimensions();
    vec2 fUvTransparencyAndCompositionMask = ClampUv(param, param_1, param_2);
    vec2 param_3 = fUvTransparencyAndCompositionMask;
    return SampleTransparencyAndCompositionMask(param_3);
}

bool IsUvInside(vec2 fUv)
{
    bool _485 = fUv.x >= 0.0;
    bool _491;
    if (_485)
    {
        _491 = fUv.x <= 1.0;
    }
    else
    {
        _491 = _485;
    }
    bool _504;
    if (_491)
    {
        bool _497 = fUv.y >= 0.0;
        bool _503;
        if (_497)
        {
            _503 = fUv.y <= 1.0;
        }
        else
        {
            _503 = _497;
        }
        _504 = _503;
    }
    else
    {
        _504 = _491;
    }
    return _504;
}

ivec2 PreviousFrameRenderSize()
{
    return cbFSR3Upscaler.iPreviousFrameRenderSize;
}

float SampleAccumulation(vec2 fUV)
{
    return textureLod(SPIRV_Cross_Combinedr_accumulations_LinearClamp, fUV, 0.0).x;
}

float AccumulationAddedPerFrame()
{
    return cbFSR3Upscaler.fAccumulationAddedPerFrame;
}

float MinDisocclusionAccumulation()
{
    return cbFSR3Upscaler.fMinDisocclusionAccumulation;
}

void StoreAccumulation(ivec2 iPxPos, float fAccumulation)
{
    imageStore(rw_accumulation, iPxPos, vec4(fAccumulation, 0.0, 0.0, 0.0));
}

float UpdateAccumulation(ivec2 iPxPos, vec2 fUv, vec2 fMotionVector, float fDisocclusion, float fShadingChange)
{
    vec2 fReprojectedUv = fUv + fMotionVector;
    float fAccumulation = 0.0;
    vec2 param = fReprojectedUv;
    if (IsUvInside(param))
    {
        vec2 param_1 = fReprojectedUv;
        ivec2 param_2 = PreviousFrameRenderSize();
        ivec2 param_3 = MaxRenderSize();
        vec2 fReprojectedUv_HW = ClampUv(param_1, param_2, param_3);
        vec2 param_4 = fReprojectedUv_HW;
        float param_5 = SampleAccumulation(param_4);
        fAccumulation = ffxSaturate(param_5);
    }
    float fAccumulationAddedPerFrame = AccumulationAddedPerFrame();
    float param_6 = fAccumulation;
    float param_7 = 0.0;
    float param_8 = fShadingChange;
    fAccumulation = ffxLerp(param_6, param_7, param_8);
    float fMinDisocclusionAccumulation = MinDisocclusionAccumulation();
    float param_9 = fMinDisocclusionAccumulation;
    float param_10 = fAccumulation;
    float param_11 = fAccumulation;
    float param_12 = ffxMin(param_9, param_10);
    float param_13 = fDisocclusion;
    fAccumulation = ffxLerp(param_11, param_12, param_13);
    fAccumulation *= float(round(fAccumulation * 100.0) > 1.0);
    float param_14 = fAccumulation + fAccumulationAddedPerFrame;
    float fAccumulatedFramesToStore = ffxSaturate(param_14);
    ivec2 param_15 = iPxPos;
    float param_16 = fAccumulatedFramesToStore;
    StoreAccumulation(param_15, param_16);
    return fAccumulation;
}

void StoreDilatedReactiveMasks(ivec2 iPxPos, vec4 fDilatedReactiveMasks)
{
    imageStore(rw_dilated_reactive_masks, iPxPos, fDilatedReactiveMasks);
}

float LoadCurrentLuma(ivec2 iPxPos)
{
    return texelFetch(SPIRV_Cross_Combinedr_current_lumaSPIRV_Cross_DummySampler, iPxPos, 0).x;
}

float Exposure()
{
    float exposure = texelFetch(SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler, ivec2(0), 0).x;
    if (exposure == 0.0)
    {
        exposure = 1.0;
    }
    return exposure;
}

float ComputeThinFeatureConfidence(ivec2 iPxPos)
{
    float fLumaMin = 3.4028234663852885981170418348452e+38;
    float fLumaMax = 1.1754943508222875079687365372222e-38;
    float fSamples[9];
    for (int iSampleIndex = 0; iSampleIndex < 9; iSampleIndex++)
    {
        ivec2 param = iPxPos;
        ivec2 param_1 = _946[iSampleIndex];
        ivec2 param_2 = RenderSize();
        ivec2 iPxSamplePos = ClampLoad(param, param_1, param_2);
        ivec2 param_3 = iPxSamplePos;
        fSamples[iSampleIndex] = LoadCurrentLuma(param_3) * Exposure();
        float param_4 = fLumaMin;
        float param_5 = fSamples[iSampleIndex];
        fLumaMin = ffxMin(param_4, param_5);
        float param_6 = fLumaMax;
        float param_7 = fSamples[iSampleIndex];
        fLumaMax = ffxMax(param_6, param_7);
    }
    float fDissimilarLumaMin = 3.4028234663852885981170418348452e+38;
    float fDissimilarLumaMax = 0.0;
    uint uPatternMask = 1u;
    for (int iBitIndex = 1, iSampleIndex_1 = 1; iSampleIndex_1 < 9; iSampleIndex_1++, iBitIndex++)
    {
        float fDifference = abs(fSamples[iSampleIndex_1] - fSamples[0]) / (fLumaMax - fLumaMin);
        if (fDifference < 0.89999997615814208984375)
        {
            uPatternMask |= (1u << uint(iBitIndex));
        }
        else
        {
            float param_8 = fDissimilarLumaMin;
            float param_9 = fSamples[iSampleIndex_1];
            fDissimilarLumaMin = ffxMin(param_8, param_9);
            float param_10 = fDissimilarLumaMax;
            float param_11 = fSamples[iSampleIndex_1];
            fDissimilarLumaMax = ffxMax(param_10, param_11);
        }
    }
    bool _1041 = fSamples[0] > fDissimilarLumaMax;
    bool _1049;
    if (!_1041)
    {
        _1049 = fSamples[0] < fDissimilarLumaMin;
    }
    else
    {
        _1049 = _1041;
    }
    bool bIsRidge = _1049;
    if (false == bIsRidge)
    {
        return 0.0;
    }
    for (int i = 0; uint(i) < 4u; i++)
    {
        if ((uPatternMask & _1071[i]) == _1071[i])
        {
            return 0.0;
        }
    }
    return 1.0 - (fLumaMin / fLumaMax);
}

ivec2 UpscaleSize()
{
    return cbFSR3Upscaler.iUpscaleSize;
}

ivec2 ComputeHrPosFromLrPos(ivec2 iPxLrPos)
{
    vec2 fSrcJitteredPos = (vec2(iPxLrPos) + vec2(0.5)) - Jitter();
    vec2 fLrPosInHr = (fSrcJitteredPos / vec2(RenderSize())) * vec2(UpscaleSize());
    ivec2 iPxHrPos = ivec2(floor(fLrPosInHr));
    return iPxHrPos;
}

void StoreNewLocks(ivec2 iPxPos, float newLock)
{
    imageStore(rw_new_locks, iPxPos, vec4(newLock, 0.0, 0.0, 0.0));
}

void PrepareReactivity(ivec2 iPxPos)
{
    vec2 fUv = (vec2(iPxPos) + vec2(0.5)) / vec2(RenderSize());
    ivec2 param = iPxPos;
    vec2 fMotionVector = LoadDilatedMotionVector(param);
    vec2 param_1 = fMotionVector;
    float f4KVelocity = Get4KVelocity(param_1);
    vec2 fDilatedUv = fUv + fMotionVector;
    ivec2 param_2 = iPxPos;
    float fDilatedDepth = LoadDilatedDepth(param_2);
    float param_3 = fDilatedDepth;
    float fDepthInMeters = GetViewSpaceDepthInMeters(param_3);
    float param_4 = fDilatedDepth;
    vec2 param_5 = fUv;
    vec2 param_6 = fMotionVector;
    float param_7 = GetViewSpaceDepth(param_4);
    float _1218 = ComputeDisocclusions(param_5, param_6, param_7);
    float fDisocclusion = _1218;
    ivec2 param_8 = iPxPos;
    vec2 param_9 = fUv;
    vec2 param_10 = fUv;
    float param_11 = DilateReactiveMasks(param_8, param_9);
    float param_12 = ComputeShadingChange(param_10);
    float fShadingChange = ffxMax(param_11, param_12);
    vec2 param_13 = fUv;
    vec2 param_14 = fMotionVector;
    float param_15 = fDilatedDepth;
    float fMotionDivergence = ComputeMotionDivergence(param_13, param_14, param_15);
    ivec2 param_16 = iPxPos;
    vec2 param_17 = fUv;
    float fDilatedTransparencyAndComposition = DilateTransparencyAndCompositionMasks(param_16, param_17);
    float param_18 = fMotionDivergence;
    float param_19 = fDilatedTransparencyAndComposition;
    float fFinalReactiveness = ffxMax(param_18, param_19);
    ivec2 param_20 = iPxPos;
    vec2 param_21 = fUv;
    vec2 param_22 = fMotionVector;
    float param_23 = fDisocclusion;
    float param_24 = fShadingChange;
    float _1262 = UpdateAccumulation(param_20, param_21, param_22, param_23, param_24);
    float fAccumulation = _1262;
    vec4 fOutput;
    fOutput.x = fFinalReactiveness;
    fOutput.y = fDisocclusion;
    fOutput.z = fShadingChange;
    fOutput.w = fAccumulation;
    ivec2 param_25 = iPxPos;
    vec4 param_26 = fOutput;
    StoreDilatedReactiveMasks(param_25, param_26);
    ivec2 param_27 = iPxPos;
    float fLockStrength = ComputeThinFeatureConfidence(param_27);
    if (fLockStrength > 0.00999999977648258209228515625)
    {
        ivec2 param_28 = iPxPos;
        ivec2 param_29 = ComputeHrPosFromLrPos(param_28);
        float param_30 = fLockStrength;
        StoreNewLocks(param_29, param_30);
    }
}

void main()
{
    ivec2 param = ivec2(gl_GlobalInvocationID.xy);
    PrepareReactivity(param);
}

)FSR3";

// luma_instability_essl320.glsl sha256 b8425c8503ca6371a8f37e640262aba614c08ec201469c4d535529963ee2a153
inline constexpr const char* kLumaInstability = R"FSR3(#version 320 es
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

struct LumaInstabilityFactorData
{
    vec4 fLumaHistory;
    float fLumaInstabilityFactor;
};

layout(binding = 9, std140) uniform cbFSR3UPSCALER_t
{
    ivec2 iRenderSize;
    ivec2 iPreviousFrameRenderSize;
    ivec2 iUpscaleSize;
    ivec2 iPreviousFrameUpscaleSize;
    ivec2 iMaxRenderSize;
    ivec2 iMaxUpscaleSize;
    vec4 fDeviceToViewDepth;
    vec2 fJitter;
    vec2 fPreviousFrameJitter;
    vec2 fMotionVectorScale;
    vec2 fDownscaleFactor;
    vec2 fMotionVectorJitterCancellation;
    float fTanHalfFOV;
    float fJitterSequenceLength;
    float fDeltaTime;
    float fDeltaPreExposure;
    float fViewSpaceToMetersFactor;
    float fFrameIndex;
    float fVelocityFactor;
    float fReactivenessScale;
    float fShadingChangeScale;
    float fAccumulationAddedPerFrame;
    float fMinDisocclusionAccumulation;
} cbFSR3Upscaler;

layout(binding = 7, rgba16f) uniform writeonly highp image2D rw_luma_history;
layout(binding = 8, rgba16f) uniform writeonly highp image2D rw_luma_instability;
uniform highp sampler2D SPIRV_Cross_Combinedr_dilated_motion_vectorsSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_dilated_reactive_maskss_LinearClamp;
uniform highp sampler2D SPIRV_Cross_Combinedr_current_lumas_LinearClamp;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_luma_historys_LinearClamp;
uniform highp sampler2D SPIRV_Cross_Combinedr_farthest_depth_mip1SPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_farthest_depth_mip1s_LinearClamp;

vec2 LoadDilatedMotionVector(ivec2 iPxInput)
{
    return texelFetch(SPIRV_Cross_Combinedr_dilated_motion_vectorsSPIRV_Cross_DummySampler, iPxInput, 0).xy;
}

ivec2 RenderSize()
{
    return cbFSR3Upscaler.iRenderSize;
}

vec2 Jitter()
{
    return cbFSR3Upscaler.fJitter;
}

vec2 PreviousFrameJitter()
{
    return cbFSR3Upscaler.fPreviousFrameJitter;
}

ivec2 PreviousFrameRenderSize()
{
    return cbFSR3Upscaler.iPreviousFrameRenderSize;
}

bool IsUvInside(vec2 fUv)
{
    bool _306 = fUv.x >= 0.0;
    bool _312;
    if (_306)
    {
        _312 = fUv.x <= 1.0;
    }
    else
    {
        _312 = _306;
    }
    bool _325;
    if (_312)
    {
        bool _318 = fUv.y >= 0.0;
        bool _324;
        if (_318)
        {
            _324 = fUv.y <= 1.0;
        }
        else
        {
            _324 = _318;
        }
        _325 = _324;
    }
    else
    {
        _325 = _312;
    }
    return _325;
}

ivec2 MaxRenderSize()
{
    return cbFSR3Upscaler.iMaxRenderSize;
}

vec2 ffxMin(vec2 x, vec2 y)
{
    return min(x, y);
}

vec2 ffxMax(vec2 x, vec2 y)
{
    return max(x, y);
}

vec2 ClampUv(vec2 fUv, ivec2 iTextureSize, ivec2 iResourceSize)
{
    vec2 fSampleLocation = fUv * vec2(iTextureSize);
    vec2 param = fSampleLocation;
    vec2 param_1 = vec2(iTextureSize) - vec2(0.5);
    vec2 param_2 = vec2(0.5);
    vec2 param_3 = ffxMin(param, param_1);
    vec2 fClampedLocation = ffxMax(param_2, param_3);
    vec2 fClampedUv = fClampedLocation / vec2(iResourceSize);
    return fClampedUv;
}

vec4 SampleDilatedReactiveMasks(vec2 fUV)
{
    return textureLod(SPIRV_Cross_Combinedr_dilated_reactive_maskss_LinearClamp, fUV, 0.0);
}

float ffxSaturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

float SampleCurrentLuma(vec2 uv)
{
    return textureLod(SPIRV_Cross_Combinedr_current_lumas_LinearClamp, uv, 0.0).x;
}

float Exposure()
{
    float exposure = texelFetch(SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler, ivec2(0), 0).x;
    if (exposure == 0.0)
    {
        exposure = 1.0;
    }
    return exposure;
}

vec4 SampleLumaHistory(vec2 fUV)
{
    return textureLod(SPIRV_Cross_Combinedr_luma_historys_LinearClamp, fUV, 0.0);
}

float DeltaPreExposure()
{
    return cbFSR3Upscaler.fDeltaPreExposure;
}

ivec2 GetFarthestDepthMip1ResourceDimensions()
{
    return textureSize(SPIRV_Cross_Combinedr_farthest_depth_mip1SPIRV_Cross_DummySampler, 0);
}

float SampleFarthestDepthMip1(vec2 fUV)
{
    return textureLod(SPIRV_Cross_Combinedr_farthest_depth_mip1s_LinearClamp, fUV, 0.0).x;
}

float ffxMax(float x, float y)
{
    return max(x, y);
}

float ffxMin(float x, float y)
{
    return min(x, y);
}

float MinDividedByMax(float v0, float v1, float fOnZeroReturnValue)
{
    float param = v0;
    float param_1 = v1;
    float m = ffxMax(param, param_1);
    float _274;
    if (m != 0.0)
    {
        float param_2 = v0;
        float param_3 = v1;
        _274 = ffxMin(param_2, param_3) / m;
    }
    else
    {
        _274 = fOnZeroReturnValue;
    }
    return _274;
}

float MinDividedByMax(float v0, float v1)
{
    float param = v0;
    float param_1 = v1;
    float m = ffxMax(param, param_1);
    float _292;
    if (m != 0.0)
    {
        float param_2 = v0;
        float param_3 = v1;
        _292 = ffxMin(param_2, param_3) / m;
    }
    else
    {
        _292 = 0.0;
    }
    return _292;
}

LumaInstabilityFactorData ComputeLumaInstabilityFactor(inout LumaInstabilityFactorData data, float fCurrentFrameLuma, float fFarthestDepthInMeters)
{
    float fLumaInstability = 0.0;
    float fDiffs0 = fCurrentFrameLuma - data.fLumaHistory.x;
    float fSimilarity0 = MinDividedByMax(fCurrentFrameLuma, data.fLumaHistory.x, 1.0);
    float fMaxSimilarity = fSimilarity0;
    if (fSimilarity0 < 1.0)
    {
        for (int i = 1; i <= 3; i++)
        {
            float fDiffs1 = fCurrentFrameLuma - data.fLumaHistory[i];
            float fSimilarity1 = MinDividedByMax(fCurrentFrameLuma, data.fLumaHistory[i]);
            if (sign(fDiffs0) == sign(fDiffs1))
            {
                float param = fMaxSimilarity;
                float param_1 = fSimilarity1;
                fMaxSimilarity = ffxMax(param, param_1);
            }
        }
        fLumaInstability = float(fMaxSimilarity > fSimilarity0);
    }
    data.fLumaHistory.w = data.fLumaHistory.z;
    data.fLumaHistory.z = data.fLumaHistory.y;
    data.fLumaHistory.y = data.fLumaHistory.x;
    data.fLumaHistory.x = fCurrentFrameLuma;
    data.fLumaHistory /= vec4(Exposure());
    data.fLumaInstabilityFactor = fLumaInstability * float(data.fLumaHistory.w != 0.0);
    return data;
}

float Get4KVelocity(vec2 fMotionVector)
{
    return length(fMotionVector * vec2(3840.0, 2160.0));
}

void StoreLumaHistory(ivec2 iPxPos, vec4 fLumaHistory)
{
    imageStore(rw_luma_history, iPxPos, fLumaHistory);
}

void StoreLumaInstability(ivec2 iPxPos, float fLumaInstability)
{
    imageStore(rw_luma_instability, iPxPos, vec4(fLumaInstability, 0.0, 0.0, 0.0));
}

void LumaInstability(ivec2 iPxPos)
{
    LumaInstabilityFactorData data;
    data.fLumaInstabilityFactor = 0.0;
    data.fLumaHistory = vec4(0.0);
    ivec2 param = iPxPos;
    vec2 fDilatedMotionVector = LoadDilatedMotionVector(param);
    vec2 fUv = (vec2(iPxPos) + vec2(0.5)) / vec2(RenderSize());
    vec2 fUvCurrFrameJittered = fUv + (Jitter() / vec2(RenderSize()));
    vec2 fUvPrevFrameJittered = fUv + (PreviousFrameJitter() / vec2(PreviousFrameRenderSize()));
    vec2 fReprojectedUv = fUvPrevFrameJittered + fDilatedMotionVector;
    vec2 param_1 = fReprojectedUv;
    if (IsUvInside(param_1))
    {
        vec2 param_2 = fUvCurrFrameJittered;
        ivec2 param_3 = RenderSize();
        ivec2 param_4 = MaxRenderSize();
        vec2 fUvReactive_HW = ClampUv(param_2, param_3, param_4);
        vec2 param_5 = fUvReactive_HW;
        vec4 fDilatedReactiveMasks = SampleDilatedReactiveMasks(param_5);
        float param_6 = fDilatedReactiveMasks.x;
        float fReactiveMask = ffxSaturate(param_6);
        float param_7 = fDilatedReactiveMasks.y;
        float fDisocclusion = ffxSaturate(param_7);
        float param_8 = fDilatedReactiveMasks.z;
        float fShadingChange = ffxSaturate(param_8);
        float param_9 = fDilatedReactiveMasks.w;
        float fAccumulation = ffxSaturate(param_9);
        bool bAccumulationFactor = fAccumulation > 0.89999997615814208984375;
        bool bComputeInstability = bAccumulationFactor;
        if (bComputeInstability)
        {
            vec2 param_10 = fUvCurrFrameJittered;
            ivec2 param_11 = RenderSize();
            ivec2 param_12 = MaxRenderSize();
            vec2 fUv_HW = ClampUv(param_10, param_11, param_12);
            vec2 param_13 = fUv_HW;
            float fCurrentFrameLuma = SampleCurrentLuma(param_13) * Exposure();
            vec2 param_14 = fReprojectedUv;
            ivec2 param_15 = PreviousFrameRenderSize();
            ivec2 param_16 = MaxRenderSize();
            vec2 fReprojectedUv_HW = ClampUv(param_14, param_15, param_16);
            vec2 param_17 = fReprojectedUv_HW;
            data.fLumaHistory = (SampleLumaHistory(param_17) * DeltaPreExposure()) * Exposure();
            vec2 param_18 = fUvCurrFrameJittered;
            ivec2 param_19 = RenderSize() / ivec2(2);
            ivec2 param_20 = GetFarthestDepthMip1ResourceDimensions();
            vec2 fFarthestDepthUv_HW = ClampUv(param_18, param_19, param_20);
            vec2 param_21 = fFarthestDepthUv_HW;
            float fFarthestDepthInMeters = SampleFarthestDepthMip1(param_21);
            LumaInstabilityFactorData param_22 = data;
            float param_23 = fCurrentFrameLuma;
            float param_24 = fFarthestDepthInMeters;
            LumaInstabilityFactorData _572 = ComputeLumaInstabilityFactor(param_22, param_23, param_24);
            data = _572;
            vec2 param_25 = fDilatedMotionVector;
            float param_26 = Get4KVelocity(param_25) / 20.0;
            float fVelocityWeight = 1.0 - ffxSaturate(param_26);
            data.fLumaInstabilityFactor *= (((fVelocityWeight * (1.0 - fDisocclusion)) * (1.0 - fReactiveMask)) * (1.0 - fShadingChange));
        }
    }
    ivec2 param_27 = iPxPos;
    vec4 param_28 = data.fLumaHistory;
    StoreLumaHistory(param_27, param_28);
    ivec2 param_29 = iPxPos;
    float param_30 = data.fLumaInstabilityFactor;
    StoreLumaInstability(param_29, param_30);
}

void main()
{
    ivec2 param = ivec2(gl_GlobalInvocationID.xy);
    LumaInstability(param);
}

)FSR3";

// accumulate_essl320.glsl sha256 f02e2a4f97841efa1ed124066e353091e0aca5fc043d0471489cdc2c45f22b16
inline constexpr const char* kAccumulate = R"FSR3(#version 320 es
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

struct RectificationBox
{
    vec3 boxCenter;
    vec3 boxVec;
    vec3 aabbMin;
    vec3 aabbMax;
    float fBoxCenterWeight;
};

struct FetchedBicubicSamples
{
    vec4 fColor00;
    vec4 fColor10;
    vec4 fColor20;
    vec4 fColor30;
    vec4 fColor01;
    vec4 fColor11;
    vec4 fColor21;
    vec4 fColor31;
    vec4 fColor02;
    vec4 fColor12;
    vec4 fColor22;
    vec4 fColor32;
    vec4 fColor03;
    vec4 fColor13;
    vec4 fColor23;
    vec4 fColor33;
};

struct AccumulationPassCommonParams
{
    ivec2 iPxHrPos;
    vec2 fHrUv;
    vec2 fLrUvJittered;
    vec2 fLrUv_HwSampler;
    vec2 fMotionVector;
    vec2 fReprojectedHrUv;
    float f4KVelocity;
    float fDisocclusion;
    float fReactiveMask;
    float fShadingChange;
    float fAccumulation;
    float fLumaInstabilityFactor;
    float fFarthestDepthInMeters;
    bool bIsExistingSample;
    bool bIsNewSample;
};

struct AccumulationPassData
{
    RectificationBox clippingBox;
    vec3 fUpsampledColor;
    float fUpsampledWeight;
    vec3 fHistoryColor;
    float fHistoryWeight;
    float fLock;
    float fLockContributionThisFrame;
};

layout(binding = 12, std140) uniform cbFSR3UPSCALER_t
{
    ivec2 iRenderSize;
    ivec2 iPreviousFrameRenderSize;
    ivec2 iUpscaleSize;
    ivec2 iPreviousFrameUpscaleSize;
    ivec2 iMaxRenderSize;
    ivec2 iMaxUpscaleSize;
    vec4 fDeviceToViewDepth;
    vec2 fJitter;
    vec2 fPreviousFrameJitter;
    vec2 fMotionVectorScale;
    vec2 fDownscaleFactor;
    vec2 fMotionVectorJitterCancellation;
    float fTanHalfFOV;
    float fJitterSequenceLength;
    float fDeltaTime;
    float fDeltaPreExposure;
    float fViewSpaceToMetersFactor;
    float fFrameIndex;
    float fVelocityFactor;
    float fReactivenessScale;
    float fShadingChangeScale;
    float fAccumulationAddedPerFrame;
    float fMinDisocclusionAccumulation;
} cbFSR3Upscaler;

layout(binding = 9, rgba16f) uniform writeonly highp image2D rw_internal_upscaled_color;
layout(binding = 10, rgba8) uniform writeonly highp image2D rw_upscaled_output;
layout(binding = 11, rgba8) uniform coherent readonly highp image2D r_rw_new_locks;
layout(binding = 23, rgba8) uniform coherent writeonly highp image2D rw_new_locks;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_motion_vectorsSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_luma_instabilitys_LinearClamp;
uniform highp sampler2D SPIRV_Cross_Combinedr_farthest_depth_mip1SPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_farthest_depth_mip1s_LinearClamp;
uniform highp sampler2D SPIRV_Cross_Combinedr_dilated_reactive_maskss_LinearClamp;
uniform highp sampler2D SPIRV_Cross_Combinedr_internal_upscaled_colorSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_color_jitteredSPIRV_Cross_DummySampler;

ivec2 UpscaleSize()
{
    return cbFSR3Upscaler.iUpscaleSize;
}

vec2 Jitter()
{
    return cbFSR3Upscaler.fJitter;
}

ivec2 RenderSize()
{
    return cbFSR3Upscaler.iRenderSize;
}

ivec2 MaxRenderSize()
{
    return cbFSR3Upscaler.iMaxRenderSize;
}

vec2 ffxMin(vec2 x, vec2 y)
{
    return min(x, y);
}

vec2 ffxMax(vec2 x, vec2 y)
{
    return max(x, y);
}

vec2 ClampUv(vec2 fUv, ivec2 iTextureSize, ivec2 iResourceSize)
{
    vec2 fSampleLocation = fUv * vec2(iTextureSize);
    vec2 param = fSampleLocation;
    vec2 param_1 = vec2(iTextureSize) - vec2(0.5);
    vec2 param_2 = vec2(0.5);
    vec2 param_3 = ffxMin(param, param_1);
    vec2 fClampedLocation = ffxMax(param_2, param_3);
    vec2 fClampedUv = fClampedLocation / vec2(iResourceSize);
    return fClampedUv;
}

vec2 MotionVectorScale()
{
    return cbFSR3Upscaler.fMotionVectorScale;
}

vec2 LoadInputMotionVector(ivec2 iPxDilatedMotionVectorPos)
{
    vec2 fSrcMotionVector = texelFetch(SPIRV_Cross_Combinedr_input_motion_vectorsSPIRV_Cross_DummySampler, iPxDilatedMotionVectorPos, 0).xy;
    vec2 fUvMotionVector = fSrcMotionVector * MotionVectorScale();
    return fUvMotionVector;
}

vec2 GetMotionVector(ivec2 iPxHrPos, vec2 fHrUv)
{
    ivec2 param = iPxHrPos;
    vec2 fDilatedMotionVector = LoadInputMotionVector(param);
    return fDilatedMotionVector;
}

float Get4KVelocity(vec2 fMotionVector)
{
    return length(fMotionVector * vec2(3840.0, 2160.0));
}

bool IsUvInside(vec2 fUv)
{
    bool _729 = fUv.x >= 0.0;
    bool _735;
    if (_729)
    {
        _735 = fUv.x <= 1.0;
    }
    else
    {
        _735 = _729;
    }
    bool _747;
    if (_735)
    {
        bool _740 = fUv.y >= 0.0;
        bool _746;
        if (_740)
        {
            _746 = fUv.y <= 1.0;
        }
        else
        {
            _746 = _740;
        }
        _747 = _746;
    }
    else
    {
        _747 = _735;
    }
    return _747;
}

void ComputeReprojectedUVs(inout AccumulationPassCommonParams params)
{
    params.fReprojectedHrUv = params.fHrUv + params.fMotionVector;
    vec2 param = params.fReprojectedHrUv;
    params.bIsExistingSample = IsUvInside(param);
}

float SampleLumaInstability(vec2 uv)
{
    return textureLod(SPIRV_Cross_Combinedr_luma_instabilitys_LinearClamp, uv, 0.0).x;
}

ivec2 GetFarthestDepthMip1ResourceDimensions()
{
    return textureSize(SPIRV_Cross_Combinedr_farthest_depth_mip1SPIRV_Cross_DummySampler, 0);
}

float SampleFarthestDepthMip1(vec2 fUV)
{
    return textureLod(SPIRV_Cross_Combinedr_farthest_depth_mip1s_LinearClamp, fUV, 0.0).x;
}

float FrameIndex()
{
    return cbFSR3Upscaler.fFrameIndex;
}

vec4 SampleDilatedReactiveMasks(vec2 fUV)
{
    return textureLod(SPIRV_Cross_Combinedr_dilated_reactive_maskss_LinearClamp, fUV, 0.0);
}

float ffxSaturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

void InitPassData(ivec2 iPxHrPos, inout AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    params.iPxHrPos = iPxHrPos;
    vec2 fHrUv = (vec2(iPxHrPos) + vec2(0.5)) / vec2(UpscaleSize());
    params.fHrUv = fHrUv;
    params.fLrUvJittered = fHrUv + (Jitter() / vec2(RenderSize()));
    vec2 param = params.fLrUvJittered;
    ivec2 param_1 = RenderSize();
    ivec2 param_2 = MaxRenderSize();
    params.fLrUv_HwSampler = ClampUv(param, param_1, param_2);
    ivec2 param_3 = iPxHrPos;
    vec2 param_4 = fHrUv;
    params.fMotionVector = GetMotionVector(param_3, param_4);
    vec2 param_5 = params.fMotionVector;
    params.f4KVelocity = Get4KVelocity(param_5);
    AccumulationPassCommonParams param_6 = params;
    ComputeReprojectedUVs(param_6);
    params = param_6;
    vec2 param_7 = fHrUv;
    ivec2 param_8 = RenderSize();
    ivec2 param_9 = MaxRenderSize();
    vec2 fLumaInstabilityUv_HW = ClampUv(param_7, param_8, param_9);
    vec2 param_10 = fLumaInstabilityUv_HW;
    params.fLumaInstabilityFactor = SampleLumaInstability(param_10);
    vec2 param_11 = params.fLrUvJittered;
    ivec2 param_12 = RenderSize() / ivec2(2);
    ivec2 param_13 = GetFarthestDepthMip1ResourceDimensions();
    vec2 fFarthestDepthUv = ClampUv(param_11, param_12, param_13);
    vec2 param_14 = fFarthestDepthUv;
    params.fFarthestDepthInMeters = SampleFarthestDepthMip1(param_14);
    bool _2091 = params.bIsExistingSample == false;
    bool _2097;
    if (!_2091)
    {
        _2097 = 0.0 == FrameIndex();
    }
    else
    {
        _2097 = _2091;
    }
    params.bIsNewSample = _2097;
    vec2 param_15 = params.fLrUv_HwSampler;
    vec4 fDilatedReactiveMasks = SampleDilatedReactiveMasks(param_15);
    float param_16 = fDilatedReactiveMasks.x;
    params.fReactiveMask = ffxSaturate(param_16);
    float param_17 = fDilatedReactiveMasks.y;
    params.fDisocclusion = ffxSaturate(param_17);
    float param_18 = fDilatedReactiveMasks.z;
    params.fShadingChange = ffxSaturate(param_18);
    float param_19 = fDilatedReactiveMasks.w;
    params.fAccumulation = ffxSaturate(param_19);
    params.fAccumulation *= float(round(params.fAccumulation * 100.0) > 1.0);
    data.fUpsampledColor = vec3(0.0);
    data.fHistoryColor = vec3(0.0);
    data.fHistoryWeight = 1.0;
    data.fUpsampledWeight = 0.0;
    data.fLock = 0.0;
    data.fLockContributionThisFrame = 0.0;
}

ivec2 PreviousFrameUpscaleSize()
{
    return cbFSR3Upscaler.iPreviousFrameUpscaleSize;
}

vec2 ffxFract(vec2 x)
{
    return fract(x);
}

float ffxMin(float x, float y)
{
    return min(x, y);
}

float ffxMax(float x, float y)
{
    return max(x, y);
}

int ffxMin(int x, int y)
{
    return min(x, y);
}

int ffxMax(int x, int y)
{
    return max(x, y);
}

ivec2 ClampCoord(ivec2 iPxSample, ivec2 iPxOffset, ivec2 iTextureSize)
{
    ivec2 result = iPxSample + iPxOffset;
    int param = result.x;
    int param_1 = iTextureSize.x - 2;
    int param_2 = 1;
    int param_3 = ffxMin(param, param_1);
    result.x = ffxMax(param_2, param_3);
    int param_4 = result.y;
    int param_5 = iTextureSize.y - 2;
    int param_6 = 1;
    int param_7 = ffxMin(param_4, param_5);
    result.y = ffxMax(param_6, param_7);
    return result;
}

vec4 LoadHistory(ivec2 iPxHistory)
{
    return texelFetch(SPIRV_Cross_Combinedr_internal_upscaled_colorSPIRV_Cross_DummySampler, iPxHistory, 0);
}

vec4 WrapHistory(ivec2 iPxSample)
{
    ivec2 param = iPxSample;
    return LoadHistory(param);
}

FetchedBicubicSamples FetchHistorySamples(ivec2 iPxSample, ivec2 iTextureSize)
{
    ivec2 param = iPxSample;
    ivec2 param_1 = ivec2(-1);
    ivec2 param_2 = iTextureSize;
    ivec2 param_3 = ClampCoord(param, param_1, param_2);
    FetchedBicubicSamples Samples;
    Samples.fColor00 = WrapHistory(param_3);
    ivec2 param_4 = iPxSample;
    ivec2 param_5 = ivec2(0, -1);
    ivec2 param_6 = iTextureSize;
    ivec2 param_7 = ClampCoord(param_4, param_5, param_6);
    Samples.fColor10 = WrapHistory(param_7);
    ivec2 param_8 = iPxSample;
    ivec2 param_9 = ivec2(1, -1);
    ivec2 param_10 = iTextureSize;
    ivec2 param_11 = ClampCoord(param_8, param_9, param_10);
    Samples.fColor20 = WrapHistory(param_11);
    ivec2 param_12 = iPxSample;
    ivec2 param_13 = ivec2(2, -1);
    ivec2 param_14 = iTextureSize;
    ivec2 param_15 = ClampCoord(param_12, param_13, param_14);
    Samples.fColor30 = WrapHistory(param_15);
    ivec2 param_16 = iPxSample;
    ivec2 param_17 = ivec2(-1, 0);
    ivec2 param_18 = iTextureSize;
    ivec2 param_19 = ClampCoord(param_16, param_17, param_18);
    Samples.fColor01 = WrapHistory(param_19);
    ivec2 param_20 = iPxSample;
    ivec2 param_21 = ivec2(0);
    ivec2 param_22 = iTextureSize;
    ivec2 param_23 = ClampCoord(param_20, param_21, param_22);
    Samples.fColor11 = WrapHistory(param_23);
    ivec2 param_24 = iPxSample;
    ivec2 param_25 = ivec2(1, 0);
    ivec2 param_26 = iTextureSize;
    ivec2 param_27 = ClampCoord(param_24, param_25, param_26);
    Samples.fColor21 = WrapHistory(param_27);
    ivec2 param_28 = iPxSample;
    ivec2 param_29 = ivec2(2, 0);
    ivec2 param_30 = iTextureSize;
    ivec2 param_31 = ClampCoord(param_28, param_29, param_30);
    Samples.fColor31 = WrapHistory(param_31);
    ivec2 param_32 = iPxSample;
    ivec2 param_33 = ivec2(-1, 1);
    ivec2 param_34 = iTextureSize;
    ivec2 param_35 = ClampCoord(param_32, param_33, param_34);
    Samples.fColor02 = WrapHistory(param_35);
    ivec2 param_36 = iPxSample;
    ivec2 param_37 = ivec2(0, 1);
    ivec2 param_38 = iTextureSize;
    ivec2 param_39 = ClampCoord(param_36, param_37, param_38);
    Samples.fColor12 = WrapHistory(param_39);
    ivec2 param_40 = iPxSample;
    ivec2 param_41 = ivec2(1);
    ivec2 param_42 = iTextureSize;
    ivec2 param_43 = ClampCoord(param_40, param_41, param_42);
    Samples.fColor22 = WrapHistory(param_43);
    ivec2 param_44 = iPxSample;
    ivec2 param_45 = ivec2(2, 1);
    ivec2 param_46 = iTextureSize;
    ivec2 param_47 = ClampCoord(param_44, param_45, param_46);
    Samples.fColor32 = WrapHistory(param_47);
    ivec2 param_48 = iPxSample;
    ivec2 param_49 = ivec2(-1, 2);
    ivec2 param_50 = iTextureSize;
    ivec2 param_51 = ClampCoord(param_48, param_49, param_50);
    Samples.fColor03 = WrapHistory(param_51);
    ivec2 param_52 = iPxSample;
    ivec2 param_53 = ivec2(0, 2);
    ivec2 param_54 = iTextureSize;
    ivec2 param_55 = ClampCoord(param_52, param_53, param_54);
    Samples.fColor13 = WrapHistory(param_55);
    ivec2 param_56 = iPxSample;
    ivec2 param_57 = ivec2(1, 2);
    ivec2 param_58 = iTextureSize;
    ivec2 param_59 = ClampCoord(param_56, param_57, param_58);
    Samples.fColor23 = WrapHistory(param_59);
    ivec2 param_60 = iPxSample;
    ivec2 param_61 = ivec2(2);
    ivec2 param_62 = iTextureSize;
    ivec2 param_63 = ClampCoord(param_60, param_61, param_62);
    Samples.fColor33 = WrapHistory(param_63);
    return Samples;
}

float Lanczos2NoClamp(float x)
{
    float _820;
    if (abs(x) < 6.099999882280826568603515625e-05)
    {
        _820 = 1.0;
    }
    else
    {
        _820 = (sin(3.1415927410125732421875 * x) / (3.1415927410125732421875 * x)) * (sin(1.57079637050628662109375 * x) / (1.57079637050628662109375 * x));
    }
    return _820;
}

float Lanczos2(inout float x)
{
    float param = abs(x);
    float param_1 = 2.0;
    x = ffxMin(param, param_1);
    float param_2 = x;
    return Lanczos2NoClamp(param_2);
}

vec4 Lanczos2(vec4 fColor0, vec4 fColor1, vec4 fColor2, vec4 fColor3, float t)
{
    float param = (-1.0) - t;
    float _890 = Lanczos2(param);
    float fWeight0 = _890;
    float param_1 = (-0.0) - t;
    float _896 = Lanczos2(param_1);
    float fWeight1 = _896;
    float param_2 = 1.0 - t;
    float _901 = Lanczos2(param_2);
    float fWeight2 = _901;
    float param_3 = 2.0 - t;
    float _906 = Lanczos2(param_3);
    float fWeight3 = _906;
    return ((((fColor0 * fWeight0) + (fColor1 * fWeight1)) + (fColor2 * fWeight2)) + (fColor3 * fWeight3)) / vec4(((fWeight0 + fWeight1) + fWeight2) + fWeight3);
}

vec4 ffxMin(vec4 x, vec4 y)
{
    return min(x, y);
}

vec4 ffxMax(vec4 x, vec4 y)
{
    return max(x, y);
}

vec4 Lanczos2(FetchedBicubicSamples Samples, vec2 fPxFrac)
{
    vec4 param = Samples.fColor00;
    vec4 param_1 = Samples.fColor10;
    vec4 param_2 = Samples.fColor20;
    vec4 param_3 = Samples.fColor30;
    float param_4 = fPxFrac.x;
    vec4 fColorX0 = Lanczos2(param, param_1, param_2, param_3, param_4);
    vec4 param_5 = Samples.fColor01;
    vec4 param_6 = Samples.fColor11;
    vec4 param_7 = Samples.fColor21;
    vec4 param_8 = Samples.fColor31;
    float param_9 = fPxFrac.x;
    vec4 fColorX1 = Lanczos2(param_5, param_6, param_7, param_8, param_9);
    vec4 param_10 = Samples.fColor02;
    vec4 param_11 = Samples.fColor12;
    vec4 param_12 = Samples.fColor22;
    vec4 param_13 = Samples.fColor32;
    float param_14 = fPxFrac.x;
    vec4 fColorX2 = Lanczos2(param_10, param_11, param_12, param_13, param_14);
    vec4 param_15 = Samples.fColor03;
    vec4 param_16 = Samples.fColor13;
    vec4 param_17 = Samples.fColor23;
    vec4 param_18 = Samples.fColor33;
    float param_19 = fPxFrac.x;
    vec4 fColorX3 = Lanczos2(param_15, param_16, param_17, param_18, param_19);
    vec4 param_20 = fColorX0;
    vec4 param_21 = fColorX1;
    vec4 param_22 = fColorX2;
    vec4 param_23 = fColorX3;
    float param_24 = fPxFrac.y;
    vec4 fColorXY = Lanczos2(param_20, param_21, param_22, param_23, param_24);
    vec4 fDeringingSamples[4] = vec4[](Samples.fColor11, Samples.fColor21, Samples.fColor12, Samples.fColor22);
    vec4 fDeringingMin = fDeringingSamples[0];
    vec4 fDeringingMax = fDeringingSamples[0];
    for (int iSampleIndex = 1; iSampleIndex < 4; iSampleIndex++)
    {
        vec4 param_25 = fDeringingMin;
        vec4 param_26 = fDeringingSamples[iSampleIndex];
        fDeringingMin = ffxMin(param_25, param_26);
        vec4 param_27 = fDeringingMax;
        vec4 param_28 = fDeringingSamples[iSampleIndex];
        fDeringingMax = ffxMax(param_27, param_28);
    }
    fColorXY = clamp(fColorXY, fDeringingMin, fDeringingMax);
    return fColorXY;
}

vec4 HistorySample(vec2 fUvSample, ivec2 iTextureSize)
{
    vec2 fPxSample = (fUvSample * vec2(iTextureSize)) - vec2(0.5);
    vec2 param = fPxSample;
    vec2 fPxFrac = ffxFract(param);
    float param_1 = float(iTextureSize.x - 1);
    float param_2 = fPxSample.x;
    float param_3 = 0.0;
    float param_4 = ffxMin(param_1, param_2);
    fPxSample.x = ffxMax(param_3, param_4);
    float param_5 = float(iTextureSize.y - 1);
    float param_6 = fPxSample.y;
    float param_7 = 0.0;
    float param_8 = ffxMin(param_5, param_6);
    fPxSample.y = ffxMax(param_7, param_8);
    ivec2 iPxSample = ivec2(floor(fPxSample));
    ivec2 param_9 = iPxSample;
    ivec2 param_10 = iTextureSize;
    FetchedBicubicSamples param_11 = FetchHistorySamples(param_9, param_10);
    vec2 param_12 = fPxFrac;
    vec4 fColorXY = Lanczos2(param_11, param_12);
    return fColorXY;
}

float DeltaPreExposure()
{
    return cbFSR3Upscaler.fDeltaPreExposure;
}

float Exposure()
{
    float exposure = texelFetch(SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler, ivec2(0), 0).x;
    if (exposure == 0.0)
    {
        exposure = 1.0;
    }
    return exposure;
}

vec3 RGBToYCoCg(vec3 fRgb)
{
    vec3 fYCoCg = vec3(((0.25 * fRgb.x) + (0.5 * fRgb.y)) + (0.25 * fRgb.z), (0.5 * fRgb.x) - (0.5 * fRgb.z), (((-0.25) * fRgb.x) + (0.5 * fRgb.y)) - (0.25 * fRgb.z));
    return fYCoCg;
}

void ReprojectHistoryColor(AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    vec2 param = params.fReprojectedHrUv;
    ivec2 param_1 = PreviousFrameUpscaleSize();
    vec4 fReprojectedHistory = HistorySample(param, param_1);
    data.fHistoryColor = fReprojectedHistory.xyz;
    data.fHistoryColor *= DeltaPreExposure();
    data.fHistoryColor *= Exposure();
    vec3 param_2 = data.fHistoryColor;
    data.fHistoryColor = RGBToYCoCg(param_2);
    data.fLock = fReprojectedHistory.w;
}

float LoadRwNewLocks(ivec2 iPxPos)
{
    return imageLoad(r_rw_new_locks, iPxPos).x;
}

float JitterSequenceLength()
{
    return cbFSR3Upscaler.fJitterSequenceLength;
}

void UpdateLockStatus(AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    data.fLock *= float(params.bIsNewSample == false);
    float param = params.fShadingChange;
    float param_1 = params.fReactiveMask;
    float param_2 = params.fDisocclusion;
    float param_3 = ffxSaturate(param);
    float param_4 = ffxMax(param_1, param_2);
    float fLifetimeDecreaseFactor = ffxMax(param_3, param_4);
    float param_5 = 0.0;
    float param_6 = data.fLock - (fLifetimeDecreaseFactor * 2.0);
    data.fLock = ffxMax(param_5, param_6);
    float param_7 = data.fLock - 1.0;
    float param_8 = ffxSaturate(param_7) * 1.0;
    data.fLockContributionThisFrame = ffxSaturate(param_8);
    ivec2 param_9 = params.iPxHrPos;
    float param_10 = params.fShadingChange * 0.0;
    float param_11 = params.fReactiveMask;
    float fNewLockIntensity = LoadRwNewLocks(param_9) * (1.0 - ffxMax(param_10, param_11));
    float param_12 = data.fLock + fNewLockIntensity;
    float param_13 = 2.0;
    float param_14 = 0.0;
    float param_15 = ffxMin(param_12, param_13);
    data.fLock = ffxMax(param_14, param_15);
    float fLifetimeDecrease = (0.100000001490116119384765625 / JitterSequenceLength()) * (1.0 - fLifetimeDecreaseFactor);
    float param_16 = 0.0;
    float param_17 = data.fLock - fLifetimeDecrease;
    data.fLock = ffxMax(param_16, param_17);
    vec2 fEstimatedUvNextFrame = params.fHrUv - params.fMotionVector;
    vec2 param_18 = fEstimatedUvNextFrame;
    data.fLock *= float(IsUvInside(param_18) == true);
}

float VelocityFactor()
{
    return cbFSR3Upscaler.fVelocityFactor;
}

float ffxLerp(float x, float y, float t)
{
    return mix(x, y, t);
}

void ComputeBaseAccumulationWeight(AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    float fBaseAccumulation = params.fAccumulation;
    float param = 0.0;
    float param_1 = (params.f4KVelocity * VelocityFactor()) / 0.5;
    float param_2 = ffxMax(param, param_1);
    float param_3 = fBaseAccumulation;
    float param_4 = 0.1500000059604644775390625;
    float param_5 = ffxSaturate(param_2);
    float param_6 = fBaseAccumulation;
    float param_7 = ffxLerp(param_3, param_4, param_5);
    fBaseAccumulation = ffxMin(param_6, param_7);
    data.fHistoryWeight = fBaseAccumulation;
}

vec2 DownscaleFactor()
{
    return cbFSR3Upscaler.fDownscaleFactor;
}

ivec2 ClampLoad(ivec2 iPxSample, ivec2 iPxOffset, ivec2 iTextureSize)
{
    ivec2 result = iPxSample + iPxOffset;
    int param = result.x;
    int param_1 = iTextureSize.x - 1;
    int param_2 = 0;
    int param_3 = ffxMin(param, param_1);
    result.x = ffxMax(param_2, param_3);
    int param_4 = result.y;
    int param_5 = iTextureSize.y - 1;
    int param_6 = 0;
    int param_7 = ffxMin(param_4, param_5);
    result.y = ffxMax(param_6, param_7);
    return result;
}

vec3 LoadInputColor(ivec2 iPxPos)
{
    return texelFetch(SPIRV_Cross_Combinedr_input_color_jitteredSPIRV_Cross_DummySampler, iPxPos, 0).xyz;
}

vec3 ffxMax(vec3 x, vec3 y)
{
    return max(x, y);
}

vec3 LoadPreparedColor(ivec2 iSamplePos)
{
    ivec2 param = iSamplePos;
    vec3 param_1 = vec3(0.0);
    vec3 param_2 = LoadInputColor(param);
    vec3 fRgb = ffxMax(param_1, param_2) * Exposure();
    vec3 param_3 = fRgb;
    vec3 fPreparedYCoCg = RGBToYCoCg(param_3);
    return fPreparedYCoCg;
}

float ComputeMaxKernelWeight(AccumulationPassCommonParams params, AccumulationPassData data)
{
    float fKernelSizeBias = 1.0 + ((vec2(1.0) / DownscaleFactor()) - vec2(1.0)).x;
    float param = 1.9900000095367431640625;
    float param_1 = fKernelSizeBias;
    return ffxMin(param, param_1);
}

bool IsOnScreen(ivec2 pos, ivec2 size)
{
    return all(lessThan(uvec2(pos), uvec2(size)));
}

float Lanczos2ApproxSqNoClamp(float x2)
{
    float a = (0.4000000059604644775390625 * x2) - 1.0;
    float b = (0.25 * x2) - 1.0;
    return (((1.5625 * a) * a) - 0.5625) * (b * b);
}

float Lanczos2ApproxSq(inout float x2)
{
    float param = x2;
    float param_1 = 4.0;
    x2 = ffxMin(param, param_1);
    float param_2 = x2;
    return Lanczos2ApproxSqNoClamp(param_2);
}

float GetUpsampleLanczosWeight(vec2 fSrcSampleOffset, float fKernelWeight)
{
    vec2 fSrcSampleOffsetBiased = fSrcSampleOffset * vec2(fKernelWeight);
    float param = dot(fSrcSampleOffsetBiased, fSrcSampleOffsetBiased);
    float _1117 = Lanczos2ApproxSq(param);
    float fSampleWeight = _1117;
    return fSampleWeight;
}

void RectificationBoxAddInitialSample(inout RectificationBox rectificationBox, vec3 colorSample, float fSampleWeight)
{
    rectificationBox.aabbMin = colorSample;
    rectificationBox.aabbMax = colorSample;
    vec3 weightedSample = colorSample * fSampleWeight;
    rectificationBox.boxCenter = weightedSample;
    rectificationBox.boxVec = colorSample * weightedSample;
    rectificationBox.fBoxCenterWeight = fSampleWeight;
}

vec3 ffxMin(vec3 x, vec3 y)
{
    return min(x, y);
}

void RectificationBoxAddSample(bool bInitialSample, inout RectificationBox rectificationBox, vec3 colorSample, float fSampleWeight)
{
    if (bInitialSample)
    {
        RectificationBox param = rectificationBox;
        RectificationBoxAddInitialSample(param, colorSample, fSampleWeight);
        rectificationBox = param;
    }
    else
    {
        vec3 param_1 = rectificationBox.aabbMin;
        vec3 param_2 = colorSample;
        rectificationBox.aabbMin = ffxMin(param_1, param_2);
        vec3 param_3 = rectificationBox.aabbMax;
        vec3 param_4 = colorSample;
        rectificationBox.aabbMax = ffxMax(param_3, param_4);
        vec3 weightedSample = colorSample * fSampleWeight;
        rectificationBox.boxCenter += weightedSample;
        rectificationBox.boxVec += (colorSample * weightedSample);
        rectificationBox.fBoxCenterWeight += fSampleWeight;
    }
}

void RectificationBoxComputeVarianceBoxData(inout RectificationBox rectificationBox)
{
    float _627;
    if (abs(rectificationBox.fBoxCenterWeight) > 1.1754943508222875079687365372222e-38)
    {
        _627 = rectificationBox.fBoxCenterWeight;
    }
    else
    {
        _627 = 1.0;
    }
    rectificationBox.fBoxCenterWeight = _627;
    rectificationBox.boxCenter /= vec3(rectificationBox.fBoxCenterWeight);
    rectificationBox.boxVec /= vec3(rectificationBox.fBoxCenterWeight);
    vec3 stdDev = sqrt(abs(rectificationBox.boxVec - (rectificationBox.boxCenter * rectificationBox.boxCenter)));
    rectificationBox.boxVec = stdDev;
}

void Deringing(RectificationBox clippingBox, inout vec3 fColor)
{
    fColor = clamp(fColor, clippingBox.aabbMin, clippingBox.aabbMax);
}

void ComputeUpsampledColorAndWeight(AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    vec2 fDstOutputPos = vec2(params.iPxHrPos) + vec2(0.5);
    vec2 fSrcOutputPos = fDstOutputPos * DownscaleFactor();
    ivec2 iSrcInputPos = ivec2(floor(fSrcOutputPos));
    vec2 fSrcUnjitteredPos = (vec2(iSrcInputPos) + vec2(0.5)) - Jitter();
    vec2 fBaseSampleOffset = fSrcUnjitteredPos - fSrcOutputPos;
    ivec2 offsetTL;
    offsetTL.x = (fSrcUnjitteredPos.x > fSrcOutputPos.x) ? (-2) : (-1);
    offsetTL.y = (fSrcUnjitteredPos.y > fSrcOutputPos.y) ? (-2) : (-1);
    bool bFlipRow = fSrcUnjitteredPos.y > fSrcOutputPos.y;
    bool bFlipCol = fSrcUnjitteredPos.x > fSrcOutputPos.x;
    vec2 fOffsetTL = vec2(offsetTL);
    bool bIsInitialSample = params.fAccumulation == 0.0;
    int iSampleIndex = 0;
    int _1229;
    int _1238;
    vec3 fSamples[9];
    for (int row = 0; row < 3; row++)
    {
        for (int col = 0; col < 3; col++)
        {
            if (bFlipCol)
            {
                _1229 = 3 - col;
            }
            else
            {
                _1229 = col;
            }
            if (bFlipRow)
            {
                _1238 = 3 - row;
            }
            else
            {
                _1238 = row;
            }
            ivec2 iSampleColRow = ivec2(_1229, _1238);
            ivec2 iSrcSamplePos = (iSrcInputPos + offsetTL) + iSampleColRow;
            ivec2 param = iSrcSamplePos;
            ivec2 param_1 = ivec2(0);
            ivec2 param_2 = RenderSize();
            ivec2 iSampleCoord = ClampLoad(param, param_1, param_2);
            ivec2 param_3 = iSampleCoord;
            fSamples[iSampleIndex] = LoadPreparedColor(param_3);
            iSampleIndex++;
        }
    }
    AccumulationPassData param_4 = data;
    data = param_4;
    float fKernelBiasMax = ComputeMaxKernelWeight(params, param_4);
    float param_5 = 1.0;
    float param_6 = (1.0 + fKernelBiasMax) * 0.300000011920928955078125;
    float fKernelBiasMin = ffxMax(param_5, param_6);
    float param_7 = data.fHistoryWeight * 5.0;
    float param_8 = 1.0 - params.fShadingChange;
    float param_9 = ffxSaturate(param_7);
    float param_10 = 1.0 - (params.fDisocclusion * 0.5);
    float param_11 = ffxMin(param_8, param_9);
    float fKernelBiasWeight = ffxMin(param_10, param_11);
    float param_12 = fKernelBiasMin;
    float param_13 = fKernelBiasMax;
    float param_14 = fKernelBiasWeight;
    float fKernelBias = ffxLerp(param_12, param_13, param_14);
    iSampleIndex = 0;
    int _1332;
    int _1341;
    for (int row_1 = 0; row_1 < 3; row_1++)
    {
        for (int col_1 = 0; col_1 < 3; col_1++)
        {
            if (bFlipCol)
            {
                _1332 = 3 - col_1;
            }
            else
            {
                _1332 = col_1;
            }
            if (bFlipRow)
            {
                _1341 = 3 - row_1;
            }
            else
            {
                _1341 = row_1;
            }
            ivec2 sampleColRow = ivec2(_1332, _1341);
            vec2 fOffset = fOffsetTL + vec2(sampleColRow);
            vec2 fSrcSampleOffset = fBaseSampleOffset + fOffset;
            ivec2 iSrcSamplePos_1 = (iSrcInputPos + offsetTL) + sampleColRow;
            ivec2 param_15 = iSrcSamplePos_1;
            ivec2 param_16 = RenderSize();
            float fOnScreenFactor = float(IsOnScreen(param_15, param_16));
            if (!bIsInitialSample)
            {
                vec2 param_17 = fSrcSampleOffset;
                float param_18 = fKernelBias;
                float fSampleWeight = fOnScreenFactor * GetUpsampleLanczosWeight(param_17, param_18);
                data.fUpsampledColor += (fSamples[iSampleIndex] * fSampleWeight);
                data.fUpsampledWeight += fSampleWeight;
            }
            float fSrcSampleOffsetSq = dot(fSrcSampleOffset, fSrcSampleOffset);
            float fBoxSampleWeight = exp((-2.2999999523162841796875) * fSrcSampleOffsetSq) * fOnScreenFactor;
            bool bInitialSample = (row_1 == 0) && (col_1 == 0);
            bool param_19 = bInitialSample;
            RectificationBox param_20 = data.clippingBox;
            RectificationBoxAddSample(param_19, param_20, fSamples[iSampleIndex], fBoxSampleWeight);
            data.clippingBox = param_20;
            iSampleIndex++;
        }
    }
    RectificationBox param_21 = data.clippingBox;
    RectificationBoxComputeVarianceBoxData(param_21);
    data.clippingBox = param_21;
    data.fUpsampledWeight *= float(data.fUpsampledWeight > 6.099999882280826568603515625e-05);
    if (data.fUpsampledWeight > 6.099999882280826568603515625e-05)
    {
        data.fUpsampledColor /= vec3(data.fUpsampledWeight);
        data.fUpsampledWeight *= 0.04625000059604644775390625;
        RectificationBox param_22 = data.clippingBox;
        vec3 param_23 = data.fUpsampledColor;
        Deringing(param_22, param_23);
        data.fUpsampledColor = param_23;
    }
    if (bIsInitialSample)
    {
        data.fUpsampledColor = data.clippingBox.boxCenter;
        data.fUpsampledWeight = 1.0;
        data.fHistoryWeight = 0.0;
    }
}

float ffxPow(float x, float y)
{
    return pow(x, y);
}

vec3 ffxLerp(vec3 x, vec3 y, float t)
{
    return mix(x, y, vec3(t));
}

void RectifyHistory(AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    float param = params.f4KVelocity / 20.0;
    float f4kVelocityFactor = ffxSaturate(param);
    float param_1 = 0.75 - (params.fFarthestDepthInMeters / 20.0);
    float fDistanceFactor = ffxSaturate(param_1);
    float fAccumulationFactor = 1.0 - params.fAccumulation;
    float param_2 = params.fReactiveMask;
    float param_3 = 0.5;
    float fReactiveFactor = ffxPow(param_2, param_3);
    float fShadingChangeFactor = params.fShadingChange;
    float param_4 = fReactiveFactor;
    float param_5 = fShadingChangeFactor;
    float param_6 = fAccumulationFactor;
    float param_7 = ffxMax(param_4, param_5);
    float param_8 = fDistanceFactor;
    float param_9 = ffxMax(param_6, param_7);
    float param_10 = f4kVelocityFactor;
    float param_11 = ffxMax(param_8, param_9);
    float fBoxScaleT = ffxMax(param_10, param_11);
    float param_12 = 3.0;
    float param_13 = 1.0;
    float param_14 = fBoxScaleT;
    float fBoxScale = ffxLerp(param_12, param_13, param_14);
    vec3 fScaledBoxVec = (data.clippingBox.boxVec * vec3(1.7000000476837158203125, 1.0, 1.0)) * fBoxScale;
    vec3 param_15 = fScaledBoxVec;
    vec3 param_16 = vec3(1.19299997436428384389728307724e-07);
    vec3 fClampedScaledBoxVec = ffxMax(param_15, param_16);
    vec3 fTransformedHistoryColor = (data.fHistoryColor - data.clippingBox.boxCenter) / fClampedScaledBoxVec;
    if (length(fTransformedHistoryColor) > 1.0)
    {
        vec3 fClampedHistoryColor = normalize(fTransformedHistoryColor);
        vec3 fFinalClampedHistoryColor = (fClampedHistoryColor * fScaledBoxVec) + data.clippingBox.boxCenter;
        float param_17 = params.fLumaInstabilityFactor;
        float param_18 = data.fLockContributionThisFrame;
        float fHistoryContribution = (ffxMax(param_17, param_18) * params.fAccumulation) * (1.0 - params.fDisocclusion);
        float param_19 = fHistoryContribution;
        vec3 param_20 = fFinalClampedHistoryColor;
        vec3 param_21 = data.fHistoryColor;
        float param_22 = ffxSaturate(param_19);
        data.fHistoryColor = ffxLerp(param_20, param_21, param_22);
    }
}

vec3 YCoCgToRGB(vec3 fYCoCg)
{
    vec3 fRgb = vec3((fYCoCg.x + fYCoCg.y) - fYCoCg.z, fYCoCg.x + fYCoCg.z, (fYCoCg.x - fYCoCg.y) - fYCoCg.z);
    return fRgb;
}

void Accumulate(AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    data.fHistoryWeight *= float(data.fHistoryWeight > 6.099999882280826568603515625e-05);
    float param = 6.099999882280826568603515625e-05;
    float param_1 = data.fHistoryWeight + data.fUpsampledWeight;
    data.fHistoryWeight = ffxMax(param, param_1);
    float param_2 = data.fUpsampledWeight / data.fHistoryWeight;
    float fAlpha = ffxSaturate(param_2);
    vec3 param_3 = data.fHistoryColor;
    vec3 param_4 = data.fUpsampledColor;
    float param_5 = fAlpha;
    data.fHistoryColor = ffxLerp(param_3, param_4, param_5);
    vec3 param_6 = data.fHistoryColor;
    data.fHistoryColor = YCoCgToRGB(param_6);
}

void StoreInternalColorAndWeight(ivec2 iPxPos, vec4 fColorAndWeight)
{
    imageStore(rw_internal_upscaled_color, iPxPos, fColorAndWeight);
}

void StoreUpscaledOutput(ivec2 iPxPos, vec3 fColor)
{
    imageStore(rw_upscaled_output, iPxPos, vec4(fColor, 1.0));
}

void StoreNewLocks(ivec2 iPxPos, float newLock)
{
    imageStore(rw_new_locks, iPxPos, vec4(newLock, 0.0, 0.0, 0.0));
}

void Accumulate(ivec2 iPxHrPos)
{
    ivec2 param = iPxHrPos;
    AccumulationPassCommonParams params;
    AccumulationPassCommonParams param_1 = params;
    AccumulationPassData data;
    AccumulationPassData param_2 = data;
    InitPassData(param, param_1, param_2);
    params = param_1;
    data = param_2;
    bool _2159;
    if (params.bIsExistingSample)
    {
        _2159 = !params.bIsNewSample;
    }
    else
    {
        _2159 = params.bIsExistingSample;
    }
    if (_2159)
    {
        AccumulationPassData param_3 = data;
        ReprojectHistoryColor(params, param_3);
        data = param_3;
    }
    AccumulationPassCommonParams param_4 = params;
    AccumulationPassData param_5 = data;
    UpdateLockStatus(param_4, param_5);
    data = param_5;
    AccumulationPassData param_6 = data;
    ComputeBaseAccumulationWeight(params, param_6);
    data = param_6;
    AccumulationPassData param_7 = data;
    ComputeUpsampledColorAndWeight(params, param_7);
    data = param_7;
    AccumulationPassData param_8 = data;
    RectifyHistory(params, param_8);
    data = param_8;
    AccumulationPassData param_9 = data;
    Accumulate(params, param_9);
    data = param_9;
    data.fHistoryColor /= vec3(Exposure());
    vec3 param_10 = data.fHistoryColor;
    vec3 param_11 = vec3(0.0);
    data.fHistoryColor = ffxMax(param_10, param_11);
    ivec2 param_12 = iPxHrPos;
    vec4 param_13 = vec4(data.fHistoryColor, data.fLock);
    StoreInternalColorAndWeight(param_12, param_13);
    ivec2 param_14 = iPxHrPos;
    vec3 param_15 = data.fHistoryColor;
    StoreUpscaledOutput(param_14, param_15);
    ivec2 param_16 = iPxHrPos;
    float param_17 = 0.0;
    StoreNewLocks(param_16, param_17);
}

void main()
{
    ivec2 param = ivec2(gl_GlobalInvocationID.xy);
    Accumulate(param);
}

)FSR3";

// accumulate_hdr_essl320.glsl sha256 46c3a815cafccaf03386fa1406e847c2fc0cbbd6df5b061f6d7fa5c3bbb7aec3
inline constexpr const char* kAccumulateHdr = R"FSR3(#version 320 es
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

struct RectificationBox
{
    vec3 boxCenter;
    vec3 boxVec;
    vec3 aabbMin;
    vec3 aabbMax;
    float fBoxCenterWeight;
};

struct FetchedBicubicSamples
{
    vec4 fColor00;
    vec4 fColor10;
    vec4 fColor20;
    vec4 fColor30;
    vec4 fColor01;
    vec4 fColor11;
    vec4 fColor21;
    vec4 fColor31;
    vec4 fColor02;
    vec4 fColor12;
    vec4 fColor22;
    vec4 fColor32;
    vec4 fColor03;
    vec4 fColor13;
    vec4 fColor23;
    vec4 fColor33;
};

struct AccumulationPassCommonParams
{
    ivec2 iPxHrPos;
    vec2 fHrUv;
    vec2 fLrUvJittered;
    vec2 fLrUv_HwSampler;
    vec2 fMotionVector;
    vec2 fReprojectedHrUv;
    float f4KVelocity;
    float fDisocclusion;
    float fReactiveMask;
    float fShadingChange;
    float fAccumulation;
    float fLumaInstabilityFactor;
    float fFarthestDepthInMeters;
    bool bIsExistingSample;
    bool bIsNewSample;
};

struct AccumulationPassData
{
    RectificationBox clippingBox;
    vec3 fUpsampledColor;
    float fUpsampledWeight;
    vec3 fHistoryColor;
    float fHistoryWeight;
    float fLock;
    float fLockContributionThisFrame;
};

layout(binding = 12, std140) uniform cbFSR3UPSCALER_t
{
    ivec2 iRenderSize;
    ivec2 iPreviousFrameRenderSize;
    ivec2 iUpscaleSize;
    ivec2 iPreviousFrameUpscaleSize;
    ivec2 iMaxRenderSize;
    ivec2 iMaxUpscaleSize;
    vec4 fDeviceToViewDepth;
    vec2 fJitter;
    vec2 fPreviousFrameJitter;
    vec2 fMotionVectorScale;
    vec2 fDownscaleFactor;
    vec2 fMotionVectorJitterCancellation;
    float fTanHalfFOV;
    float fJitterSequenceLength;
    float fDeltaTime;
    float fDeltaPreExposure;
    float fViewSpaceToMetersFactor;
    float fFrameIndex;
    float fVelocityFactor;
    float fReactivenessScale;
    float fShadingChangeScale;
    float fAccumulationAddedPerFrame;
    float fMinDisocclusionAccumulation;
} cbFSR3Upscaler;

layout(binding = 9, rgba16f) uniform writeonly highp image2D rw_internal_upscaled_color;
layout(binding = 10, rgba16f) uniform writeonly highp image2D rw_upscaled_output;
layout(binding = 11, rgba8) uniform coherent readonly highp image2D r_rw_new_locks;
layout(binding = 23, rgba8) uniform coherent writeonly highp image2D rw_new_locks;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_motion_vectorsSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_luma_instabilitys_LinearClamp;
uniform highp sampler2D SPIRV_Cross_Combinedr_farthest_depth_mip1SPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_farthest_depth_mip1s_LinearClamp;
uniform highp sampler2D SPIRV_Cross_Combinedr_dilated_reactive_maskss_LinearClamp;
uniform highp sampler2D SPIRV_Cross_Combinedr_internal_upscaled_colorSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_color_jitteredSPIRV_Cross_DummySampler;

ivec2 UpscaleSize()
{
    return cbFSR3Upscaler.iUpscaleSize;
}

vec2 Jitter()
{
    return cbFSR3Upscaler.fJitter;
}

ivec2 RenderSize()
{
    return cbFSR3Upscaler.iRenderSize;
}

ivec2 MaxRenderSize()
{
    return cbFSR3Upscaler.iMaxRenderSize;
}

vec2 ffxMin(vec2 x, vec2 y)
{
    return min(x, y);
}

vec2 ffxMax(vec2 x, vec2 y)
{
    return max(x, y);
}

vec2 ClampUv(vec2 fUv, ivec2 iTextureSize, ivec2 iResourceSize)
{
    vec2 fSampleLocation = fUv * vec2(iTextureSize);
    vec2 param = fSampleLocation;
    vec2 param_1 = vec2(iTextureSize) - vec2(0.5);
    vec2 param_2 = vec2(0.5);
    vec2 param_3 = ffxMin(param, param_1);
    vec2 fClampedLocation = ffxMax(param_2, param_3);
    vec2 fClampedUv = fClampedLocation / vec2(iResourceSize);
    return fClampedUv;
}

vec2 MotionVectorScale()
{
    return cbFSR3Upscaler.fMotionVectorScale;
}

vec2 LoadInputMotionVector(ivec2 iPxDilatedMotionVectorPos)
{
    vec2 fSrcMotionVector = texelFetch(SPIRV_Cross_Combinedr_input_motion_vectorsSPIRV_Cross_DummySampler, iPxDilatedMotionVectorPos, 0).xy;
    vec2 fUvMotionVector = fSrcMotionVector * MotionVectorScale();
    return fUvMotionVector;
}

vec2 GetMotionVector(ivec2 iPxHrPos, vec2 fHrUv)
{
    ivec2 param = iPxHrPos;
    vec2 fDilatedMotionVector = LoadInputMotionVector(param);
    return fDilatedMotionVector;
}

float Get4KVelocity(vec2 fMotionVector)
{
    return length(fMotionVector * vec2(3840.0, 2160.0));
}

bool IsUvInside(vec2 fUv)
{
    bool _729 = fUv.x >= 0.0;
    bool _735;
    if (_729)
    {
        _735 = fUv.x <= 1.0;
    }
    else
    {
        _735 = _729;
    }
    bool _747;
    if (_735)
    {
        bool _740 = fUv.y >= 0.0;
        bool _746;
        if (_740)
        {
            _746 = fUv.y <= 1.0;
        }
        else
        {
            _746 = _740;
        }
        _747 = _746;
    }
    else
    {
        _747 = _735;
    }
    return _747;
}

void ComputeReprojectedUVs(inout AccumulationPassCommonParams params)
{
    params.fReprojectedHrUv = params.fHrUv + params.fMotionVector;
    vec2 param = params.fReprojectedHrUv;
    params.bIsExistingSample = IsUvInside(param);
}

float SampleLumaInstability(vec2 uv)
{
    return textureLod(SPIRV_Cross_Combinedr_luma_instabilitys_LinearClamp, uv, 0.0).x;
}

ivec2 GetFarthestDepthMip1ResourceDimensions()
{
    return textureSize(SPIRV_Cross_Combinedr_farthest_depth_mip1SPIRV_Cross_DummySampler, 0);
}

float SampleFarthestDepthMip1(vec2 fUV)
{
    return textureLod(SPIRV_Cross_Combinedr_farthest_depth_mip1s_LinearClamp, fUV, 0.0).x;
}

float FrameIndex()
{
    return cbFSR3Upscaler.fFrameIndex;
}

vec4 SampleDilatedReactiveMasks(vec2 fUV)
{
    return textureLod(SPIRV_Cross_Combinedr_dilated_reactive_maskss_LinearClamp, fUV, 0.0);
}

float ffxSaturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

void InitPassData(ivec2 iPxHrPos, inout AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    params.iPxHrPos = iPxHrPos;
    vec2 fHrUv = (vec2(iPxHrPos) + vec2(0.5)) / vec2(UpscaleSize());
    params.fHrUv = fHrUv;
    params.fLrUvJittered = fHrUv + (Jitter() / vec2(RenderSize()));
    vec2 param = params.fLrUvJittered;
    ivec2 param_1 = RenderSize();
    ivec2 param_2 = MaxRenderSize();
    params.fLrUv_HwSampler = ClampUv(param, param_1, param_2);
    ivec2 param_3 = iPxHrPos;
    vec2 param_4 = fHrUv;
    params.fMotionVector = GetMotionVector(param_3, param_4);
    vec2 param_5 = params.fMotionVector;
    params.f4KVelocity = Get4KVelocity(param_5);
    AccumulationPassCommonParams param_6 = params;
    ComputeReprojectedUVs(param_6);
    params = param_6;
    vec2 param_7 = fHrUv;
    ivec2 param_8 = RenderSize();
    ivec2 param_9 = MaxRenderSize();
    vec2 fLumaInstabilityUv_HW = ClampUv(param_7, param_8, param_9);
    vec2 param_10 = fLumaInstabilityUv_HW;
    params.fLumaInstabilityFactor = SampleLumaInstability(param_10);
    vec2 param_11 = params.fLrUvJittered;
    ivec2 param_12 = RenderSize() / ivec2(2);
    ivec2 param_13 = GetFarthestDepthMip1ResourceDimensions();
    vec2 fFarthestDepthUv = ClampUv(param_11, param_12, param_13);
    vec2 param_14 = fFarthestDepthUv;
    params.fFarthestDepthInMeters = SampleFarthestDepthMip1(param_14);
    bool _2091 = params.bIsExistingSample == false;
    bool _2097;
    if (!_2091)
    {
        _2097 = 0.0 == FrameIndex();
    }
    else
    {
        _2097 = _2091;
    }
    params.bIsNewSample = _2097;
    vec2 param_15 = params.fLrUv_HwSampler;
    vec4 fDilatedReactiveMasks = SampleDilatedReactiveMasks(param_15);
    float param_16 = fDilatedReactiveMasks.x;
    params.fReactiveMask = ffxSaturate(param_16);
    float param_17 = fDilatedReactiveMasks.y;
    params.fDisocclusion = ffxSaturate(param_17);
    float param_18 = fDilatedReactiveMasks.z;
    params.fShadingChange = ffxSaturate(param_18);
    float param_19 = fDilatedReactiveMasks.w;
    params.fAccumulation = ffxSaturate(param_19);
    params.fAccumulation *= float(round(params.fAccumulation * 100.0) > 1.0);
    data.fUpsampledColor = vec3(0.0);
    data.fHistoryColor = vec3(0.0);
    data.fHistoryWeight = 1.0;
    data.fUpsampledWeight = 0.0;
    data.fLock = 0.0;
    data.fLockContributionThisFrame = 0.0;
}

ivec2 PreviousFrameUpscaleSize()
{
    return cbFSR3Upscaler.iPreviousFrameUpscaleSize;
}

vec2 ffxFract(vec2 x)
{
    return fract(x);
}

float ffxMin(float x, float y)
{
    return min(x, y);
}

float ffxMax(float x, float y)
{
    return max(x, y);
}

int ffxMin(int x, int y)
{
    return min(x, y);
}

int ffxMax(int x, int y)
{
    return max(x, y);
}

ivec2 ClampCoord(ivec2 iPxSample, ivec2 iPxOffset, ivec2 iTextureSize)
{
    ivec2 result = iPxSample + iPxOffset;
    int param = result.x;
    int param_1 = iTextureSize.x - 2;
    int param_2 = 1;
    int param_3 = ffxMin(param, param_1);
    result.x = ffxMax(param_2, param_3);
    int param_4 = result.y;
    int param_5 = iTextureSize.y - 2;
    int param_6 = 1;
    int param_7 = ffxMin(param_4, param_5);
    result.y = ffxMax(param_6, param_7);
    return result;
}

vec4 LoadHistory(ivec2 iPxHistory)
{
    return texelFetch(SPIRV_Cross_Combinedr_internal_upscaled_colorSPIRV_Cross_DummySampler, iPxHistory, 0);
}

vec4 WrapHistory(ivec2 iPxSample)
{
    ivec2 param = iPxSample;
    return LoadHistory(param);
}

FetchedBicubicSamples FetchHistorySamples(ivec2 iPxSample, ivec2 iTextureSize)
{
    ivec2 param = iPxSample;
    ivec2 param_1 = ivec2(-1);
    ivec2 param_2 = iTextureSize;
    ivec2 param_3 = ClampCoord(param, param_1, param_2);
    FetchedBicubicSamples Samples;
    Samples.fColor00 = WrapHistory(param_3);
    ivec2 param_4 = iPxSample;
    ivec2 param_5 = ivec2(0, -1);
    ivec2 param_6 = iTextureSize;
    ivec2 param_7 = ClampCoord(param_4, param_5, param_6);
    Samples.fColor10 = WrapHistory(param_7);
    ivec2 param_8 = iPxSample;
    ivec2 param_9 = ivec2(1, -1);
    ivec2 param_10 = iTextureSize;
    ivec2 param_11 = ClampCoord(param_8, param_9, param_10);
    Samples.fColor20 = WrapHistory(param_11);
    ivec2 param_12 = iPxSample;
    ivec2 param_13 = ivec2(2, -1);
    ivec2 param_14 = iTextureSize;
    ivec2 param_15 = ClampCoord(param_12, param_13, param_14);
    Samples.fColor30 = WrapHistory(param_15);
    ivec2 param_16 = iPxSample;
    ivec2 param_17 = ivec2(-1, 0);
    ivec2 param_18 = iTextureSize;
    ivec2 param_19 = ClampCoord(param_16, param_17, param_18);
    Samples.fColor01 = WrapHistory(param_19);
    ivec2 param_20 = iPxSample;
    ivec2 param_21 = ivec2(0);
    ivec2 param_22 = iTextureSize;
    ivec2 param_23 = ClampCoord(param_20, param_21, param_22);
    Samples.fColor11 = WrapHistory(param_23);
    ivec2 param_24 = iPxSample;
    ivec2 param_25 = ivec2(1, 0);
    ivec2 param_26 = iTextureSize;
    ivec2 param_27 = ClampCoord(param_24, param_25, param_26);
    Samples.fColor21 = WrapHistory(param_27);
    ivec2 param_28 = iPxSample;
    ivec2 param_29 = ivec2(2, 0);
    ivec2 param_30 = iTextureSize;
    ivec2 param_31 = ClampCoord(param_28, param_29, param_30);
    Samples.fColor31 = WrapHistory(param_31);
    ivec2 param_32 = iPxSample;
    ivec2 param_33 = ivec2(-1, 1);
    ivec2 param_34 = iTextureSize;
    ivec2 param_35 = ClampCoord(param_32, param_33, param_34);
    Samples.fColor02 = WrapHistory(param_35);
    ivec2 param_36 = iPxSample;
    ivec2 param_37 = ivec2(0, 1);
    ivec2 param_38 = iTextureSize;
    ivec2 param_39 = ClampCoord(param_36, param_37, param_38);
    Samples.fColor12 = WrapHistory(param_39);
    ivec2 param_40 = iPxSample;
    ivec2 param_41 = ivec2(1);
    ivec2 param_42 = iTextureSize;
    ivec2 param_43 = ClampCoord(param_40, param_41, param_42);
    Samples.fColor22 = WrapHistory(param_43);
    ivec2 param_44 = iPxSample;
    ivec2 param_45 = ivec2(2, 1);
    ivec2 param_46 = iTextureSize;
    ivec2 param_47 = ClampCoord(param_44, param_45, param_46);
    Samples.fColor32 = WrapHistory(param_47);
    ivec2 param_48 = iPxSample;
    ivec2 param_49 = ivec2(-1, 2);
    ivec2 param_50 = iTextureSize;
    ivec2 param_51 = ClampCoord(param_48, param_49, param_50);
    Samples.fColor03 = WrapHistory(param_51);
    ivec2 param_52 = iPxSample;
    ivec2 param_53 = ivec2(0, 2);
    ivec2 param_54 = iTextureSize;
    ivec2 param_55 = ClampCoord(param_52, param_53, param_54);
    Samples.fColor13 = WrapHistory(param_55);
    ivec2 param_56 = iPxSample;
    ivec2 param_57 = ivec2(1, 2);
    ivec2 param_58 = iTextureSize;
    ivec2 param_59 = ClampCoord(param_56, param_57, param_58);
    Samples.fColor23 = WrapHistory(param_59);
    ivec2 param_60 = iPxSample;
    ivec2 param_61 = ivec2(2);
    ivec2 param_62 = iTextureSize;
    ivec2 param_63 = ClampCoord(param_60, param_61, param_62);
    Samples.fColor33 = WrapHistory(param_63);
    return Samples;
}

float Lanczos2NoClamp(float x)
{
    float _820;
    if (abs(x) < 6.099999882280826568603515625e-05)
    {
        _820 = 1.0;
    }
    else
    {
        _820 = (sin(3.1415927410125732421875 * x) / (3.1415927410125732421875 * x)) * (sin(1.57079637050628662109375 * x) / (1.57079637050628662109375 * x));
    }
    return _820;
}

float Lanczos2(inout float x)
{
    float param = abs(x);
    float param_1 = 2.0;
    x = ffxMin(param, param_1);
    float param_2 = x;
    return Lanczos2NoClamp(param_2);
}

vec4 Lanczos2(vec4 fColor0, vec4 fColor1, vec4 fColor2, vec4 fColor3, float t)
{
    float param = (-1.0) - t;
    float _890 = Lanczos2(param);
    float fWeight0 = _890;
    float param_1 = (-0.0) - t;
    float _896 = Lanczos2(param_1);
    float fWeight1 = _896;
    float param_2 = 1.0 - t;
    float _901 = Lanczos2(param_2);
    float fWeight2 = _901;
    float param_3 = 2.0 - t;
    float _906 = Lanczos2(param_3);
    float fWeight3 = _906;
    return ((((fColor0 * fWeight0) + (fColor1 * fWeight1)) + (fColor2 * fWeight2)) + (fColor3 * fWeight3)) / vec4(((fWeight0 + fWeight1) + fWeight2) + fWeight3);
}

vec4 ffxMin(vec4 x, vec4 y)
{
    return min(x, y);
}

vec4 ffxMax(vec4 x, vec4 y)
{
    return max(x, y);
}

vec4 Lanczos2(FetchedBicubicSamples Samples, vec2 fPxFrac)
{
    vec4 param = Samples.fColor00;
    vec4 param_1 = Samples.fColor10;
    vec4 param_2 = Samples.fColor20;
    vec4 param_3 = Samples.fColor30;
    float param_4 = fPxFrac.x;
    vec4 fColorX0 = Lanczos2(param, param_1, param_2, param_3, param_4);
    vec4 param_5 = Samples.fColor01;
    vec4 param_6 = Samples.fColor11;
    vec4 param_7 = Samples.fColor21;
    vec4 param_8 = Samples.fColor31;
    float param_9 = fPxFrac.x;
    vec4 fColorX1 = Lanczos2(param_5, param_6, param_7, param_8, param_9);
    vec4 param_10 = Samples.fColor02;
    vec4 param_11 = Samples.fColor12;
    vec4 param_12 = Samples.fColor22;
    vec4 param_13 = Samples.fColor32;
    float param_14 = fPxFrac.x;
    vec4 fColorX2 = Lanczos2(param_10, param_11, param_12, param_13, param_14);
    vec4 param_15 = Samples.fColor03;
    vec4 param_16 = Samples.fColor13;
    vec4 param_17 = Samples.fColor23;
    vec4 param_18 = Samples.fColor33;
    float param_19 = fPxFrac.x;
    vec4 fColorX3 = Lanczos2(param_15, param_16, param_17, param_18, param_19);
    vec4 param_20 = fColorX0;
    vec4 param_21 = fColorX1;
    vec4 param_22 = fColorX2;
    vec4 param_23 = fColorX3;
    float param_24 = fPxFrac.y;
    vec4 fColorXY = Lanczos2(param_20, param_21, param_22, param_23, param_24);
    vec4 fDeringingSamples[4] = vec4[](Samples.fColor11, Samples.fColor21, Samples.fColor12, Samples.fColor22);
    vec4 fDeringingMin = fDeringingSamples[0];
    vec4 fDeringingMax = fDeringingSamples[0];
    for (int iSampleIndex = 1; iSampleIndex < 4; iSampleIndex++)
    {
        vec4 param_25 = fDeringingMin;
        vec4 param_26 = fDeringingSamples[iSampleIndex];
        fDeringingMin = ffxMin(param_25, param_26);
        vec4 param_27 = fDeringingMax;
        vec4 param_28 = fDeringingSamples[iSampleIndex];
        fDeringingMax = ffxMax(param_27, param_28);
    }
    fColorXY = clamp(fColorXY, fDeringingMin, fDeringingMax);
    return fColorXY;
}

vec4 HistorySample(vec2 fUvSample, ivec2 iTextureSize)
{
    vec2 fPxSample = (fUvSample * vec2(iTextureSize)) - vec2(0.5);
    vec2 param = fPxSample;
    vec2 fPxFrac = ffxFract(param);
    float param_1 = float(iTextureSize.x - 1);
    float param_2 = fPxSample.x;
    float param_3 = 0.0;
    float param_4 = ffxMin(param_1, param_2);
    fPxSample.x = ffxMax(param_3, param_4);
    float param_5 = float(iTextureSize.y - 1);
    float param_6 = fPxSample.y;
    float param_7 = 0.0;
    float param_8 = ffxMin(param_5, param_6);
    fPxSample.y = ffxMax(param_7, param_8);
    ivec2 iPxSample = ivec2(floor(fPxSample));
    ivec2 param_9 = iPxSample;
    ivec2 param_10 = iTextureSize;
    FetchedBicubicSamples param_11 = FetchHistorySamples(param_9, param_10);
    vec2 param_12 = fPxFrac;
    vec4 fColorXY = Lanczos2(param_11, param_12);
    return fColorXY;
}

float DeltaPreExposure()
{
    return cbFSR3Upscaler.fDeltaPreExposure;
}

float Exposure()
{
    float exposure = texelFetch(SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler, ivec2(0), 0).x;
    if (exposure == 0.0)
    {
        exposure = 1.0;
    }
    return exposure;
}

vec3 RGBToYCoCg(vec3 fRgb)
{
    vec3 fYCoCg = vec3(((0.25 * fRgb.x) + (0.5 * fRgb.y)) + (0.25 * fRgb.z), (0.5 * fRgb.x) - (0.5 * fRgb.z), (((-0.25) * fRgb.x) + (0.5 * fRgb.y)) - (0.25 * fRgb.z));
    return fYCoCg;
}

void ReprojectHistoryColor(AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    vec2 param = params.fReprojectedHrUv;
    ivec2 param_1 = PreviousFrameUpscaleSize();
    vec4 fReprojectedHistory = HistorySample(param, param_1);
    data.fHistoryColor = fReprojectedHistory.xyz;
    data.fHistoryColor *= DeltaPreExposure();
    data.fHistoryColor *= Exposure();
    vec3 param_2 = data.fHistoryColor;
    data.fHistoryColor = RGBToYCoCg(param_2);
    data.fLock = fReprojectedHistory.w;
}

float LoadRwNewLocks(ivec2 iPxPos)
{
    return imageLoad(r_rw_new_locks, iPxPos).x;
}

float JitterSequenceLength()
{
    return cbFSR3Upscaler.fJitterSequenceLength;
}

void UpdateLockStatus(AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    data.fLock *= float(params.bIsNewSample == false);
    float param = params.fShadingChange;
    float param_1 = params.fReactiveMask;
    float param_2 = params.fDisocclusion;
    float param_3 = ffxSaturate(param);
    float param_4 = ffxMax(param_1, param_2);
    float fLifetimeDecreaseFactor = ffxMax(param_3, param_4);
    float param_5 = 0.0;
    float param_6 = data.fLock - (fLifetimeDecreaseFactor * 2.0);
    data.fLock = ffxMax(param_5, param_6);
    float param_7 = data.fLock - 1.0;
    float param_8 = ffxSaturate(param_7) * 1.0;
    data.fLockContributionThisFrame = ffxSaturate(param_8);
    ivec2 param_9 = params.iPxHrPos;
    float param_10 = params.fShadingChange * 0.0;
    float param_11 = params.fReactiveMask;
    float fNewLockIntensity = LoadRwNewLocks(param_9) * (1.0 - ffxMax(param_10, param_11));
    float param_12 = data.fLock + fNewLockIntensity;
    float param_13 = 2.0;
    float param_14 = 0.0;
    float param_15 = ffxMin(param_12, param_13);
    data.fLock = ffxMax(param_14, param_15);
    float fLifetimeDecrease = (0.100000001490116119384765625 / JitterSequenceLength()) * (1.0 - fLifetimeDecreaseFactor);
    float param_16 = 0.0;
    float param_17 = data.fLock - fLifetimeDecrease;
    data.fLock = ffxMax(param_16, param_17);
    vec2 fEstimatedUvNextFrame = params.fHrUv - params.fMotionVector;
    vec2 param_18 = fEstimatedUvNextFrame;
    data.fLock *= float(IsUvInside(param_18) == true);
}

float VelocityFactor()
{
    return cbFSR3Upscaler.fVelocityFactor;
}

float ffxLerp(float x, float y, float t)
{
    return mix(x, y, t);
}

void ComputeBaseAccumulationWeight(AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    float fBaseAccumulation = params.fAccumulation;
    float param = 0.0;
    float param_1 = (params.f4KVelocity * VelocityFactor()) / 0.5;
    float param_2 = ffxMax(param, param_1);
    float param_3 = fBaseAccumulation;
    float param_4 = 0.1500000059604644775390625;
    float param_5 = ffxSaturate(param_2);
    float param_6 = fBaseAccumulation;
    float param_7 = ffxLerp(param_3, param_4, param_5);
    fBaseAccumulation = ffxMin(param_6, param_7);
    data.fHistoryWeight = fBaseAccumulation;
}

vec2 DownscaleFactor()
{
    return cbFSR3Upscaler.fDownscaleFactor;
}

ivec2 ClampLoad(ivec2 iPxSample, ivec2 iPxOffset, ivec2 iTextureSize)
{
    ivec2 result = iPxSample + iPxOffset;
    int param = result.x;
    int param_1 = iTextureSize.x - 1;
    int param_2 = 0;
    int param_3 = ffxMin(param, param_1);
    result.x = ffxMax(param_2, param_3);
    int param_4 = result.y;
    int param_5 = iTextureSize.y - 1;
    int param_6 = 0;
    int param_7 = ffxMin(param_4, param_5);
    result.y = ffxMax(param_6, param_7);
    return result;
}

vec3 LoadInputColor(ivec2 iPxPos)
{
    return texelFetch(SPIRV_Cross_Combinedr_input_color_jitteredSPIRV_Cross_DummySampler, iPxPos, 0).xyz;
}

vec3 ffxMax(vec3 x, vec3 y)
{
    return max(x, y);
}

vec3 LoadPreparedColor(ivec2 iSamplePos)
{
    ivec2 param = iSamplePos;
    vec3 param_1 = vec3(0.0);
    vec3 param_2 = LoadInputColor(param);
    vec3 fRgb = ffxMax(param_1, param_2) * Exposure();
    vec3 param_3 = fRgb;
    vec3 fPreparedYCoCg = RGBToYCoCg(param_3);
    return fPreparedYCoCg;
}

float ComputeMaxKernelWeight(AccumulationPassCommonParams params, AccumulationPassData data)
{
    float fKernelSizeBias = 1.0 + ((vec2(1.0) / DownscaleFactor()) - vec2(1.0)).x;
    float param = 1.9900000095367431640625;
    float param_1 = fKernelSizeBias;
    return ffxMin(param, param_1);
}

bool IsOnScreen(ivec2 pos, ivec2 size)
{
    return all(lessThan(uvec2(pos), uvec2(size)));
}

float Lanczos2ApproxSqNoClamp(float x2)
{
    float a = (0.4000000059604644775390625 * x2) - 1.0;
    float b = (0.25 * x2) - 1.0;
    return (((1.5625 * a) * a) - 0.5625) * (b * b);
}

float Lanczos2ApproxSq(inout float x2)
{
    float param = x2;
    float param_1 = 4.0;
    x2 = ffxMin(param, param_1);
    float param_2 = x2;
    return Lanczos2ApproxSqNoClamp(param_2);
}

float GetUpsampleLanczosWeight(vec2 fSrcSampleOffset, float fKernelWeight)
{
    vec2 fSrcSampleOffsetBiased = fSrcSampleOffset * vec2(fKernelWeight);
    float param = dot(fSrcSampleOffsetBiased, fSrcSampleOffsetBiased);
    float _1117 = Lanczos2ApproxSq(param);
    float fSampleWeight = _1117;
    return fSampleWeight;
}

void RectificationBoxAddInitialSample(inout RectificationBox rectificationBox, vec3 colorSample, float fSampleWeight)
{
    rectificationBox.aabbMin = colorSample;
    rectificationBox.aabbMax = colorSample;
    vec3 weightedSample = colorSample * fSampleWeight;
    rectificationBox.boxCenter = weightedSample;
    rectificationBox.boxVec = colorSample * weightedSample;
    rectificationBox.fBoxCenterWeight = fSampleWeight;
}

vec3 ffxMin(vec3 x, vec3 y)
{
    return min(x, y);
}

void RectificationBoxAddSample(bool bInitialSample, inout RectificationBox rectificationBox, vec3 colorSample, float fSampleWeight)
{
    if (bInitialSample)
    {
        RectificationBox param = rectificationBox;
        RectificationBoxAddInitialSample(param, colorSample, fSampleWeight);
        rectificationBox = param;
    }
    else
    {
        vec3 param_1 = rectificationBox.aabbMin;
        vec3 param_2 = colorSample;
        rectificationBox.aabbMin = ffxMin(param_1, param_2);
        vec3 param_3 = rectificationBox.aabbMax;
        vec3 param_4 = colorSample;
        rectificationBox.aabbMax = ffxMax(param_3, param_4);
        vec3 weightedSample = colorSample * fSampleWeight;
        rectificationBox.boxCenter += weightedSample;
        rectificationBox.boxVec += (colorSample * weightedSample);
        rectificationBox.fBoxCenterWeight += fSampleWeight;
    }
}

void RectificationBoxComputeVarianceBoxData(inout RectificationBox rectificationBox)
{
    float _627;
    if (abs(rectificationBox.fBoxCenterWeight) > 1.1754943508222875079687365372222e-38)
    {
        _627 = rectificationBox.fBoxCenterWeight;
    }
    else
    {
        _627 = 1.0;
    }
    rectificationBox.fBoxCenterWeight = _627;
    rectificationBox.boxCenter /= vec3(rectificationBox.fBoxCenterWeight);
    rectificationBox.boxVec /= vec3(rectificationBox.fBoxCenterWeight);
    vec3 stdDev = sqrt(abs(rectificationBox.boxVec - (rectificationBox.boxCenter * rectificationBox.boxCenter)));
    rectificationBox.boxVec = stdDev;
}

void Deringing(RectificationBox clippingBox, inout vec3 fColor)
{
    fColor = clamp(fColor, clippingBox.aabbMin, clippingBox.aabbMax);
}

void ComputeUpsampledColorAndWeight(AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    vec2 fDstOutputPos = vec2(params.iPxHrPos) + vec2(0.5);
    vec2 fSrcOutputPos = fDstOutputPos * DownscaleFactor();
    ivec2 iSrcInputPos = ivec2(floor(fSrcOutputPos));
    vec2 fSrcUnjitteredPos = (vec2(iSrcInputPos) + vec2(0.5)) - Jitter();
    vec2 fBaseSampleOffset = fSrcUnjitteredPos - fSrcOutputPos;
    ivec2 offsetTL;
    offsetTL.x = (fSrcUnjitteredPos.x > fSrcOutputPos.x) ? (-2) : (-1);
    offsetTL.y = (fSrcUnjitteredPos.y > fSrcOutputPos.y) ? (-2) : (-1);
    bool bFlipRow = fSrcUnjitteredPos.y > fSrcOutputPos.y;
    bool bFlipCol = fSrcUnjitteredPos.x > fSrcOutputPos.x;
    vec2 fOffsetTL = vec2(offsetTL);
    bool bIsInitialSample = params.fAccumulation == 0.0;
    int iSampleIndex = 0;
    int _1229;
    int _1238;
    vec3 fSamples[9];
    for (int row = 0; row < 3; row++)
    {
        for (int col = 0; col < 3; col++)
        {
            if (bFlipCol)
            {
                _1229 = 3 - col;
            }
            else
            {
                _1229 = col;
            }
            if (bFlipRow)
            {
                _1238 = 3 - row;
            }
            else
            {
                _1238 = row;
            }
            ivec2 iSampleColRow = ivec2(_1229, _1238);
            ivec2 iSrcSamplePos = (iSrcInputPos + offsetTL) + iSampleColRow;
            ivec2 param = iSrcSamplePos;
            ivec2 param_1 = ivec2(0);
            ivec2 param_2 = RenderSize();
            ivec2 iSampleCoord = ClampLoad(param, param_1, param_2);
            ivec2 param_3 = iSampleCoord;
            fSamples[iSampleIndex] = LoadPreparedColor(param_3);
            iSampleIndex++;
        }
    }
    AccumulationPassData param_4 = data;
    data = param_4;
    float fKernelBiasMax = ComputeMaxKernelWeight(params, param_4);
    float param_5 = 1.0;
    float param_6 = (1.0 + fKernelBiasMax) * 0.300000011920928955078125;
    float fKernelBiasMin = ffxMax(param_5, param_6);
    float param_7 = data.fHistoryWeight * 5.0;
    float param_8 = 1.0 - params.fShadingChange;
    float param_9 = ffxSaturate(param_7);
    float param_10 = 1.0 - (params.fDisocclusion * 0.5);
    float param_11 = ffxMin(param_8, param_9);
    float fKernelBiasWeight = ffxMin(param_10, param_11);
    float param_12 = fKernelBiasMin;
    float param_13 = fKernelBiasMax;
    float param_14 = fKernelBiasWeight;
    float fKernelBias = ffxLerp(param_12, param_13, param_14);
    iSampleIndex = 0;
    int _1332;
    int _1341;
    for (int row_1 = 0; row_1 < 3; row_1++)
    {
        for (int col_1 = 0; col_1 < 3; col_1++)
        {
            if (bFlipCol)
            {
                _1332 = 3 - col_1;
            }
            else
            {
                _1332 = col_1;
            }
            if (bFlipRow)
            {
                _1341 = 3 - row_1;
            }
            else
            {
                _1341 = row_1;
            }
            ivec2 sampleColRow = ivec2(_1332, _1341);
            vec2 fOffset = fOffsetTL + vec2(sampleColRow);
            vec2 fSrcSampleOffset = fBaseSampleOffset + fOffset;
            ivec2 iSrcSamplePos_1 = (iSrcInputPos + offsetTL) + sampleColRow;
            ivec2 param_15 = iSrcSamplePos_1;
            ivec2 param_16 = RenderSize();
            float fOnScreenFactor = float(IsOnScreen(param_15, param_16));
            if (!bIsInitialSample)
            {
                vec2 param_17 = fSrcSampleOffset;
                float param_18 = fKernelBias;
                float fSampleWeight = fOnScreenFactor * GetUpsampleLanczosWeight(param_17, param_18);
                data.fUpsampledColor += (fSamples[iSampleIndex] * fSampleWeight);
                data.fUpsampledWeight += fSampleWeight;
            }
            float fSrcSampleOffsetSq = dot(fSrcSampleOffset, fSrcSampleOffset);
            float fBoxSampleWeight = exp((-2.2999999523162841796875) * fSrcSampleOffsetSq) * fOnScreenFactor;
            bool bInitialSample = (row_1 == 0) && (col_1 == 0);
            bool param_19 = bInitialSample;
            RectificationBox param_20 = data.clippingBox;
            RectificationBoxAddSample(param_19, param_20, fSamples[iSampleIndex], fBoxSampleWeight);
            data.clippingBox = param_20;
            iSampleIndex++;
        }
    }
    RectificationBox param_21 = data.clippingBox;
    RectificationBoxComputeVarianceBoxData(param_21);
    data.clippingBox = param_21;
    data.fUpsampledWeight *= float(data.fUpsampledWeight > 6.099999882280826568603515625e-05);
    if (data.fUpsampledWeight > 6.099999882280826568603515625e-05)
    {
        data.fUpsampledColor /= vec3(data.fUpsampledWeight);
        data.fUpsampledWeight *= 0.04625000059604644775390625;
        RectificationBox param_22 = data.clippingBox;
        vec3 param_23 = data.fUpsampledColor;
        Deringing(param_22, param_23);
        data.fUpsampledColor = param_23;
    }
    if (bIsInitialSample)
    {
        data.fUpsampledColor = data.clippingBox.boxCenter;
        data.fUpsampledWeight = 1.0;
        data.fHistoryWeight = 0.0;
    }
}

float ffxPow(float x, float y)
{
    return pow(x, y);
}

vec3 ffxLerp(vec3 x, vec3 y, float t)
{
    return mix(x, y, vec3(t));
}

void RectifyHistory(AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    float param = params.f4KVelocity / 20.0;
    float f4kVelocityFactor = ffxSaturate(param);
    float param_1 = 0.75 - (params.fFarthestDepthInMeters / 20.0);
    float fDistanceFactor = ffxSaturate(param_1);
    float fAccumulationFactor = 1.0 - params.fAccumulation;
    float param_2 = params.fReactiveMask;
    float param_3 = 0.5;
    float fReactiveFactor = ffxPow(param_2, param_3);
    float fShadingChangeFactor = params.fShadingChange;
    float param_4 = fReactiveFactor;
    float param_5 = fShadingChangeFactor;
    float param_6 = fAccumulationFactor;
    float param_7 = ffxMax(param_4, param_5);
    float param_8 = fDistanceFactor;
    float param_9 = ffxMax(param_6, param_7);
    float param_10 = f4kVelocityFactor;
    float param_11 = ffxMax(param_8, param_9);
    float fBoxScaleT = ffxMax(param_10, param_11);
    float param_12 = 3.0;
    float param_13 = 1.0;
    float param_14 = fBoxScaleT;
    float fBoxScale = ffxLerp(param_12, param_13, param_14);
    vec3 fScaledBoxVec = (data.clippingBox.boxVec * vec3(1.7000000476837158203125, 1.0, 1.0)) * fBoxScale;
    vec3 param_15 = fScaledBoxVec;
    vec3 param_16 = vec3(1.19299997436428384389728307724e-07);
    vec3 fClampedScaledBoxVec = ffxMax(param_15, param_16);
    vec3 fTransformedHistoryColor = (data.fHistoryColor - data.clippingBox.boxCenter) / fClampedScaledBoxVec;
    if (length(fTransformedHistoryColor) > 1.0)
    {
        vec3 fClampedHistoryColor = normalize(fTransformedHistoryColor);
        vec3 fFinalClampedHistoryColor = (fClampedHistoryColor * fScaledBoxVec) + data.clippingBox.boxCenter;
        float param_17 = params.fLumaInstabilityFactor;
        float param_18 = data.fLockContributionThisFrame;
        float fHistoryContribution = (ffxMax(param_17, param_18) * params.fAccumulation) * (1.0 - params.fDisocclusion);
        float param_19 = fHistoryContribution;
        vec3 param_20 = fFinalClampedHistoryColor;
        vec3 param_21 = data.fHistoryColor;
        float param_22 = ffxSaturate(param_19);
        data.fHistoryColor = ffxLerp(param_20, param_21, param_22);
    }
}

vec3 YCoCgToRGB(vec3 fYCoCg)
{
    vec3 fRgb = vec3((fYCoCg.x + fYCoCg.y) - fYCoCg.z, fYCoCg.x + fYCoCg.z, (fYCoCg.x - fYCoCg.y) - fYCoCg.z);
    return fRgb;
}

void Accumulate(AccumulationPassCommonParams params, inout AccumulationPassData data)
{
    data.fHistoryWeight *= float(data.fHistoryWeight > 6.099999882280826568603515625e-05);
    float param = 6.099999882280826568603515625e-05;
    float param_1 = data.fHistoryWeight + data.fUpsampledWeight;
    data.fHistoryWeight = ffxMax(param, param_1);
    float param_2 = data.fUpsampledWeight / data.fHistoryWeight;
    float fAlpha = ffxSaturate(param_2);
    vec3 param_3 = data.fHistoryColor;
    vec3 param_4 = data.fUpsampledColor;
    float param_5 = fAlpha;
    data.fHistoryColor = ffxLerp(param_3, param_4, param_5);
    vec3 param_6 = data.fHistoryColor;
    data.fHistoryColor = YCoCgToRGB(param_6);
}

void StoreInternalColorAndWeight(ivec2 iPxPos, vec4 fColorAndWeight)
{
    imageStore(rw_internal_upscaled_color, iPxPos, fColorAndWeight);
}

void StoreUpscaledOutput(ivec2 iPxPos, vec3 fColor)
{
    imageStore(rw_upscaled_output, iPxPos, vec4(fColor, 1.0));
}

void StoreNewLocks(ivec2 iPxPos, float newLock)
{
    imageStore(rw_new_locks, iPxPos, vec4(newLock, 0.0, 0.0, 0.0));
}

void Accumulate(ivec2 iPxHrPos)
{
    ivec2 param = iPxHrPos;
    AccumulationPassCommonParams params;
    AccumulationPassCommonParams param_1 = params;
    AccumulationPassData data;
    AccumulationPassData param_2 = data;
    InitPassData(param, param_1, param_2);
    params = param_1;
    data = param_2;
    bool _2159;
    if (params.bIsExistingSample)
    {
        _2159 = !params.bIsNewSample;
    }
    else
    {
        _2159 = params.bIsExistingSample;
    }
    if (_2159)
    {
        AccumulationPassData param_3 = data;
        ReprojectHistoryColor(params, param_3);
        data = param_3;
    }
    AccumulationPassCommonParams param_4 = params;
    AccumulationPassData param_5 = data;
    UpdateLockStatus(param_4, param_5);
    data = param_5;
    AccumulationPassData param_6 = data;
    ComputeBaseAccumulationWeight(params, param_6);
    data = param_6;
    AccumulationPassData param_7 = data;
    ComputeUpsampledColorAndWeight(params, param_7);
    data = param_7;
    AccumulationPassData param_8 = data;
    RectifyHistory(params, param_8);
    data = param_8;
    AccumulationPassData param_9 = data;
    Accumulate(params, param_9);
    data = param_9;
    data.fHistoryColor /= vec3(Exposure());
    vec3 param_10 = data.fHistoryColor;
    vec3 param_11 = vec3(0.0);
    data.fHistoryColor = ffxMax(param_10, param_11);
    ivec2 param_12 = iPxHrPos;
    vec4 param_13 = vec4(data.fHistoryColor, data.fLock);
    StoreInternalColorAndWeight(param_12, param_13);
    ivec2 param_14 = iPxHrPos;
    vec3 param_15 = data.fHistoryColor;
    StoreUpscaledOutput(param_14, param_15);
    ivec2 param_16 = iPxHrPos;
    float param_17 = 0.0;
    StoreNewLocks(param_16, param_17);
}

void main()
{
    ivec2 param = ivec2(gl_GlobalInvocationID.xy);
    Accumulate(param);
}

)FSR3";

// rcas_essl320.glsl sha256 b64ac08df15ef7ae92176881cd29696e92e02acecbb2e11f60f19aecc6aeaf7d
inline constexpr const char* kRcas = R"FSR3(#version 320 es
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(binding = 4, std140) uniform cbRCAS_t
{
    uvec4 rcasConfig;
} cbRCAS;

layout(binding = 3, std140) uniform cbFSR3UPSCALER_t
{
    ivec2 iRenderSize;
    ivec2 iPreviousFrameRenderSize;
    ivec2 iUpscaleSize;
    ivec2 iPreviousFrameUpscaleSize;
    ivec2 iMaxRenderSize;
    ivec2 iMaxUpscaleSize;
    vec4 fDeviceToViewDepth;
    vec2 fJitter;
    vec2 fPreviousFrameJitter;
    vec2 fMotionVectorScale;
    vec2 fDownscaleFactor;
    vec2 fMotionVectorJitterCancellation;
    float fTanHalfFOV;
    float fJitterSequenceLength;
    float fDeltaTime;
    float fDeltaPreExposure;
    float fViewSpaceToMetersFactor;
    float fFrameIndex;
    float fVelocityFactor;
    float fReactivenessScale;
    float fShadingChangeScale;
    float fAccumulationAddedPerFrame;
    float fMinDisocclusionAccumulation;
} cbFSR3Upscaler;

layout(binding = 2, rgba8) uniform writeonly highp image2D rw_upscaled_output;
uniform highp sampler2D SPIRV_Cross_Combinedr_rcas_inputSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler;

uint ffxBitfieldExtract(uint src, uint off, uint bits)
{
    return bitfieldExtract(src, int(off), int(bits));
}

uint ffxBitfieldInsertMask(uint src, uint ins, uint bits)
{
    return bitfieldInsert(src, ins, 0, int(bits));
}

uvec2 ffxRemapForQuad(uint a)
{
    uint param = a;
    uint param_1 = 1u;
    uint param_2 = 3u;
    uint param_3 = a;
    uint param_4 = 3u;
    uint param_5 = 3u;
    uint param_6 = ffxBitfieldExtract(param_3, param_4, param_5);
    uint param_7 = a;
    uint param_8 = 1u;
    return uvec2(ffxBitfieldExtract(param, param_1, param_2), ffxBitfieldInsertMask(param_6, param_7, param_8));
}

uvec4 RCASConfig()
{
    return cbRCAS.rcasConfig;
}

vec4 LoadRCAS_Input(ivec2 iPxPos)
{
    return texelFetch(SPIRV_Cross_Combinedr_rcas_inputSPIRV_Cross_DummySampler, iPxPos, 0);
}

float Exposure()
{
    float exposure = texelFetch(SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler, ivec2(0), 0).x;
    if (exposure == 0.0)
    {
        exposure = 1.0;
    }
    return exposure;
}

vec4 FsrRcasLoadF(ivec2 p)
{
    ivec2 param = p;
    vec4 fColor = LoadRCAS_Input(param);
    vec4 _262 = fColor;
    vec3 _264 = _262.xyz * Exposure();
    fColor.x = _264.x;
    fColor.y = _264.y;
    fColor.z = _264.z;
    return fColor;
}

void FsrRcasInputF(float r, float g, float b)
{
}

float ffxMax3(float x, float y, float z)
{
    return max(x, max(y, z));
}

float ffxMin3(float x, float y, float z)
{
    return min(x, min(y, z));
}

uint ffxAsUInt32(float x)
{
    return floatBitsToUint(x);
}

float ffxAsFloat(uint x)
{
    return uintBitsToFloat(x);
}

float ffxApproximateReciprocalMedium(float value)
{
    float param = value;
    uint param_1 = 2129764351u - ffxAsUInt32(param);
    float b = ffxAsFloat(param_1);
    return b * (((-b) * value) + 2.0);
}

float ffxSaturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

float ffxMin(float x, float y)
{
    return min(x, y);
}

float ffxReciprocal(float x)
{
    return 1.0 / x;
}

void FsrRcasF(out float pixR, out float pixG, out float pixB, uvec2 ip, uvec4 con)
{
    ivec2 sp = ivec2(ip);
    ivec2 param = sp + ivec2(0, -1);
    vec3 b = FsrRcasLoadF(param).xyz;
    ivec2 param_1 = sp + ivec2(-1, 0);
    vec3 d = FsrRcasLoadF(param_1).xyz;
    ivec2 param_2 = sp;
    vec3 e = FsrRcasLoadF(param_2).xyz;
    ivec2 param_3 = sp + ivec2(1, 0);
    vec3 f = FsrRcasLoadF(param_3).xyz;
    ivec2 param_4 = sp + ivec2(0, 1);
    vec3 h = FsrRcasLoadF(param_4).xyz;
    float bR = b.x;
    float bG = b.y;
    float bB = b.z;
    float dR = d.x;
    float dG = d.y;
    float dB = d.z;
    float eR = e.x;
    float eG = e.y;
    float eB = e.z;
    float fR = f.x;
    float fG = f.y;
    float fB = f.z;
    float hR = h.x;
    float hG = h.y;
    float hB = h.z;
    float param_5 = bR;
    float param_6 = bG;
    float param_7 = bB;
    FsrRcasInputF(param_5, param_6, param_7);
    bR = param_5;
    bG = param_6;
    bB = param_7;
    float param_8 = dR;
    float param_9 = dG;
    float param_10 = dB;
    FsrRcasInputF(param_8, param_9, param_10);
    dR = param_8;
    dG = param_9;
    dB = param_10;
    float param_11 = eR;
    float param_12 = eG;
    float param_13 = eB;
    FsrRcasInputF(param_11, param_12, param_13);
    eR = param_11;
    eG = param_12;
    eB = param_13;
    float param_14 = fR;
    float param_15 = fG;
    float param_16 = fB;
    FsrRcasInputF(param_14, param_15, param_16);
    fR = param_14;
    fG = param_15;
    fB = param_16;
    float param_17 = hR;
    float param_18 = hG;
    float param_19 = hB;
    FsrRcasInputF(param_17, param_18, param_19);
    hR = param_17;
    hG = param_18;
    hB = param_19;
    float bL = (bB * 0.5) + ((bR * 0.5) + bG);
    float dL = (dB * 0.5) + ((dR * 0.5) + dG);
    float eL = (eB * 0.5) + ((eR * 0.5) + eG);
    float fL = (fB * 0.5) + ((fR * 0.5) + fG);
    float hL = (hB * 0.5) + ((hR * 0.5) + hG);
    float nz = ((((0.25 * bL) + (0.25 * dL)) + (0.25 * fL)) + (0.25 * hL)) - eL;
    float param_20 = bL;
    float param_21 = dL;
    float param_22 = eL;
    float param_23 = ffxMax3(param_20, param_21, param_22);
    float param_24 = fL;
    float param_25 = hL;
    float param_26 = bL;
    float param_27 = dL;
    float param_28 = eL;
    float param_29 = ffxMin3(param_26, param_27, param_28);
    float param_30 = fL;
    float param_31 = hL;
    float param_32 = ffxMax3(param_23, param_24, param_25) - ffxMin3(param_29, param_30, param_31);
    float param_33 = abs(nz) * ffxApproximateReciprocalMedium(param_32);
    nz = ffxSaturate(param_33);
    nz = ((-0.5) * nz) + 1.0;
    float param_34 = bR;
    float param_35 = dR;
    float param_36 = fR;
    float param_37 = ffxMin3(param_34, param_35, param_36);
    float param_38 = hR;
    float mn4R = ffxMin(param_37, param_38);
    float param_39 = bG;
    float param_40 = dG;
    float param_41 = fG;
    float param_42 = ffxMin3(param_39, param_40, param_41);
    float param_43 = hG;
    float mn4G = ffxMin(param_42, param_43);
    float param_44 = bB;
    float param_45 = dB;
    float param_46 = fB;
    float param_47 = ffxMin3(param_44, param_45, param_46);
    float param_48 = hB;
    float mn4B = ffxMin(param_47, param_48);
    float param_49 = bR;
    float param_50 = dR;
    float param_51 = fR;
    float mx4R = max(ffxMax3(param_49, param_50, param_51), hR);
    float param_52 = bG;
    float param_53 = dG;
    float param_54 = fG;
    float mx4G = max(ffxMax3(param_52, param_53, param_54), hG);
    float param_55 = bB;
    float param_56 = dB;
    float param_57 = fB;
    float mx4B = max(ffxMax3(param_55, param_56, param_57), hB);
    vec2 peakC = vec2(1.0, -4.0);
    float param_58 = 4.0 * mx4R;
    float hitMinR = mn4R * ffxReciprocal(param_58);
    float param_59 = 4.0 * mx4G;
    float hitMinG = mn4G * ffxReciprocal(param_59);
    float param_60 = 4.0 * mx4B;
    float hitMinB = mn4B * ffxReciprocal(param_60);
    float param_61 = (4.0 * mn4R) + peakC.y;
    float hitMaxR = (peakC.x - mx4R) * ffxReciprocal(param_61);
    float param_62 = (4.0 * mn4G) + peakC.y;
    float hitMaxG = (peakC.x - mx4G) * ffxReciprocal(param_62);
    float param_63 = (4.0 * mn4B) + peakC.y;
    float hitMaxB = (peakC.x - mx4B) * ffxReciprocal(param_63);
    float lobeR = max(-hitMinR, hitMaxR);
    float lobeG = max(-hitMinG, hitMaxG);
    float lobeB = max(-hitMinB, hitMaxB);
    float param_64 = lobeR;
    float param_65 = lobeG;
    float param_66 = lobeB;
    float param_67 = ffxMax3(param_64, param_65, param_66);
    float param_68 = 0.0;
    uint param_69 = con.x;
    float lobe = max(-0.1875, ffxMin(param_67, param_68)) * ffxAsFloat(param_69);
    lobe *= nz;
    float param_70 = (4.0 * lobe) + 1.0;
    float rcpL = ffxApproximateReciprocalMedium(param_70);
    pixR = (((((lobe * bR) + (lobe * dR)) + (lobe * hR)) + (lobe * fR)) + eR) * rcpL;
    pixG = (((((lobe * bG) + (lobe * dG)) + (lobe * hG)) + (lobe * fG)) + eG) * rcpL;
    pixB = (((((lobe * bB) + (lobe * dB)) + (lobe * hB)) + (lobe * fB)) + eB) * rcpL;
}

void StoreUpscaledOutput(ivec2 iPxPos, vec3 fColor)
{
    imageStore(rw_upscaled_output, iPxPos, vec4(fColor, 1.0));
}

void WriteUpscaledOutput(uvec2 iPxHrPos, vec3 fUpscaledColor)
{
    ivec2 param = ivec2(iPxHrPos);
    vec3 param_1 = fUpscaledColor;
    StoreUpscaledOutput(param, param_1);
}

void CurrFilter(uvec2 pos)
{
    uvec2 param_3 = pos;
    uvec4 param_4 = RCASConfig();
    float param;
    float param_1;
    float param_2;
    FsrRcasF(param, param_1, param_2, param_3, param_4);
    vec3 c;
    c.x = param;
    c.y = param_1;
    c.z = param_2;
    c /= vec3(Exposure());
    uvec2 param_5 = pos;
    vec3 param_6 = c;
    WriteUpscaledOutput(param_5, param_6);
}

void RCAS(uvec3 LocalThreadId, uvec3 WorkGroupId, uvec3 Dtid)
{
    uint param = LocalThreadId.x;
    uvec2 gxy = ffxRemapForQuad(param) + uvec2(WorkGroupId.x << 4u, WorkGroupId.y << 4u);
    uvec2 param_1 = gxy;
    CurrFilter(param_1);
    gxy.x += 8u;
    uvec2 param_2 = gxy;
    CurrFilter(param_2);
    gxy.y += 8u;
    uvec2 param_3 = gxy;
    CurrFilter(param_3);
    gxy.x -= 8u;
    uvec2 param_4 = gxy;
    CurrFilter(param_4);
}

void main()
{
    uvec3 param = gl_LocalInvocationID;
    uvec3 param_1 = gl_WorkGroupID;
    uvec3 param_2 = gl_GlobalInvocationID;
    RCAS(param, param_1, param_2);
}

)FSR3";

// rcas_hdr_essl320.glsl sha256 955883faf2dfb899bbe9dc2d4082dfcbd966bff719500aa65bb3a2e6be643646
inline constexpr const char* kRcasHdr = R"FSR3(#version 320 es
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(binding = 4, std140) uniform cbRCAS_t
{
    uvec4 rcasConfig;
} cbRCAS;

layout(binding = 3, std140) uniform cbFSR3UPSCALER_t
{
    ivec2 iRenderSize;
    ivec2 iPreviousFrameRenderSize;
    ivec2 iUpscaleSize;
    ivec2 iPreviousFrameUpscaleSize;
    ivec2 iMaxRenderSize;
    ivec2 iMaxUpscaleSize;
    vec4 fDeviceToViewDepth;
    vec2 fJitter;
    vec2 fPreviousFrameJitter;
    vec2 fMotionVectorScale;
    vec2 fDownscaleFactor;
    vec2 fMotionVectorJitterCancellation;
    float fTanHalfFOV;
    float fJitterSequenceLength;
    float fDeltaTime;
    float fDeltaPreExposure;
    float fViewSpaceToMetersFactor;
    float fFrameIndex;
    float fVelocityFactor;
    float fReactivenessScale;
    float fShadingChangeScale;
    float fAccumulationAddedPerFrame;
    float fMinDisocclusionAccumulation;
} cbFSR3Upscaler;

layout(binding = 2, rgba16f) uniform writeonly highp image2D rw_upscaled_output;
uniform highp sampler2D SPIRV_Cross_Combinedr_rcas_inputSPIRV_Cross_DummySampler;
uniform highp sampler2D SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler;

uint ffxBitfieldExtract(uint src, uint off, uint bits)
{
    return bitfieldExtract(src, int(off), int(bits));
}

uint ffxBitfieldInsertMask(uint src, uint ins, uint bits)
{
    return bitfieldInsert(src, ins, 0, int(bits));
}

uvec2 ffxRemapForQuad(uint a)
{
    uint param = a;
    uint param_1 = 1u;
    uint param_2 = 3u;
    uint param_3 = a;
    uint param_4 = 3u;
    uint param_5 = 3u;
    uint param_6 = ffxBitfieldExtract(param_3, param_4, param_5);
    uint param_7 = a;
    uint param_8 = 1u;
    return uvec2(ffxBitfieldExtract(param, param_1, param_2), ffxBitfieldInsertMask(param_6, param_7, param_8));
}

uvec4 RCASConfig()
{
    return cbRCAS.rcasConfig;
}

vec4 LoadRCAS_Input(ivec2 iPxPos)
{
    return texelFetch(SPIRV_Cross_Combinedr_rcas_inputSPIRV_Cross_DummySampler, iPxPos, 0);
}

float Exposure()
{
    float exposure = texelFetch(SPIRV_Cross_Combinedr_input_exposureSPIRV_Cross_DummySampler, ivec2(0), 0).x;
    if (exposure == 0.0)
    {
        exposure = 1.0;
    }
    return exposure;
}

vec4 FsrRcasLoadF(ivec2 p)
{
    ivec2 param = p;
    vec4 fColor = LoadRCAS_Input(param);
    vec4 _262 = fColor;
    vec3 _264 = _262.xyz * Exposure();
    fColor.x = _264.x;
    fColor.y = _264.y;
    fColor.z = _264.z;
    return fColor;
}

void FsrRcasInputF(float r, float g, float b)
{
}

float ffxMax3(float x, float y, float z)
{
    return max(x, max(y, z));
}

float ffxMin3(float x, float y, float z)
{
    return min(x, min(y, z));
}

uint ffxAsUInt32(float x)
{
    return floatBitsToUint(x);
}

float ffxAsFloat(uint x)
{
    return uintBitsToFloat(x);
}

float ffxApproximateReciprocalMedium(float value)
{
    float param = value;
    uint param_1 = 2129764351u - ffxAsUInt32(param);
    float b = ffxAsFloat(param_1);
    return b * (((-b) * value) + 2.0);
}

float ffxSaturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

float ffxMin(float x, float y)
{
    return min(x, y);
}

float ffxReciprocal(float x)
{
    return 1.0 / x;
}

void FsrRcasF(out float pixR, out float pixG, out float pixB, uvec2 ip, uvec4 con)
{
    ivec2 sp = ivec2(ip);
    ivec2 param = sp + ivec2(0, -1);
    vec3 b = FsrRcasLoadF(param).xyz;
    ivec2 param_1 = sp + ivec2(-1, 0);
    vec3 d = FsrRcasLoadF(param_1).xyz;
    ivec2 param_2 = sp;
    vec3 e = FsrRcasLoadF(param_2).xyz;
    ivec2 param_3 = sp + ivec2(1, 0);
    vec3 f = FsrRcasLoadF(param_3).xyz;
    ivec2 param_4 = sp + ivec2(0, 1);
    vec3 h = FsrRcasLoadF(param_4).xyz;
    float bR = b.x;
    float bG = b.y;
    float bB = b.z;
    float dR = d.x;
    float dG = d.y;
    float dB = d.z;
    float eR = e.x;
    float eG = e.y;
    float eB = e.z;
    float fR = f.x;
    float fG = f.y;
    float fB = f.z;
    float hR = h.x;
    float hG = h.y;
    float hB = h.z;
    float param_5 = bR;
    float param_6 = bG;
    float param_7 = bB;
    FsrRcasInputF(param_5, param_6, param_7);
    bR = param_5;
    bG = param_6;
    bB = param_7;
    float param_8 = dR;
    float param_9 = dG;
    float param_10 = dB;
    FsrRcasInputF(param_8, param_9, param_10);
    dR = param_8;
    dG = param_9;
    dB = param_10;
    float param_11 = eR;
    float param_12 = eG;
    float param_13 = eB;
    FsrRcasInputF(param_11, param_12, param_13);
    eR = param_11;
    eG = param_12;
    eB = param_13;
    float param_14 = fR;
    float param_15 = fG;
    float param_16 = fB;
    FsrRcasInputF(param_14, param_15, param_16);
    fR = param_14;
    fG = param_15;
    fB = param_16;
    float param_17 = hR;
    float param_18 = hG;
    float param_19 = hB;
    FsrRcasInputF(param_17, param_18, param_19);
    hR = param_17;
    hG = param_18;
    hB = param_19;
    float bL = (bB * 0.5) + ((bR * 0.5) + bG);
    float dL = (dB * 0.5) + ((dR * 0.5) + dG);
    float eL = (eB * 0.5) + ((eR * 0.5) + eG);
    float fL = (fB * 0.5) + ((fR * 0.5) + fG);
    float hL = (hB * 0.5) + ((hR * 0.5) + hG);
    float nz = ((((0.25 * bL) + (0.25 * dL)) + (0.25 * fL)) + (0.25 * hL)) - eL;
    float param_20 = bL;
    float param_21 = dL;
    float param_22 = eL;
    float param_23 = ffxMax3(param_20, param_21, param_22);
    float param_24 = fL;
    float param_25 = hL;
    float param_26 = bL;
    float param_27 = dL;
    float param_28 = eL;
    float param_29 = ffxMin3(param_26, param_27, param_28);
    float param_30 = fL;
    float param_31 = hL;
    float param_32 = ffxMax3(param_23, param_24, param_25) - ffxMin3(param_29, param_30, param_31);
    float param_33 = abs(nz) * ffxApproximateReciprocalMedium(param_32);
    nz = ffxSaturate(param_33);
    nz = ((-0.5) * nz) + 1.0;
    float param_34 = bR;
    float param_35 = dR;
    float param_36 = fR;
    float param_37 = ffxMin3(param_34, param_35, param_36);
    float param_38 = hR;
    float mn4R = ffxMin(param_37, param_38);
    float param_39 = bG;
    float param_40 = dG;
    float param_41 = fG;
    float param_42 = ffxMin3(param_39, param_40, param_41);
    float param_43 = hG;
    float mn4G = ffxMin(param_42, param_43);
    float param_44 = bB;
    float param_45 = dB;
    float param_46 = fB;
    float param_47 = ffxMin3(param_44, param_45, param_46);
    float param_48 = hB;
    float mn4B = ffxMin(param_47, param_48);
    float param_49 = bR;
    float param_50 = dR;
    float param_51 = fR;
    float mx4R = max(ffxMax3(param_49, param_50, param_51), hR);
    float param_52 = bG;
    float param_53 = dG;
    float param_54 = fG;
    float mx4G = max(ffxMax3(param_52, param_53, param_54), hG);
    float param_55 = bB;
    float param_56 = dB;
    float param_57 = fB;
    float mx4B = max(ffxMax3(param_55, param_56, param_57), hB);
    vec2 peakC = vec2(1.0, -4.0);
    float param_58 = 4.0 * mx4R;
    float hitMinR = mn4R * ffxReciprocal(param_58);
    float param_59 = 4.0 * mx4G;
    float hitMinG = mn4G * ffxReciprocal(param_59);
    float param_60 = 4.0 * mx4B;
    float hitMinB = mn4B * ffxReciprocal(param_60);
    float param_61 = (4.0 * mn4R) + peakC.y;
    float hitMaxR = (peakC.x - mx4R) * ffxReciprocal(param_61);
    float param_62 = (4.0 * mn4G) + peakC.y;
    float hitMaxG = (peakC.x - mx4G) * ffxReciprocal(param_62);
    float param_63 = (4.0 * mn4B) + peakC.y;
    float hitMaxB = (peakC.x - mx4B) * ffxReciprocal(param_63);
    float lobeR = max(-hitMinR, hitMaxR);
    float lobeG = max(-hitMinG, hitMaxG);
    float lobeB = max(-hitMinB, hitMaxB);
    float param_64 = lobeR;
    float param_65 = lobeG;
    float param_66 = lobeB;
    float param_67 = ffxMax3(param_64, param_65, param_66);
    float param_68 = 0.0;
    uint param_69 = con.x;
    float lobe = max(-0.1875, ffxMin(param_67, param_68)) * ffxAsFloat(param_69);
    lobe *= nz;
    float param_70 = (4.0 * lobe) + 1.0;
    float rcpL = ffxApproximateReciprocalMedium(param_70);
    pixR = (((((lobe * bR) + (lobe * dR)) + (lobe * hR)) + (lobe * fR)) + eR) * rcpL;
    pixG = (((((lobe * bG) + (lobe * dG)) + (lobe * hG)) + (lobe * fG)) + eG) * rcpL;
    pixB = (((((lobe * bB) + (lobe * dB)) + (lobe * hB)) + (lobe * fB)) + eB) * rcpL;
}

void StoreUpscaledOutput(ivec2 iPxPos, vec3 fColor)
{
    imageStore(rw_upscaled_output, iPxPos, vec4(fColor, 1.0));
}

void WriteUpscaledOutput(uvec2 iPxHrPos, vec3 fUpscaledColor)
{
    ivec2 param = ivec2(iPxHrPos);
    vec3 param_1 = fUpscaledColor;
    StoreUpscaledOutput(param, param_1);
}

void CurrFilter(uvec2 pos)
{
    uvec2 param_3 = pos;
    uvec4 param_4 = RCASConfig();
    float param;
    float param_1;
    float param_2;
    FsrRcasF(param, param_1, param_2, param_3, param_4);
    vec3 c;
    c.x = param;
    c.y = param_1;
    c.z = param_2;
    c /= vec3(Exposure());
    uvec2 param_5 = pos;
    vec3 param_6 = c;
    WriteUpscaledOutput(param_5, param_6);
}

void RCAS(uvec3 LocalThreadId, uvec3 WorkGroupId, uvec3 Dtid)
{
    uint param = LocalThreadId.x;
    uvec2 gxy = ffxRemapForQuad(param) + uvec2(WorkGroupId.x << 4u, WorkGroupId.y << 4u);
    uvec2 param_1 = gxy;
    CurrFilter(param_1);
    gxy.x += 8u;
    uvec2 param_2 = gxy;
    CurrFilter(param_2);
    gxy.y += 8u;
    uvec2 param_3 = gxy;
    CurrFilter(param_3);
    gxy.x -= 8u;
    uvec2 param_4 = gxy;
    CurrFilter(param_4);
}

void main()
{
    uvec3 param = gl_LocalInvocationID;
    uvec3 param_1 = gl_WorkGroupID;
    uvec3 param_2 = gl_GlobalInvocationID;
    RCAS(param, param_1, param_2);
}

)FSR3";

} // namespace fsr3_shaders

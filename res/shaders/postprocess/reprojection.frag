#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "res/shaders/common/common.glsl"

#define CLIPPING_MODE_NONE 0
#define CLIPPING_MODE_FAST 1
#define CLIPPING_MODE_ACCURATE 2

layout(location = 0) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO1 {
    uvec2 resolution;
    vec2 taaCurrentJitterOffset;
    vec2 taaPreviousJitterOffset;
    float taaHistoryFadeFactor;
    bool useCatmullRomFilter;
    uint clippingMode;
    bool useMitchellFilter;
    float mitchellB;
    float mitchellC;
    bool taaEnabled;
};

layout(set = 0, binding = 1) uniform sampler2D frameTexture;
layout(set = 0, binding = 2) uniform sampler2D velocityTexture;
layout(set = 0, binding = 3) uniform sampler2D depthTexture;
layout(set = 0, binding = 4) uniform sampler2D previousFrameTexture;
layout(set = 0, binding = 5) uniform sampler2D previousVelocityTexture;


vec3 findClosestFragment3x3(in vec2 texCoord) {
    const vec2 du = vec2(1.0 / resolution.x, 0.0);
    const vec2 dv = vec2(0.0, 1.0 / resolution.y);

    vec3 currentFragment;
    currentFragment.xy = texCoord - dv - du;
    currentFragment.z = texture(depthTexture, currentFragment.xy).r;
    vec3 closestFragment = currentFragment;
    currentFragment.xy = texCoord - dv;
    currentFragment.z = texture(depthTexture, currentFragment.xy).r;
    if (currentFragment.z < closestFragment.z) closestFragment = currentFragment;
    currentFragment.xy = texCoord - dv + du;
    currentFragment.z = texture(depthTexture, currentFragment.xy).r;
    if (currentFragment.z < closestFragment.z) closestFragment = currentFragment;
    currentFragment.xy = texCoord - du;
    currentFragment.z = texture(depthTexture, currentFragment.xy).r;
    if (currentFragment.z < closestFragment.z) closestFragment = currentFragment;
    currentFragment.xy = texCoord;
    currentFragment.z = texture(depthTexture, currentFragment.xy).r;
    if (currentFragment.z < closestFragment.z) closestFragment = currentFragment;
    currentFragment.xy = texCoord + du;
    currentFragment.z = texture(depthTexture, currentFragment.xy).r;
    if (currentFragment.z < closestFragment.z) closestFragment = currentFragment;
    currentFragment.xy = texCoord + dv - du;
    currentFragment.z = texture(depthTexture, currentFragment.xy).r;
    if (currentFragment.z < closestFragment.z) closestFragment = currentFragment;
    currentFragment.xy = texCoord + dv;
    currentFragment.z = texture(depthTexture, currentFragment.xy).r;
    if (currentFragment.z < closestFragment.z) closestFragment = currentFragment;
    currentFragment.xy = texCoord + dv + du;
    currentFragment.z = texture(depthTexture, currentFragment.xy).r;
    if (currentFragment.z < closestFragment.z) closestFragment = currentFragment;
    return closestFragment;
}

vec2 sampleVelocity(in vec2 texCoord) {
    return texture(velocityTexture, texCoord).xy / VELOCITY_PRECISION_SCALE;
}

float filterCubic(in float x, in float B, in float C) {
    float y = 0.0;
    float x2 = x * x;
    float x3 = x2 * x;
    if(x < 1)
        y = (12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 + (6 - 2 * B);
    else if (x <= 2)
        y = (-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 + (-12 * B - 48 * C) * x + (8 * B + 24 * C);
    return y * 0.16666666666666666;
}

float mitchell(in float dist) {
    return filterCubic(dist, mitchellB, mitchellC);
}

// https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
vec4 sampleTextureCatmullRom(in sampler2D tex, in vec2 uv, in vec2 texSize) {
    // We're going to sample a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    vec2 samplePos = uv * texSize;
    vec2 texPos1 = floor(samplePos - 0.5) + 0.5;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    vec2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    vec2 w12 = w1 + w2;
    vec2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    vec2 texPos0 = texPos1 - vec2(1.0);
    vec2 texPos3 = texPos1 + vec2(2.0);
    vec2 texPos12 = texPos1 + offset12;

    texPos0 /= texSize;
    texPos3 /= texSize;
    texPos12 /= texSize;

    vec4 result = vec4(0.0);
    result += texture(tex, vec2(texPos0.x, texPos0.y)) * w0.x * w0.y;
    result += texture(tex, vec2(texPos12.x, texPos0.y)) * w12.x * w0.y;
    result += texture(tex, vec2(texPos3.x, texPos0.y)) * w3.x * w0.y;

    result += texture(tex, vec2(texPos0.x, texPos12.y)) * w0.x * w12.y;
    result += texture(tex, vec2(texPos12.x, texPos12.y)) * w12.x * w12.y;
    result += texture(tex, vec2(texPos3.x, texPos12.y)) * w3.x * w12.y;

    result += texture(tex, vec2(texPos0.x, texPos3.y)) * w0.x * w3.y;
    result += texture(tex, vec2(texPos12.x, texPos3.y)) * w12.x * w3.y;
    result += texture(tex, vec2(texPos3.x, texPos3.y)) * w3.x * w3.y;

    return result;
}


// https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/TemporalReprojection.shader#L122
vec3 clip_aabb_fast(vec3 aabb_min, vec3 aabb_max, vec3 p, vec3 q) {
    // note: only clips towards aabb center (but fast!)
    vec3 p_clip = 0.5 * (aabb_max + aabb_min);
    vec3 e_clip = 0.5 * (aabb_max - aabb_min) + 1e-4;

    vec3 v_clip = q - p_clip;
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (ma_unit > 1.0)
    return p_clip + v_clip / ma_unit;
    else
    return q;// point inside aabb
}

vec3 clip_aabb_accurate(vec3 aabb_min, vec3 aabb_max, vec3 p, vec3 q) {
    vec3 r = q - p;
    vec3 rmax = aabb_max - p.xyz;
    vec3 rmin = aabb_min - p.xyz;

    const float eps = 1e-6;

    if (r.x > rmax.x + eps)
    r *= (rmax.x / r.x);
    if (r.y > rmax.y + eps)
    r *= (rmax.y / r.y);
    if (r.z > rmax.z + eps)
    r *= (rmax.z / r.z);

    if (r.x < rmin.x - eps)
    r *= (rmin.x / r.x);
    if (r.y < rmin.y - eps)
    r *= (rmin.y / r.y);
    if (r.z < rmin.z - eps)
    r *= (rmin.z / r.z);

    return p + r;
}

float calcLuminance(vec3 colour) {
    return dot(colour, vec3(0.2127, 0.7152, 0.0722));
}

//vec3 calculateTemporalAntiAliasing(in vec2 texCoord, in vec2 velocity) {
//    const vec2 KERNEL_OFFSETS[] = vec2[8](vec2(-1,-1),vec2(0,-1),vec2(+1,-1),vec2(-1,0),vec2(+1,0),vec2(-1,+1),vec2(0,+1),vec2(+1,+1));
//    const float KERNEL_DISTANCES[] = float[8](SQRT2, 1, SQRT2, 1, 1, SQRT2, 1, SQRT2);
//    const float invSampleCount = 1.0 / 9.0;
//
//    const vec2 pixelSize = vec2(1.0) / vec2(resolution);
//    vec2 closestCoord = unjitteredTexCoord;
//    float closestDepth = texture(depthTexture, closestCoord).r;
//    float sampleWeight;
//    vec2 sampleCoord;
//    float sampleDepth;
//    vec3 sampleColour;
//
//    sampleColour = max(vec3(0.0), frameColour);
//
//    sampleWeight = mitchell(0.0);
//    vec3 sourceSampleColour = sampleColour * sampleWeight;
//    float totalSampleWeight = sampleWeight;
//
//    vec3 colourBoundMin = sampleColour;
//    vec3 colourBoundMax = sampleColour;
//
//    for (uint i = 0; i < 8; ++i) {
//        vec2 sampleOffset = KERNEL_OFFSETS[i];
//        float sampleDistance = KERNEL_DISTANCES[i];
//
//        sampleWeight = mitchell(sampleDistance);
//        sampleCoord = unjitteredTexCoord + sampleOffset * pixelSize;
//        sampleDepth = texture(depthTexture, sampleCoord).r;
//        sampleColour = max(vec3(0.0), texture(frameTexture, sampleCoord).rgb);
//        sourceSampleColour += sampleColour * sampleWeight;
//        totalSampleWeight += sampleWeight;
//        colourBoundMin = min(colourBoundMin, sampleColour);
//        colourBoundMax = max(colourBoundMax, sampleColour);
//
//        if (sampleDepth < closestDepth) {
//            closestDepth = sampleDepth;
//            closestCoord = sampleCoord;
//        }
//    }
//
//    sourceSampleColour /= totalSampleWeight;
//
//    vec2 currVelocity = texture(velocityTexture, closestCoord).xy / VELOCITY_PRECISION_SCALE;
//    vec2 prevCoord = unjitteredTexCoord - currVelocity;
//
////    vec3 historySampleColour = texture(previousFrameTexture, prevCoord).rgb;
//    vec3 historySampleColour = sampleTextureCatmullRom(previousFrameTexture, prevCoord, vec2(resolution)).rgb;
//    historySampleColour = clip_aabb_fast(colourBoundMin, colourBoundMax, sourceSampleColour, historySampleColour);
//
////    vec2 prevVelocity = texture(previousVelocityTexture, prevCoord).xy / VELOCITY_PRECISION_SCALE;
////
////    float velocityLength = length(prevVelocity - currVelocity);
////    float velocityDisocclusion = clamp((velocityLength - 1e-3) * 10.0, 0.0, 1.0);
//
//    return mix(historySampleColour, sourceSampleColour, taaHistoryFadeFactor);
//
////    return mix(accumulation, frameColour, velocityDisocclusion);
//}


vec3 calculateTemporalAntiAliasing(in vec2 texCoord, in vec2 velocity) {
    const vec2 du = vec2(1.0 / resolution.x, 0.0);
    const vec2 dv = vec2(0.0, 1.0 / resolution.y);

    vec2 uv = texCoord - taaCurrentJitterOffset;
    vec3 sourceColour = max(vec3(0.0), texture(frameTexture, uv).rgb);
    vec3 sourceSample;

    vec3 ctl = max(vec3(0.0), texture(frameTexture, uv - dv - du).rgb);
    vec3 ctc = max(vec3(0.0), texture(frameTexture, uv - dv).rgb);
    vec3 ctr = max(vec3(0.0), texture(frameTexture, uv - dv + du).rgb);
    vec3 cml = max(vec3(0.0), texture(frameTexture, uv - du).rgb);
    vec3 cmc = sourceColour;
    vec3 cmr = max(vec3(0.0), texture(frameTexture, uv + du).rgb);
    vec3 cbl = max(vec3(0.0), texture(frameTexture, uv + dv - du).rgb);
    vec3 cbc = max(vec3(0.0), texture(frameTexture, uv + dv).rgb);
    vec3 cbr = max(vec3(0.0), texture(frameTexture, uv + dv + du).rgb);

    if (useMitchellFilter) {
        const float sampleWeight0 = mitchell(0.0);// center sample
        const float sampleWeight1 = mitchell(1.0);// side samples
        const float sampleWeight2 = mitchell(SQRT2);// corner samples
        float totalSampleWeight = sampleWeight0 + sampleWeight1 * 4.0 + sampleWeight2 * 4.0;
        sourceSample = sourceColour * sampleWeight0;
        sourceSample += ctl * sampleWeight2 + ctr * sampleWeight2 + cbl * sampleWeight2 + cbr * sampleWeight2;
        sourceSample += ctc * sampleWeight1 + cml * sampleWeight1 + cmr * sampleWeight1 + cbc * sampleWeight1;
        sourceSample /= totalSampleWeight;
    } else {
        sourceSample = sourceColour;
    }

    vec3 cmin = min(ctl, min(ctc, min(ctr, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
    vec3 cmax = max(ctl, max(ctc, max(ctr, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));
    vec3 cavg = (ctl + ctc + ctr + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;

    vec3 historySample;

    if (useCatmullRomFilter) {
        historySample = sampleTextureCatmullRom(previousFrameTexture, texCoord - velocity, vec2(resolution)).rgb;
    } else {
        historySample = texture(previousFrameTexture, texCoord - velocity).rgb;
    }

    if (clippingMode == CLIPPING_MODE_FAST) {
        historySample = clip_aabb_fast(cmin.xyz, cmax.xyz, clamp(cavg, cmin, cmax), historySample);
    } else if (clippingMode == CLIPPING_MODE_ACCURATE) {
        historySample = clip_aabb_accurate(cmin.xyz, cmax.xyz, clamp(cavg, cmin, cmax), historySample);
    } else { // Simple clamp
        historySample = clamp(historySample, cmin, cmax);
    }


//    float sourceLuminance = calcLuminance(sourceSample);
//    float historyLuminance = calcLuminance(historySample);
//
//    float unbiased_diff = abs(sourceLuminance - historyLuminance) / max(sourceLuminance, max(historyLuminance, 0.2));
//    float unbiased_weight = 1.0 - unbiased_diff;
//    float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
//    float k_feedback = lerp(_FeedbackMin, _FeedbackMax, unbiased_weight_sqr);

    return mix(historySample, sourceSample, taaHistoryFadeFactor);
}

void main() {
    vec3 finalColour;
    if (taaEnabled) {
        vec2 unjitteredTexCoord = fs_texture - taaCurrentJitterOffset;
        vec3 closestFragment = findClosestFragment3x3(unjitteredTexCoord);

        if (closestFragment.z < 1.0) {

            vec2 velocity = sampleVelocity(closestFragment.xy);
            finalColour = calculateTemporalAntiAliasing(fs_texture, velocity);
        } else {
            finalColour = texture(frameTexture, fs_texture).rgb;
        }
    } else {
        finalColour = texture(frameTexture, fs_texture).rgb;
    }

    outColor = vec4(finalColour, 1.0);
//    outColor = vec4(abs(velocity) * 100.0, 0.0, 1.0);
//    outColor = vec4(vec3(texture(depthTexture, fs_texture).r), 1.0);
}
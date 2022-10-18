#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "res/shaders/common/common.glsl"
#include "res/shaders/common/pbr.glsl"


#define LIGHT_TYPE_IINVALID 0
#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_POINT 2
#define LIGHT_TYPE_SPOT 3
#define LIGHT_TYPE_AREA 4

#define LIGHT_FLAG_CSM_USE_MAP_BASED_SELECTION uint(1 << 0)

struct SurfacePoint {
    bool exists;
    float depth;
    vec3 viewPosition;
    vec3 albedo;
    float roughness;
    vec3 viewNormal;
    vec3 worldNormal;
    float metallic;
    float ambientOcclusion;
    vec3 emission;
    vec2 velocity;
    vec3 viewDirToCamera;
    vec3 worldDirToCamera;
    float distToCamera;
    vec3 F0;
};

struct DirectionalShadowMap {
    mat4 invViewProjectionMatrix;
    uint shadowMapIndex;
};

struct PointLight {
    vec3 position;
    vec3 intensity;
};

struct LightInfo {
    vec4 worldPosition;
    vec4 worldDirection;
    vec4 intensity;
    vec4 _pad1;
    uint shadowMapIndex;
    uint shadowMapCount;
    uint type;
    uint flags;
};

struct ShadowMapInfo {
    mat4 viewProjectionMatrix;
    float cascadeStartZ;
    float cascadeEndZ;
    float _pad0;
    float _pad1;
};


layout(location = 0) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO1 {
    mat4 viewMatrix;
    mat4 projectionMatrix;
    mat4 viewProjectionMatrix;
    mat4 invViewMatrix;
    mat4 invProjectionMatrix;
    mat4 invViewProjectionMatrix;
    mat4 cameraRays;
    bool showDebugShadowCascades;
    uint debugShadowCascadeLightIndex;
    float debugShadowCascadeOpacity;
    float debugTestFactor;
};

const float MAX_REFLECTION_LOD = 4.0;

layout(set = 0, binding = 1) uniform sampler2D texture_AlbedoRGB_Roughness;
layout(set = 0, binding = 2) uniform sampler2D texture_NormalXYZ_Metallic;
layout(set = 0, binding = 3) uniform sampler2D texture_EmissionRGB_AO;
layout(set = 0, binding = 4) uniform sampler2D texture_VelocityXY;
layout(set = 0, binding = 5) uniform sampler2D texture_Depth;
layout(set = 0, binding = 6) uniform sampler2D previousFrameTexture;
layout(set = 0, binding = 7) uniform samplerCube environmentCubeMap;
layout(set = 0, binding = 8) uniform samplerCube specularReflectionCubeMap;
layout(set = 0, binding = 9) uniform samplerCube diffuseIrradianceCubeMap;
layout(set = 0, binding = 10) uniform sampler2D BRDFIntegrationMap;


layout(set = 1, binding = 0) uniform UBO2 {
    uint lightCount;
};

layout(set = 1, binding = 1) readonly buffer LightBuffer {
    LightInfo lights[];
};

layout(set = 1, binding = 2) readonly buffer ShadowMapBuffer {
    ShadowMapInfo shadowMaps[];
};

layout(set = 1, binding = 3) uniform sampler2D shadowDepthTextures[];




vec3 depthToViewSpacePosition(in float depth, in vec2 coord, in mat4 invProjectionMatrix) {
    vec4 point;
    point.xy = coord * 2.0 - 1.0;
    point.z = depth; // For OpenGL this needs to be depth * 2.0 - 1.0
    point.w = 1.0;
    point = invProjectionMatrix * point;
    point.y *= -1.0;
    return point.xyz / point.w;
}

void loadSurface(in vec2 textureCoord, inout SurfacePoint surface) {
    vec4 texelValue;
    
    surface.depth = texture(texture_Depth, fs_texture).r;
    if (surface.depth >= 1.0) {
        surface.exists = false;
        return;
    } else {
        surface.exists = true;
    }

    mat3 invViewRotationMatrix = mat3(invViewMatrix);

    surface.viewPosition = depthToViewSpacePosition(surface.depth, textureCoord, invProjectionMatrix);
    surface.viewDirToCamera = -1.0 * surface.viewPosition; // Position is in view-space, thus camera is at origin, thus viewDirToCamera is the negative viewPosition
    surface.distToCamera = length(surface.viewDirToCamera);
    surface.viewDirToCamera /= surface.distToCamera;

    surface.worldDirToCamera = invViewRotationMatrix * surface.viewDirToCamera;

    texelValue = texture(texture_AlbedoRGB_Roughness, fs_texture);
    surface.albedo = texelValue.rgb;
    surface.roughness = texelValue.w;
    texelValue = texture(texture_NormalXYZ_Metallic, fs_texture);
    surface.viewNormal = normalize(texelValue.xyz);
    surface.worldNormal = invViewRotationMatrix * surface.viewNormal;
    surface.metallic = texelValue.w;
    texelValue = texture(texture_EmissionRGB_AO, fs_texture);
    surface.emission = texelValue.rgb * 255.0;
    surface.ambientOcclusion = texelValue.a;
    texelValue = texture(texture_VelocityXY, fs_texture);
    surface.velocity = texelValue.xy;

    surface.F0 = mix(vec3(0.04), surface.albedo, surface.metallic);
}

float linstep(in float vMin, in float vMax, in float v) {
    return clamp((v - vMin) / (vMax - vMin), 0.0, 1.0);
}

float sampleShadowVSM(in sampler2D shadowMap, in vec2 coord, in float compareDepth) {
    vec3 vsmTexel = texture(shadowMap, coord).xyz;

    float M1 = vsmTexel[0];
    float M2 = vsmTexel[1];

    float p = step(compareDepth, M1);
    float variance = max(M2 - M1 * M1, 1e-6);
    float diff = compareDepth - M1;
    float pMax = variance / (variance + diff * diff);
    pMax = linstep(0.5, 1.0, pMax);
    float shadow = min(max(p, pMax), 1.0);
    shadow = smoothstep(0.1, 0.9, shadow);
    return shadow;
}

vec3 calculateShadowMapCoord(in SurfacePoint surface, in uint shadowMapIndex) {
    mat4 lightViewProjectionMatrix = shadowMaps[shadowMapIndex].viewProjectionMatrix;
    // TODO: define a common origin CPU-side, and a matrix/inverse-matrix to move to/from that origin. The origin will always be near the camera to avoid precision errors in planet-scale scenes.
    vec4 lightProjectedPosition = lightViewProjectionMatrix * invViewMatrix * vec4(surface.viewPosition, 1.0); // This conversion to world-space will loose precision in large scenes
    lightProjectedPosition.xyz /= lightProjectedPosition.w;
    lightProjectedPosition.xy = lightProjectedPosition.xy * 0.5 + 0.5;
    return lightProjectedPosition.xyz;
}

uint calculateShadowCascade(in SurfacePoint surface, in LightInfo lightInfo, inout vec3 shadowMapCoord) {
    uint cascadeIndex = lightInfo.shadowMapCount;

    if (bool(lightInfo.flags & LIGHT_FLAG_CSM_USE_MAP_BASED_SELECTION)) {
        // Select the highest resolution shadow map that this pixel lands on (slightly more expensive)
        for (uint i = 0; i < lightInfo.shadowMapCount; ++i) {
            shadowMapCoord = calculateShadowMapCoord(surface, lightInfo.shadowMapIndex + i);
            if (shadowMapCoord.x >= 0 && shadowMapCoord.x <= 1 && shadowMapCoord.y >= 0 && shadowMapCoord.y <= 1) {
                cascadeIndex = i;
                break;
            }
        }
    } else {
        // Directly select the shadow map corresponding to the depth of this pixel
        float viewZ = -surface.viewPosition.z;

        for (uint i = 0; i < lightInfo.shadowMapCount; ++i) {
            if (viewZ >= shadowMaps[lightInfo.shadowMapIndex + i].cascadeStartZ && viewZ <= shadowMaps[lightInfo.shadowMapIndex + i].cascadeEndZ) {
                cascadeIndex = i;
                break;
            }
        }
        shadowMapCoord = calculateShadowMapCoord(surface, lightInfo.shadowMapIndex + cascadeIndex);
    }

    // TODO: blend between cascade edges
    return cascadeIndex;
}


float calculateShadow(in SurfacePoint surface, in LightInfo lightInfo) {
    if (lightInfo.shadowMapCount == 0)
        return 1.0; // No shadow map, fully lit.

    float NdotL = dot(surface.worldNormal, -1.0 * vec3(lightInfo.worldDirection));
    if (NdotL <= 1e-5)
        return 0.0; // Inverted surface, fully shadowed.

    vec3 shadowMapCoord;
    uint cascadeIndex = calculateShadowCascade(surface, lightInfo, shadowMapCoord);

    if (cascadeIndex >= lightInfo.shadowMapCount)
        return 1.0; // No shadow map, fully lit.

    float currentDepth = shadowMapCoord.z;
    float shadow = sampleShadowVSM(shadowDepthTextures[lightInfo.shadowMapIndex + cascadeIndex], shadowMapCoord.xy, currentDepth);
    
    // float closestDepth = texture(shadowDepthTextures[lightInfo.shadowMapIndex], shadowMapCoord.xy).r;
    // float shadow = currentDepth > closestDepth ? 0.0 : 1.0;

    return shadow * NdotL;
}

vec3 calculateOutgoingRadiance(in SurfacePoint surface, in vec3 incomingRadiance, in vec3 viewDirToLight) {
    vec3 N = surface.viewNormal;
    vec3 V = surface.viewDirToCamera;
    vec3 L = viewDirToLight;
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    float NDF = distributionGGX(NdotH, surface.roughness);
    float k = geometryRoughnessDirect(surface.roughness);
    float G = geometrySmith(NdotV, NdotL, k);
    vec3 F = fresnelSchlick(HdotV, surface.F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - surface.metallic;	 

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = numerator / denominator;  

    return (kD * surface.albedo / PI + specular) * incomingRadiance * NdotL;
}

vec3 calculatePointLight(in SurfacePoint surface, in LightInfo light) {
    vec3 lightViewPosition = vec3(viewMatrix * light.worldPosition);
    vec3 L = lightViewPosition - surface.viewPosition;
    float distToLight = length(L);
    L /= distToLight;

    float attenuation = 1.0 / (distToLight * distToLight);
    vec3 lightRadiance = light.intensity.rgb * attenuation;

    return calculateOutgoingRadiance(surface, lightRadiance, L);
}

vec3 calculateDirectionalLight(in SurfacePoint surface, in LightInfo light) {
    vec3 L = -1.0 * vec3(viewMatrix * light.worldDirection);

    float shadow = calculateShadow(surface, light);
    vec3 lightRadiance = light.intensity.rgb * shadow;

    return calculateOutgoingRadiance(surface, lightRadiance, L);
}

void main() {
    SurfacePoint surface;
    loadSurface(fs_texture, surface);

    vec3 finalColour = vec3(0.0);

    if (!surface.exists) {
        finalColour = textureLod(environmentCubeMap, getViewRay(fs_texture, cameraRays), 0.5).rgb;

    } else {
        vec3 cameraPos = vec3(0.0); // camera at origin

        vec3 Lo = surface.emission;

        for (uint i = 0; i < lightCount; ++i) {
            switch (lights[i].type) {
            case LIGHT_TYPE_DIRECTIONAL:
                Lo += surface.albedo * calculateDirectionalLight(surface, lights[i]);
                break;
            case LIGHT_TYPE_POINT:
                Lo += surface.albedo * calculatePointLight(surface, lights[i]);
                break;
            }
        }

        vec3 R = reflect(-surface.worldDirToCamera, surface.worldNormal);
        float NdotV = max(dot(surface.viewNormal, surface.viewDirToCamera), 0.0);

        vec3 kS = fresnelSchlickRoughness(NdotV, surface.F0, surface.roughness); 
        vec3 kD = (1.0 - kS) * (1.0 - surface.metallic);
        
        vec3 irradiance = texture(diffuseIrradianceCubeMap, surface.worldNormal).rgb;
        vec3 diffuse = irradiance * surface.albedo;
        vec3 prefilteredColor = textureLod(specularReflectionCubeMap, R, surface.roughness * MAX_REFLECTION_LOD).rgb;   
        vec2 integratedBRDF = texture(BRDFIntegrationMap, vec2(NdotV, surface.roughness)).xy;
        vec3 specular = prefilteredColor * (kS * integratedBRDF[0] + integratedBRDF[1]);

        //const float shadowIntensity = 0.95;
        //float shadow = 1.0;//calculateShadow(surface, lights[0]) * shadowIntensity + (1.0 - shadowIntensity);

        vec3 ambient = (kD * diffuse + specular) * surface.ambientOcclusion;// * shadow;

         finalColour = ambient + Lo;
    }


    finalColour = finalColour / (finalColour + vec3(1.0));
    finalColour = pow(finalColour, vec3(1.0 / 2.2));  

    // finalColour = vec3(calculateShadow(surface, lights[0]));

    // vec2 testPos = vec2(fs_texture.x, 1.0 - fs_texture.y);
    // vec2 testOffset = vec2(0.05);
    // vec2 testSize = vec2(0.25);
    // if (testPos.x >= testOffset.x && testPos.x < testSize.x && testPos.y >= testOffset.y && testPos.y < testSize.y) {
    //     testPos = vec2(linstep(testOffset.x, testOffset.x + testSize.x, testPos.x), linstep(testOffset.y, testOffset.y + testSize.y, testPos.y));
    //     finalColour = vec3(linstep(0.43, 0.56, texture(shadowDepthTextures[0], testPos).r));
    // }

    // finalColour = vec3(surface.emission / 32.0);

    if (showDebugShadowCascades) {
        if (surface.exists) {
            vec3 shadowMapCoord;
            uint cascadeIndex = calculateShadowCascade(surface, lights[debugShadowCascadeLightIndex], shadowMapCoord);
            const vec3 cascadeDebugColours[6] = vec3[6](vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 1.0), vec3(0.0, 1.0, 0.0), vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 1.0), vec3(1.0, 0.0, 0.0));
            vec3 debugColour = cascadeIndex < lights[debugShadowCascadeLightIndex].shadowMapCount ? cascadeDebugColours[cascadeIndex % 6] : vec3(0.0);
            finalColour = mix(finalColour, debugColour, debugShadowCascadeOpacity);
        }
    }

//    finalColour = vec3(isnan(surface.worldNormal[0]) ? vec3(1.0) : (surface.worldNormal * 0.5 + 0.5));
//    finalColour = vec3(surface.albedo);
//    finalColour = vec3(abs(surface.velocity), 0.0);

    vec3 prevFinalColour = texture(previousFrameTexture, fs_texture).rgb;
    finalColour = mix(prevFinalColour, finalColour, debugTestFactor);

    outColor = vec4(finalColour, 1.0);

}
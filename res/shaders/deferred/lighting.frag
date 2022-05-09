#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "res/shaders/common/common.glsl"
#include "res/shaders/common/pbr.glsl"

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
};

const float MAX_REFLECTION_LOD = 4.0;

layout(set = 0, binding = 1) uniform sampler2D texture_AlbedoRGB_Roughness;
layout(set = 0, binding = 2) uniform sampler2D texture_NormalXYZ_Metallic;
layout(set = 0, binding = 3) uniform sampler2D texture_Depth;
layout(set = 0, binding = 4) uniform samplerCube environmentCubeMap;
layout(set = 0, binding = 5) uniform samplerCube specularReflectionCubeMap;
layout(set = 0, binding = 6) uniform samplerCube diffuseIrradianceCubeMap;
layout(set = 0, binding = 7) uniform sampler2D BRDFIntegrationMap;


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
    vec3 viewDirToCamera;
    vec3 worldDirToCamera;
    float distToCamera;
    vec3 F0;
};

struct PointLight {
    vec3 position;
    vec3 intensity;
};


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
    surface.ambientOcclusion = 1.0;
    surface.emission = vec3(0.0);

    surface.F0 = mix(vec3(0.04), surface.albedo, surface.metallic);
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

vec3 calculatePointLight(in SurfacePoint surface, in PointLight light) {
    vec3 L = light.position - surface.viewPosition;
    float distToLight = length(L);
    L /= distToLight;

    float attenuation = 1.0 / (distToLight * distToLight);
    vec3 lightRadiance = light.intensity * attenuation;

    return calculateOutgoingRadiance(surface, lightRadiance, L);
}

void main() {
    SurfacePoint surface;
    loadSurface(fs_texture, surface);

    vec3 finalColour = vec3(0.0);

    if (!surface.exists) {
        // outColor = vec4(0.0, 0.0, 0.0, 1.0);
        finalColour = textureLod(environmentCubeMap, getViewRay(fs_texture, cameraRays), 0.5).rgb;

    } else {
        vec3 cameraPos = vec3(0.0); // camera at origin

        const uint numPointLights = 4;
        PointLight[4] pointLights;
        pointLights[0].position = vec3(viewMatrix * vec4(vec3(3.0, 0.8, -1.0), 1.0));
        pointLights[0].intensity = vec3(32.0, 8.0, 0.0);
        pointLights[1].position = vec3(viewMatrix * vec4(vec3(0.4, 1.3, 2.0), 1.0));
        pointLights[1].intensity = vec3(32.0, 32.0, 32.0);
        pointLights[2].position = vec3(viewMatrix * vec4(vec3(-2.0, 1.1, -1.2), 1.0));
        pointLights[2].intensity = vec3(0.8, 6.4, 32.0);
        pointLights[3].position = vec3(viewMatrix * vec4(vec3(-2.1, 1.1, 2.3), 1.0));
        pointLights[3].intensity = vec3(0.8, 32.0, 6.4);

        vec3 Lo = vec3(0.0);

        for (uint i = 0; i < numPointLights; ++i) {
            Lo += surface.albedo * calculatePointLight(surface, pointLights[i]);
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

        vec3 ambient = (kD * diffuse + specular) * surface.ambientOcclusion; 

        finalColour = ambient + Lo;
    }

    // finalColour = texture(BRDFIntegrationMap, vec2(fs_texture.x, 1.0 - fs_texture.y)).rgb;

    finalColour = finalColour / (finalColour + vec3(1.0));
    finalColour = pow(finalColour, vec3(1.0 / 2.2));  

    outColor = vec4(finalColour, 1.0);



    // outColor = vec4(texture(diffuseIrradianceCubeMap, getViewRay(fs_texture, cameraRays)).rgb, 1.0);




    // vec3 ray = getViewRay(fs_texture, cameraRays);
    // if (abs(ray.x) > abs(ray.y)) {
    //     if (abs(ray.x) > abs(ray.z))
    //         ray = vec3(sign(ray.x), 0, 0);
    //     else
    //         ray = vec3(0, 0, sign(ray.z));
    // } else {
    //     if (abs(ray.y) > abs(ray.z))
    //         ray = vec3(0, sign(ray.y), 0);
    //     else
    //         ray = vec3(0, 0, sign(ray.z));
    // }

    // outColor = vec4(ray * 0.5 + 0.5, 1.0);
}
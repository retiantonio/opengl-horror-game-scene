#version 410 core
in vec3 fPosition;
in vec3 fNormal;
in vec2 fTexCoords;
in vec4 fPosLightSpace[5]; //mod inainte era fara [5]

out vec4 fColor;

//matrices
uniform mat4 model;
uniform mat4 view;
uniform mat3 normalMatrix;

//lighting
uniform vec3 lightDir;
uniform vec3 lightColor;

// textures
uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;
//uniform sampler2D shadowMap;

uniform sampler2DArray shadowMapArray;

//flashlight uniforms
uniform vec3 spotLightPos;          
uniform vec3 spotLightDir;         
uniform vec3 spotLightColor;
uniform float spotLightCutOff;     
uniform float spotLightOuterCutOff;
uniform float spotLightConstant;
uniform float spotLightLinear;
uniform float spotLightQuadratic;

//candles
#define MAX_POINT_LIGHTS 8

struct PointLight {
    vec3 position;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
};

uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int numPointLights;

//components
vec3 ambient;
float ambientStrength = 0.00f;
vec3 diffuse;
vec3 specular;
float specularStrength = 0.3f;
int shininess = 16;

vec3 computePointLight(PointLight light) {
    vec3 normalEye = normalize(fNormal);
    
    vec3 lightPosEye = (view * vec4(light.position, 1.0)).xyz;
    
    vec3 lightDir = normalize(lightPosEye - fPosition);
    
    float diff = max(dot(normalEye, lightDir), 0.0);
    vec3 diffuse = light.color * diff;
    
    vec3 viewDir = normalize(-fPosition);
    vec3 halfVector = normalize(lightDir + viewDir);
    float spec = 0.0;
    if (diff > 0.0) {
        spec = pow(max(dot(normalEye, halfVector), 0.0), shininess);
    }
    vec3 specular = light.color * spec * specularStrength;
    
    float distance = length(lightPosEye - fPosition);
    float attenuation = 1.0 / (light.constant + light.linear * distance + 
                               light.quadratic * (distance * distance));
    
    diffuse *= attenuation;
    specular *= attenuation;
    
    return diffuse + specular;
}

float computeShadow(int layer, vec3 lightDirForBias) {
    vec3 projCoords = fPosLightSpace[layer].xyz / fPosLightSpace[layer].w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if(projCoords.z > 1.0) return 0.0;
    
    float currentDepth = projCoords.z;
    
    float bias = max(0.005 * (1.0 - dot(normalize(fNormal), lightDirForBias)), 0.0005);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMapArray, 0).xy;
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMapArray, vec3(projCoords.xy + vec2(x, y) * texelSize, layer)).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

vec3 getEyeDir(vec3 worldDir) {
    return normalize((view * vec4(worldDir, 0.0)).xyz);
}

void computeDirLight() {
    vec3 normalEye = normalize(fNormal);
    
    vec3 lightDirEye = (view * vec4(-lightDir, 0.0)).xyz;
    vec3 lightDirN = normalize(lightDirEye);
    
    vec3 viewDirN = normalize(-fPosition);
    
    vec3 halfVector = normalize(lightDirN + viewDirN);
    
    ambient = ambientStrength * lightColor;
    
    diffuse = max(dot(normalEye, lightDirN), 0.0f) * lightColor;
    
    if (dot(normalEye, lightDirN) > 0.0) {
        float specCoeff = pow(max(dot(normalEye, halfVector), 0.0f), shininess);
        specular = specularStrength * specCoeff * lightColor;
    } else {
        specular = vec3(0.0);
    }
}

vec3 computeSpotLight() {
    vec3 normalEye = normalize(fNormal);
    
    vec3 spotPosEye = (view * vec4(spotLightPos, 1.0)).xyz;
    vec3 spotDirEye = normalize((view * vec4(spotLightDir, 0.0)).xyz);
    
    vec3 lightDirToFrag = normalize(spotPosEye - fPosition);
    
    float theta = dot(lightDirToFrag, normalize(-spotDirEye));
    float epsilon = spotLightCutOff - spotLightOuterCutOff;
    float intensity = clamp((theta - spotLightOuterCutOff) / epsilon, 0.0, 1.0);
    
    float spotDiff = max(dot(normalEye, lightDirToFrag), 0.0);
    
    vec3 viewDirN = normalize(-fPosition);
    vec3 halfVector = normalize(lightDirToFrag + viewDirN);
    float spotSpec = 0.0;
    if (spotDiff > 0.0) {
        spotSpec = pow(max(dot(normalEye, halfVector), 0.0), shininess);
    }
    
    float distance = length(spotPosEye - fPosition);
    float attenuation = 1.0 / (spotLightConstant + spotLightLinear * distance + 
                               spotLightQuadratic * (distance * distance));
    
    // Combine diffuse and specular with intensity and attenuation
    vec3 spotDiffuse = spotLightColor * spotDiff * intensity * attenuation;
    vec3 spotSpecular = spotLightColor * spotSpec * specularStrength * intensity * attenuation;
    
    return spotDiffuse + spotSpecular;
}

void main() {
    computeDirLight();
    float moonShadow = computeShadow(0, getEyeDir(-lightDir));
    
    vec3 spotLightContrib = computeSpotLight();
    float flashShadow = computeShadow(1, getEyeDir(spotLightDir));

    vec3 pointLightContrib = vec3(0.0);
    for(int i = 0; i < numPointLights && i < MAX_POINT_LIGHTS; i++) {
        vec3 pLight = computePointLight(pointLights[i]);
        float pShadow = 0.0;
    
        if (i < 3) {
            vec3 dirToCandle = normalize(pointLights[i].position - fPosition);
            pShadow = computeShadow(i + 2, dirToCandle);
        }
        pointLightContrib += pLight * (1.0 - pShadow);
    }

    vec4 texDiff = texture(diffuseTexture, fTexCoords);
    vec4 texSpec = texture(specularTexture, fTexCoords);
    
    vec3 color = (ambient * texDiff.rgb); // Ambient is always there
    color += (diffuse * texDiff.rgb + specular * texSpec.rgb);
    color += (1.0 - flashShadow) * spotLightContrib * texDiff.rgb;
    color += pointLightContrib * texDiff.rgb;

    fColor = vec4(color, 1.0f);
}

void main2() {
    vec3 projCoords = fPosLightSpace[1].xyz / fPosLightSpace[1].w;
    projCoords = projCoords * 0.5 + 0.5;
    float depthValue = texture(shadowMapArray, vec3(projCoords.xy, 1)).r;
    
    fColor = vec4(vec3(depthValue), 1.0); 
    return;
}

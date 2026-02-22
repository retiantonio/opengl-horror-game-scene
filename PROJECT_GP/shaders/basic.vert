#version 410 core
layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec2 vTexCoords;

out vec3 fPosition;
out vec3 fNormal;
out vec2 fTexCoords;
//out vec4 fPosLightSpace;

out vec4 fPosLightSpace[5];

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
uniform mat4 lightSpaceMatrix;

uniform mat4 lightSpaceMatrices[5];

void main() {
    vec4 worldPos = model * vec4(vPosition, 1.0f);
    vec4 fragPosEye = view * worldPos;
    
    fPosition = fragPosEye.xyz;
    
    fNormal = normalize(normalMatrix * vNormal);
    fTexCoords = vTexCoords;

    for(int i = 0; i < 5; i++) {
        fPosLightSpace[i] = lightSpaceMatrices[i] * worldPos;
    }   

    gl_Position = projection * fragPosEye;
}
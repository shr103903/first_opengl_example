#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 3) in ivec4 boneIds;
layout (location = 4) in vec4 weights;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[100];

void main()
{
    vec4 totalPosition = vec4(0.0);
    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
        if (boneIds[i] == -1) continue; // 유효하지 않은 본 ID 무시
        totalPosition += finalBonesMatrices[boneIds[i]] * vec4(aPos, 1.0) * weights[i];
    }
    
    gl_Position = lightSpaceMatrix * totalPosition; // 광원 공간에서의 위치
    // gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
} 
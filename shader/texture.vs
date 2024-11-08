#version 330 core

uniform mat4 worldMat, viewMat, projMat;
uniform vec3 eyePos;
uniform mat4 transform;

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

out vec3 v_normal, v_view;
out vec2 v_texCoord;
out vec3 v_worldPos;

void main() {
    v_normal = normalize(transpose(inverse(mat3(worldMat))) * normal);
    vec3 worldPos = vec3(worldMat * vec4(position, 1.0));
    v_view = normalize(eyePos - worldPos);
    v_texCoord = texCoord;
    // gl_Position = projMat * viewMat * vec4(worldPos, 1.0);
    gl_Position = transform * vec4(worldPos, 1.0);
    v_worldPos = worldPos;
}
#version 330 core

// precision mediump float;

uniform sampler2D diffuse_tex;
uniform sampler2D specular_tex;
uniform vec3 matSpecular, matAmbient, matEmit;
uniform float matShininess;
uniform vec3 srcDiffuse, srcSpecular, srcAmbient;
uniform vec3 lightDir;
uniform vec3 lightPos;

in vec3 v_normal, v_view;
in vec2 v_texCoord;
in vec3 v_worldPos;

layout(location = 0) out vec4 fragColor;

void main() {
    vec3 normal = normalize(v_normal);
    vec3 viewDir = normalize(v_view);
    vec3 light = normalize(lightDir);

    // specular
    // vec3 reflectDir = 2.0 * dot(light, normal) * normal - light;
    vec3 reflectDir = reflect(-light, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), matShininess);
    vec3 matSpecularColor = texture(specular_tex, v_texCoord).rgb;
    vec3 specular = spec * srcSpecular  * matSpecular;

    // diffuse
    float diff = max(dot(normal, light), 0.0); // 라이트 방향이 다르면 완전 0이 됨
    //vec3 matDiffuse = mix(texture(diffuse_tex, v_texCoord), texture(specular_tex, v_texCoord), 0.5).rgb;
    vec3 matDiffuse = texture(diffuse_tex, v_texCoord).rgb;
    vec3 diffuse = diff * srcDiffuse *  matDiffuse;

    // ambient
    vec3 ambient = srcAmbient * matAmbient; // 잘 전달됨    

    vec3 result = ambient + diffuse + specular + matEmit; 
    fragColor = vec4(result, 1.0);
}
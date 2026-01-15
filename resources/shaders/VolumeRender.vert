#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vLocalPos; // 输出局部坐标 (0~1) 用于采样纹理
out vec3 vWorldPos;

void main()
{
    vLocalPos = aPos; // 假设传入的立方体顶点在 0~1 之间
    vec4 worldPos = model * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    gl_Position = projection * view * worldPos;
}
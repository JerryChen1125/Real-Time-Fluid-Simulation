#version 330 core
out vec4 FragColor;

in vec3 vLocalPos;
in vec3 vWorldPos;

uniform sampler3D densityTex; // 3D 纹理
uniform vec3 cameraPos;
uniform float stepSize;
uniform float contrast;

void main()
{
    // 1. 计算射线方向
    vec3 rayDir = normalize(vWorldPos - cameraPos);
    
    // 2. Ray Marching 起点
    // 简单起见，我们从物体表面开始步进 (vLocalPos 已经在 0~1 范围内)
    vec3 pos = vLocalPos;
    
    // 累积颜色和不透明度
    vec4 accumulatedColor = vec4(0.0);
    float rayLength = 0.0;
    
    // 3. 循环步进
    // MAX_STEPS 决定渲染质量和性能
    for(int i = 0; i < 128; i++) 
    {
        // 采样密度
        float density = texture(densityTex, pos).r * contrast;
        
        if(density > 0.01) // 只有有烟雾的地方才计算
        {
            vec4 srcColor = vec4(1.0, 1.0, 1.0, density); // 白色烟雾
            // 简单的 Alpha Blending (Front-to-Back)
            srcColor.rgb *= srcColor.a;
            accumulatedColor = accumulatedColor + srcColor * (1.0 - accumulatedColor.a);
            
            if(accumulatedColor.a > 0.95) break; // 提前退出
        }
        
        // 前进
        pos += rayDir * stepSize;
        
        // 边界检测：如果跑出 [0,1] 范围则停止
        if(pos.x < 0.0 || pos.x > 1.0 || 
           pos.y < 0.0 || pos.y > 1.0 || 
           pos.z < 0.0 || pos.z > 1.0) break;
    }
    
    FragColor = accumulatedColor;
}
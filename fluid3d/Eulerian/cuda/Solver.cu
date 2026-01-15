#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <math_functions.h>

// =========================================================
// 基础数学运算符重载
// =========================================================
inline __host__ __device__ float3 operator+(float3 a, float3 b) { return make_float3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline __host__ __device__ float3 operator-(float3 a, float3 b) { return make_float3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline __host__ __device__ float3 operator*(float3 a, float s)  { return make_float3(a.x * s, a.y * s, a.z * s); }
inline __host__ __device__ float3 operator*(float s, float3 a)  { return make_float3(a.x * s, a.y * s, a.z * s); }

// =========================================================
// helper function：速度场三线性插值
// =========================================================
__device__ float3 sample_velocity_trilinear(float3* vel, float3 pos, int3 dim) 
{
    // 1. 边界钳制 (Clamp) - 防止越界读取
    // 假设 Grid 范围是 [0, dim], Cell 中心在 x.5
    float x = fmaxf(0.5f, fminf(pos.x, dim.x - 0.5f));
    float y = fmaxf(0.5f, fminf(pos.y, dim.y - 0.5f));
    float z = fmaxf(0.5f, fminf(pos.z, dim.z - 0.5f));

    // 2. 计算基准索引 (0-based)
    // u, v, w 是网格内的局部偏移量 [0, 1)
    float u = x - 0.5f; float v = y - 0.5f; float w = z - 0.5f;
    int x0 = (int)u; int y0 = (int)v; int z0 = (int)w;
    int x1 = min(x0 + 1, dim.x - 1);
    int y1 = min(y0 + 1, dim.y - 1);
    int z1 = min(z0 + 1, dim.z - 1);

    float tx = u - x0; float ty = v - y0; float tz = w - z0;

    // Lambda: 计算 1D 索引
    auto idx = [&](int i, int j, int k) { return i + j * dim.x + k * dim.x * dim.y; };

    // 3. 采样 8 个邻居
    float3 c000 = vel[idx(x0, y0, z0)]; float3 c100 = vel[idx(x1, y0, z0)];
    float3 c010 = vel[idx(x0, y1, z0)]; float3 c110 = vel[idx(x1, y1, z0)];
    float3 c001 = vel[idx(x0, y0, z1)]; float3 c101 = vel[idx(x1, y0, z1)];
    float3 c011 = vel[idx(x0, y1, z1)]; float3 c111 = vel[idx(x1, y1, z1)];

    // 4. 三线性插值
    float3 lerpX00 = (1.0f - tx) * c000 + tx * c100;
    float3 lerpX10 = (1.0f - tx) * c010 + tx * c110;
    float3 lerpX01 = (1.0f - tx) * c001 + tx * c101;
    float3 lerpX11 = (1.0f - tx) * c011 + tx * c111;

    float3 lerpY0 = (1.0f - ty) * lerpX00 + ty * lerpX10;
    float3 lerpY1 = (1.0f - ty) * lerpX01 + ty * lerpX11;

    return (1.0f - tz) * lerpY0 + tz * lerpY1;
}

// =========================================================
// 核心 Kernels
// =========================================================

// 半拉格朗日平流 (Semi-Lagrangian)
__global__ void advect_density_kernel(
    cudaSurfaceObject_t outputSurf, cudaTextureObject_t inputTex,
    float3* velocity, float dt, int width, int height, int depth)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;
    if (x >= width || y >= height || z >= depth) return;

    // 坐标转换
    int idx = x + y * width + z * width * height;
    float3 pos = make_float3(x + 0.5f, y + 0.5f, z + 0.5f);

    // 单步回溯
    float3 vel = velocity[idx];
    float3 prevPos = pos - vel * dt;

    // 采样并写入
    float result = tex3D<float>(inputTex, prevPos.x, prevPos.y, prevPos.z);
    result = fmaxf(0.0f, result);
    surf3Dwrite(result, outputSurf, x * sizeof(float), y, z);
}

// BFECC 平流 (Back and Forth Error Compensation and Correction)
// 能够显著减少数值耗散，保持烟雾细节
__global__ void advect_density_BFECC_kernel(
    cudaSurfaceObject_t outputSurf, cudaTextureObject_t inputTex,
    float3* velocity, float dt, int width, int height, int depth)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;
    if (x >= width || y >= height || z >= depth) return;

    int3 dim = make_int3(width, height, depth);
    float3 pos = make_float3(x + 0.5f, y + 0.5f, z + 0.5f);
    int idx = x + y * width + z * width * height;

    // --- BFECC 步骤 ---
    // 1. Backward: 找到前一时刻位置 (Standard Semi-Lagrangian)
    float3 vel1 = velocity[idx]; 
    float3 pos_back = pos - vel1 * dt;

    // 2. Forward: 从前一位置正向追踪回当前时刻
    //    由于 pos_back 是浮点坐标，需要三线性插值采样速度
    float3 vel2 = sample_velocity_trilinear(velocity, pos_back, dim);
    float3 pos_forward = pos_back + vel2 * dt;

    // 3. Correction: 计算误差并补偿
    //    pos_forward 应该等于 pos，偏差即为误差 error
    //    为了抵消这个误差，我们在采样时向相反方向偏移
    float3 error = pos_forward - pos;
    float3 pos_final = pos_back - error * 0.5f;

    // 4. Sample & Write
    float result = tex3D<float>(inputTex, pos_final.x, pos_final.y, pos_final.z);
    result = fmaxf(0.0f, result);
    surf3Dwrite(result, outputSurf, x * sizeof(float), y, z);
}

__global__ void advect_velocity_kernel(
    float3* new_vel, float3* old_vel, 
    float dt, int width, int height, int depth)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;
    if (x >= width || y >= height || z >= depth) return;

    int idx = x + y * width + z * width * height;
    int3 dim = make_int3(width, height, depth);
    float3 pos = make_float3(x + 0.5f, y + 0.5f, z + 0.5f);

    // 获取当前速度
    float3 u = old_vel[idx];

    // 回溯路径
    float3 prevPos = pos - u * dt;

    // 在旧位置插值速度，并写入新位置
    new_vel[idx] = sample_velocity_trilinear(old_vel, prevPos, dim);
}

// Force: Apply Buoyancy
__global__ void apply_buoyancy_kernel(
    float3* velocity, cudaTextureObject_t densityTex, cudaTextureObject_t tempTex,
    float dt, float alpha, float beta, float ambientTemp,
    int width, int height, int depth)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;
    if (x >= width || y >= height || z >= depth) return;

    int idx = x + y * width + z * width * height;

    // 读取密度和温度
    // 注意：Texture 坐标需加 0.5
    float d = tex3D<float>(densityTex, x + 0.5f, y + 0.5f, z + 0.5f);
    float T = tex3D<float>(tempTex,    x + 0.5f, y + 0.5f, z + 0.5f);

    // 公式: F = -alpha * density + beta * (temp - ambientTemp)
    if (d > 0.0001f || fabsf(T - ambientTemp) > 0.0001f) {
        float buoyancy = -alpha * d + beta * (T - ambientTemp);
        velocity[idx].z += buoyancy * dt;
    }
}

// Project A: Compute Divergence
__global__ void compute_divergence_kernel(
    float* divergence, float3* velocity,
    int width, int height, int depth, float halfrdx)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;
    if (x >= width || y >= height || z >= depth) return;

    int idx = x + y * width + z * width * height;

    // 获取邻居速度 (处理边界：如果是边界，假设速度为0或取当前点速度)
    int xl = max(x - 1, 0); int xr = min(x + 1, width - 1);
    int yl = max(y - 1, 0); int yr = min(y + 1, height - 1);
    int zl = max(z - 1, 0); int zr = min(z + 1, depth - 1);

    // 计算中心差分 (Collocated Grid)
    float3 v_xr = velocity[xr + y * width + z * width * height];
    float3 v_xl = velocity[xl + y * width + z * width * height];
    float3 v_yr = velocity[x + yr * width + z * width * height];
    float3 v_yl = velocity[x + yl * width + z * width * height];
    float3 v_zr = velocity[x + y * width + zr * width * height];
    float3 v_zl = velocity[x + y * width + zl * width * height];

    // div = (du/dx + dv/dy + dw/dz)
    float div = (v_xr.x - v_xl.x + v_yr.y - v_yl.y + v_zr.z - v_zl.z) * halfrdx;
    divergence[idx] = div;
}

// Project B: Jacobi Solve
__global__ void jacobi_pressure_kernel(
    float* p_next, float* p_curr, float* divergence,
    int width, int height, int depth)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;
    
    // 排除最外层边界 (Pure Neumann 边界条件简化处理：不更新边界压力)
    if (x < 1 || x >= width - 1 || y < 1 || y >= height - 1 || z < 1 || z >= depth - 1) return;

    int idx = x + y * width + z * width * height;

    // 获取 6 个邻居的压力
    float pl = p_curr[(x - 1) + y * width + z * width * height];
    float pr = p_curr[(x + 1) + y * width + z * width * height];
    float pd = p_curr[x + (y - 1) * width + z * width * height];
    float pu = p_curr[x + (y + 1) * width + z * width * height];
    float pb = p_curr[x + y * width + (z - 1) * width * height];
    float pf = p_curr[x + y * width + (z + 1) * width * height];

    float div = divergence[idx];

    // Jacobi 公式: p_new = (sum(p_neighbors) - divergence) / 6
    p_next[idx] = (pl + pr + pd + pu + pb + pf - div) / 6.0f;
}

// Project C: Subtract Gradient
__global__ void subtract_gradient_kernel(
    float3* velocity, float* pressure,
    int width, int height, int depth, float halfrdx, float airDensity)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;
    if (x < 1 || x >= width - 1 || y < 1 || y >= height - 1 || z < 1 || z >= depth - 1) return;

    int idx = x + y * width + z * width * height;

    // 计算压力梯度 grad(P)
    float pl = pressure[(x - 1) + y * width + z * width * height];
    float pr = pressure[(x + 1) + y * width + z * width * height];
    float pd = pressure[x + (y - 1) * width + z * width * height];
    float pu = pressure[x + (y + 1) * width + z * width * height];
    float pb = pressure[x + y * width + (z - 1) * width * height];
    float pf = pressure[x + y * width + (z + 1) * width * height];

    float3 gradP = make_float3(
        (pr - pl) * halfrdx,
        (pu - pd) * halfrdx,
        (pf - pb) * halfrdx
    );

    // u_new = u_old - (dt / (rho * dx)) * grad(P)
    if (airDensity > 0.0001f) {
        velocity[idx] = velocity[idx] - gradP * (1.0f / airDensity);
    }
}

// 速度场半步反射
// 能够显著减少能量损耗
__global__ void reflect_velocity_kernel(float3* vel_curr, float3* vel_old, int size) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    float3 u_mid = vel_curr[idx];
    float3 u_old = vel_old[idx];
    
    // 反射计算：U_reflect = 2 * U_curr - U_old
    vel_curr[idx] = 2.0f * u_mid - u_old;
}

// 添加源
__global__ void add_source_kernel(cudaSurfaceObject_t outputSurf, int x, int y, int z, float radius, float amount) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;
    int k = blockIdx.z * blockDim.z + threadIdx.z;
    float dist = sqrtf((float)((i-x)*(i-x) + (j-y)*(j-y) + (k-z)*(k-z)));
    if (dist < radius) {
         float oldVal;
         surf3Dread(&oldVal, outputSurf, i * sizeof(float), j, k);
         surf3Dwrite(oldVal + amount, outputSurf, i * sizeof(float), j, k);
    }
}

__global__ void add_source_velocity_kernel(float3* velocity, int x, int y, int z, float radius, float3 amount, int width, int height, int depth) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;
    int k = blockIdx.z * blockDim.z + threadIdx.z;
    if (i >= width || j >= height || k >= depth) return;
    
    // 计算距离
    float dist = sqrtf((float)((i-x)*(i-x) + (j-y)*(j-y) + (k-z)*(k-z)));
    if (dist < radius) {
         int idx = i + j * width + k * width * height;
         // 叠加速度
         velocity[idx] = velocity[idx] + amount; 
    }
}

__global__ void dissipate_kernel(cudaSurfaceObject_t densitySurf, int width, int height, int depth, float dissipationRate) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    int z = blockIdx.z * blockDim.z + threadIdx.z;
    if (x >= width || y >= height || z >= depth) return;

    float val;
    surf3Dread(&val, densitySurf, x * sizeof(float), y, z);
    
    // 衰减公式: val = val / (1 + dt * rate)
    val *= dissipationRate;
    
    surf3Dwrite(val, densitySurf, x * sizeof(float), y, z);
}

// =========================================================
// Wrappers (供 C++ 调用)
// =========================================================

extern "C" void LaunchAdvect(
    cudaSurfaceObject_t targetSurf, cudaTextureObject_t sourceTex, 
    float3* d_velocity, float dt, int w, int h, int d,
    bool useBFECC)
{
    dim3 blockSize(8, 8, 8);
    dim3 gridSize((w + 7) / 8, (h + 7) / 8, (d + 7) / 8);
    
    if (useBFECC) {
        advect_density_BFECC_kernel<<<gridSize, blockSize>>>(targetSurf, sourceTex, d_velocity, dt, w, h, d);
    } else {
        advect_density_kernel<<<gridSize, blockSize>>>(targetSurf, sourceTex, d_velocity, dt, w, h, d);
    }
}

extern "C" void LaunchAdvectVelocity(float3* new_vel, float3* old_vel, float dt, int w, int h, int d) {
    dim3 blockSize(8, 8, 8);
    dim3 gridSize((w + 7) / 8, (h + 7) / 8, (d + 7) / 8);
    advect_velocity_kernel<<<gridSize, blockSize>>>(new_vel, old_vel, dt, w, h, d);
}

extern "C" void LaunchApplyBuoyancy(float3* d_velocity, cudaTextureObject_t densityTex, cudaTextureObject_t tempTex, float dt, float alpha, float beta, float ambientTemp, int w, int h, int d) {
    dim3 blockSize(8, 8, 8);
    dim3 gridSize((w + 7) / 8, (h + 7) / 8, (d + 7) / 8);
    apply_buoyancy_kernel<<<gridSize, blockSize>>>(d_velocity, densityTex, tempTex, dt, alpha, beta, ambientTemp, w, h, d);
}

extern "C" void LaunchComputeDivergence(float* d_div, float3* d_vel, int w, int h, int d, float halfrdx) {
    dim3 blockSize(8, 8, 8);
    dim3 gridSize((w + 7) / 8, (h + 7) / 8, (d + 7) / 8);
    compute_divergence_kernel<<<gridSize, blockSize>>>(d_div, d_vel, w, h, d, halfrdx);
}

extern "C" void LaunchJacobiPressure(float* p_next, float* p_curr, float* d_div, int w, int h, int d) {
    dim3 blockSize(8, 8, 8);
    dim3 gridSize((w + 7) / 8, (h + 7) / 8, (d + 7) / 8);
    jacobi_pressure_kernel<<<gridSize, blockSize>>>(p_next, p_curr, d_div, w, h, d);
}

extern "C" void LaunchSubtractGradient(float3* d_vel, float* d_p, int w, int h, int d, float halfrdx, float airDensity) {
    dim3 blockSize(8, 8, 8);
    dim3 gridSize((w + 7) / 8, (h + 7) / 8, (d + 7) / 8);
    subtract_gradient_kernel<<<gridSize, blockSize>>>(d_vel, d_p, w, h, d, halfrdx, airDensity);
}

extern "C" void LaunchReflectVelocity(float3* d_vel_curr, float3* d_vel_old, int size) {
    int blockSize = 256;
    int numBlocks = (size + 255) / blockSize;
    reflect_velocity_kernel<<<numBlocks, blockSize>>>(d_vel_curr, d_vel_old, size);
}

extern "C" void LaunchAddSource(cudaSurfaceObject_t destSurf, int x, int y, int z, float radius, float amount, int w, int h, int d) {
    dim3 blockSize(8, 8, 8);
    dim3 gridSize((w + 7) / 8, (h + 7) / 8, (d + 7) / 8);
    add_source_kernel<<<gridSize, blockSize>>>(destSurf, x, y, z, radius, amount);
}

extern "C" void LaunchAddSourceVelocity(float3* velocity, int x, int y, int z, float radius, float3 amount, int w, int h, int d) {
    dim3 blockSize(8, 8, 8);
    dim3 gridSize((w + 7) / 8, (h + 7) / 8, (d + 7) / 8);
    add_source_velocity_kernel<<<gridSize, blockSize>>>(velocity, x, y, z, radius, amount, w, h, d);
}

extern "C" void LaunchDissipate(cudaSurfaceObject_t densitySurf, int w, int h, int d, float rate) {
    dim3 blockSize(8, 8, 8);
    dim3 gridSize((w + 7) / 8, (h + 7) / 8, (d + 7) / 8);
    dissipate_kernel<<<gridSize, blockSize>>>(densitySurf, w, h, d, rate);
}
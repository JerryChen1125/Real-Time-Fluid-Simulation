/**
 * Solver.cpp: 3D欧拉流体求解器实现文件
 * 实现流体仿真的核心算法
 */

#include "fluid3d/Eulerian/include/Solver.h"
#include "Configure.h"
#include "Global.h"

// Declare CUDA kernel launchers
extern "C" void LaunchAdvect(cudaSurfaceObject_t targetSurf, cudaTextureObject_t sourceTex, float3* d_velocity, float dt, int w, int h, int d, bool useBFECC);
extern "C" void LaunchAdvectVelocity(float3* new_vel, float3* old_vel, float dt, int w, int h, int d);
extern "C" void LaunchApplyBuoyancy(float3* d_velocity, cudaTextureObject_t densityTex, cudaTextureObject_t tempTex, float dt, float alpha, float beta, float ambientTemp, int w, int h, int d);
extern "C" void LaunchSubtractGradient(float3* d_vel, float* d_p, int w, int h, int d, float halfrdx, float airDensity);
extern "C" void LaunchComputeDivergence(float* d_div, float3* d_vel, int w, int h, int d, float halfrdx);
extern "C" void LaunchJacobiPressure(float* p_next, float* p_curr, float* d_div, int w, int h, int d);
extern "C" void LaunchReflectVelocity(float3* d_vel_curr, float3* d_vel_old, int size);
extern "C" void LaunchAddSource(cudaSurfaceObject_t destSurf, int x, int y, int z, float radius, float amount, int w, int h, int d);
extern "C" void LaunchAddSourceVelocity(float3* velocity, int x, int y, int z, float radius, float3 amount, int w, int h, int d);
extern "C" void LaunchDissipate(cudaSurfaceObject_t densitySurf, int w, int h, int d, float rate);

namespace FluidSimulation
{
    namespace Eulerian3d
    {
        /**
         * 构造函数，初始化求解器并重置网格
         * @param grid MAC网格引用
         */
        Solver::Solver(MACGrid3d &grid) : mGrid(grid)
        {
            // 初始化时重置网格
            mGrid.reset();
        }

        // helper function
        void Solver::solveOneStep(cudaSurfaceObject_t densitySurf, cudaArray* densityArrayGL, cudaSurfaceObject_t tempSurf, cudaArray* tempArrayGL, float dt)
        {
            int w = mGrid.dim[0], h = mGrid.dim[1], d = mGrid.dim[2];

            // 1. Copy: OpenGL -> Temp
            cudaMemcpy3DParms copyParams = { 0 };
            copyParams.extent = make_cudaExtent(w, h, d);
            copyParams.kind = cudaMemcpyDeviceToDevice;
            copyParams.srcArray = densityArrayGL;
            copyParams.dstArray = mGrid.d_densityArrayTemp;
            cudaMemcpy3D(&copyParams);
            copyParams.srcArray = tempArrayGL;
            copyParams.dstArray = mGrid.d_temperatureArrayTemp;
            cudaMemcpy3D(&copyParams);

            cudaMemcpy(mGrid.d_velocity_backup, mGrid.d_velocity, w * h * d * sizeof(float3), cudaMemcpyDeviceToDevice);
            LaunchAdvectVelocity(mGrid.d_velocity, mGrid.d_velocity_backup, dt, w, h, d);

            // 2. Advect
            LaunchAdvect(densitySurf, mGrid.densityTexObjRead, mGrid.d_velocity, dt, w, h, d, Eulerian3dPara::useBFECC);
            LaunchAdvect(tempSurf, mGrid.temperatureTexObjRead, mGrid.d_velocity, dt, w, h, d, Eulerian3dPara::useBFECC);

            // 3. Force
            LaunchApplyBuoyancy(
                mGrid.d_velocity,
                mGrid.densityTexObjRead, // 使用上一帧密度
				mGrid.temperatureTexObjRead, // 使用上一帧温度
                dt,
                Eulerian3dPara::boussinesqAlpha,
                Eulerian3dPara::boussinesqBeta,
                Eulerian3dPara::ambientTemp,
                w, h, d
            );

            // 4. Project
            float scaleDiv = (mGrid.cellSize * Eulerian3dPara::airDensity) / (2.0f * dt);
            LaunchComputeDivergence(mGrid.d_divergence, mGrid.d_velocity, w, h, d, scaleDiv);
            cudaMemset(mGrid.d_pressure, 0, w * h * d * sizeof(float));
            cudaMemset(mGrid.d_pressure_temp, 0, w * h * d * sizeof(float));

            int iterations = 40;
            for (int i = 0; i < iterations; i++) {
                LaunchJacobiPressure(mGrid.d_pressure_temp, mGrid.d_pressure, mGrid.d_divergence, w, h, d);
                std::swap(mGrid.d_pressure, mGrid.d_pressure_temp);
            }

            float halfrdx = 0.5f / mGrid.cellSize;
            float scaleSub = Eulerian3dPara::airDensity / dt;
            LaunchSubtractGradient(
                mGrid.d_velocity,
                mGrid.d_pressure,
                w, h, d,
                halfrdx,
                scaleSub
            );
        }

        /**
         * 求解流体方程
         * 实现一步3D流体仿真的主要步骤
         */
        void Solver::solve()
        {
            // TODO: 实现三维流体求解
            // 主要步骤包括:
            // 1. 平流(advection) - 将物理量沿速度场平流
            // 2. 计算外力(如浮力) - 添加Boussinesq浮力等外力
            // 3. 投影(projection) - 求解压力场，使速度场无散
            // 4. 边界处理 - 处理流体与固体边界的交互
            float dt = Eulerian3dPara::dt;
            int w = mGrid.dim[0], h = mGrid.dim[1], d = mGrid.dim[2];
            int size = w * h * d;

			// Map OpenGL 3D texture to CUDA
            cudaGraphicsMapResources(1, &mGrid.cuda_density_res, 0);
            cudaArray* densityArrayGL;
            cudaGraphicsSubResourceGetMappedArray(&densityArrayGL, mGrid.cuda_density_res, 0, 0);
            cudaGraphicsMapResources(1, &mGrid.cuda_temperature_res, 0);
            cudaArray* tempArrayGL;
            cudaGraphicsSubResourceGetMappedArray(&tempArrayGL, mGrid.cuda_temperature_res, 0, 0);

            // Create Surface Object (用于写入 OpenGL 纹理)
            cudaResourceDesc surfResDesc;
            memset(&surfResDesc, 0, sizeof(surfResDesc));
            surfResDesc.resType = cudaResourceTypeArray;
            surfResDesc.res.array.array = densityArrayGL;
            cudaSurfaceObject_t densitySurf;
            cudaCreateSurfaceObject(&densitySurf, &surfResDesc);
            surfResDesc.res.array.array = tempArrayGL;
            cudaSurfaceObject_t tempSurf;
            cudaCreateSurfaceObject(&tempSurf, &surfResDesc);

            if (Eulerian3dPara::useReflection) {
                if (mGrid.d_velocity_backup) {
                    cudaMemcpy(mGrid.d_velocity_backup, mGrid.d_velocity, size * sizeof(float3), cudaMemcpyDeviceToDevice);
                }
                solveOneStep(densitySurf, densityArrayGL, tempSurf, tempArrayGL, dt * 0.5f);
                if (mGrid.d_velocity_backup) {
                    LaunchReflectVelocity(mGrid.d_velocity, mGrid.d_velocity_backup, size);
                }
                solveOneStep(densitySurf, densityArrayGL, tempSurf, tempArrayGL, dt * 0.5f);
            }
            else {
                solveOneStep(densitySurf, densityArrayGL, tempSurf, tempArrayGL, dt);
            }

            cudaDeviceSynchronize();

			// Add Sources
            for (size_t i = 0; i < Eulerian3dPara::source.size(); i++) {
                auto& src = Eulerian3dPara::source[i];

                if (src.density > 0.001f) {
                    LaunchAddSource(densitySurf, src.position.x, src.position.y, src.position.z, 1.0f, src.density, w, h, d);
					LaunchAddSource(tempSurf, src.position.x, src.position.y, src.position.z, 1.0f, src.temp, w, h, d);
                    float3 velVal = make_float3(src.velocity.x, src.velocity.y, src.velocity.z);
                    LaunchAddSourceVelocity(mGrid.d_velocity, src.position.x, src.position.y, src.position.z, 1.0f, velVal, w, h, d);
                }
            }

			// Dissipate
            LaunchDissipate(densitySurf, w, h, d, 0.99f);

			// Cleanup
            cudaDestroySurfaceObject(densitySurf);
            cudaDestroySurfaceObject(tempSurf);
            cudaGraphicsUnmapResources(1, &mGrid.cuda_density_res, 0);
            cudaGraphicsUnmapResources(1, &mGrid.cuda_temperature_res, 0);
        }
    }
}
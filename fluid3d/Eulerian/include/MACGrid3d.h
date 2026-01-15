/**
 * MACGrid3d.h: 3D MAC网格头文件
 * 定义基于MAC（Marker-And-Cell）格式的3D网格数据结构
 */

#pragma once
#ifndef __EULERIAN_3D_MACGRID_3D_H__
#define __EULERIAN_3D_MACGRID_3D_H__

#include <windows.h>
#include <glm/glm.hpp>
#include "GridData3d.h"
#include <Logger.h>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

namespace FluidSimulation
{
    namespace Eulerian3d
    {
        /**
         * 3D MAC网格类
         * 存储流体的速度、密度、温度等物理量
         * MAC格式：速度分量存储在网格面中心，标量存储在网格单元中心
         */
        class MACGrid3d
        {
            friend MACGrid3d;

        public:
            MACGrid3d();
            ~MACGrid3d();
            MACGrid3d(const MACGrid3d &orig);
            MACGrid3d &operator=(const MACGrid3d &orig);

            void reset();
            void updateSources();

            glm::vec4 getRenderColor(int i, int j, int k);
            glm::vec4 getRenderColor(const glm::vec3 &pt);

            void initialize();
            void createSolids();

            glm::vec3 semiLagrangian(const glm::vec3 &pt, double dt);
            glm::vec3 getVelocity(const glm::vec3 &pt);
            double getVelocityX(const glm::vec3 &pt);
            double getVelocityY(const glm::vec3 &pt);
            double getVelocityZ(const glm::vec3 &pt);
            double getTemperature(const glm::vec3 &pt);
            double getDensity(const glm::vec3 &pt);

            enum Direction
            {
                X,
                Y,
                Z
            };

            glm::vec3 getCenter(int i, int j, int k);
            glm::vec3 getLeft(int i, int j, int k);
            glm::vec3 getRight(int i, int j, int k);
            glm::vec3 getTop(int i, int j, int k);
            glm::vec3 getBottom(int i, int j, int k);
            glm::vec3 getFront(int i, int j, int k);
            glm::vec3 getBack(int i, int j, int k);

            void getCell(int index, int &i, int &j, int &k);
            int getIndex(int i, int j, int k);
            bool isNeighbor(int i0, int j0, int k0, int i1, int j1, int k1);
            bool isValid(int i, int j, int k, Direction d);
            int isSolidCell(int i, int j, int k);
            int isSolidFace(int i, int j, int k, Direction d);
            bool inSolid(const glm::vec3 &pt);
            bool inSolid(const glm::vec3 &pt, int &i, int &j, int &k);
            bool intersects(const glm::vec3 &pt, const glm::vec3 &dir, int i, int j, int k, double &time);
            int numSolidCells();

            double getPressureCoeffBetweenCells(int i0, int j0, int k0, int i1, int j1, int k1);
            double getDivergence(int i, int j, int k);
            double checkDivergence(int i, int j, int k);
            bool checkDivergence();

            double getBoussinesqForce(const glm::vec3 &pt);

            float cellSize;             // 网格单元大小
            int dim[3];                 // 网格维度 [宽, 高, 深]

            void InitCUDA();
            void CleanupCUDA();

        public:
            Glb::GridData3dX mU;        // X方向速度分量
            Glb::GridData3dY mV;        // Y方向速度分量
            Glb::GridData3dZ mW;        // Z方向速度分量
            Glb::CubicGridData3d mD;    // 密度场
            Glb::CubicGridData3d mT;    // 温度场
            Glb::GridData3d mSolid;     // 固体标记（1表示固体，0表示流体）

            // 密度场 (用于渲染) - OpenGL 纹理
            unsigned int densityTexID = 0;
            cudaGraphicsResource* cuda_density_res = nullptr; // CUDA 映射句柄
            // 密度场 (用于计算的临时副本) - CUDA Array
            // 为了实现 Ping-Pong 或读写分离，Advection 需要从旧状态读，写入新状态
            cudaArray* d_densityArrayTemp = nullptr;
            cudaTextureObject_t densityTexObjRead = 0; // 用于读取的纹理对象 (支持三线性插值)

            // 温度场
            unsigned int temperatureTexID = 0;
            cudaGraphicsResource* cuda_temperature_res = nullptr;
            // 温度场 (用于计算的临时副本)
            cudaArray* d_temperatureArrayTemp = nullptr;
            cudaTextureObject_t temperatureTexObjRead = 0;

            float3* d_velocity = nullptr; // 速度场 (u, v, w) - CUDA 显存
            float3* d_velocity_backup = nullptr; // 用于半步反射求解器
            float* d_pressure = nullptr;      // 压力场 P
            float* d_pressure_temp = nullptr; // 用于 Jacobi 迭代的 Ping-Pong 缓冲
            float* d_divergence = nullptr;    // 速度散度 div(u)
            dim3 gpuDim;   // 网格维度的 CUDA 类型
        };

/**
 * 遍历所有网格单元的宏
 * 用于快速遍历网格中的所有单元
 */
#define FOR_EACH_CELL                                                    \
    for (int k = 0; k < Eulerian3dPara::theDim3d[MACGrid3d::Z]; k++)     \
        for (int j = 0; j < Eulerian3dPara::theDim3d[MACGrid3d::Y]; j++) \
            for (int i = 0; i < Eulerian3dPara::theDim3d[MACGrid3d::X]; i++)

/**
 * 遍历所有网格面的宏
 * 用于遍历速度分量所在的网格面
 */
#define FOR_EACH_FACE                                                        \
    for (int k = 0; k < Eulerian3dPara::theDim3d[MACGrid3d::Z] + 1; k++)     \
        for (int j = 0; j < Eulerian3dPara::theDim3d[MACGrid3d::Y] + 1; j++) \
            for (int i = 0; i < Eulerian3dPara::theDim3d[MACGrid3d::X] + 1; i++)
    }
}

#endif
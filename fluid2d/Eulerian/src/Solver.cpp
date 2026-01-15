#include "fluid2d/Eulerian/include/Solver.h"
#include "Configure.h"
#include <omp.h>

/*
namespace FluidSimulation
{
    namespace Eulerian2d
    {
        Solver::Solver(MACGrid2d& grid) : mGrid(grid)
        {
            mGrid.reset();
        }

        void Solver::solve()
        {
            // TODO
            // Solves the fluid simulation by performing some steps, which may include:
            // 1. advection
            // 2. compute external forces
            // 3. projection
            // ...

        }
    }
}
*/

namespace FluidSimulation
{
    namespace Eulerian2d
    {
        Solver::Solver(MACGrid2d& grid) : mGrid(grid)
        {
            mGrid.reset();
        }

        void Solver::solve()
        {
            float dt = Eulerian2dPara::dt;
            float halfDt = 0.5f * dt;
            //// 第一步: 对流
            //advect(dt);

            //// 第二步: 计算外部力
            //computeforces(dt);

            //// 第三步: 投影
            //project(dt);

            mGrid.mU_half = mGrid.mU;
            mGrid.mV_half = mGrid.mV;
            advect(halfDt);
            computeforces(halfDt);
            project(halfDt);
            reflectVelocity();
            advect(halfDt);
            computeforces(halfDt);
            project(halfDt);

        }
        void Solver::reflectVelocity()
        {
            int numX = Eulerian2dPara::theDim2d[MACGrid2d::X];
            int numY = Eulerian2dPara::theDim2d[MACGrid2d::Y];

            // u½reflect = 2*u½ - u½tilde
            for (int j = 0; j < numY; ++j)
                for (int i = 1; i < numX; ++i)
                    mGrid.mU(i, j) = 2.0f * mGrid.mU(i, j) - mGrid.mU_half(i, j);

            for (int i = 0; i < numX; ++i)
                for (int j = 1; j < numY; ++j)
                    mGrid.mV(i, j) = 2.0f * mGrid.mV(i, j) - mGrid.mV_half(i, j);
        }
        void Solver::advect(float dt)
        {
            // 对流步骤更新P
            // 使用半拉格朗日方法
            Glb::GridData2dX newU = mGrid.mU;
            Glb::GridData2dY newV = mGrid.mV;
            Glb::CubicGridData2d newD = mGrid.mD;
            Glb::CubicGridData2d newT = mGrid.mT;

            int numX = Eulerian2dPara::theDim2d[MACGrid2d::X];
            int numY = Eulerian2dPara::theDim2d[MACGrid2d::Y];

            // 对于速度
            
            // 1. 更新 U (左-face, i=1..numX-1, j=0..numY-1)
            for (int j = 0; j < numY; ++j)
                for (int i = 1; i < numX; ++i)
                {
                    if (mGrid.isSolidFace(i, j, MACGrid2d::Direction::X))
                    {
                        newU(i, j) = 0.0f;          // 或者继续保留原值
                        continue;
                    }
                    glm::vec2 pos = mGrid.getLeft(i, j);   // 采样位置
                    glm::vec2 vel = mGrid.semiLagrangian(pos, dt);
                    newU(i, j) = mGrid.getVelocityX(vel);
                }

            // 2. 更新 V (下-face, i=0..numX-1, j=1..numY-1)
            for (int i = 0; i < numX; ++i)
                for (int j = 1; j < numY; ++j)
                {
                    if (mGrid.isSolidFace(i, j, MACGrid2d::Direction::Y))
                    {
                        newV(i, j) = 0.0f;
                        continue;
                    }
                    glm::vec2 pos = mGrid.getBottom(i, j);
                    glm::vec2 vel = mGrid.semiLagrangian(pos, dt);
                    newV(i, j) = mGrid.getVelocityY(vel);
                }
            

            // 对于属性
            FOR_EACH_CELL
            {
                // 判断是固体或者边界
                if (mGrid.isSolidCell(i, j)) {
                    continue;
                }
                glm::vec2 pos_p = mGrid.getCenter(i, j);
                glm::vec2 new_vel_p = mGrid.semiLagrangian(pos_p, dt);
                // glm::vec2 new_vel_p = mGrid.RK2(pos_p, dt);
                
                newD(i, j) = mGrid.getDensity(new_vel_p);
                newT(i, j) = mGrid.getTemperature(new_vel_p);
            }

            // 边界条件

            mGrid.mU = newU;
            mGrid.mV = newV;
            mGrid.mD = newD;
            mGrid.mT = newT;
        }

        void Solver::computeforces(float dt)
        {
            int numX = mGrid.dim[0];
            int numY = mGrid.dim[1];
            // 浮力
            Glb::GridData2dY newV = mGrid.mV;
            FOR_EACH_CELL
            {
                if (mGrid.isSolidCell(i, j) || mGrid.isSolidCell(i, j - 1) || mGrid.isSolidCell(i, j + 1)) {
                    continue;
                }
                
                glm::vec2 pos1 = mGrid.getCenter(i, j);
                glm::vec2 pos0 = mGrid.getCenter(i, j - 1);
                // 向上的作用力
                float bforce1 = mGrid.getBoussinesqForce(pos1);
                float bforce0 = mGrid.getBoussinesqForce(pos0);

                float v = (bforce1 + bforce0) * 0.5 * dt;
                // 更新 v 分量
                newV(i, j) += v;
            }

            mGrid.mV = newV;

        }

        void Solver::project(float dt)
        {
            int numX = mGrid.dim[0];
            int numY = mGrid.dim[1];
            Glb::CubicGridData2d newP = mGrid.mP;
            newP.initialize(0.0);
            Glb::GridData2dY newV = mGrid.mV;
            Glb::GridData2dX newU = mGrid.mU;

            float aird = Eulerian2dPara::airDensity;

            float cellSize = mGrid.cellSize;

            for (int iteration = 100; iteration > 0; iteration--) {
                FOR_EACH_CELL{
                    if (mGrid.isSolidCell(i, j)) {
                        continue;
                    }
                    /*
                    if (mGrid.isSolidCell(i - 1, j)) {
                        newP(i - 1, j) = newP(i, j) - cellSize * aird * newU(i + 1, j) / dt;
                    }
                    if (mGrid.isSolidCell(i, j - 1)) {
                        newP(i, j - 1) = newP(i, j) - cellSize * aird * newV(i, j + 1) / dt;
                    }
                    */ 
                    double px1 = mGrid.isSolidCell(i + 1, j) ? 0.0 : newP(i + 1, j);
                    double px0 = mGrid.isSolidCell(i - 1, j) ? 0.0 : newP(i - 1, j);

                    double py1 = mGrid.isSolidCell(i, j + 1) ? 0.0 : newP(i, j + 1);
                    double py0 = mGrid.isSolidCell(i, j - 1) ? 0.0 : newP(i, j - 1);
                    

                    double div = mGrid.getDivergence(i, j);

                    // b
                    // double b = -1 * (newU(i + 1, j) - newU(i, j) + newV(i, j + 1) - newV(i, j)) * (aird) * cellSize / (dt);
                    double b = -1 * (div) * (aird) * cellSize * cellSize / (dt);
                    // sum
                    double sum = (px1 + px0 + py1 + py0);
                    double s = mGrid.getPressureCoeffBetweenCells(i, j, i, j);
                    newP(i, j) = (b + sum) / s;
                };
            }
           
            FOR_EACH_CELL{
                newU(i + 1,j) -= dt * (newP(i + 1,j) - newP(i,j)) / (cellSize * aird);
                newV(i, j + 1) -= dt * (newP(i, j + 1) - newP(i, j)) / (cellSize * aird);
            }

            // 边界处理
            FOR_EACH_LINE
            {
                // 对U
                if (mGrid.isSolidFace(i, j, MACGrid2d::Direction::X)) {
                    newU(i, j) = 0;
                }
                // 对V
                if (mGrid.isSolidFace(i, j, MACGrid2d::Direction::Y)) {
                    newV(i, j) = 0;
                }
            }
            
            mGrid.mU = newU;
            mGrid.mV = newV;
            mGrid.mP = newP;


        }

        
    }
}
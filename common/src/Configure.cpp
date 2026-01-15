#include "Configure.h"

// 系统默认配置参数
int imageWidth = 600;       // 渲染分辨率宽
int imageHeight = 600;      // 渲染分辨率高

int windowWidth = 1080;     // 默认窗口宽度
int windowHeight = 960;     // 默认窗口高度

float fontSize = 16.0f;     // 默认字体大小

bool simulating = false;    // 当前是否处于仿真状态

// 2D 欧拉流体模拟参数
namespace Eulerian2dPara
{
    // MAC 网格相关
    int theDim2d[2] = { 100, 100 };   // 网格维度
    float theCellSize2d = 0.5;      // 网格单元尺寸

    // 烟雾源及其参数
    std::vector<SourceSmoke> source = {
        {   // 源位置                         // 初始速度           // 密度  // 温度
            glm::ivec2(theDim2d[0] / 3, 0), glm::vec2(0.0f, 1.0f), 1.0f, 1.0f
        }
    };

    bool addSolid = true;           // 是否添加固体边界

    // 可视化相关
    float contrast = 1;             // 烟雾对比度
    int drawModel = 0;              // 绘制模式
    int gridNum = theDim2d[0];      // 用于显示的网格数量

    // 物理参数
    float dt = 0.01;                // 时间步长
    float airDensity = 1.3;         // 空气密度
    float ambientTemp = 0.0;        // 环境温度
    float boussinesqAlpha = 500.0;  // Boussinesq 公式中的 alpha 系数
    float boussinesqBeta = 2500.0;  // Boussinesq 公式中的 beta 系数
}

// 3D 欧拉流体模拟参数
namespace Eulerian3dPara
{
    // MAC 网格相关
    int theDim3d[3] = { 64, 192, 192 }; // 网格维度（保证 x <= y = z）
    float theCellSize3d = 0.5;      // 网格单元尺寸
    std::vector<SourceSmoke> source = {
        {glm::ivec3(theDim3d[0] / 2, theDim3d[1] / 2, 0), glm::vec3(0.0f, 0.0f, 1.0f), 1.0f, 1.0f}
    };
    bool addSolid = true;           // 是否添加固体边界

    // 可视化相关
    float contrast = 1;             // 烟雾对比度
    int drawModel = 0;              // 绘制模式
    int gridNumX = (int)((float)theDim3d[0] / theDim3d[2] * 100);  // X 方向网格数
    int gridNumY = (int)((float)theDim3d[1] / theDim3d[2] * 100);  // Y 方向网格数
    int gridNumZ = 100;             // Z 方向网格数

    float dt = 0.01;
    bool useBFECC = false;
    bool useReflection = false;
    
    // 物理参数
    float airDensity = 1.3;         // 空气密度
    float ambientTemp = 0.0;        // 环境温度
    float boussinesqAlpha = 500.0;  // Boussinesq 公式中的 alpha 系数
    float boussinesqBeta = 2500.0;  // Boussinesq 公式中的 beta 系数
}

// 存储系统中可选的仿真组件
std::vector<Glb::Component*> methodComponents;

// 资源路径
std::string shaderPath = "E:/File/ShanghaiTech/Course/2025_Fall/Computer_Graphics_I/Homework/project/NKU_CG_FluidSim-main/code/resources/shaders";
std::string picturePath = "E:/File/ShanghaiTech/Course/2025_Fall/Computer_Graphics_I/Homework/project/NKU_CG_FluidSim-main/code/resources/pictures";
#pragma once
#ifndef __EULERIAN_3D_RENDERER_H__
#define __EULERIAN_3D_RENDERER_H__

#include "glm/glm.hpp"
#include <glad/glad.h>
#include <glfw3.h>
#include "Shader.h"
#include "Container.h"
#include "MACGrid3d.h"
#include "Camera.h"
#include "Configure.h"
#include <Logger.h>

namespace FluidSimulation {
	namespace Eulerian3d {
		// 欧拉法流体渲染器类
		// 负责将三维MAC网格中的流体数据可视化
		class Renderer {
		public:
			Renderer(MACGrid3d& grid);
			~Renderer();

			void draw();                          // 绘制整个场景
			GLuint getTextureID();                // 获取渲染结果的纹理ID

		private:
			void initProxyCube();                 // 初始化立方体用于Ray Marching边界
			void initFBO(int width, int height);

			int imageWidth = 0;
			int imageHeight = 0;

			Glb::Shader* volumeShader;            // 体渲染着色器
			Glb::Container* container = nullptr;  // 场景容器
			MACGrid3d& mGrid;                     // MAC网格引用

			GLuint cubeVAO = 0, cubeVBO = 0;
			GLuint FBO = 0;
			GLuint textureID = 0;                 // 渲染结果纹理
			GLuint RBO = 0;
		};
	}
}

#endif

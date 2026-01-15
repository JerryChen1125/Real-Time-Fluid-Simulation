#include "fluid3d/Eulerian/include/Renderer.h"
#include <vector>
#include "stb_image.h"

namespace FluidSimulation
{
	namespace Eulerian3d
	{
		Renderer::Renderer(MACGrid3d &grid) : mGrid(grid)
		{
			container = new Glb::Container();
			float x = (float)Eulerian3dPara::theDim3d[0] / Eulerian3dPara::theDim3d[2];
			float y = (float)Eulerian3dPara::theDim3d[1] / Eulerian3dPara::theDim3d[2];
			container->resetSize(x, y, 1.0f);
			container->init();

			std::string vertPath = shaderPath + "/VolumeRender.vert";
			std::string fragPath = shaderPath + "/VolumeRender.frag";
			volumeShader = new Glb::Shader();
			volumeShader->buildFromFile(vertPath, fragPath);

			initProxyCube();
			initFBO(::imageWidth, ::imageHeight);
		}

		Renderer::~Renderer()
		{
			if (container) { delete container; container = nullptr; }
			if (volumeShader) { delete volumeShader; volumeShader = nullptr; }

			if (cubeVAO) glDeleteVertexArrays(1, &cubeVAO);
			if (cubeVBO) glDeleteBuffers(1, &cubeVBO);
			if (FBO) glDeleteFramebuffers(1, &FBO);
			if (textureID) glDeleteTextures(1, &textureID);
			if (RBO) glDeleteRenderbuffers(1, &RBO);
		}

		void Renderer::initProxyCube() {
			float vertices[] = {
				// Back face (z = 0)
				0.0f, 0.0f, 0.0f, // Bottom-left
				1.0f, 1.0f, 0.0f, // Top-right
				1.0f, 0.0f, 0.0f, // Bottom-right         
				1.0f, 1.0f, 0.0f, // Top-right
				0.0f, 0.0f, 0.0f, // Bottom-left
				0.0f, 1.0f, 0.0f, // Top-left

				// Front face (z = 1)
				0.0f, 0.0f, 1.0f, // Bottom-left
				1.0f, 0.0f, 1.0f, // Bottom-right
				1.0f, 1.0f, 1.0f, // Top-right
				1.0f, 1.0f, 1.0f, // Top-right
				0.0f, 1.0f, 1.0f, // Top-left
				0.0f, 0.0f, 1.0f, // Bottom-left

				// Left face (x = 0)
				0.0f, 1.0f, 1.0f, // Top-right
				0.0f, 1.0f, 0.0f, // Top-left
				0.0f, 0.0f, 0.0f, // Bottom-left
				0.0f, 0.0f, 0.0f, // Bottom-left
				0.0f, 0.0f, 1.0f, // Bottom-right
				0.0f, 1.0f, 1.0f, // Top-right

				// Right face (x = 1)
				1.0f, 1.0f, 1.0f, // Top-left
				1.0f, 0.0f, 0.0f, // Bottom-right
				1.0f, 1.0f, 0.0f, // Top-right         
				1.0f, 0.0f, 0.0f, // Bottom-right
				1.0f, 1.0f, 1.0f, // Top-left
				1.0f, 0.0f, 1.0f, // Bottom-left     

				// Bottom face (y = 0)
				0.0f, 0.0f, 0.0f, // Top-right
				1.0f, 0.0f, 0.0f, // Top-left
				1.0f, 0.0f, 1.0f, // Bottom-left
				1.0f, 0.0f, 1.0f, // Bottom-left
				0.0f, 0.0f, 1.0f, // Bottom-right
				0.0f, 0.0f, 0.0f, // Top-right

				// Top face (y = 1)
				0.0f, 1.0f, 0.0f, // Top-left
				1.0f, 1.0f, 1.0f, // Bottom-right
				1.0f, 1.0f, 0.0f, // Top-right     
				1.0f, 1.0f, 1.0f, // Bottom-right
				0.0f, 1.0f, 0.0f, // Top-left
				0.0f, 1.0f, 1.0f  // Bottom-left        
			};

			glGenVertexArrays(1, &cubeVAO);
			glGenBuffers(1, &cubeVBO);

			glBindVertexArray(cubeVAO);
			glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
		}

		void Renderer::initFBO(int width, int height)
		{
			imageWidth = width;
			imageHeight = height;

			if (FBO != 0) {
				glDeleteFramebuffers(1, &FBO);
				FBO = 0;
			}
			if (textureID != 0) {
				glDeleteTextures(1, &textureID);
				textureID = 0;
			}
			if (RBO != 0) {
				glDeleteRenderbuffers(1, &RBO);
				RBO = 0;
			}

			glGenFramebuffers(1, &FBO);
			glBindFramebuffer(GL_FRAMEBUFFER, FBO);

			// 1. 颜色附件 (纹理)
			glGenTextures(1, &textureID);
			glBindTexture(GL_TEXTURE_2D, textureID);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, imageWidth, imageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);

			// 2. 深度和模板附件 (Renderbuffer)
			glGenRenderbuffers(1, &RBO);
			glBindRenderbuffer(GL_RENDERBUFFER, RBO);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, imageWidth, imageHeight);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				Glb::Logger::getInstance().addLog("Error: Framebuffer is not complete!");

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void Renderer::draw()
		{
			glBindFramebuffer(GL_FRAMEBUFFER, FBO);
			glViewport(0, 0, imageWidth, imageHeight);

			glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glEnable(GL_DEPTH_TEST);
			if (container) {
				container->draw();
			}

			// 1. 开启混合 (烟雾是半透明的)
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			// 2. 绑定 Shader
			volumeShader->use();
			volumeShader->setFloat("contrast", Eulerian3dPara::contrast);

			// 3. 绑定 3D 纹理 (从 MACGrid3d 获取)
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_3D, mGrid.densityTexID);
			volumeShader->setInt("densityTex", 0);

			// 4. 传入 Uniforms
			glm::mat4 view = Glb::Camera::getInstance().GetView();
			glm::mat4 projection = Glb::Camera::getInstance().GetProjection();
			glm::mat4 model = glm::mat4(1.0f);
			// 缩放模型以匹配 Container 的物理尺寸
			float scaleX = (float)mGrid.dim[0] / mGrid.dim[2];
			float scaleY = (float)mGrid.dim[1] / mGrid.dim[2];
			float scaleZ = 1.0f;
			model = glm::scale(model, glm::vec3(scaleX, scaleY, scaleZ));

			volumeShader->setMat4("model", model);
			volumeShader->setMat4("view", view);
			volumeShader->setMat4("projection", projection);
			glm::vec3 camPos = Glb::Camera::getInstance().GetPosition();
			volumeShader->setVec3("cameraPos", camPos);
			volumeShader->setFloat("stepSize", 0.01f); // 步长

			// 5. 绘制立方体
			glBindVertexArray(cubeVAO);
			glDrawArrays(GL_TRIANGLES, 0, 36);
			glBindVertexArray(0);

			glDisable(GL_BLEND);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		GLuint Renderer::getTextureID()
		{
			return textureID;
		}
	}
}
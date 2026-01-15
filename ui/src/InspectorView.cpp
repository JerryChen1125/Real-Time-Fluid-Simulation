/**
 * InspectorView.cpp: UI检查器视图实现文件
 * 负责显示和编辑仿真参数
 */

#include "InspectorView.h"

namespace FluidSimulation
{
	/**
	 * 检视器视图类
	 */
	InspectorView::InspectorView()
	{
		// 初始化UI元素
	}

	/**
	 * 构造函数
	 * @param window GLFW
	 */
	InspectorView::InspectorView(GLFWwindow *window)
	{
		// 初始化UI元素
		this->window = window;
		showID = false;
	}

	/**
	 * 显示参数编辑面板
	 */
	void InspectorView::display()
	{
		// 开始一个新的窗口
		ImGui::Begin("Inspector", NULL, ImGuiWindowFlags_NoCollapse);

		// 设置窗口样式
		ImGui::PushItemWidth(200);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(20.0f, 7.0f));

		// 显示仿真方法选择下拉框
		ImGui::Text("Simulation Method:");
		if (ImGui::BeginCombo("methods", Manager::getInstance().getMethod() == NULL ? NULL : Manager::getInstance().getMethod()->description))
		{
			// 遍历可用的仿真方法
			for (int i = 0; i < methodComponents.size(); i++)
			{
				bool is_selected = (Manager::getInstance().getMethod() == methodComponents[i]);
				if (ImGui::Selectable(methodComponents[i]->description, is_selected))
				{
					// 切换仿真方法
					if (Manager::getInstance().getMethod() != methodComponents[i])
					{
						if (Manager::getInstance().getMethod() != NULL)
						{
							Manager::getInstance().getMethod()->shutDown();
						}
						Manager::getInstance().setMethod(methodComponents[i]);
						Manager::getInstance().getMethod()->init();
						Manager::getInstance().getSceneView()->texture = -1;
					}
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		// 控制仿真开始/暂停按钮
		ImGui::SetNextItemWidth(300);
		if (ImGui::Button(simulating ? "Stop" : "Continue"))
		{
			simulating = !simulating;
			if (simulating)
			{
				Glb::Logger::getInstance().addLog("Simulating...");
			}
			else
			{
				Glb::Logger::getInstance().addLog("Stopped.");
			}
		}

		if (ImGui::Button("Rerun"))
		{
			glfwMakeContextCurrent(window);

			Manager::getInstance().getMethod()->init();

			simulating = false;
			Manager::getInstance().getSceneView()->texture = -1;
			Glb::Logger::getInstance().addLog("Rerun succeeded.");
		}

		ImGui::Separator();

		if (Manager::getInstance().getMethod() == NULL)
		{
			ImGui::Text("Please select a simulation method.");
		}
		else
		{
			int intStep = 1;
			float floatStep1 = 0.1;
			float floatStep3 = 0.001;
			double doubleStep4 = 0.0001;

			switch (Manager::getInstance().getMethod()->id)
			{
			// eulerian 2d
			case 0:

				ImGui::Text("MAC grid:");
				ImGui::InputScalar("Dim.x", ImGuiDataType_S32, &Eulerian2dPara::theDim2d[0], &intStep, NULL);
				ImGui::InputScalar("Dim.y", ImGuiDataType_S32, &Eulerian2dPara::theDim2d[1], &intStep, NULL);

				ImGui::Checkbox("Add Solid", &Eulerian2dPara::addSolid);
				ImGui::Text("---------------------------------");
				for (int i = 0; i < Eulerian2dPara::source.size(); i++) {
					ImGui::Text(("source grid " + std::to_string(i)).c_str());
					ImGui::PushID(i);
					ImGui::SameLine();
					if (ImGui::Button("delete")) {
						Eulerian2dPara::source.erase(Eulerian2dPara::source.begin() + i);
						i--;
					}
					else {
						ImGui::InputInt2("position(x,y)", &Eulerian2dPara::source[i].position.x);
						ImGui::InputFloat2("velocity(x,y)", &Eulerian2dPara::source[i].velocity.x);
						ImGui::InputScalar("density", ImGuiDataType_Float, &Eulerian2dPara::source[i].density, &floatStep1, NULL);
						ImGui::InputScalar("temperature", ImGuiDataType_Float, &Eulerian2dPara::source[i].temp, &floatStep1, NULL);
					}
					ImGui::PopID();
					ImGui::Text("---------------------------------");
				}

				if (ImGui::Button("add source grid")) {
					Eulerian2dPara::source.push_back(Eulerian2dPara::SourceSmoke({}));
				}

				ImGui::Text("note: Please rerun after setting");
				ImGui::Separator();

				ImGui::Text("Physical Parameters:");
				ImGui::SliderFloat("Air Density", &Eulerian2dPara::airDensity, 0.10f, 3.0f);
				ImGui::SliderFloat("Ambient Temperature", &Eulerian2dPara::ambientTemp, 0.0f, 50.0f);
				ImGui::SliderFloat("Boussinesq Alpha", &Eulerian2dPara::boussinesqAlpha, 0.0f, 1000.0f);
				ImGui::SliderFloat("Boussinesq Beta", &Eulerian2dPara::boussinesqBeta, 0.0f, 5000.0f);

				ImGui::Separator();

				ImGui::Text("Solver:");
				ImGui::SliderFloat("Delta Time", &Eulerian2dPara::dt, 0.0f, 0.1f, "%.5f");

				ImGui::Separator();

				ImGui::Text("Renderer:");
				ImGui::RadioButton("Pixel", &Eulerian2dPara::drawModel, 0);
				ImGui::RadioButton("Grid", &Eulerian2dPara::drawModel, 1);
				ImGui::SliderFloat("Contrast", &Eulerian2dPara::contrast, 0.0f, 3.0f);

				break;
			// eulerian 3d
			case 1:
				ImGui::Text("Camera:");
				ImGui::InputFloat3("Position", &Glb::Camera::getInstance().mPosition.x);
				ImGui::InputScalar("Fov", ImGuiDataType_Float, &Glb::Camera::getInstance().fovyDeg, &floatStep1, NULL);
				ImGui::InputScalar("Aspect", ImGuiDataType_Float, &Glb::Camera::getInstance().aspect, &floatStep1, NULL);
				ImGui::InputScalar("Near", ImGuiDataType_Float, &Glb::Camera::getInstance().nearPlane, &floatStep1, NULL);
				ImGui::InputScalar("Far", ImGuiDataType_Float, &Glb::Camera::getInstance().farPlane, &floatStep1, NULL);
				ImGui::InputScalar("Yaw", ImGuiDataType_Float, &Glb::Camera::getInstance().mYaw, &floatStep1, NULL);
				ImGui::InputScalar("Pitch", ImGuiDataType_Float, &Glb::Camera::getInstance().mPitch, &floatStep1, NULL);
				Glb::Camera::getInstance().UpdateView();

				ImGui::Separator();

				ImGui::Text("MAC grid:");
				ImGui::InputScalar("Dim.x", ImGuiDataType_S32, &Eulerian3dPara::theDim3d[0], &intStep, NULL);
				ImGui::InputScalar("Dim.y", ImGuiDataType_S32, &Eulerian3dPara::theDim3d[1], &intStep, NULL);
				ImGui::InputScalar("Dim.z", ImGuiDataType_S32, &Eulerian3dPara::theDim3d[2], &intStep, NULL);

				ImGui::Checkbox("Add Solid", &Eulerian3dPara::addSolid);
				ImGui::Text("---------------------------------");
				for (int i = 0; i < Eulerian3dPara::source.size(); i++) {
					ImGui::Text(("source grid " + std::to_string(i)).c_str());
					ImGui::PushID(i);
					ImGui::SameLine();
					if (ImGui::Button("delete")) {
						Eulerian3dPara::source.erase(Eulerian3dPara::source.begin() + i);
						i--;
					}
					else {
						ImGui::InputInt3("position(x,y,z)", &Eulerian3dPara::source[i].position.x);
						ImGui::InputFloat3("velocity(x,y,z)", &Eulerian3dPara::source[i].velocity.x);
						ImGui::InputScalar("density", ImGuiDataType_Float, &Eulerian3dPara::source[i].density, &floatStep1, NULL);
						ImGui::InputScalar("temperature", ImGuiDataType_Float, &Eulerian3dPara::source[i].temp, &floatStep1, NULL);
					}
					ImGui::PopID();
					ImGui::Text("---------------------------------");
				}

				if (ImGui::Button("add source grid")) {
					Eulerian3dPara::source.push_back(Eulerian3dPara::SourceSmoke({}));
				}

				ImGui::Text("note: Please rerun after setting");
				ImGui::Separator();

				ImGui::Text("Physical Parameters:");
				ImGui::SliderFloat("Air Density", &Eulerian3dPara::airDensity, 0.10f, 3.0f);
				ImGui::SliderFloat("Ambient Temperature", &Eulerian3dPara::ambientTemp, 0.0f, 50.0f);
				ImGui::SliderFloat("Boussinesq Alpha", &Eulerian3dPara::boussinesqAlpha, 0.0f, 1000.0f);
				ImGui::SliderFloat("Boussinesq Beta", &Eulerian3dPara::boussinesqBeta, 0.0f, 5000.0f);				

				ImGui::Separator();

				ImGui::Text("Solver:");
				ImGui::SliderFloat("Delta Time", &Eulerian3dPara::dt, 0.0f, 0.01f, "%.05f");
				ImGui::Checkbox("Back and Forth Error Compensation and Correction", &Eulerian3dPara::useBFECC);
				ImGui::Checkbox("Half-Step Reflection", &Eulerian3dPara::useReflection);

				ImGui::Separator();

				ImGui::Text("Renderer:");
				ImGui::RadioButton("Pixel", &Eulerian3dPara::drawModel, 0);
				ImGui::RadioButton("Grid", &Eulerian3dPara::drawModel, 1);
				ImGui::SliderFloat("Contrast", &Eulerian3dPara::contrast, 0.0f, 3.0f);
				break;

			case 2:
				// TODO(optional)
				// add other method's parameters

				break;
			}



			if (!Glb::Timer::getInstance().empty())
			{
				ImGui::Separator();
				ImGui::Text("Timing:");
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.8f, 1.0f, 1.0f));
				ImGui::Text(Glb::Timer::getInstance().currentStatus().c_str());
				ImGui::PopStyleColor();
			}
		}

		ImGui::PopStyleVar();
		ImGui::PopItemWidth();

		ImGui::End();
	}
}

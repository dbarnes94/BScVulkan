#include "VulkanBase.h"

int main()
{
	VulkanBase &vulkan = VulkanBase::getSingleton();

	//Loop continuously until we ask the glfw window to close
	while (!glfwWindowShouldClose(vulkan.window)) 
	{
		glfwPollEvents();

		vulkan.AcquireSubmitPresent();

		vulkan.UpdateUniformBuffer();

		vulkan.showFPS(vulkan.window);

		vulkan.windowTimer();
	}

	vulkan.showAverages();

	return 0;
}
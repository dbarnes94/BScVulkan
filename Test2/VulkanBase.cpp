#pragma once

#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include "VulkanBase.h"

VulkanBase::VulkanBase()
{
	startTime = std::chrono::high_resolution_clock::now();

	auto initStart = std::chrono::high_resolution_clock::now();

	InitialiseVulkan();

	auto initEnd = std::chrono::high_resolution_clock::now();

	auto elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>(initEnd - initStart).count();

	std::cout << "Time taken to initialise Vulkan Systems: " << elapsedTime << " seconds.\n";
}

//Initialise GLFW and Vulkan
void VulkanBase::InitialiseVulkan()
{
	if (!glfwInit())
	{
		std::cout << "GLFW Initialisation Error.\n";
		exit(-1);
	}

	if (!glfwVulkanSupported())
	{
		std::cout << "Vulkan not supported.\n\n";
		exit(-1);
	}

	result = VK_SUCCESS;

	//GLFW window creation
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //Disable context creation as it is not needed
	window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Comparison", nullptr, nullptr); //The GLFW window 800 x 600 resolution, windowed mode.

	glfwSetWindowUserPointer(window, this);
	glfwSetWindowSizeCallback(window, VulkanBase::windowResize);//GLFW window resize callback function

	CreateInstance();
	EnumeratePhysicalDevices();
	CreateSurface();
	CreateLogicalDevice();
	CreateSwapchain();
	CreateSwapchainImageViews();
	CreateRenderPass();
	CreateDescriptorSetLayout();
	LoadShaders();
	CreateGraphicsPipeline();
	CreateCommandPool(commandPool, 0); //Create Draw command pool
	CreateCommandPool(transferPool, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT); //Create Transfer command pool
	CreateDepthImageResources();
	CreateFramebuffers();
	CreateTextureImage();
	CreateTextureImageView();
	CreateTextureSampler();
	CreateModel();
	if (multiCopy)
	{
		auto copyStart = std::chrono::steady_clock::now();

		for (int i = 0; i <= 100; i++)
		{
			CreateVertexBuffer();
			CreateIndexBuffer();
		}

		auto copyEnd = std::chrono::steady_clock::now();

		auto elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>(copyEnd - copyStart).count();

		std::cout << "Time taken to copy 100 model's data to GPU: " << elapsedTime << " seconds.\n";
	}
	else
	{
		CreateVertexBuffer();
		CreateIndexBuffer();
	}
	CreateUniformBuffer();
	CreateDescriptorPool();
	CreateDescriptorSet();
	CreateSemaphores();
	CreateCommandBuffers();
	RecordCommandBuffers();

	UpdateUniformBuffer();
}

//Termination of program
VulkanBase::~VulkanBase()
{
	vkDeviceWaitIdle(logicalDevice);

	vkDestroySemaphore(logicalDevice, renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(logicalDevice, imageAcquiredSemaphore, nullptr);

	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);

	vkDestroySampler(logicalDevice, textureSampler, nullptr);
	vkDestroyImageView(logicalDevice, textureImageView, nullptr);
	vkFreeMemory(logicalDevice, textureImageMemory, nullptr); //May not be in correct order
	vkDestroyImage(logicalDevice, textureImage, nullptr); //May not be in correct order

	vkFreeMemory(logicalDevice, uniformBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, uniformBuffer, nullptr);
	vkFreeMemory(logicalDevice, uniformStagingBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, uniformStagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, indexBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
	vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);

	vkDestroyImageView(logicalDevice, multisampleDepthImageView, nullptr);
	vkFreeMemory(logicalDevice, multisampleDepthImageMemory, nullptr);
	vkDestroyImage(logicalDevice, multisampleDepthImage, nullptr);
	vkDestroyImageView(logicalDevice, multisampleColourImageView, nullptr);
	vkFreeMemory(logicalDevice, multisampleColourImageMemory, nullptr);
	vkDestroyImage(logicalDevice, multisampleColourImage, nullptr);

	vkDestroyImageView(logicalDevice, depthImageView, nullptr);
	vkFreeMemory(logicalDevice, depthImageMemory, nullptr);
	vkDestroyImage(logicalDevice, depthImage, nullptr);

	vkDestroyCommandPool(logicalDevice, transferPool, nullptr);
	vkFreeCommandBuffers(logicalDevice, commandPool, (uint32_t)commandBuffers.size(), commandBuffers.data());
	vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
	for (uint32_t i = 0; i < framebuffers.size(); i++)
	{
		vkDestroyFramebuffer(logicalDevice, framebuffers[i], nullptr);
	}
	vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);
	vkDestroyShaderModule(logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(logicalDevice, vertexShaderModule, nullptr);
	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
	for (uint32_t i = 0; i < swapchainImageViews.size(); i++)
	{
		vkDestroyImageView(logicalDevice, swapchainImageViews[i], nullptr);
	}
	vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
	vkDestroyDevice(logicalDevice, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr); //Sever the connection between Vulkan and the native surface
	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();

	std::cout << "Draw Timing Average: " << elapsedSum / drawFrames << " milliseconds.\n";

	std::cout << std::endl;
}

void VulkanBase::CreateInstance()
{
	if (enableValidationLayers && !checkValidationLayerSupport()) 
	{
		std::cout << "Validation Layers not available.\n";
	}

	VkApplicationInfo application_info = {};
	application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	application_info.pNext = nullptr;
	application_info.pApplicationName = "Vulkan Comparison";
	application_info.applicationVersion = 1;
	application_info.pEngineName = "Open Vulkan";
	application_info.engineVersion = 1;
	application_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);

	uint32_t extensionCount;
	std::vector<const char *> extensions;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);

	for (uint32_t i = 0; i < extensionCount; i++)
	{
		extensions.push_back(glfwExtensions[i]);
	}

	if (enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	VkInstanceCreateInfo instance_create_info = {};
	instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_create_info.pNext = nullptr;
	instance_create_info.flags = 0;
	instance_create_info.pApplicationInfo = &application_info;
	if (enableValidationLayers)
	{
		instance_create_info.enabledLayerCount = (uint32_t)validationLayers.size();
		instance_create_info.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		instance_create_info.enabledLayerCount = 0;
	}

	instance_create_info.enabledExtensionCount = (uint32_t)extensions.size();
	instance_create_info.ppEnabledExtensionNames = extensions.data();

	std::cout << extensionCount << " extensions enabled.\n\n";

	result = vkCreateInstance(&instance_create_info, nullptr, &instance);

	if (result == VK_SUCCESS)
	{
		std::cout << "Instance Created Successfully.\n";
	}
	else if (result == VK_ERROR_INCOMPATIBLE_DRIVER)
	{
		std::cout << "Cannot find a compatible Vulkan driver.\n";
		exit(-1);
	}
	else if (result)
	{
		std::cout << "Unknown Error.\n";
		std::cout << "Error Code: " << result << ".\n";
		exit(-1);
	}
}

void VulkanBase::EnumeratePhysicalDevices()
{
	//How many devices we have in the system are obtained
	uint32_t physicalDeviceCount = 0;
	result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

	//Set the amount of physical devices in the system creating handles at the same time
	physicalDevices.resize(physicalDeviceCount);

	//If devices exist
	if (result == VK_SUCCESS)
	{
		result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, &physicalDevices[0]);

		//If physical devices have been set appropriately
		if (result == VK_SUCCESS)
		{
			std::cout << physicalDeviceCount << " Physical Device Enumerated Successfully.\n";
		}
	}

	for (uint32_t i = 0; i < physicalDeviceCount; ++i)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);	

		//Supported Vulkan version by the device
		std::cout << "API Version: ";
		std::cout << ((properties.apiVersion >> 22)) << '.'; //Major
		std::cout << ((properties.apiVersion >> 12) & 0x3ff) << '.'; //Minor
		std::cout << (properties.apiVersion & 0xfff) << '\n'; //Patch

		std::cout << "Driver Version: " << properties.driverVersion << '\n';

		std::cout << std::showbase << std::internal << std::setfill('0') << std::hex;
		std::cout << "Vendor Id: " << std::setw(6) << properties.vendorID << '\n';
		std::cout << "Device Id: " << std::setw(6) << properties.deviceID << '\n';
		std::cout << std::noshowbase << std::right << std::setfill(' ') << std::dec;

		std::cout << "Device Type: ";
		switch (properties.deviceType) {
		case VK_PHYSICAL_DEVICE_TYPE_OTHER:
			std::cout << "VK_PHYSICAL_DEVICE_TYPE_OTHER";
			break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			std::cout << "VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU";
			break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			std::cout << "VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU";
			break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			std::cout << "VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU";
			break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			std::cout << "VK_PHYSICAL_DEVICE_TYPE_CPU";
			break;
		default:
			break;
		}
		std::cout << '\n';

		std::cout << "Device Name: " << properties.deviceName << '\n';
	}
}

void VulkanBase::CreateSurface()
{
	//Platform agnostic surface creation
	result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
	if (result == VK_SUCCESS)
	{
		std::cout << "Surface created successfully.\n";
	}
}

void VulkanBase::CreateLogicalDevice()
{
	VkPhysicalDeviceFeatures supported_features;
	VkPhysicalDeviceFeatures required_features = {};

	vkGetPhysicalDeviceFeatures(physicalDevices[0], &supported_features);

	uint32_t queue_family_count = 0;
	std::vector<VkQueueFamilyProperties> queueFamilyProperties;

	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[0], &queue_family_count, nullptr);

	queueFamilyProperties.resize(queue_family_count);

	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[0], &queue_family_count, queueFamilyProperties.data());

	/////Query queue family for presentation support
	std::vector<VkBool32> pQueueSupportsPresent;

	pQueueSupportsPresent.resize(queue_family_count); //Temp storage for multiple possible present queue true events

	for (uint32_t i = 0; i < queue_family_count; i++)
	{
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevices[0], i, surface, &pQueueSupportsPresent[i]);
	}

	//Iterate through the queue families on the physical device to check for a queue that supports both graphics and presentation
	for (uint32_t i = 0; i < pQueueSupportsPresent.size(); i++)
	{
		if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			if (graphics_queue_family_index == UINT32_MAX)
			{
				graphics_queue_family_index = i; //We assume here that any queue should have graphics support due to VK_QUEUE_GRAPHICS_BIT being greater than 0
				std::cout << "Device supports a graphics queue\n";
			}

			if (pQueueSupportsPresent[i] == VK_TRUE) //Found a queue that supports both graphics and present type 
			{
				graphics_queue_family_index = i;
				present_queue_family_index = i;
				std::cout << "Both graphics and present support found on a queue\n";
				break;
			}
		}
	}

	//If a present queue was not found then check for one without graphics capabilities
	if (present_queue_family_index == UINT32_MAX)
	{
		for (uint32_t i = 0; i < queue_family_count; i++)
		{
			if (pQueueSupportsPresent[i] == VK_TRUE)
			{
				present_queue_family_index = i;
				std::cout << "Separate present queue found and index stored\n";
				break;
			}
		}
	}

	if (present_queue_family_index == UINT32_MAX || graphics_queue_family_index == UINT32_MAX)
	{
		std::cout << "Unable to find either present or graphics queue support.\n"; //No means for rasterization and displaying
		exit(-1);
	}

	VkDeviceQueueCreateInfo queue_info = {};
	queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info.pNext = nullptr;
	queue_info.flags = 0;
	queue_info.queueFamilyIndex = graphics_queue_family_index;
	queue_info.queueCount = 1;

	float queuePriority = 1.0f;
	queue_info.pQueuePriorities = &queuePriority;

	std::vector<const char *> extensions;
	extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	//////Device Creation
	VkDeviceCreateInfo device_info = {};
	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.pNext = nullptr;
	device_info.flags = 0;
	device_info.queueCreateInfoCount = 1;
	device_info.pQueueCreateInfos = &queue_info;
	if (enableValidationLayers) {
		device_info.enabledLayerCount = (uint32_t)validationLayers.size();
		device_info.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		device_info.enabledLayerCount = 0;
	}
	device_info.enabledExtensionCount = (uint32_t)extensions.size();
	device_info.ppEnabledExtensionNames = extensions.data();
	device_info.pEnabledFeatures = &required_features;

	result = vkCreateDevice(physicalDevices[0], &device_info, nullptr, &logicalDevice);
	if (result == VK_SUCCESS)
	{
		std::cout << "Logical Device created successfully.\n";
	}

	//////Initialise Queue handles needed for work submission and swapchain creation 
	vkGetDeviceQueue(logicalDevice, graphics_queue_family_index, 0, &graphicsQueue);//Set our handle appropriately to the index found earlier

	if (graphics_queue_family_index == present_queue_family_index)//If our graphics and present queue support is found on the same queue
	{
		presentQueue = graphicsQueue;
	}
	else
	{
		vkGetDeviceQueue(logicalDevice, present_queue_family_index, 0, &presentQueue);
	}
}

void VulkanBase::CreateSwapchain()
{
	//////Swapchain Support
	std::cout << "\n---SWAPCHAIN INFORMATION---\n";

	uint32_t surfaceFormatCount;
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevices[0], surface, &surfaceFormatCount, nullptr); //Obtain the number of formats and assign this to our count
	std::cout << "Number of surface formats available for the physical device: " << surfaceFormatCount << "\n";

	VkSurfaceFormatKHR *surfaceFormats = (VkSurfaceFormatKHR *)malloc(surfaceFormatCount * sizeof(VkSurfaceFormatKHR)); //Temporary format storage
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevices[0], surface, &surfaceFormatCount, surfaceFormats); //Assign surface formats available to our variable
	if (result == VK_SUCCESS)
	{
		std::cout << "Surface Formats assigned successfully.\n";
	}

	if (surfaceFormatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED) //No preferred surface format
	{
		swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM; //Choose a decent option
	}
	else
	{
		if (surfaceFormatCount >= 1)
		{
			swapchainImageFormat = surfaceFormats[0].format; //Choose the first option thats available if there is there is a preference
		}
	}
	free(surfaceFormats); //Done with our temporary storage

	std::cout << "Chosen image format: " << swapchainImageFormat << " (Refer to Vulkan Spec for enumerated value)\n";

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevices[0], surface, &surfaceCapabilities);
	if (result == VK_SUCCESS)
	{
		std::cout << "Surface Capabilities retrieved successfully.\n";
	}

	int width, height;
	glfwGetWindowSize(window, &width, &height);
	if (surfaceCapabilities.currentExtent.width == UINT32_MAX)//If the surface size is undefined set our surface size appropriately
	{
		//Set the correct surface size for the swapchain for both width and height
		swapchainExtent.width = width;
		swapchainExtent.height = height;

		if (swapchainExtent.width < surfaceCapabilities.minImageExtent.width) //If the current width is less than the minimum then set it to the minimum capable
		{
			swapchainExtent.width = surfaceCapabilities.minImageExtent.width;
		}
		else if (swapchainExtent.width > surfaceCapabilities.maxImageExtent.width) //If the current width is greater than the maximum then set it to the maximum capable
		{
			swapchainExtent.width = surfaceCapabilities.maxImageExtent.width;
		}

		if (swapchainExtent.height < surfaceCapabilities.minImageExtent.height) //If the current height is less than the minimum then set it to the minimum capable
		{
			swapchainExtent.height = surfaceCapabilities.minImageExtent.height;
		}
		else if (swapchainExtent.height > surfaceCapabilities.maxImageExtent.height) //If the current height is greater than the maximum then set it to the maximum capable
		{
			swapchainExtent.height = surfaceCapabilities.maxImageExtent.height;
		}
	}
	else //If the current extent is already defined then set the swapchain to match in size
	{
		swapchainExtent = surfaceCapabilities.currentExtent;
	}

	uint32_t desiredNumberOfSwapchainImages = surfaceCapabilities.minImageCount; //1 presentable image at any one time would be 2 if using triple-buffering
	std::cout << "Minimum amount of images available for the surface (min is chosen): " << surfaceCapabilities.minImageCount << "\n";
	std::cout << "Maximum amount of images available for the surface (min is chosen): " << surfaceCapabilities.maxImageCount << "\n";

	VkSurfaceTransformFlagBitsKHR preTransform;
	if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) //If identity transform is supported set the preTransform to it
	{
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		preTransform = surfaceCapabilities.currentTransform;
	}

	uint32_t presentModeCount; // How many presentation modes are available on the device
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevices[0], surface, &presentModeCount, nullptr);
	std::cout << "Number of presentation modes available on the device: " << presentModeCount << "\n";

	VkPresentModeKHR *presentModes = (VkPresentModeKHR *)malloc(presentModeCount * sizeof(VkPresentModeKHR)); //Temporary storage for present modes available
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevices[0], surface, &presentModeCount, presentModes);
	if (result == VK_SUCCESS)
	{
		std::cout << "Present Modes assigned successfully.\n";
	}

	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; //Guaranteed to be supported - Mailbox could be used for triple-buffering 
	free(presentModes); //No need for available modes unless anything other than FIFO is used

	//Swapchain create information
	VkSwapchainCreateInfoKHR swapchain_info = {};
	swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_info.pNext = nullptr;
	swapchain_info.flags = 0;
	swapchain_info.surface = surface;
	swapchain_info.minImageCount = desiredNumberOfSwapchainImages;
	swapchain_info.imageFormat = swapchainImageFormat;
	swapchain_info.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR; //Deprecated color space naming convention, needed to be used due to possibly outdated extensions
	swapchain_info.imageExtent = swapchainExtent;
	swapchain_info.imageArrayLayers = 1;
	swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (graphics_queue_family_index == present_queue_family_index) //If both graphics and present are from the same family we don't need to swap ownership between queues
	{
		swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_info.queueFamilyIndexCount = 0;
		swapchain_info.pQueueFamilyIndices = nullptr;
	}
	else //If they use separate queues then we need to specify a concurrent ownership
	{
		swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		uint32_t queueFamilyIndices[2] = { (graphics_queue_family_index, present_queue_family_index) };
		swapchain_info.queueFamilyIndexCount = 2;
		swapchain_info.pQueueFamilyIndices = queueFamilyIndices;
	}
	swapchain_info.preTransform = preTransform;
	swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_info.presentMode = swapchainPresentMode; //CAREFUL HERE SHOULD QUERY
	swapchain_info.clipped = VK_TRUE;

	if (swapchain_info.imageSharingMode == VK_SHARING_MODE_EXCLUSIVE)
	{
		std::cout << "Image sharing mode set to EXCLUSIVE, graphics and present queues are from the same family\n";
	}
	else if (swapchain_info.imageSharingMode == VK_SHARING_MODE_CONCURRENT)
	{
		std::cout << "Image sharing mode set to CONCURRENT, graphics and present queues are from different families\n";
	}

	//If we need to recreate our swapchain we can exchange the old info to the new swapchain
	VkSwapchainKHR oldSwapchain = swapchain;
	swapchain_info.oldSwapchain = oldSwapchain;

	result = vkCreateSwapchainKHR(logicalDevice, &swapchain_info, nullptr, &swapchain);
	if (result == VK_SUCCESS)
	{
		std::cout << "Swapchain created successfully.\n";
	}
	else
	{
		std::cout << "Swapchain creation failed.\n";
	}

	if (oldSwapchain)
	{
		vkDestroySwapchainKHR(logicalDevice, oldSwapchain, nullptr);
	}
}

void VulkanBase::CreateSwapchainImageViews()
{
	std::vector<VkImage> swapchainImages;
	uint32_t swapchainImageCount;
	result = vkGetSwapchainImagesKHR(logicalDevice, swapchain, &swapchainImageCount, nullptr);
	if (result == VK_SUCCESS)
	{
		std::cout << "Swapchain image count retrieved successfully.\n";
		std::cout << swapchainImageCount << " Swapchain image count.\n";
	}
	swapchainImages.resize(swapchainImageCount);
	result = vkGetSwapchainImagesKHR(logicalDevice, swapchain, &swapchainImageCount, swapchainImages.data());
	if (result == VK_SUCCESS)
	{
		std::cout << "Swapchain image handles assigned successfully.\n";
	}

	swapchainImageViews.resize(swapchainImages.size());

	for (uint32_t i = 0; i < swapchainImages.size(); i++)
	{
		CreateImageView(swapchainImages[i], swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, swapchainImageViews[i]);
	}

	std::cout << "---END SWAPCHAIN INFORMATION---\n\n";
}

void VulkanBase::CreateRenderPass()
{
	//Multisample resolve attachments
	VkAttachmentDescription multisample_color_attachment = {};
	multisample_color_attachment.format = swapchainImageFormat;
	multisample_color_attachment.samples = SAMPLE_COUNT;
	multisample_color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	multisample_color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	multisample_color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	multisample_color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	multisample_color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	multisample_color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference multisample_color_reference = {};
	multisample_color_reference.attachment = 1;
	multisample_color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription multisample_depth_attachment = {};
	multisample_depth_attachment.format = findDepthFormat();
	multisample_depth_attachment.samples = SAMPLE_COUNT;
	multisample_depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	multisample_depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	multisample_depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	multisample_depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	multisample_depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	multisample_depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference multisample_depth_reference = {};
	multisample_depth_reference.attachment = 3;
	multisample_depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	//Default Render Pass/Subpass Creation
	VkAttachmentDescription color_attachment = {};
	color_attachment.flags = 0;
	color_attachment.format = swapchainImageFormat;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; //CHANGED DEFAULT
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_reference = {};
	color_attachment_reference.attachment = 0;
	color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depth_attachment = {};
	depth_attachment.flags = 0;
	depth_attachment.format = findDepthFormat();
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_reference = {};
	depth_attachment_reference.attachment = 2;
	depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 2> resolveReferences{ multisample_color_reference, multisample_depth_reference };

	VkSubpassDescription subpass = {};
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = nullptr;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_reference;
	subpass.pResolveAttachments = resolveReferences.data();
	subpass.pDepthStencilAttachment = &depth_attachment_reference;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = nullptr;

	VkSubpassDependency subpass_dependency = {};
	subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpass_dependency.dstSubpass = 0;
	subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpass_dependency.srcAccessMask = 0;
	subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpass_dependency.dependencyFlags = 0;

	std::array<VkAttachmentDescription, 4> attachments = { multisample_color_attachment, color_attachment, multisample_depth_attachment, depth_attachment };

	VkRenderPassCreateInfo renderPass_info = {};
	renderPass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPass_info.pNext = nullptr;
	renderPass_info.flags = 0;
	renderPass_info.attachmentCount = (uint32_t)attachments.size();
	renderPass_info.pAttachments = attachments.data();
	renderPass_info.subpassCount = 1;
	renderPass_info.pSubpasses = &subpass;
	renderPass_info.dependencyCount = 1;
	renderPass_info.pDependencies = &subpass_dependency;

	result = vkCreateRenderPass(logicalDevice, &renderPass_info, nullptr, &renderPass);
	if (result == VK_SUCCESS)
	{
		std::cout << "Render Pass created successfully.\n";
	}
}

void VulkanBase::LoadShaders()
{
	std::vector<char> vertexShaderCode = readFile("shaders/vert.spv");
	std::vector<char> fragmentShaderCode = readFile("shaders/frag.spv");

	//Vertex Shader Information
	VkShaderModuleCreateInfo vertex_module_info = {};
	vertex_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertex_module_info.pNext = nullptr;
	vertex_module_info.flags = 0;
	vertex_module_info.codeSize = vertexShaderCode.size();
	vertex_module_info.pCode = (uint32_t *)vertexShaderCode.data();

	result = vkCreateShaderModule(logicalDevice, &vertex_module_info, nullptr, &vertexShaderModule);
	if (result == VK_SUCCESS)
	{
		std::cout << "Vertex Shader Module created successfully.\n";
	}

	VkPipelineShaderStageCreateInfo vertex_stage_info = {};
	vertex_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertex_stage_info.pNext = nullptr;
	vertex_stage_info.flags = 0;
	vertex_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertex_stage_info.module = vertexShaderModule;
	vertex_stage_info.pName = "main"; //Our entry point for the shader possible here to have multiple shaders combined for an uber-shader
	vertex_stage_info.pSpecializationInfo = nullptr;

	//Fragment Shader Information
	VkShaderModuleCreateInfo fragment_module_info = {};
	fragment_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragment_module_info.pNext = nullptr;
	fragment_module_info.flags = 0;
	fragment_module_info.codeSize = fragmentShaderCode.size();
	fragment_module_info.pCode = (uint32_t *)fragmentShaderCode.data();

	result = vkCreateShaderModule(logicalDevice, &fragment_module_info, nullptr, &fragmentShaderModule);
	if (result == VK_SUCCESS)
	{
		std::cout << "Fragment Shader Module created successfully.\n";
	}

	VkPipelineShaderStageCreateInfo fragment_stage_info = {};
	fragment_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragment_stage_info.pNext = nullptr;
	fragment_stage_info.flags = 0;
	fragment_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragment_stage_info.module = fragmentShaderModule;
	fragment_stage_info.pName = "main"; //Entry point for the shader, can possibly create an uber-shader here
	fragment_stage_info.pSpecializationInfo = nullptr;
	
	shaderStages[0] = vertex_stage_info;
	shaderStages[1] = fragment_stage_info;
}

void VulkanBase::CreateDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding ubo_layout_binding = {};
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	ubo_layout_binding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding texture_sampler_layout_binding = {};
	texture_sampler_layout_binding.binding = 1;
	texture_sampler_layout_binding.descriptorCount = 1;
	texture_sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texture_sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	texture_sampler_layout_binding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { ubo_layout_binding, texture_sampler_layout_binding };

	VkDescriptorSetLayoutCreateInfo layout_info = {};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.pNext = nullptr;
	layout_info.flags = 0;
	layout_info.bindingCount = (uint32_t)bindings.size();
	layout_info.pBindings = bindings.data();

	result = vkCreateDescriptorSetLayout(logicalDevice, &layout_info, nullptr, &descriptorSetLayout);
	if (result == VK_SUCCESS)
	{
		std::cout << "Descriptor Set Layout created successfully.\n";
	}
} //FINE

void VulkanBase::CreateGraphicsPipeline()
{
	/////Fixed-Function Pipeline States

	VkVertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
	std::array<VkVertexInputAttributeDescription, 3> attributeDescription = Vertex::getAttributeDescriptions();

	//Vertex Input
	VkPipelineVertexInputStateCreateInfo vertex_input_state_info = {};
	vertex_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state_info.pNext = nullptr;
	vertex_input_state_info.flags = 0;
	vertex_input_state_info.vertexBindingDescriptionCount = 1;
	vertex_input_state_info.pVertexBindingDescriptions = &bindingDescription;
	vertex_input_state_info.vertexAttributeDescriptionCount = (uint32_t)attributeDescription.size();
	vertex_input_state_info.pVertexAttributeDescriptions = attributeDescription.data();

	//Input Assembly
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info = {};
	input_assembly_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state_info.pNext = nullptr;
	input_assembly_state_info.flags = 0;
	input_assembly_state_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_state_info.primitiveRestartEnable = VK_FALSE;

	//Tessellation
	//SKIPPED AS TESSELLATION NOT USED

	//Viewport and Scissor
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapchainExtent.width;
	viewport.height = (float)swapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchainExtent;

	VkPipelineViewportStateCreateInfo viewport_scissor_state_info = {};
	viewport_scissor_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_scissor_state_info.pNext = nullptr;
	viewport_scissor_state_info.flags = 0;
	viewport_scissor_state_info.viewportCount = 1;
	viewport_scissor_state_info.pViewports = &viewport;
	viewport_scissor_state_info.scissorCount = 1;
	viewport_scissor_state_info.pScissors = &scissor;

	//Rasterization
	VkPipelineRasterizationStateCreateInfo rasterizer_state_info = {};
	rasterizer_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_state_info.pNext = nullptr;
	rasterizer_state_info.flags = 0;
	rasterizer_state_info.depthClampEnable = VK_FALSE;
	rasterizer_state_info.rasterizerDiscardEnable = VK_FALSE;
	rasterizer_state_info.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer_state_info.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer_state_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer_state_info.depthBiasEnable = VK_FALSE;
	rasterizer_state_info.depthBiasConstantFactor = 0.0f;
	rasterizer_state_info.depthBiasClamp = 0.0f;
	rasterizer_state_info.depthBiasSlopeFactor = 0.0f;
	rasterizer_state_info.lineWidth = 1.0f;

	//Multisampling
	VkPipelineMultisampleStateCreateInfo multisample_state_info = {};
	multisample_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state_info.pNext = nullptr;	
	multisample_state_info.flags = 0;
	multisample_state_info.rasterizationSamples = SAMPLE_COUNT;
	multisample_state_info.sampleShadingEnable = VK_FALSE;
	multisample_state_info.minSampleShading = 1.0f;
	multisample_state_info.pSampleMask = nullptr;
	multisample_state_info.alphaToCoverageEnable = VK_FALSE;
	multisample_state_info.alphaToOneEnable = VK_FALSE;

	//Depth and Stencil Test
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info = {};
	depth_stencil_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state_info.pNext = nullptr;
	depth_stencil_state_info.flags = 0;
	depth_stencil_state_info.depthTestEnable = VK_TRUE;
	depth_stencil_state_info.depthWriteEnable = VK_TRUE;
	depth_stencil_state_info.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil_state_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state_info.stencilTestEnable = VK_FALSE;
	depth_stencil_state_info.front = {}; //Stencil Test Parameters
	depth_stencil_state_info.back = {}; //Stencil Test Parameters
	depth_stencil_state_info.minDepthBounds = 0.0f; //Depth Bounds Test Parameter
	depth_stencil_state_info.maxDepthBounds = 1.0f; //Depth Bounds Test Parameter

	//Color Blending

	//Attachment State - Settings for attached framebuffer
	VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
	color_blend_attachment_state.blendEnable = VK_FALSE;
	color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	//Color Blending State Info - Global blending rather than attached framebuffer
	VkPipelineColorBlendStateCreateInfo color_blend_state_info = {};
	color_blend_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_state_info.pNext = nullptr;
	color_blend_state_info.flags = 0;
	color_blend_state_info.logicOpEnable = VK_FALSE;
	color_blend_state_info.logicOp = VK_LOGIC_OP_COPY;
	color_blend_state_info.attachmentCount = 1;
	color_blend_state_info.pAttachments = &color_blend_attachment_state;
	color_blend_state_info.blendConstants[0] = 0.0f;
	color_blend_state_info.blendConstants[1] = 0.0f;
	color_blend_state_info.blendConstants[2] = 0.0f;
	color_blend_state_info.blendConstants[3] = 0.0f;

	//Dynamic States - Only set if dynamic state enabled for certain fixed-function features allowing for changes without pipeline recreation
	//Set to nullptr for now at pipeline creation as no dynamic states are enabled

	VkDescriptorSetLayout setLayouts[] = { descriptorSetLayout };
	//Pipeline Layout - Here we can use uniform variables for use in shaders given as descriptor sets. In addition, push constants can be used.
	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.pNext = nullptr;
	pipeline_layout_info.flags = 0;
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = setLayouts;
	pipeline_layout_info.pushConstantRangeCount = 0;
	pipeline_layout_info.pPushConstantRanges = nullptr;

	result = vkCreatePipelineLayout(logicalDevice, &pipeline_layout_info, nullptr, &pipelineLayout);
	if (result == VK_SUCCESS)
	{
		std::cout << "Pipeline Layout created successfully.\n";
	}

	//Graphics Pipeline Info
	VkGraphicsPipelineCreateInfo graphics_pipeline_info = {};
	graphics_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphics_pipeline_info.pNext = nullptr;
	graphics_pipeline_info.flags = 0;
	graphics_pipeline_info.stageCount = 2;
	graphics_pipeline_info.pStages = shaderStages;
	graphics_pipeline_info.pVertexInputState = &vertex_input_state_info;
	graphics_pipeline_info.pInputAssemblyState = &input_assembly_state_info;
	graphics_pipeline_info.pTessellationState = nullptr;
	graphics_pipeline_info.pViewportState = &viewport_scissor_state_info;
	graphics_pipeline_info.pRasterizationState = &rasterizer_state_info;
	graphics_pipeline_info.pMultisampleState = &multisample_state_info;
	graphics_pipeline_info.pDepthStencilState = &depth_stencil_state_info;
	graphics_pipeline_info.pColorBlendState = &color_blend_state_info;
	graphics_pipeline_info.pDynamicState = nullptr;
	graphics_pipeline_info.layout = pipelineLayout;
	graphics_pipeline_info.renderPass = renderPass;
	graphics_pipeline_info.subpass = 0;
	graphics_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

	//Create a Graphics Pipeline
	result = vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &graphics_pipeline_info, nullptr, &graphicsPipeline); //Pipeline cache can be set here for pipeline recreation
	if (result == VK_SUCCESS)
	{
		std::cout << "Graphics Pipeline created successfully.\n";
	}
}

void VulkanBase::CreateFramebuffers()
{
	framebuffers.resize(swapchainImageViews.size());

	CreateMultisampleTargets();

	for (uint32_t i = 0; i < framebuffers.size(); i++) 
	{
		std::array<VkImageView, 4> attachments = { multisampleColourImageView, swapchainImageViews[i], multisampleDepthImageView, depthImageView };

		VkFramebufferCreateInfo framebuffer_info = {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.pNext = nullptr;
		framebuffer_info.flags = 0;
		framebuffer_info.renderPass = renderPass;
		framebuffer_info.attachmentCount = (uint32_t)attachments.size();
		framebuffer_info.pAttachments = attachments.data();
		framebuffer_info.width = swapchainExtent.width;
		framebuffer_info.height = swapchainExtent.height;
		framebuffer_info.layers = 1;
	
		result = vkCreateFramebuffer(logicalDevice, &framebuffer_info, nullptr, &framebuffers[i]);
		if (result == VK_SUCCESS)
		{
			std::cout << "Framebuffer " << i << " created successfully.\n";
		}
	}
}

void VulkanBase::CreateCommandPool(VkCommandPool &commandpool, VkCommandPoolCreateFlags flags)
{
	VkCommandPoolCreateInfo command_pool_info = {};
	command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_info.pNext = nullptr;
	command_pool_info.flags = flags;
	command_pool_info.queueFamilyIndex = graphics_queue_family_index;

	//Create a command pool to assign command buffers from
	result = vkCreateCommandPool(logicalDevice, &command_pool_info, nullptr, &commandpool);
	if (result == VK_SUCCESS)
	{
		std::cout << "Command Pool created successfully.\n";
	}
}

void VulkanBase::CreateModel()
{
	tinyobj::attrib_t attributes;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string error;
	
	if (!LoadObj(&attributes, &shapes, &materials, &error, MODEL_PATH.c_str()))
	{
		std::cout << error;
	}
	else
	{
		std::cout << "Model Loaded Successfully.\n";
	}

	std::unordered_map<Vertex, int> uniqueVertices = {};

	for (const auto &shape : shapes)
	{
		for (const auto &index : shape.mesh.indices)
		{
			Vertex vertex = {};
			
			vertex.position = {
				attributes.vertices[3 * index.vertex_index + 0],
				attributes.vertices[3 * index.vertex_index + 1],
				attributes.vertices[3 * index.vertex_index + 2]
			};

			vertex.texCoord = {
				attributes.texcoords[2 * index.texcoord_index + 0],
				1.0f - attributes.texcoords[2 * index.texcoord_index + 1]
			};

			vertex.color = { 1.0f, 1.0f, 1.0f };

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = (uint32_t)vertices.size();
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}
	}
}

void VulkanBase::CreateVertexBuffer()
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	//Map our vertex data to the correct memory buffer
	void *data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

	copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
}

void VulkanBase::CreateIndexBuffer()
{
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	//Map our vertex data to the correct memory buffer
	void *data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

	copyBuffer(stagingBuffer, indexBuffer, bufferSize);
}

void VulkanBase::CreateUniformBuffer()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformStagingBuffer, uniformStagingBufferMemory);

	CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, uniformBuffer, uniformBufferMemory);
}

void VulkanBase::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> pool_sizes = {};
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes[0].descriptorCount = 1;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.pNext = nullptr;
	pool_info.flags = 0;
	pool_info.poolSizeCount = (uint32_t)pool_sizes.size();
	pool_info.pPoolSizes = pool_sizes.data();
	pool_info.maxSets = 1;

	result = vkCreateDescriptorPool(logicalDevice, &pool_info, nullptr, &descriptorPool);
	if (result == VK_SUCCESS)
	{
		std::cout << "Descriptor Pool created successfully.\n";
	}
}

void VulkanBase::CreateDescriptorSet()
{
	VkDescriptorSetLayout layouts[] = { descriptorSetLayout };
	VkDescriptorSetAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocate_info.pNext = nullptr;
	allocate_info.descriptorPool = descriptorPool;
	allocate_info.descriptorSetCount = 1;
	allocate_info.pSetLayouts = layouts;

	result = vkAllocateDescriptorSets(logicalDevice, &allocate_info, &descriptorSet);
	if (result == VK_SUCCESS)
	{
		std::cout << "Descriptor Set allocated successfully.\n";
	}

	VkDescriptorBufferInfo descriptor_buffer_info = {};
	descriptor_buffer_info.buffer = uniformBuffer;
	descriptor_buffer_info.offset = 0;
	descriptor_buffer_info.range = sizeof(UniformBufferObject);

	VkDescriptorImageInfo descriptor_image_info = {};
	descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptor_image_info.imageView = textureImageView;
	descriptor_image_info.sampler = textureSampler;

	std::array<VkWriteDescriptorSet, 2> descriptor_writes = {};
	descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_writes[0].pNext = nullptr;
	descriptor_writes[0].dstSet = descriptorSet;
	descriptor_writes[0].dstBinding = 0;
	descriptor_writes[0].dstArrayElement = 0;
	descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptor_writes[0].descriptorCount = 1;
	descriptor_writes[0].pBufferInfo = &descriptor_buffer_info;
	descriptor_writes[0].pImageInfo = nullptr;
	descriptor_writes[0].pTexelBufferView = nullptr;

	descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptor_writes[1].pNext = nullptr;
	descriptor_writes[1].dstSet = descriptorSet;
	descriptor_writes[1].dstBinding = 1;
	descriptor_writes[1].dstArrayElement = 0;
	descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptor_writes[1].descriptorCount = 1;
	descriptor_writes[1].pBufferInfo = nullptr;
	descriptor_writes[1].pImageInfo = &descriptor_image_info;
	descriptor_writes[1].pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(logicalDevice, (uint32_t)descriptor_writes.size(), descriptor_writes.data(), 0, nullptr);
}

void VulkanBase::CreateDepthImageResources()
{
	VkFormat depthFormat = findDepthFormat();

	CreateImage(swapchainExtent.width, swapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLE_COUNT_1_BIT, depthImage, depthImageMemory);

	CreateImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthImageView);

	transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void VulkanBase::CreateTextureImage()
{
	int texWidth, texHeight, texChannels;
	stbi_uc *pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels)
	{
		std::cout << "Failed to load texture.\n";
	}

	VkImage stagingImage = VK_NULL_HANDLE;
	VkDeviceMemory stagingImageMemory = VK_NULL_HANDLE;
	
	CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_SAMPLE_COUNT_1_BIT, stagingImage, stagingImageMemory);

	VkImageSubresource image_subresource = {};
	image_subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_subresource.mipLevel = 0;
	image_subresource.arrayLayer = 0;

	VkSubresourceLayout stagingImageLayout;
	vkGetImageSubresourceLayout(logicalDevice, stagingImage, &image_subresource, &stagingImageLayout);

	void *data;
	vkMapMemory(logicalDevice, stagingImageMemory, 0, imageSize, 0, &data);

	if (stagingImageLayout.rowPitch == texWidth * 4)
	{
		memcpy(data, pixels, (size_t)imageSize);
	}
	else
	{
		uint8_t *dataBytes = reinterpret_cast<uint8_t*>(data);

		for (int u = 0; u < texHeight; u++)
		{
			memcpy(&dataBytes[u * stagingImageLayout.rowPitch], &pixels[u * texWidth * 4], texWidth * 4);
		}
	}

	vkUnmapMemory(logicalDevice, stagingImageMemory);

	stbi_image_free(pixels);

	CreateImage(texWidth, texWidth, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLE_COUNT_1_BIT, textureImage, textureImageMemory);

	transitionImageLayout(stagingImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyImage(stagingImage, textureImage, texWidth, texHeight);
	
	transitionImageLayout(textureImage, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkFreeMemory(logicalDevice, stagingImageMemory, nullptr);
	vkDestroyImage(logicalDevice, stagingImage, nullptr);
}

void VulkanBase::CreateTextureImageView()
{
	CreateImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, textureImageView);
}

void VulkanBase::CreateTextureSampler()
{
	VkSamplerCreateInfo sampler_info = {};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.pNext = nullptr;
	sampler_info.flags = 0;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.anisotropyEnable = VK_TRUE;
	sampler_info.maxAnisotropy = 16;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;

	result = vkCreateSampler(logicalDevice, &sampler_info, nullptr, &textureSampler);
	if (result == VK_SUCCESS)
	{
		std::cout << "Texture Sampler created successfully.\n";
	}
}

void VulkanBase::CreateCommandBuffers()
{
	//Check there are no existing command buffers allocated
	if (commandBuffers.size() > 0)
	{
		vkFreeCommandBuffers(logicalDevice, commandPool, (uint32_t)commandBuffers.size(), commandBuffers.data());
	}

	//Allocate command buffer memory from command pool
	commandBuffers.resize(framebuffers.size());

	//Info for an allocated command buffer from the created command pool
	VkCommandBufferAllocateInfo command_buffer_info = {};
	command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_info.pNext = nullptr;
	command_buffer_info.commandPool = commandPool;
	command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; //Enum to 0
	command_buffer_info.commandBufferCount = (uint32_t)commandBuffers.size(); //Amount needs to be in accordance with amount in commandBuffer vector

	result = vkAllocateCommandBuffers(logicalDevice, &command_buffer_info, commandBuffers.data()); //Allocated buffers must match the bufferCount in info
	if (result == VK_SUCCESS)
	{
		std::cout << command_buffer_info.commandBufferCount << " Command Buffer allocated successfully.\n";
	}
}

void VulkanBase::RecordCommandBuffers()
{
	/////Recording for command buffers - IMPORTANT HERE FOR MULTI-THREAD IMPLEMENTATION, MULTIPLE RECORDINGS ACROSS DIFFERENT THREADS

	for (uint32_t i = 0; i < commandBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo command_buffer_begin_info = {};
		command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_buffer_begin_info.pNext = nullptr;
		command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		command_buffer_begin_info.pInheritanceInfo = nullptr; //Used for secondary command buffers

		result = vkBeginCommandBuffer(commandBuffers[i], &command_buffer_begin_info);
		if (result == VK_SUCCESS)
		{
			std::cout << "Begin Recording Successful.\n";
		}

		VkRenderPassBeginInfo renderpass_begin_info = {};
		renderpass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderpass_begin_info.pNext = nullptr;
		renderpass_begin_info.renderPass = renderPass;
		renderpass_begin_info.framebuffer = framebuffers[i];
		renderpass_begin_info.renderArea.offset.x = 0;
		renderpass_begin_info.renderArea.offset.y = 0;
		renderpass_begin_info.renderArea.extent.width = swapchainExtent.width;
		renderpass_begin_info.renderArea.extent.height = swapchainExtent.height;

		std::array<VkClearValue, 3> clearValues = {};
		clearValues[0].color = { 0.2f, 0.2f, 0.2f, 0.2f };
		clearValues[1].color = { 0.2f, 0.2f, 0.2f, 0.2f };
		clearValues[2].depthStencil = { 1.0f, 0 };
		renderpass_begin_info.clearValueCount = (uint32_t)clearValues.size();
		renderpass_begin_info.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffers[i], &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		VkBuffer vertexBuffers[] = { vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

		vkCmdDrawIndexed(commandBuffers[i], (uint32_t)indices.size(), 1, 0, 0, 0);

		vkCmdEndRenderPass(commandBuffers[i]);

		result = vkEndCommandBuffer(commandBuffers[i]);
		if (result == VK_SUCCESS)
		{
			std::cout << "End Recording Successful.\n";
		}
	}
}

void VulkanBase::CreateSemaphores()
{
	VkSemaphoreCreateInfo semaphore_info = {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphore_info.pNext = nullptr;
	semaphore_info.flags = 0;

	result = vkCreateSemaphore(logicalDevice, &semaphore_info, nullptr, &imageAcquiredSemaphore);
	if (result == VK_SUCCESS)
	{
		std::cout << "Image Acquired Semaphore created successfully.\n";
	}
	result = vkCreateSemaphore(logicalDevice, &semaphore_info, nullptr, &renderFinishedSemaphore);
	if (result == VK_SUCCESS)
	{
		std::cout << "Render Finished Semaphore created successfully.\n";
	}
}

void VulkanBase::AcquireSubmitPresent()
{
	startFrame = std::chrono::steady_clock::now();

	uint32_t imageIndex;
	//VkPipelineStageFlags pipeline_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	result = vkAcquireNextImageKHR(logicalDevice, swapchain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE, &imageIndex);
	if (result == VK_SUCCESS)
	{
#ifdef DEBUG
		std::cout << "---Acquired image from swapchain---.\n";
#endif // DEBUG
	}
	else if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapchain();
		return;
	}

	VkSemaphore waitSemaphores[] = { imageAcquiredSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = nullptr;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = waitSemaphores;
	submit_info.pWaitDstStageMask = waitStages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &commandBuffers[imageIndex];
	VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signalSemaphores;

	result = vkQueueSubmit(graphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
	if (result == VK_SUCCESS)
	{
#ifdef DEBUG
		std::cout << "Command Buffer queued for execution.\n";
#endif // DEBUG
	}

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signalSemaphores;
	present_info.swapchainCount = 1;
	VkSwapchainKHR swapChains[] = { swapchain };
	present_info.pSwapchains = swapChains;
	present_info.pImageIndices = &imageIndex;
	present_info.pResults = nullptr;

	result = vkQueuePresentKHR(presentQueue, &present_info);
	if (result == VK_SUCCESS)
	{
#ifdef DEBUG
		std::cout << "Presented successfully to the present queue.\n\n\n";
#endif // DEBUG
	}
	else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		RecreateSwapchain();
	}

	endFrame = std::chrono::steady_clock::now();

	auto elapsedTime = std::chrono::duration_cast<std::chrono::duration<double>>(endFrame - startFrame).count();

	if (elapsedTime > 0)
	{
		drawFrames++;
		elapsedSum += elapsedTime;
	}
}

void VulkanBase::UpdateUniformBuffer()
{ 
	glm::mat4 model = glm::scale(glm::mat4(), glm::vec3(0.05f, 0.05f, 0.05f));
	model *= glm::rotate(glm::mat4(), glm::radians(105.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	model *= glm::rotate(glm::mat4(), glm::radians(10.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	model *= glm::rotate(glm::mat4(), glm::radians(120.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	model *= glm::translate(glm::mat4(), glm::vec3(-15.0f, 5.0f, 0.0f));
	glm::mat4 view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 projection = glm::perspective(glm::radians(65.0f), swapchainExtent.width / (float)swapchainExtent.height, 0.1f, 10.0f);
	projection[1][1] *= -1; //Flip the y axis

	UniformBufferObject ubo = {};
	ubo.mvp = projection * view * model;

	void *data;
	vkMapMemory(logicalDevice, uniformStagingBufferMemory, 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(logicalDevice, uniformStagingBufferMemory);

	copyBuffer(uniformStagingBuffer, uniformBuffer, sizeof(ubo));
}

void VulkanBase::RecreateSwapchain()
{
	//Wait until our we aren't doing anything
	vkDeviceWaitIdle(logicalDevice);

	vkDestroyImageView(logicalDevice, depthImageView, nullptr);
	vkFreeMemory(logicalDevice, depthImageMemory, nullptr);
	vkDestroyImage(logicalDevice, depthImage, nullptr);

	vkDestroyImageView(logicalDevice, multisampleDepthImageView, nullptr);
	vkFreeMemory(logicalDevice, multisampleDepthImageMemory, nullptr);
	vkDestroyImage(logicalDevice, multisampleDepthImage, nullptr);
	vkDestroyImageView(logicalDevice, multisampleColourImageView, nullptr);
	vkFreeMemory(logicalDevice, multisampleColourImageMemory, nullptr);
	vkDestroyImage(logicalDevice, multisampleColourImage, nullptr);

	//Destroy previous Vulkan systems
	for (uint32_t i = 0; i < framebuffers.size(); i++)
	{
		vkDestroyFramebuffer(logicalDevice, framebuffers[i], nullptr);
	}
	vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
	for (uint32_t i = 0; i < swapchainImageViews.size(); i++)
	{
		vkDestroyImageView(logicalDevice, swapchainImageViews[i], nullptr);
	}

	//Create new ones
	CreateSwapchain();
	CreateSwapchainImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateDepthImageResources();
	CreateFramebuffers();
	CreateCommandBuffers();
	RecordCommandBuffers();

	UpdateUniformBuffer();
}

void VulkanBase::CreateMultisampleTargets()
{
	VkFormat depthFormat = findDepthFormat();

	CreateMultisampleImage(swapchainExtent.width, swapchainExtent.height, swapchainImageFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, SAMPLE_COUNT, multisampleColourImage, multisampleColourImageMemory);

	CreateImageView(multisampleColourImage, swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, multisampleColourImageView);

	CreateMultisampleImage(swapchainExtent.width, swapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, SAMPLE_COUNT, multisampleDepthImage, multisampleDepthImageMemory);

	CreateImageView(multisampleDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, multisampleDepthImageView);
}

void VulkanBase::windowResize(GLFWwindow *window, int width, int height)
{
	if (width == 0 || height == 0) 
	{ 
		return; 
	}

	VulkanBase *vulkan = (VulkanBase*)(glfwGetWindowUserPointer(window));
	vulkan->RecreateSwapchain();
}

void VulkanBase::showAverages()
{
	double averageFPS = fpssum / fpscount;
	double averageMSPF = mspfsum / mspfcount;
	std::cout << "\n\nAverage FPS for scene: " << averageFPS << " frames per second.\n";
	std::cout << "Average MSPF for scene: " << averageMSPF << " milliseconds per frame.\n";
}

void VulkanBase::windowTimer()
{
	if (timedWindow)
	{
		auto currentTime = std::chrono::high_resolution_clock::now();

		auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

		if (elapsedTime >= 60)
		{
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}		
	}
}

uint32_t VulkanBase::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	//Retrieve the memory properties of the device i.e types and heaps
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevices[0], &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		if (typeFilter & (1 << i) && ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties))
		{
			return i;
		}
	}

	std::cout << "No suitable memory types found on physical device.\n";
}

VkFormat VulkanBase::findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(physicalDevices[0], format, &properties);

		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features)
		{
			return format;
		}

		std::cout << "Failed to obtain supported format.\n";
	}
}

VkFormat VulkanBase::findDepthFormat()
{
	return findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanBase::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer transferCommandBuffer = beginSingleTransferCommand();

	VkBufferCopy copy_region = {};
	//copy_region.srcOffset = 0;
	//copy_region.dstOffset = 0;
	copy_region.size = size;

	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &copy_region);

	endSingleTransferCommand(transferCommandBuffer);
}

void VulkanBase::copyImage(VkImage srcImage, VkImage dstImage, uint32_t imageWidth, uint32_t imageHeight)
{
	VkCommandBuffer transferCommandBuffer = beginSingleTransferCommand();

	VkImageSubresourceLayers subresource_layers = {};
	subresource_layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource_layers.baseArrayLayer = 0;
	subresource_layers.mipLevel = 0;
	subresource_layers.layerCount = 1;

	VkImageCopy copy_region = {};
	copy_region.srcSubresource = subresource_layers;
	copy_region.dstSubresource = subresource_layers;
	copy_region.srcOffset = { 0, 0, 0 };
	copy_region.dstOffset = { 0, 0, 0 };
	copy_region.extent.width = imageWidth;
	copy_region.extent.height = imageHeight;
	copy_region.extent.depth = 1;

	vkCmdCopyImage(transferCommandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

	endSingleTransferCommand(transferCommandBuffer);
}

void VulkanBase::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory)
{
	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.pNext = nullptr;
	buffer_info.flags = 0;
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; //Only used by the graphics queue not present

	result = vkCreateBuffer(logicalDevice, &buffer_info, nullptr, &buffer);
	if (result == VK_SUCCESS)
	{
		std::cout << "Vertex Buffer created successfully.\n";
	}

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(logicalDevice, buffer, &memoryRequirements);

	VkMemoryAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.pNext = nullptr;
	allocate_info.allocationSize = memoryRequirements.size;
	allocate_info.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties);

	result = vkAllocateMemory(logicalDevice, &allocate_info, nullptr, &bufferMemory);
	if (result == VK_SUCCESS)
	{
		std::cout << "Memory allocated to buffer successfully.\n";
	}

	result = vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
	if (result == VK_SUCCESS)
	{
		std::cout << "Buffer memory bound to buffer successfully.\n";
	}
}

void VulkanBase::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkSampleCountFlagBits samples, VkImage &image, VkDeviceMemory &imageMemory)
{
	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.pNext = nullptr;
	image_info.flags = 0;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;
	image_info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.samples = samples; //RELEVANT FOR MULTI-SAMPLING
	
	result = vkCreateImage(logicalDevice, &image_info, nullptr, &image);
	if (result == VK_SUCCESS)
	{
		std::cout << "Image created successfully.\n";
	}

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(logicalDevice, image, &memoryRequirements);

	VkMemoryAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.allocationSize = memoryRequirements.size;
	allocate_info.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties);

	result = vkAllocateMemory(logicalDevice, &allocate_info, nullptr, &imageMemory);
	if (result == VK_SUCCESS)
	{
		std::cout << "Image memory allocated successfully.\n";
	}

	vkBindImageMemory(logicalDevice, image, imageMemory, 0);
}

void VulkanBase::CreateMultisampleImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkSampleCountFlagBits samples, VkImage &image, VkDeviceMemory &imageMemory)
{
	VkImageCreateInfo image_info = {};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.pNext = nullptr;
	image_info.flags = 0;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.format = format;
	image_info.tiling = tiling;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_info.usage = usage;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.samples = samples; //RELEVANT FOR MULTI-SAMPLING

	result = vkCreateImage(logicalDevice, &image_info, nullptr, &image);
	if (result == VK_SUCCESS)
	{
		std::cout << "Image created successfully.\n";
	}

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(logicalDevice, image, &memoryRequirements);

	VkMemoryAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.allocationSize = memoryRequirements.size;
	allocate_info.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties);

	result = vkAllocateMemory(logicalDevice, &allocate_info, nullptr, &imageMemory);
	if (result == VK_SUCCESS)
	{
		std::cout << "Image memory allocated successfully.\n";
	}

	vkBindImageMemory(logicalDevice, image, imageMemory, 0);
}

void VulkanBase::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView &imageView)
{
	VkImageViewCreateInfo image_view_info = {};
	image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_info.pNext = nullptr;
	image_view_info.flags = 0;
	image_view_info.image = image;
	image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_info.format = format;
	image_view_info.subresourceRange.aspectMask = aspectFlags;
	image_view_info.subresourceRange.baseMipLevel = 0;
	image_view_info.subresourceRange.levelCount = 1;
	image_view_info.subresourceRange.baseArrayLayer = 0;
	image_view_info.subresourceRange.layerCount = 1;
	image_view_info.components.r = VK_COMPONENT_SWIZZLE_R;
	image_view_info.components.g = VK_COMPONENT_SWIZZLE_G;
	image_view_info.components.b = VK_COMPONENT_SWIZZLE_B;
	image_view_info.components.a = VK_COMPONENT_SWIZZLE_A;


	result = vkCreateImageView(logicalDevice, &image_view_info, nullptr, &imageView);
	if (result == VK_SUCCESS)
	{
		std::cout << "Image View created successfully.\n";
	}
}

VkCommandBuffer VulkanBase::beginSingleTransferCommand()
{
	VkCommandBufferAllocateInfo allocate_info = {};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.pNext = nullptr;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandPool = transferPool;
	allocate_info.commandBufferCount = 1;

	VkCommandBuffer transferCommandBuffer;
	result = vkAllocateCommandBuffers(logicalDevice, &allocate_info, &transferCommandBuffer);
	if (result == VK_SUCCESS)
	{
#ifdef DEBUG
		std::cout << allocate_info.commandBufferCount << " command buffers allocated for transfer use.\n";
#endif // DEBUG
	}

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = nullptr;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	result = vkBeginCommandBuffer(transferCommandBuffer, &begin_info);
	if (result = VK_SUCCESS)
	{
		std::cout << "Begin transfer recording successful.\n";
	}

	return transferCommandBuffer;
}

void VulkanBase::endSingleTransferCommand(VkCommandBuffer transferCommandBuffer)
{
	result = vkEndCommandBuffer(transferCommandBuffer);
	if (result == VK_SUCCESS)
	{
#ifdef DEBUG
		std::cout << "Transfer successful.\n";
#endif // DEBUG
	}

	VkSubmitInfo submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = nullptr;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &transferCommandBuffer;

	//NO FENCES USED - COULD BE USED TO EXECUTE MULTIPLE TRANSFERS OPERATIONS AND WAIT FOR ALL TO FINISH
	vkQueueSubmit(graphicsQueue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(logicalDevice, transferPool, 1, &transferCommandBuffer);
}

void VulkanBase::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer transferCommandBuffer = beginSingleTransferCommand();

	VkImageMemoryBarrier image_barrier = {};
	image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	image_barrier.pNext = nullptr;
	image_barrier.oldLayout = oldLayout;
	image_barrier.newLayout = newLayout;
	image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_barrier.image = image;
	image_barrier.subresourceRange.baseMipLevel = 0;
	image_barrier.subresourceRange.levelCount = 1;
	image_barrier.subresourceRange.baseArrayLayer = 0;
	image_barrier.subresourceRange.layerCount = 1;

	if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		image_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		image_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		image_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		image_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		image_barrier.srcAccessMask = 0;
		image_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}
	else
	{
		std::cout << "Unsupported image layout transition.\n";
	}

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		
		if (hasStencilComponent(format))
		{
			image_barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else
	{
		image_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}


	vkCmdPipelineBarrier(transferCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_barrier);

	endSingleTransferCommand(transferCommandBuffer);
}

void VulkanBase::showFPS(GLFWwindow *pWindow)
{
	//Measure speed
	double currentTime = glfwGetTime();
	double delta = currentTime - lastTime;
	frames++;

	double fps = float(frames) / delta;
	double mspf = 1000.0f / (float)frames;

	//Only display once a second
	if (delta >= 1.0) 
	{ 
		std::cout << 1000.0f / float(frames) << " milliseconds per frame.\n";
		std::cout << (float)frames / (currentTime - lastTime) << " frames per second.\n";

		std::stringstream ss;
		ss << "Vulkan Comparison" << " " << " " << fps << " FPS.    " << mspf << " MSPF.";

		glfwSetWindowTitle(pWindow, ss.str().c_str());

		frames = 0;
		lastTime = currentTime;

		fpscount++;
		fpssum += fps;
		mspfcount++;
		mspfsum += mspf;
	}
}

bool VulkanBase::checkValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}
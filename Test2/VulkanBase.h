#pragma once

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <GLFW/glfw3.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <array>
#include <chrono>
#include <unordered_map>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/hash.hpp"

#include "ReadFile.h"
#include "stb_image.h"
#include "tiny_obj_loader.h"

#define SAMPLE_COUNT VK_SAMPLE_COUNT_4_BIT

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription getBindingDescription() 
	{
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		
		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() 
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, position);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}

	bool operator == (const Vertex &other) const { return position == other.position && color == other.color && texCoord == other.texCoord; }
};

template<> struct std::hash<Vertex>
{
	size_t operator()(Vertex const &vertex) const {
		return ((std::hash<glm::vec3>()(vertex.position) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (std::hash<glm::vec2>()(vertex.texCoord) << 1);
	}
};

//Model-View-Projection matrix
struct UniformBufferObject {
	glm::mat4 mvp;
};

const std::vector<const char*> validationLayers = {
	"VK_LAYER_LUNARG_standard_validation"
};

const bool enableValidationLayers = false;

const bool timedWindow = true;

const bool multiCopy = false;

class VulkanBase
{
private:
	//Base Constructor
	VulkanBase();
	~VulkanBase();

	//Intended window size
	const uint32_t WIDTH = 1920;
	const uint32_t HEIGHT = 1080;

	const std::string MODEL_PATH = "models/vari3d.obj";
	const std::string TEXTURE_PATH = "textures/vari3d.jpg";

	uint32_t graphics_queue_family_index = UINT32_MAX;
	uint32_t present_queue_family_index = UINT32_MAX;

	//Timing/Testing variables
	double lastTime = glfwGetTime();
	int frames = 1000;
	int fpscount = 0;
	double fpssum = 0;
	int mspfcount = 0;
	double mspfsum = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;


	std::chrono::time_point<std::chrono::steady_clock> startFrame;
	std::chrono::time_point<std::chrono::steady_clock> endFrame;
	double drawFrames = 0;
	double elapsedSum = 0;


	//Validation Handle
	VkResult result;

	//Display context/window
	VkSurfaceKHR surface;

	//Abstract and physical hardware information
	VkInstance instance;
	std::vector<VkPhysicalDevice> physicalDevices;
	VkDevice logicalDevice;

	//Queue Handles
	VkQueue graphicsQueue; //Handle on our graphics queue - Destroyed on Logical Device destruction (only when idle)
	VkQueue presentQueue; //Handle on our present queue - Destroyed on Logical Device destruction (only when idle)

	//Swapchain Information
	VkFormat swapchainImageFormat; //Our chosen format for the swapchain from those available on the device
	VkExtent2D swapchainExtent;
	VkSwapchainKHR swapchain;
	std::vector<VkImageView> swapchainImageViews;

	//RenderPass/Subpass information
	VkRenderPass renderPass;

	//Shader Information
	VkShaderModule vertexShaderModule;
	VkShaderModule fragmentShaderModule;
	VkPipelineShaderStageCreateInfo shaderStages[2];

	//Descriptor Set Information - Uniform Buffer
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;

	//Pipeline Creation
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	//Framebuffer Handles
	std::vector<VkFramebuffer> framebuffers;

	//Pools for command buffers
	VkCommandPool commandPool;
	VkCommandPool transferPool;

	//Command buffers to record to
	std::vector<VkCommandBuffer> commandBuffers;

	//Semaphores Handles for rendering
	VkSemaphore imageAcquiredSemaphore;
	VkSemaphore renderFinishedSemaphore;

	//Handle on staging buffer and its associated memory - used for transfer to local vertex buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	//Handle on uniform staging buffer and its associated memory
	VkBuffer uniformStagingBuffer;
	VkDeviceMemory uniformStagingBufferMemory;

	//Handle on uniform buffer and its associated memory
	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;

	//Handle on our vertex buffer and its associated memory
	std::vector<Vertex> vertices;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	//Handle on our index buffer and its associated memory
	std::vector<uint32_t> indices;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	//Handle for our texture image, associated memory, its view and sampler
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;

	//Handles for our depth attachments
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	//Handle for Multisample resolve images
	VkImage multisampleColourImage;
	VkImage multisampleDepthImage;
	VkDeviceMemory multisampleColourImageMemory;
	VkDeviceMemory multisampleDepthImageMemory;
	VkImageView multisampleColourImageView;
	VkImageView multisampleDepthImageView;

	//Initialise our systems
	void InitialiseVulkan();

	//Core Functions
	void CreateInstance();
	void EnumeratePhysicalDevices();
	void CreateSurface();
	void CreateLogicalDevice();
	void CreateSwapchain();
	void CreateSwapchainImageViews();
	void CreateRenderPass();
	void LoadShaders();
	void CreateDescriptorSetLayout();
	void CreateGraphicsPipeline();
	void CreateFramebuffers();
	void CreateCommandPool(VkCommandPool &commandpool, VkCommandPoolCreateFlags flags);
	void CreateDepthImageResources();
	void CreateTextureImage();
	void CreateTextureImageView();
	void CreateTextureSampler();
	void CreateCommandBuffers();
	void RecordCommandBuffers();
	void CreateSemaphores();
	void CreateModel();
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	void CreateUniformBuffer();
	void CreateDescriptorPool();
	void CreateDescriptorSet();

	void CreateMultisampleTargets();

	//Abstract Helper Functions
	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory);
	void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkSampleCountFlagBits samples, VkImage &image, VkDeviceMemory &imageMemory);
	void CreateMultisampleImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkSampleCountFlagBits samples, VkImage &image, VkDeviceMemory &imageMemory);
	void CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView &imageView);

	VkCommandBuffer beginSingleTransferCommand();
	void endSingleTransferCommand(VkCommandBuffer transferCommandBuffer);
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat findDepthFormat();

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void copyImage(VkImage srcImage, VkImage dstImage, uint32_t imageWidth, uint32_t imageHeight);

	void RecreateSwapchain();

	static void windowResize(GLFWwindow *window, int width, int height);

	bool checkValidationLayerSupport();

public:
	GLFWwindow *window;

	//In-line singleton functions
	static VulkanBase& getSingleton() { static VulkanBase singleton; return singleton; }
	VulkanBase(VulkanBase const&) = delete;
	void operator = (VulkanBase const&) = delete;

	//Draw frame to swapchain
	void AcquireSubmitPresent();

	//Update the uniform buffer
	void UpdateUniformBuffer();

	//Handle our fps output to the GLFW window
	void showFPS(GLFWwindow *pWindow);
	void showAverages();

	void windowTimer();
};
﻿#include "pch.h"
#include "GE/GraphicsEngine.h"

#include "JLib/Array.h"
#include "JLib/ArrayUtils.h"
#include "JLib/LinkedList.h"
#include "JLib/LinkedListUtils.h"
#include "Vk/VkFreeArena.h"
#include "Vk/VkImage.h"
#include "Vk/VkInit.h"
#include "Vk/VkLayout.h"
#include "Vk/VkPipeline.h"
#include "Vk/VkShader.h"
#include "Vk/VkSwapChain.h"
#include "VkHL/VkGLFWApp.h"
#include "VkHL/VkMesh.h"

namespace jv::ge
{
	constexpr uint32_t ARENA_SIZE = 4096;

	enum class AllocationType
	{
		image,
		mesh,
		buffer,
		sampler,
		pool
	};

	struct Image final
	{
		vk::Image image;
		VkImageView view;
		ImageCreateInfo info;
		struct Scene* scene;
	};

	struct Mesh final
	{
		vk::Mesh mesh;
		MeshCreateInfo info;
	};

	struct Buffer final
	{
		vk::Buffer buffer;
		BufferCreateInfo info;
	};

	struct Sampler final
	{
		VkSampler sampler;
	};

	struct Pool final
	{
		VkDescriptorPool pool;
		Array<VkDescriptorSet> sets;
	};

	struct Shader final
	{
		VkShaderModule vertModule = nullptr;
		VkShaderModule fragModule = nullptr;
	};

	struct Layout final
	{
		VkDescriptorSetLayout layout;
		Array<BindingType> bindings;
	};

	struct Pipeline final
	{
		Array<VkDescriptorSetLayout> layouts;
		VkRenderPass renderPass;
		vk::Pipeline pipeline;
	};

	struct Scene final
	{
		Arena arena;
		void* arenaMem;
		vk::FreeArena freeArena;
		LinkedList<AllocationType> allocations;
		LinkedList<Image> images{};
		LinkedList<Mesh> meshes{};
		LinkedList<Buffer> buffers{};
		LinkedList<Sampler> samplers{};
		LinkedList<Pool> pools{};
	};

	struct GraphicsEngine final
	{
		bool initialized = false;

		Arena arena;
		void* arenaMem;
		Arena tempArena;
		void* tempArenaMem;

		vk::GLFWApp glfwApp;
		vk::App app;
		vk::SwapChain swapChain;

		uint64_t scope;

		LinkedList<Scene> scenes{};
		LinkedList<Shader> shaders{};
		LinkedList<Layout> layouts{};
		LinkedList<Pipeline> pipelines{};
	} ge{};

	void* Alloc(const uint32_t size)
	{
		return malloc(size);
	}

	void Free(void* ptr)
	{
		return free(ptr);
	}

	VkFormat FindSupportedFormat(const Array<VkFormat>& candidates,
		const VkImageTiling tiling, const VkFormatFeatureFlags features)
	{
		assert(ge.initialized);

		for (const auto& format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(ge.app.physicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
				return format;
			if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
				return format;
		}

		throw std::exception("Format not available!");
	}

	VkFormat GetDepthBufferFormat()
	{
		VkFormat values[]{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
		Array<VkFormat> formats{};
		formats.ptr = values;
		formats.length = sizeof values / sizeof(VkFormat);
		return FindSupportedFormat(formats, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	void Initialize(const CreateInfo& info)
	{
		assert(!ge.initialized);
		ge.initialized = true;

		ge.arenaMem = malloc(ARENA_SIZE);
		ge.tempArenaMem = malloc(ARENA_SIZE);

		ArenaCreateInfo arenaInfo{};
		arenaInfo.alloc = Alloc;
		arenaInfo.free = Free;
		arenaInfo.memory = ge.arenaMem;
		arenaInfo.memorySize = ARENA_SIZE;
		ge.arena = Arena::Create(arenaInfo);
		arenaInfo.memory = ge.tempArenaMem;
		ge.tempArena = Arena::Create(arenaInfo);

		ge.glfwApp = vk::GLFWApp::Create(info.name, info.resolution, info.fullscreen);

		Array<const char*> extensions{};
		extensions.ptr = vk::GLFWApp::GetRequiredExtensions(extensions.length);

		vk::init::Info vkInfo{};
		vkInfo.tempArena = &ge.tempArena;
		vkInfo.createSurface = vk::GLFWApp::CreateSurface;
		vkInfo.userPtr = &ge.glfwApp;
		vkInfo.instanceExtensions = extensions;
		ge.app = CreateApp(vkInfo);

		ge.swapChain = vk::SwapChain::Create(ge.arena, ge.tempArena, ge.app, info.resolution);
		ge.scope = ge.arena.CreateScope();
	}

	void Resize(const glm::ivec2 resolution, const bool fullScreen)
	{
		assert(ge.initialized);

		ge.glfwApp.Resize(resolution, fullScreen);
		ge.swapChain.Recreate(ge.tempArena, ge.app, resolution);
	}

	Resource CreateScene()
	{
		constexpr uint32_t SCENE_ARENA_SIZE = 512;
		assert(ge.initialized);

		auto& scene = Add(ge.arena, ge.scenes) = {};
		scene.arenaMem = malloc(SCENE_ARENA_SIZE);

		ArenaCreateInfo arenaInfo{};
		arenaInfo.alloc = Alloc;
		arenaInfo.free = Free;
		arenaInfo.memory = scene.arenaMem;
		arenaInfo.memorySize = SCENE_ARENA_SIZE;
		scene.arena = Arena::Create(arenaInfo);
		scene.freeArena = vk::FreeArena::Create(scene.arena, ge.app);

		return &scene;
	}

	void ClearScene(const Resource sceneHandle)
	{
		assert(ge.initialized);
		const auto scene = static_cast<Scene*>(sceneHandle);

		uint32_t imageIndex = 0;
		uint32_t meshIndex = 0;
		uint32_t bufferIndex = 0;
		uint32_t samplerIndex = 0;
		uint32_t poolIndex = 0;

		Image image;
		Mesh mesh;
		Buffer buffer;
		Sampler sampler;
		Pool pool;

		for (const auto& allocation : scene->allocations)
		{
			switch (allocation)
			{
			case AllocationType::image:
				image = scene->images[imageIndex++];
				vkDestroyImageView(ge.app.device, image.view, nullptr);
				vk::Image::Destroy(scene->freeArena, ge.app, image.image);
				break;
			case AllocationType::mesh:
				mesh = scene->meshes[meshIndex++];
				vk::Mesh::Destroy(scene->freeArena, ge.app, mesh.mesh);
				break;
			case AllocationType::buffer:
				buffer = scene->buffers[bufferIndex++];
				vkDestroyBuffer(ge.app.device, buffer.buffer.buffer, nullptr);
				break;
			case AllocationType::sampler:
				sampler = scene->samplers[samplerIndex++];
				vkDestroySampler(ge.app.device, sampler.sampler, nullptr);
				break;
			case AllocationType::pool:
				pool = scene->pools[poolIndex++];
				vkDestroyDescriptorPool(ge.app.device, pool.pool, nullptr);
				break;
			default:
				std::cerr << "Allocation type not supported." << std::endl;
			}
		}

		scene->arena.Clear();
		scene->allocations = {};
		scene->images = {};
		scene->meshes = {};
		scene->buffers = {};
	}

	Resource AddImage(const ImageCreateInfo& info, const Resource sceneHandle)
	{
		assert(ge.initialized);
		const auto scene = static_cast<Scene*>(sceneHandle);
		auto& image = Add(scene->arena, scene->images) = {};

		vk::ImageCreateInfo vkImageCreateInfo{};
		glm::vec3 resolution = glm::vec3(info.resolution, 1);

		switch (info.format)
		{
			case ImageCreateInfo::Format::color:
				vkImageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
				vkImageCreateInfo.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
				vkImageCreateInfo.usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
				resolution.z = 3;
				break;
			case ImageCreateInfo::Format::grayScale:
				vkImageCreateInfo.format = VK_FORMAT_R8_UNORM;
				vkImageCreateInfo.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
				vkImageCreateInfo.usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
				break;
			case ImageCreateInfo::Format::depth:
				vkImageCreateInfo.format = VK_FORMAT_R8_UNORM;
				vkImageCreateInfo.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
				vkImageCreateInfo.usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
				break;
			case ImageCreateInfo::Format::stencil:
				vkImageCreateInfo.format = VK_FORMAT_R8_UNORM;
				vkImageCreateInfo.aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
				vkImageCreateInfo.usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
				break;
			default:
				std::cerr << "Format not supported." << std::endl;
		}

		const auto vkImage = vk::Image::Create(scene->arena, scene->freeArena, ge.app, vkImageCreateInfo, resolution);

		VkImageViewCreateInfo viewCreateInfo{};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.subresourceRange.aspectMask = vkImage.aspectFlags;
		viewCreateInfo.subresourceRange.baseMipLevel = 0;
		viewCreateInfo.subresourceRange.levelCount = 1;
		viewCreateInfo.subresourceRange.baseArrayLayer = 0;
		viewCreateInfo.subresourceRange.layerCount = 1;
		viewCreateInfo.image = vkImage.image;
		viewCreateInfo.format = vkImage.format;

		VkImageView view;
		const auto result = vkCreateImageView(ge.app.device, &viewCreateInfo, nullptr, &view);
		assert(!result);

		image.image = vkImage;
		image.view = view;
		image.info = info;
		image.scene = scene;

		Add(scene->arena, scene->allocations) = AllocationType::image;
		return &image;
	}

	void FillImage(const Resource imageHandle, unsigned char* pixels)
	{
		assert(ge.initialized);
		const auto image = static_cast<Image*>(imageHandle);
		const auto scene = image->scene;
		image->image.FillImage(scene->arena, scene->freeArena, ge.app, pixels);
	}

	Resource AddMesh(const MeshCreateInfo& info, const Resource sceneHandle)
	{
		assert(ge.initialized);
		const auto scene = static_cast<Scene*>(sceneHandle);
		auto& mesh = Add(scene->arena, scene->meshes) = {};

		Array<uint16_t> indices{};
		indices.ptr = info.indices;
		indices.length = info.indicesLength;

		vk::Mesh vkMesh{};
		if(info.vertexType == VertexType::v2D)
		{
			Array<vk::Vertex2d> v2d{};
			v2d.length = info.verticesLength;
			v2d.ptr = reinterpret_cast<vk::Vertex2d*>(info.vertices2d);
			vkMesh = vk::Mesh::Create(scene->arena, scene->freeArena, ge.app, v2d, indices);
		}
		else if(info.vertexType == VertexType::v3D)
		{
			Array<vk::Vertex3d> v3d{};
			v3d.length = info.verticesLength;
			v3d.ptr = reinterpret_cast<vk::Vertex3d*>(info.vertices3d);
			vkMesh = vk::Mesh::Create(scene->arena, scene->freeArena, ge.app, v3d, indices);
		}
		else
			std::cerr << "Vertex type not supported." << std::endl;
		
		mesh.mesh = vkMesh;
		mesh.info = info;

		Add(scene->arena, scene->allocations) = AllocationType::mesh;
		return &mesh;
	}

	Resource AddBuffer(const BufferCreateInfo& info, const Resource sceneHandle)
	{
		assert(ge.initialized);
		const auto scene = static_cast<Scene*>(sceneHandle);
		auto& buffer = Add(scene->arena, scene->buffers) = {};

		VkBufferCreateInfo vertBufferInfo{};
		vertBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertBufferInfo.size = info.size;
		vertBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		switch (info.type)
		{
			case BufferCreateInfo::Type::uniform:
				vertBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
				break;
			case BufferCreateInfo::Type::storage:
				vertBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
				break;
			default: 
				std::cerr << "Buffer type not supported." << std::endl;
		}
		
		vk::Buffer vkBuffer{};
		const auto result = vkCreateBuffer(ge.app.device, &vertBufferInfo, nullptr, &vkBuffer.buffer);
		assert(!result);

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(ge.app.device, vkBuffer.buffer, &memRequirements);

		constexpr VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		vkBuffer.memoryHandle = scene->freeArena.Alloc(scene->arena, ge.app, memRequirements, memoryPropertyFlags, 1, vkBuffer.memory);

		buffer.buffer = vkBuffer;
		buffer.info = info;

		Add(scene->arena, scene->allocations) = AllocationType::buffer;
		return &buffer;
	}

	VkSamplerAddressMode GetAddressMode(const SamplerCreateInfo::AddressMode mode)
	{
		VkSamplerAddressMode addressMode{};
		switch (mode)
		{
		case SamplerCreateInfo::AddressMode::repeat:
			addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			break;
		case SamplerCreateInfo::AddressMode::mirroredRepeat:
			break;
		case SamplerCreateInfo::AddressMode::clampToEdge:
			break;
		case SamplerCreateInfo::AddressMode::clampToBorder:
			break;
		case SamplerCreateInfo::AddressMode::mirroredClampToBorder:
			break;
		case SamplerCreateInfo::AddressMode::mirroredClampToEdge:
			break;
		default:
			std::cerr << "Address mode not supported." << std::endl;
		}
		return addressMode;
	}

	Resource AddSampler(const SamplerCreateInfo& info, const Resource sceneHandle)
	{
		assert(ge.initialized);
		const auto scene = static_cast<Scene*>(sceneHandle);
		auto& sampler = Add(scene->arena, scene->samplers) = {};

		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(ge.app.physicalDevice, &properties);

		VkFilter filter{};
		switch (info.filter)
		{
		case SamplerCreateInfo::Filter::nearest:
			filter = VK_FILTER_NEAREST;
			break;
		case SamplerCreateInfo::Filter::linear:
			filter = VK_FILTER_LINEAR;
			break;
		default:
			std::cerr << "Filter type not supported." << std::endl;
		}

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = filter;
		samplerInfo.minFilter = filter;
		samplerInfo.addressModeU = GetAddressMode(info.addressModeU);
		samplerInfo.addressModeV = GetAddressMode(info.addressModeV);
		samplerInfo.addressModeW = GetAddressMode(info.addressModeW);
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0;
		samplerInfo.maxLod = 0;

		const auto result = vkCreateSampler(ge.app.device, &samplerInfo, nullptr, &sampler.sampler);
		assert(!result);

		Add(scene->arena, scene->allocations) = AllocationType::sampler;
		return &sampler;
	}

	Resource AddPool(const PoolCreateInfo& info, const Resource sceneHandle)
	{
		assert(ge.initialized);
		const auto layout = static_cast<Layout*>(info.layout);
		const auto scene = static_cast<Scene*>(sceneHandle);
		auto& pool = Add(scene->arena, scene->pools) = {};

		const auto scope = ge.tempArena.CreateScope();
		const auto sizes = CreateArray<VkDescriptorPoolSize>(ge.tempArena, layout->bindings.length);
		for (uint32_t i = 0; i < layout->bindings.length; ++i)
		{
			auto& size = sizes[i];
			switch (layout->bindings[i])
			{
				case BindingType::uniformBuffer:
					size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					break;
				case BindingType::storageBuffer:
					size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					break;
				case BindingType::sampler:
					size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					break;
			default: 
				std::cerr << "Binding type not supported." << std::endl;
			}
			size.descriptorCount = info.capacity;
		}

		// Create descriptor pool.
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = sizes.length;
		poolInfo.pPoolSizes = sizes.ptr;
		poolInfo.maxSets = info.capacity;

		auto result = vkCreateDescriptorPool(ge.app.device, &poolInfo, nullptr, &pool.pool);
		assert(!result);

		const auto vkLayouts = CreateArray<VkDescriptorSetLayout>(ge.tempArena, info.capacity);
		for (auto& vkLayout : vkLayouts)
			vkLayout = layout->layout;

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool.pool;
		allocInfo.descriptorSetCount = vkLayouts.length;
		allocInfo.pSetLayouts = vkLayouts.ptr;

		pool.sets = CreateArray<VkDescriptorSet>(scene->arena, info.capacity);
		result = vkAllocateDescriptorSets(ge.app.device, &allocInfo, pool.sets.ptr);
		assert(!result);
		
		ge.tempArena.DestroyScope(scope);
		Add(scene->arena, scene->allocations) = AllocationType::pool;
		return &pool;
	}

	void Bind(const BindInfo& info)
	{
		assert(ge.initialized);
		const auto scope = ge.tempArena.CreateScope();
		const auto pool = static_cast<Pool*>(info.pool);
		const auto& descriptorSet = pool->sets[info.descriptorIndex];

		const auto writes = CreateArray<VkWriteDescriptorSet>(ge.tempArena, info.writeCount);
		for (uint32_t i = 0; i < info.writeCount; ++i)
		{
			const auto& writeInfo = info.writes[i];
			auto& write = writes[i] = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstBinding = writeInfo.index;
			write.dstSet = descriptorSet;
			write.descriptorCount = 1;
			write.dstArrayElement = 0;

			VkDescriptorBufferInfo* bufferInfo;
			VkDescriptorImageInfo* imageInfo;
			Buffer* buffer{};
			Sampler* sampler{};
			Image* image{};

			switch (writeInfo.type)
			{
				case BindingType::uniformBuffer:
					bufferInfo = ge.tempArena.New<VkDescriptorBufferInfo>();
					*bufferInfo = {};
					buffer = static_cast<Buffer*>(writeInfo.buffer.buffer);
					bufferInfo->buffer = buffer->buffer.buffer;
					bufferInfo->offset = writeInfo.buffer.offset;
					bufferInfo->range = writeInfo.buffer.range;
					write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					write.pBufferInfo = bufferInfo;
					break;
				case BindingType::storageBuffer:
					bufferInfo = ge.tempArena.New<VkDescriptorBufferInfo>();
					*bufferInfo = {};
					buffer = static_cast<Buffer*>(writeInfo.buffer.buffer);
					bufferInfo->buffer = buffer->buffer.buffer;
					bufferInfo->offset = writeInfo.buffer.offset;
					bufferInfo->range = writeInfo.buffer.range;
					write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					write.pBufferInfo = bufferInfo;
					break;
				case BindingType::sampler:
					imageInfo = ge.tempArena.New<VkDescriptorImageInfo>();
					*imageInfo = {};
					sampler = static_cast<Sampler*>(writeInfo.image.sampler);
					image = static_cast<Image*>(writeInfo.image.image);
					imageInfo->imageLayout = image->image.layout;
					imageInfo->imageView = image->view;
					imageInfo->sampler = sampler->sampler;
					write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					write.pImageInfo = imageInfo;
					break;
				default:
					std::cerr << "Binding type not supported." << std::endl;
			}
		}

		vkUpdateDescriptorSets(ge.app.device, writes.length, writes.ptr, 0, nullptr);
		ge.tempArena.DestroyScope(scope);
	}

	void UpdateBuffer(const Resource bufferHandle, const void* data, const uint32_t size, const uint32_t offset)
	{
		assert(ge.initialized);
		const auto buffer = static_cast<Buffer*>(bufferHandle);

		void* vkData;
		const auto result = vkMapMemory(ge.app.device, buffer->buffer.memory.memory, buffer->buffer.memory.offset + offset, size, 0, &vkData);
		assert(!result);
		memcpy(vkData, data, size);
		vkUnmapMemory(ge.app.device, buffer->buffer.memory.memory);
	}

	Resource CreateShader(const ShaderCreateInfo& info)
	{
		assert(ge.initialized);
		auto& shader = Add(ge.arena, ge.shaders);
		if(info.vertexCode)
			shader.vertModule = CreateShaderModule(ge.app, info.vertexCode, info.vertexCodeLength);
		if (info.fragmentCode)
			shader.fragModule = CreateShaderModule(ge.app, info.fragmentCode, info.fragmentCodeLength);
		return &shader;
	}

	Resource CreateLayout(const LayoutCreateInfo& info)
	{
		assert(ge.initialized);
		auto& layout = Add(ge.arena, ge.layouts);

		const auto bindings = CreateArray<vk::Binding>(ge.tempArena, info.bindingsCount);

		for (uint32_t j = 0; j < info.bindingsCount; ++j)
		{
			auto& binding = bindings[j] = {};
			const auto& bindingInfo = info.bindings[j];

			switch (bindingInfo.type)
			{
			case BindingType::uniformBuffer:
				binding.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				break;
			case BindingType::storageBuffer:
				binding.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				break;
			case BindingType::sampler:
				binding.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				break;
			default:
				std::cerr << "Binding type not supported." << std::endl;
			}

			switch (bindingInfo.stage)
			{
			case ShaderStage::vertex:
				binding.flag = VK_SHADER_STAGE_VERTEX_BIT;
				break;
			case ShaderStage::fragment:
				binding.flag = VK_SHADER_STAGE_FRAGMENT_BIT;
				break;
			default:
				std::cerr << "Binding stage not supported." << std::endl;
			}
		}

		layout.layout = CreateLayout(ge.tempArena, ge.app, bindings);
		layout.bindings = CreateArray<BindingType>(ge.arena, info.bindingsCount);
		for (uint32_t i = 0; i < info.bindingsCount; ++i)
			layout.bindings[i] = info.bindings[i].type;
		return &layout;
	}

	Resource CreatePipeline(const PipelineCreateInfo& info)
	{
		assert(ge.initialized);
		auto& pipeline = Add(ge.arena, ge.pipelines);
		pipeline.layouts = CreateArray<VkDescriptorSetLayout>(ge.arena, info.layoutCount);

		for (uint32_t i = 0; i < info.layoutCount; ++i)
		{
			const auto layout = static_cast<Layout*>(info.layouts[i]);
			pipeline.layouts[i] = layout->layout;
		}

		const auto shader = static_cast<Shader*>(info.shader);

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription attachmentDescriptions[2]{};
		
		VkAttachmentDescription attachmentInfo = attachmentDescriptions[0];
		attachmentInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentInfo.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentInfo.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentInfo.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachmentInfo.format = ge.swapChain.GetFormat();

		VkAttachmentDescription depthAttachment = attachmentDescriptions[info.colorOutput];
		depthAttachment.format = GetDepthBufferFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = info.colorOutput;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription{};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = info.colorOutput;
		subpassDescription.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency subpassDependency{};
		subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpassDependency.dstSubpass = 0;
		subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.srcAccessMask = 0;
		subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		if (info.depthOutput)
		{
			subpassDescription.pDepthStencilAttachment = &depthAttachmentRef;
			subpassDependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			subpassDependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			subpassDependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.pAttachments = &attachmentInfo;
		renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(info.colorOutput) + info.depthOutput;
		renderPassCreateInfo.pSubpasses = &subpassDescription;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pDependencies = &subpassDependency;
		renderPassCreateInfo.dependencyCount = 1;

		const auto renderPassResult = vkCreateRenderPass(ge.app.device, &renderPassCreateInfo, nullptr, &pipeline.renderPass);
		assert(!renderPassResult);

		vk::PipelineCreateInfo::Module modules[2]{};
		uint32_t moduleCount = 0;
		{
			if(shader->vertModule)
			{
				auto& mod = modules[moduleCount++];
				mod.stage = VK_SHADER_STAGE_VERTEX_BIT;
				mod.module = shader->vertModule;
			}
			if (shader->fragModule)
			{
				auto& mod = modules[moduleCount++];
				mod.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
				mod.module = shader->fragModule;
			}
		}

		vk::PipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.modules.ptr = modules;
		pipelineCreateInfo.modules.length = moduleCount;
		pipelineCreateInfo.resolution = info.resolution;
		pipelineCreateInfo.renderPass = pipeline.renderPass;
		pipelineCreateInfo.layouts.ptr = pipeline.layouts.ptr;
		pipelineCreateInfo.layouts.length = info.layoutCount;
		pipelineCreateInfo.pushConstantSize = info.pushConstantSize;

		switch (info.vertexType)
		{
			case VertexType::v2D:
				pipelineCreateInfo.getBindingDescriptions = vk::Vertex2d::GetBindingDescriptions;
				pipelineCreateInfo.getAttributeDescriptions = vk::Vertex2d::GetAttributeDescriptions;
				break;
			case VertexType::v3D:
				pipelineCreateInfo.getBindingDescriptions = vk::Vertex3d::GetBindingDescriptions;
				pipelineCreateInfo.getAttributeDescriptions = vk::Vertex3d::GetAttributeDescriptions;
				break;
			default:
				std::cerr << "Vertex type not supported." << std::endl;
		}

		pipeline.pipeline = vk::Pipeline::Create(pipelineCreateInfo, ge.tempArena, ge.app);
		return &pipeline;
	}

	void DestroyScenes()
	{
		assert(ge.initialized);

		const auto length = ge.scenes.GetCount();
		for (uint32_t i = 0; i < length; ++i)
		{
			auto& scene = ge.scenes[i];
			ClearScene(&scene);

			vk::FreeArena::Destroy(scene.arena, ge.app, scene.freeArena);
			Arena::Destroy(scene.arena);
			free(scene.arenaMem);
		}
	}

	bool RenderFrame()
	{
		assert(ge.initialized);

		if (!ge.glfwApp.BeginFrame())
			return false;

		ge.swapChain.WaitForImage(ge.app);
		auto cmdBuffer = ge.swapChain.BeginFrame(ge.app, true);
		ge.swapChain.EndFrame(ge.tempArena, ge.app);

		return true;
	}

	uint32_t GetFrameCount()
	{
		assert(ge.initialized);
		return ge.swapChain.GetLength();
	}

	uint32_t GetFrameIndex()
	{
		assert(ge.initialized);
		return ge.swapChain.GetIndex();
	}

	void Shutdown()
	{
		assert(ge.initialized);

		const auto result = vkDeviceWaitIdle(ge.app.device);
		assert(!result);

		DestroyScenes();
		ge.arena.DestroyScope(ge.scope);
		vk::SwapChain::Destroy(ge.arena, ge.app, ge.swapChain);

		for (const auto& pipeline : ge.pipelines)
		{
			vk::Pipeline::Destroy(pipeline.pipeline, ge.app);
			vkDestroyRenderPass(ge.app.device, pipeline.renderPass, nullptr);
		}

		for (const auto& layout : ge.layouts)
			vkDestroyDescriptorSetLayout(ge.app.device, layout.layout, nullptr);

		for (const auto& shader : ge.shaders)
		{
			if (shader.vertModule)
				vkDestroyShaderModule(ge.app.device, shader.vertModule, nullptr);
			if(shader.fragModule)
				vkDestroyShaderModule(ge.app.device, shader.fragModule, nullptr);
		}

		vk::init::DestroyApp(ge.app);
		vk::GLFWApp::Destroy(ge.glfwApp);
		Arena::Destroy(ge.tempArena);
		Arena::Destroy(ge.arena);
		free(ge.tempArenaMem);
		free(ge.arenaMem);

		ge.initialized = false;
	}
}

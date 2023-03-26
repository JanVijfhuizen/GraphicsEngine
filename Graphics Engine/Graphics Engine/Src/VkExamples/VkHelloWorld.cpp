#include "pch.h"
#include "VkExamples/VkHelloWorld.h"

#include "JLib/ArrayUtils.h"
#include "JLib/FileLoader.h"
#include "Vk/VkImage.h"
#include "Vk/VkLayout.h"
#include "Vk/VkPipeline.h"
#include "Vk/VkShader.h"
#include "VkHL/VkMesh.h"
#include "VkHL/VkShapes.h"
#include "VkHL/VkTexture.h"

namespace jv::vk::example
{
	struct HelloWorldProgram final
	{
		uint64_t scope;
		Mesh mesh;
		Image image;
		Pipeline pipeline;
		VkShaderModule modules[2];
		VkDescriptorSetLayout layout;

		VkImageView view;
		VkDescriptorPool descriptorPool;
		VkSampler sampler;
		Array<VkDescriptorSet> descriptorSets;
	};

	void* OnBegin(Program& program)
	{
		const auto ptr = program.arena.New<HelloWorldProgram>();
		ptr->scope = program.arena.CreateScope();

		Array<Vertex2d> vertices;
		Array<VertexIndex> indices;
		const auto tempScope = CreateQuadShape(program.tempArena, vertices, indices);

		ptr->mesh = Mesh::Create(program.vkCPUArena, program.vkGPUArena, program.vkApp, vertices, indices);

		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageCreateInfo.usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		ptr->image = LoadTexture(program.vkCPUArena, program.vkGPUArena, program.vkApp,
			imageCreateInfo, "Art/logo.png");

		const auto vertCode = file::Load(program.tempArena, "Shaders/vert.spv");
		const auto fragCode = file::Load(program.tempArena, "Shaders/frag.spv");

		const auto shaderModules = CreateArray<PipelineCreateInfo::Module>(program.tempArena, 2);
		shaderModules[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderModules[0].module = CreateShaderModule(program.vkApp, vertCode.ptr, vertCode.length);
		shaderModules[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderModules[1].module = CreateShaderModule(program.vkApp, fragCode.ptr, fragCode.length);

		for (uint32_t i = 0; i < 2; ++i)
			ptr->modules[i] = shaderModules[i].module;

		const auto bindings = CreateArray<Binding>(program.tempArena, 1);
		bindings[0] = {};
		bindings[0].flag = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ptr->layout = CreateLayout(program.tempArena, program.vkApp, bindings);

		PipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.modules = shaderModules;
		pipelineCreateInfo.resolution = program.resolution;
		pipelineCreateInfo.renderPass = program.swapChainRenderPass;
		pipelineCreateInfo.getBindingDescriptions = Vertex2d::GetBindingDescriptions;
		pipelineCreateInfo.getAttributeDescriptions = Vertex2d::GetAttributeDescriptions;
		pipelineCreateInfo.layouts.ptr = &ptr->layout;
		pipelineCreateInfo.layouts.length = 1;
		ptr->pipeline = Pipeline::Create(pipelineCreateInfo, program.tempArena, program.vkApp);

		// Swap chain resources.

		VkImageViewCreateInfo viewCreateInfo{};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.subresourceRange.aspectMask = ptr->image.aspectFlags;
		viewCreateInfo.subresourceRange.baseMipLevel = 0;
		viewCreateInfo.subresourceRange.levelCount = 1;
		viewCreateInfo.subresourceRange.baseArrayLayer = 0;
		viewCreateInfo.subresourceRange.layerCount = 1;
		viewCreateInfo.image = ptr->image.image;
		viewCreateInfo.format = ptr->image.format;

		auto result = vkCreateImageView(program.vkApp.device, &viewCreateInfo, nullptr, &ptr->view);
		assert(!result);

		// Create descriptor pool.
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = program.frameCount;
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = program.frameCount;

		result = vkCreateDescriptorPool(program.vkApp.device, &poolInfo, nullptr, &ptr->descriptorPool);
		assert(!result);

		auto layouts = CreateArray<VkDescriptorSetLayout>(program.tempArena, program.frameCount);
		for (auto& layout : layouts)
			layout = ptr->layout;

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = ptr->descriptorPool;
		allocInfo.descriptorSetCount = layouts.length;
		allocInfo.pSetLayouts = layouts.ptr;

		ptr->descriptorSets = CreateArray<VkDescriptorSet>(program.arena, program.frameCount);
		result = vkAllocateDescriptorSets(program.vkApp.device, &allocInfo, ptr->descriptorSets.ptr);
		assert(!result);

		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(program.vkApp.physicalDevice, &properties);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		// Since I'm going for pixelart, this will be the default.
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
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

		result = vkCreateSampler(program.vkApp.device, &samplerInfo, nullptr, &ptr->sampler);

		// Binding.
		// Bind descriptor sets for the render node.
		for (uint32_t i = 0; i < program.frameCount; ++i)
		{
			// Bind texture atlas.
			VkDescriptorImageInfo atlasInfo{};
			atlasInfo.imageLayout = ptr->image.layout;
			atlasInfo.imageView = ptr->view;
			atlasInfo.sampler = ptr->sampler;

			VkWriteDescriptorSet atlasWrite{};
			atlasWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			atlasWrite.dstBinding = 0;
			atlasWrite.dstSet = ptr->descriptorSets[i];
			atlasWrite.descriptorCount = 1;
			atlasWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			atlasWrite.pImageInfo = &atlasInfo;
			atlasWrite.dstArrayElement = 0;

			vkUpdateDescriptorSets(program.vkApp.device, 1, &atlasWrite, 0, nullptr);
		}

		program.tempArena.DestroyScope(tempScope);
		return ptr;
	}

	bool OnUpdate(Program& program, void* userPtr, const VkCommandBuffer cmd)
	{
		const auto ptr = static_cast<HelloWorldProgram*>(userPtr);

		ptr->pipeline.Bind(cmd);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ptr->pipeline.layout,
			0, 1, &ptr->descriptorSets[program.frameIndex], 0, nullptr);
		ptr->mesh.Draw(cmd, 1);

		return true;
	}

	bool OnExit(Program& program, void* userPtr)
	{
		const auto ptr = static_cast<HelloWorldProgram*>(userPtr);

		vkDestroySampler(program.vkApp.device, ptr->sampler, nullptr);
		vkDestroyImageView(program.vkApp.device, ptr->view, nullptr);
		vkDestroyDescriptorPool(program.vkApp.device, ptr->descriptorPool, nullptr);
		for (const auto& mod : ptr->modules)
			vkDestroyShaderModule(program.vkApp.device, mod, nullptr);
		vkDestroyDescriptorSetLayout(program.vkApp.device, ptr->layout, nullptr);
		Pipeline::Destroy(ptr->pipeline, program.vkApp);
		Image::Destroy(program.vkGPUArena, program.vkApp, ptr->image);
		Mesh::Destroy(program.vkGPUArena, ptr->mesh, program.vkApp);

		program.arena.DestroyScope(ptr->scope);
		program.arena.Free(userPtr);

		return true;
	}

	bool OnRenderUpdate(Program& program, void* userPtr)
	{
		return true;
	}

	void DefineHelloWorldExample(ProgramInfo& info)
	{
		info.name = "Hello World";
		info.onBegin = OnBegin;
		info.onSwapChainRenderUpdate = OnUpdate;
		info.onExit = OnExit;
		info.onRenderUpdate = OnRenderUpdate;
	}
}

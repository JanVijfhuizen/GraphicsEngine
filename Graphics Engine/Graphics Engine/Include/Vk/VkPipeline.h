#pragma once
#include "JLib/Array.h"

namespace jv::vk
{
	struct App;

	struct PipelineCreateInfo final
	{
		enum class Topology
		{
			triangle,
			line,
			points
		};

		struct Module final
		{
			VkShaderModule module = VK_NULL_HANDLE;
			VkShaderStageFlagBits stage = VK_SHADER_STAGE_ALL;
		};

		Topology topology = Topology::triangle;
		Array<Module> modules;
		glm::ivec2 resolution;
		VkRenderPass renderPass;
		Array<VkVertexInputBindingDescription>(*getBindingDescriptions)(Arena& arena);
		Array<VkVertexInputAttributeDescription>(*getAttributeDescriptions)(Arena& arena);

		Array<VkDescriptorSetLayout> layouts{};
		bool shaderSamplingEnabled = false;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
		size_t pushConstantSize = 0;
		VkShaderStageFlags pushConstantShaderStageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		VkCompareOp depthBufferCompareOp = VK_COMPARE_OP_LESS;
		bool depthBufferEnabled = false;
		VkPipeline basePipeline = VK_NULL_HANDLE;
		int32_t basePipelineIndex = 0;
	};

	// Render pipeline that defines how meshes are drawn.
	struct Pipeline final
	{
		VkPipelineLayout layout;
		VkPipeline pipeline;

		void Bind(VkCommandBuffer cmd) const;

		[[nodiscard]] static Pipeline Create(const PipelineCreateInfo& info, Arena& tempArena, const App& app);
		static void Destroy(const Pipeline& pipeline, const App& app);
	};
}

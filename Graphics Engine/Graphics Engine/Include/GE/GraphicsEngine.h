﻿#pragma once

namespace jv::ge
{
	using Resource = void*;

	enum class VertexType
	{
		v2D,
		v3D
	};

	enum class BindingType
	{
		uniformBuffer,
		storageBuffer,
		sampler
	};

	enum class ShaderStage
	{
		vertex,
		fragment
	};

	struct Vertex2D
	{
		glm::vec2 position{};
		glm::vec2 textureCoordinates{};
	};

	struct Vertex3D
	{
		glm::vec3 position{};
		glm::vec3 normal{ 0, 0, 1 };
		glm::vec2 textureCoordinates{};
	};

	struct CreateInfo final
	{
		const char* name = "Graphics Engine";
		glm::ivec2 resolution{ 800, 600 };
		bool fullscreen = false;
	};

	struct ImageCreateInfo final
	{
		Resource scene;
		enum class Format
		{
			color,
			grayScale,
			depth,
			stencil
		} format = Format::color;
		enum class Usage
		{
			read,
			write,
			readWrite
		} usage = Usage::readWrite;
		glm::ivec2 resolution;
	};

	struct MeshCreateInfo final
	{
		Resource scene;
		union
		{
			Vertex2D* vertices2d;
			Vertex3D* vertices3d;
		};
		uint32_t verticesLength;
		uint16_t* indices;
		uint32_t indicesLength;
		VertexType vertexType = VertexType::v2D;
	};

	struct BufferCreateInfo final
	{
		Resource scene;
		enum class Type
		{
			uniform,
			storage
		} type = Type::uniform;
		uint32_t size;
	};

	struct DescriptorPoolCreateInfo final
	{
		Resource scene;
		Resource layout;
		uint32_t capacity;
	};

	struct ShaderCreateInfo final
	{
		const char* vertexCode = nullptr;
		uint32_t vertexCodeLength;
		const char* fragmentCode = nullptr;
		uint32_t fragmentCodeLength;
	};

	struct LayoutCreateInfo final
	{
		struct Binding final
		{
			BindingType type;
			ShaderStage stage;
			size_t size = sizeof(int32_t);
			uint32_t count = 1;
		};
		Binding* bindings;
		uint32_t bindingsCount;
	};

	struct RenderPassCreateInfo final
	{
		enum class DrawTarget
		{
			image,
			swapChain
		} target = DrawTarget::swapChain;

		bool colorOutput = true;
		bool depthOutput = false;
	};

	struct FrameBufferCreateInfo final
	{
		Resource* images;
		Resource renderPass;
	};

	struct PipelineCreateInfo final
	{
		Resource shader;
		Resource* layouts;
		Resource renderPass;
		uint32_t layoutCount;
		glm::ivec2 resolution;
		VertexType vertexType = VertexType::v2D;
		uint32_t pushConstantSize = 0;
	};

	struct SamplerCreateInfo final
	{
		Resource scene;
		enum class Filter
		{
			nearest,
			linear
		} filter = Filter::nearest;
		enum class AddressMode
		{
			repeat,
			mirroredRepeat,
			clampToEdge,
			clampToBorder,
			mirroredClampToBorder,
			mirroredClampToEdge
		};
		AddressMode addressModeU = AddressMode::repeat;
		AddressMode addressModeV = AddressMode::repeat;
		AddressMode addressModeW = AddressMode::repeat;
	};

	struct WriteInfo final
	{
		struct Image final
		{
			Resource image;
			Resource sampler;
		};
		struct Buffer final
		{
			Resource buffer;
			uint32_t range;
			uint32_t offset = 0;
		};
		struct Binding final 
		{
			BindingType type;
			union
			{
				Image image;
				Buffer buffer{};
			};
			uint32_t index;
		};
		Resource descriptorSet;
		Binding* bindings;
		uint32_t bindingCount;
	};

	struct BufferUpdateInfo final
	{
		Resource buffer;
		const void* data;
		uint32_t size;
		uint32_t offset = 0;
	};

	struct DrawInfo final
	{
		Resource descriptorSets[4]{};
		uint32_t descriptorSetCount;
		Resource mesh;
		Resource pipeline;
		uint32_t instanceCount = 1;
	};

	void Initialize(const CreateInfo& info);
	void Resize(glm::ivec2 resolution, bool fullScreen);
	[[nodiscard]] Resource CreateScene();
	void ClearScene(Resource scene);
	[[nodiscard]] Resource AddImage(const ImageCreateInfo& info);
	void FillImage(Resource image, unsigned char* pixels);
	[[nodiscard]] Resource AddMesh(const MeshCreateInfo& info);
	[[nodiscard]] Resource AddBuffer(const BufferCreateInfo& info);
	[[nodiscard]] Resource AddSampler(const SamplerCreateInfo& info);
	[[nodiscard]] Resource AddDescriptorPool(const DescriptorPoolCreateInfo& info);
	[[nodiscard]] Resource GetDescriptorSet(Resource pool, uint32_t index);
	void Write(const WriteInfo& info);
	void UpdateBuffer(const BufferUpdateInfo& info);
	[[nodiscard]] Resource CreateShader(const ShaderCreateInfo& info);
	[[nodiscard]] Resource CreateLayout(const LayoutCreateInfo& info);
	[[nodiscard]] Resource CreateRenderPass(const RenderPassCreateInfo& info);
	[[nodiscard]] Resource CreateFrameBuffer(const FrameBufferCreateInfo& info);
	[[nodiscard]] Resource CreatePipeline(const PipelineCreateInfo& info);
	void SetRenderTarget(Resource frameBuffer = nullptr);
	void Draw(const DrawInfo& info);
	[[nodiscard]] bool RenderFrame();
	[[nodiscard]] uint32_t GetFrameCount();
	[[nodiscard]] uint32_t GetFrameIndex();
	void Shutdown();
}

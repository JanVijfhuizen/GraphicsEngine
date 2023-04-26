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
		enum class Type
		{
			uniform,
			storage
		} type = Type::uniform;

		uint32_t size;
	};

	struct PoolCreateInfo final
	{
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

	struct PipelineCreateInfo final
	{
		Resource shader;
		Resource* layouts;
		uint32_t layoutCount;
		glm::ivec2 resolution;
		VertexType vertexType = VertexType::v2D;
		bool colorOutput = true;
		bool depthOutput = false;
		uint32_t pushConstantSize = 0;
	};

	struct SamplerCreateInfo final
	{
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

	struct BindInfo final
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

		struct Write final 
		{
			BindingType type;
			union
			{
				Image image;
				Buffer buffer{};
			};
			uint32_t index;
		};

		Resource pool;
		uint32_t descriptorIndex;
		Write* writes;
		uint32_t writeCount;
	};

	void Initialize(const CreateInfo& info);
	void Resize(glm::ivec2 resolution, bool fullScreen);
	[[nodiscard]] Resource CreateScene();
	void ClearScene(Resource sceneHandle);
	[[nodiscard]] Resource AddImage(const ImageCreateInfo& info, Resource sceneHandle);
	void FillImage(Resource imageHandle, unsigned char* pixels);
	[[nodiscard]] Resource AddMesh(const MeshCreateInfo& info, Resource sceneHandle);
	[[nodiscard]] Resource AddBuffer(const BufferCreateInfo& info, Resource sceneHandle);
	[[nodiscard]] Resource AddSampler(const SamplerCreateInfo& info, Resource sceneHandle);
	[[nodiscard]] Resource AddPool(const PoolCreateInfo& info, Resource sceneHandle);
	void Bind(const BindInfo& info);
	void UpdateBuffer(Resource bufferHandle, const void* data, uint32_t size, uint32_t offset);
	[[nodiscard]] Resource CreateShader(const ShaderCreateInfo& info);
	[[nodiscard]] Resource CreateLayout(const LayoutCreateInfo& info);
	[[nodiscard]] Resource CreatePipeline(const PipelineCreateInfo& info);
	[[nodiscard]] bool RenderFrame();
	[[nodiscard]] uint32_t GetFrameCount();
	[[nodiscard]] uint32_t GetFrameIndex();
	void Shutdown();
}

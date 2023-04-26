﻿#pragma once

namespace jv::ge
{
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
		uint32_t layout;
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
		uint32_t shader;
		uint32_t* layouts;
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
			uint32_t image;
			uint32_t sampler;
		};

		struct Buffer final
		{
			uint32_t buffer;
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

		uint32_t pool;
		uint32_t descriptorIndex;
		Write* writes;
		uint32_t writeCount;
	};

	void Initialize(const CreateInfo& info);
	void Resize(glm::ivec2 resolution, bool fullScreen);
	[[nodiscard]] uint32_t CreateScene();
	void ClearScene(uint32_t handle);
	[[nodiscard]] uint32_t AddImage(const ImageCreateInfo& info, uint32_t handle);
	void FillImage(uint32_t sceneHandle, uint32_t imageHandle, unsigned char* pixels);
	[[nodiscard]] uint32_t AddMesh(const MeshCreateInfo& info, uint32_t handle);
	[[nodiscard]] uint32_t AddBuffer(const BufferCreateInfo& info, uint32_t handle);
	[[nodiscard]] uint32_t AddSampler(const SamplerCreateInfo& info, uint32_t handle);
	[[nodiscard]] uint32_t AddPool(const PoolCreateInfo& info, uint32_t handle);
	void Bind(const BindInfo& info, uint32_t handle);
	void UpdateBuffer(uint32_t sceneHandle, uint32_t bufferHandle, const void* data, uint32_t size, uint32_t offset);
	[[nodiscard]] uint32_t CreateShader(const ShaderCreateInfo& info);
	[[nodiscard]] uint32_t CreateLayout(const LayoutCreateInfo& info);
	[[nodiscard]] uint32_t CreatePipeline(const PipelineCreateInfo& info);
	[[nodiscard]] bool RenderFrame();
	[[nodiscard]] uint32_t GetFrameCount();
	[[nodiscard]] uint32_t GetFrameIndex();
	void Shutdown();
}

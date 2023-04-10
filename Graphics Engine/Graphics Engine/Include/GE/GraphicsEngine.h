#pragma once

namespace jv::ge
{
	enum class VertexType
	{
		v2D,
		v3D
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

	struct ShaderCreateInfo final
	{
		const char* vertexCode = nullptr;
		uint32_t vertexCodeLength;
		const char* fragmentCode = nullptr;
		uint32_t fragmentCodeLength;
	};

	void Initialize(const CreateInfo& info);
	void Resize(glm::ivec2 resolution, bool fullScreen);
	[[nodiscard]] uint32_t CreateScene();
	void ClearScene(uint32_t handle);
	[[nodiscard]] uint32_t AddImage(const ImageCreateInfo& info, uint32_t handle);
	void FillImage(uint32_t sceneHandle, uint32_t imageHandle, unsigned char* pixels);
	[[nodiscard]] uint32_t AddMesh(const MeshCreateInfo& info, uint32_t handle);
	[[nodiscard]] uint32_t AddBuffer(const BufferCreateInfo& info, uint32_t handle);
	void UpdateBuffer(uint32_t sceneHandle, uint32_t bufferHandle, const void* data, uint32_t size, uint32_t offset);
	[[nodiscard]] uint32_t CreateShader(const ShaderCreateInfo& info);
	[[nodiscard]] bool RenderFrame();
	[[nodiscard]] uint32_t GetFrameCount();
	[[nodiscard]] uint32_t GetFrameIndex();
	void Shutdown();
}

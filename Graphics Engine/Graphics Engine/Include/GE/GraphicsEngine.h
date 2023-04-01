#pragma once

namespace jv::ge
{
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
		
	};

	struct BufferCreateInfo final
	{
		
	};

	void Initialize(const CreateInfo& info);
	void Resize(glm::ivec2 resolution, bool fullScreen);
	[[nodiscard]] uint32_t CreateScene();
	void ClearScene(uint32_t handle);
	[[nodiscard]] uint32_t AddImage(const ImageCreateInfo& info, uint32_t handle);
	[[nodiscard]] void FillImage(uint32_t sceneHandle, uint32_t imageHandle, unsigned char* pixels);
	[[nodiscard]] uint32_t AddMesh(const ImageCreateInfo& info, uint32_t handle);
	[[nodiscard]] uint32_t AddBuffer(const ImageCreateInfo& info, uint32_t handle);
	[[nodiscard]] bool RenderFrame();
	[[nodiscard]] uint32_t GetFrameCount();
	[[nodiscard]] uint32_t GetFrameIndex();
	void Shutdown();
}

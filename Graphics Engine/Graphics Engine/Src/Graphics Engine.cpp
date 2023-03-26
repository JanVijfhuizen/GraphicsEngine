#include "pch.h"

#include "VkExamples/VkHelloWorld.h"
#include "VkHL/VkProgram.h"

int main()
{
	jv::vk::ProgramInfo info{};
	jv::vk::example::DefineHelloWorldExample(info);
	Run(info);
}

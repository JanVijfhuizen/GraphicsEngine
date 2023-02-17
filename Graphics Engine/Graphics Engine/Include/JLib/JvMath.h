#pragma once

namespace jv
{
	template <typename T>
	[[nodiscard]] T Max(const T& a, const T& b)
	{
		return a > b ? a : b;
	}

	template <typename T>
	[[nodiscard]] T Min(const T& a, const T& b)
	{
		return a > b ? b : a;
	}
}

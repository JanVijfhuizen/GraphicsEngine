#pragma once

namespace jv
{
	// Linear sort.
	template <typename T>
	void LinearSort(T* arr, const size_t length, bool (*comparer)(T& a, T& b))
	{
		for (size_t i = 1; i < length; ++i)
		{
			size_t idx = i;
			while (idx > 0)
			{
				auto& current = arr[idx];
				auto& other = arr[idx - 1];

				if (!comparer(current, other))
					break;

				T temp = current;
				current = other;
				other = temp;

				--idx;
			}
		}
	}
}
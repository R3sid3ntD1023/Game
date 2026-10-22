#pragma once
#include <algorithm>
#include <cstring>
#include <imgui.h>
#include <string>

namespace editor
{

	// InputText for std::string through a fixed buffer.
	inline bool InputText(const char *label, std::string &value)
	{
		char buf[128];
		const size_t n = std::min(value.size(), sizeof(buf) - 1);
		std::memcpy(buf, value.data(), n);
		buf[n] = '\0';
		if (!ImGui::InputText(label, buf, sizeof(buf)))
			return false;
		value = buf;
		return true;
	}

} // namespace editor

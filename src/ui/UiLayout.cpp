#include "ui/UiLayout.h"
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace ui
{

	namespace
	{
		template <class Archive>
		void SerializeVec(Archive &ar, const char *name, float *v, int n)
		{
			std::vector<float> values(v, v + n);
			ar(cereal::make_nvp(name, values));
			for (int i = 0; i < n && i < static_cast<int>(values.size()); ++i)
				v[i] = values[i];
		}
	} // namespace

	// Serialized through non-const references; SaveUiLayout works on a copy.
	template <class Archive>
	void serialize(Archive &ar, UiElement &e)
	{
		{
			int type = static_cast<int>(e.type), anchor = static_cast<int>(e.anchor), align = static_cast<int>(e.align);
			ar(cereal::make_nvp("name", e.name), cereal::make_nvp("type", type), cereal::make_nvp("anchor", anchor), cereal::make_nvp("stretchX", e.stretchX),
			   cereal::make_nvp("stretchY", e.stretchY), cereal::make_nvp("visible", e.visible), cereal::make_nvp("text", e.text), cereal::make_nvp("textSize", e.textSize),
			   cereal::make_nvp("align", align), cereal::make_nvp("value", e.value));
			SerializeVec(ar, "offset", &e.offset.x, 2);
			SerializeVec(ar, "size", &e.size.x, 2);
			SerializeVec(ar, "color", &e.color.x, 4);
			SerializeVec(ar, "secondary", &e.secondary.x, 4);
			e.type = static_cast<ElementType>(std::clamp(type, 0, 3));
			e.anchor = static_cast<Anchor>(std::clamp(anchor, 0, 8));
			e.align = static_cast<Align>(std::clamp(align, 0, 2));
		}
	}

	UiLayout LoadUiLayout(const std::string &path)
	{
		std::ifstream in(path);
		if (!in)
			throw std::runtime_error("Cannot open " + path);
		cereal::JSONInputArchive ar(in);
		UiLayout layout;
		ar(cereal::make_nvp("elements", layout.elements));
		return layout;
	}

	void SaveUiLayout(const std::string &path, const UiLayout &layout)
	{
		std::filesystem::create_directories(std::filesystem::path(path).parent_path());
		std::ofstream out(path);
		if (!out)
			throw std::runtime_error("Cannot write " + path);
		cereal::JSONOutputArchive ar(out);
		UiLayout copy = layout;
		ar(cereal::make_nvp("elements", copy.elements));
	}

	Rect ElementRect(const UiElement &e, const Rect &parent, float scale)
	{
		glm::vec2 sz = e.size * scale;
		const glm::vec2 off = e.offset * scale;
		if (e.stretchX)
			sz.x = parent.size.x;
		if (e.stretchY)
			sz.y = parent.size.y;
		const int index = static_cast<int>(e.anchor);
		const int h = index % 3, v = index / 3;
		float x = parent.pos.x, y = parent.pos.y;
		if (!e.stretchX)
			x += h == 0 ? off.x : (h == 1 ? (parent.size.x - sz.x) * 0.5f + off.x : parent.size.x - sz.x - off.x);
		if (!e.stretchY)
			y += v == 0 ? off.y : (v == 1 ? (parent.size.y - sz.y) * 0.5f + off.y : parent.size.y - sz.y - off.y);
		return {{x, y}, sz};
	}

	void BuildWidgets(Widget &root, const UiLayout &layout, std::vector<std::pair<std::string, Widget *>> *byName)
	{
		const auto apply = [](Widget &w, const UiElement &e)
		{
			w.anchor = e.anchor;
			w.offset = e.offset;
			w.size = e.size;
			w.stretchX = e.stretchX;
			w.stretchY = e.stretchY;
			w.visible = e.visible;
		};
		for (const UiElement &e : layout.elements)
		{
			Widget *created = nullptr;
			switch (e.type)
			{
			case ElementType::Panel:
				created = &root.Add<Panel>();
				static_cast<Panel *>(created)->color = e.color;
				break;
			case ElementType::Label:
			{
				auto &l = root.Add<Label>();
				l.text = e.text;
				l.textSize = e.textSize;
				l.color = e.color;
				l.align = e.align;
				created = &l;
				break;
			}
			case ElementType::ProgressBar:
			{
				auto &b = root.Add<ProgressBar>();
				b.value = e.value;
				b.fill = e.color;
				b.background = e.secondary;
				created = &b;
				break;
			}
			case ElementType::Button:
			{
				auto &b = root.Add<Button>();
				b.text = e.text;
				b.textSize = e.textSize;
				b.backgroundColor = e.color;
				b.textColor = e.secondary;
				created = &b;
				break;
			}
			}
			apply(*created, e);
			if (byName)
				byName->emplace_back(e.name, created);
		}
	}

} // namespace ui

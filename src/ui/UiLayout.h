#pragma once
#include "ui/Widget.h"
#include <string>
#include <vector>

namespace ui
{

	enum class ElementType
	{
		Panel,
		Label,
		ProgressBar,
		Button
	};

	// One widget of a saved screen layout. Positions use the widget system's design pixels (1280x720 reference).
	struct UiElement
	{
		std::string name = "Element";
		ElementType type = ElementType::Panel;
		Anchor anchor = Anchor::TopLeft;
		glm::vec2 offset{0.0f};
		glm::vec2 size{100.0f, 40.0f};
		bool stretchX = false;
		bool stretchY = false;
		bool visible = true;
		Color color{0.0f, 0.0f, 0.0f, 0.5f}; // panel fill, label text, bar fill, button background
		Color secondary{0.0f, 0.0f, 0.0f, 0.6f}; // bar background, button text
		std::string text;
		float textSize = 20.0f;
		Align align = Align::Left;
		float value = 1.0f;
	};

	struct UiLayout
	{
		std::vector<UiElement> elements;
	};

	inline constexpr const char *kUiLayoutExtension = ".ui.json";
	inline constexpr float kUiReferenceHeight = 720.0f;

	UiLayout LoadUiLayout(const std::string &path); // throws on failure
	void SaveUiLayout(const std::string &path, const UiLayout &layout);

	// Adds a widget per element under `root`; `byName` (optional) receives each created widget for binding.
	void BuildWidgets(Widget &root, const UiLayout &layout, std::vector<std::pair<std::string, Widget *>> *byName = nullptr);

	// Screen rectangle of an element inside `parent`, mirroring Widget::Layout.
	Rect ElementRect(const UiElement &element, const Rect &parent, float scale);

} // namespace ui

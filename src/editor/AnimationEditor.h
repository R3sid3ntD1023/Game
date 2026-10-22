#pragma once
#include <string>
#include <vector>

namespace editor
{

	struct AnimParam
	{
		std::string name = "param";
		bool isBool = false;
		float defaultValue = 0.0f;
	};

	struct AnimState
	{
		std::string name = "State";
		std::string clip;
		float speed = 1.0f;
		bool loop = true;
		float x = 40.0f, y = 40.0f; // position on the graph canvas
	};

	struct AnimTransition
	{
		int from = 0, to = 0;
		std::string param; // empty = unconditional (when the state finishes)
		int op = 0;        // 0: >, 1: <, 2: ==, 3: true, 4: false
		float value = 0.0f;
		float blendTime = 0.2f;
	};

	struct AnimGraph
	{
		std::vector<AnimParam> params;
		std::vector<AnimState> states;
		std::vector<AnimTransition> transitions;
		int entry = 0;
	};

	// Dockable "Animation Graph" window: node-graph editor for state machines stored in assets/animation/*.anim.json.
	class AnimationEditor
	{
	public:
		AnimationEditor();
		void Draw();
		bool open = true;

	private:
		void Refresh();
		void Open(const std::string &name);
		void Save();
		void DrawCanvas();
		void DrawSide();
		int AddState(float x, float y);
		void DeleteState(int index);

		int m_linkFrom = -1;      // state whose output handle is being dragged
		int m_contextState = -1;  // state under the right-click menu, -1 for empty canvas
		struct Vec2f
		{
			float x = 0.0f, y = 0.0f;
		} m_contextPos;

		AnimGraph m_graph;
		std::string m_name;
		std::string m_newName;
		std::vector<std::string> m_graphs;
		int m_selectedState = -1;
		int m_selectedTransition = -1;
		int m_dragging = -1;
		float m_panX = 0.0f, m_panY = 0.0f;
		bool m_dirty = false;
		std::string m_status;
	};

} // namespace editor

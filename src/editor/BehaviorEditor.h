#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace editor
{

	// One node of an authored behaviour tree. `a`/`b` carry the numeric parameters of the node type
	// (Wait: duration/deviation, MoveTo: speed/stop distance, FindRandomReachablePoint: radius, Repeat: count).
	struct BtNode
	{
		BtNode();
		int id; // editor-only identity, not saved
		std::string type = "Sequence";
		std::string name;
		float a = 0.0f;
		float b = 0.0f;
		std::string key; // Condition/Action: name of the game callback; MoveTo/Find...: blackboard key
		std::vector<BtNode> children;
	};

	// Dockable "Behavior Tree" window: node-graph editor for trees stored in assets/behavior/*.bt.json.
	class BehaviorEditor
	{
	public:
		BehaviorEditor();
		void Draw();
		bool open = false;

	private:
		struct Pos
		{
			float x, y;
		};

		void Refresh();
		void Open(const std::string &name);
		void Save();
		void DrawCanvas();
		void DrawProperties(BtNode &node, BtNode *parent, int index);
		void Layout(const BtNode &node, int depth, float &nextX);
		bool Find(BtNode &node, BtNode *parent, int index, int id, BtNode *&outNode, BtNode *&outParent, int &outIndex);
		void DeleteSelected();
		void AddChild(int parentId, const char *type);

		int m_linkFrom = -1; // node whose child handle is being dragged
		int m_dragId = -1;	 // node being dragged to reorder
		bool m_dragActive = false;
		Pos m_dragStart{0.0f, 0.0f};
		int m_pendingParent = -1; // parent for the "add child" menu opened by dropping a link on empty space

		BtNode m_root;
		std::string m_name;
		std::string m_newName;
		std::vector<std::string> m_trees;
		std::unordered_map<int, Pos> m_pos;
		int m_selectedId = -1;
		float m_panX = 20.0f, m_panY = 20.0f;
		bool m_dirty = false;
		std::string m_status;
	};

} // namespace editor

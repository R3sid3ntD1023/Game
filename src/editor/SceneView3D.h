#pragma once
#include "eng/Scene.h"
#include "editor/PlaySession.h"
#include "core/Window.h"
#include "game/Terrain.h"
#include "gfx/EnvironmentMap.h"
#include "gfx/Mesh.h"
#include "gfx/Shader.h"
#include "gfx/Texture.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace editor
{

	// Dockable "3D View" window: renders the SceneData (meshes by kind, colliders, markers) into an offscreen
	// framebuffer with a fly camera, and picks/moves objects with the mouse.
	class SceneView3D
	{
	public:
		SceneView3D();
		~SceneView3D();
		SceneView3D(const SceneView3D &) = delete;
		SceneView3D &operator=(const SceneView3D &) = delete;

		// Drop every cached GPU mesh (after loading another world).
		void Invalidate();
		// Draws the window. Returns true when the user picked a different object (selected is updated).
		// Sets dirty when the user moves an object.
		bool Draw(eng::SceneData &scene, int &selected, bool &dirty, const game::Terrain &terrain);
		bool open = true; // window visibility (closeable)
		void FocusOn(const eng::ObjectDesc &object);
		// Toolbar with the gizmo, camera speed and display options (drawn inside the 3D View window).
		void DrawControls();
		void DrawDisplayOptions();
		// Forget cached textures (after importing or changing asset files).
		void ReloadTextures();
		// Overrides the time of day used for rendering (hours); negative restores the scene's own value.
		void SetPreviewTime(float hours) { m_previewTime = hours; }
		// Shows a play/simulate session in place of the editing view; nullptr returns to editing.
		void AttachSession(PlaySession *session, core::Window *window);
		void SetSessionPaused(bool paused) { m_sessionPaused = paused; }
		// Moves the fly camera to a scene camera's pose (and field of view), or reads its pose back.
		void SetViewFrom(const glm::vec3 &position, const glm::quat &rotation, float fovDegrees);
		void GetView(glm::vec3 &position, glm::quat &rotation) const;

	private:
		PlaySession *m_session = nullptr;
		core::Window *m_window = nullptr;
		bool m_sessionPaused = false;
		float m_fov = 60.0f;
		bool m_playCaptured = false;
		void ReleasePlayInput();
		float m_previewTime = -1.0f;
		struct GpuMesh
		{
			GLuint vao = 0, vbo = 0, ebo = 0;
			GLsizei indexCount = 0;
			gfx::Bounds bounds;
			bool valid = false;
			// Imported models: one part per material with its own colour and albedo texture.
			std::vector<GpuMesh> parts;
			glm::vec3 tint{1.0f};
			glm::vec3 emissive{0.0f};
			std::shared_ptr<gfx::Texture> albedo;
		};

		static void FreeMesh(GpuMesh &mesh);
		void LoadModelParts(const eng::MeshDesc &desc, GpuMesh &gpu);

		const GpuMesh &MeshFor(const eng::SceneData &scene, int index, const game::Terrain &terrain);
		void EnsureTarget(int width, int height);
		void Render(const eng::SceneData &scene, int selected, const glm::mat4 &view, const glm::mat4 &proj, const game::Terrain &terrain);
		void UpdateEnvironment(const eng::EnvironmentDesc &env);
		gfx::Texture *AlbedoFor(const std::string &path);
		std::unordered_map<std::string, std::shared_ptr<gfx::Texture>> m_albedoCache;
		bool m_textures = true;
		bool m_skyLight = true;
		float m_skyLightIntensity = 1.0f;
		bool m_fog = false;
		float m_fogDensity = 0.004f;
		static glm::mat4 ModelMatrix(const eng::ObjectDesc &o, bool withScale);
		glm::mat4 ViewMatrix() const;
		glm::vec3 Forward() const;

		gfx::Shader m_shader;
		gfx::Shader m_tonemap;
		GLuint m_emptyVao = 0;
		GLuint m_hdrFbo = 0, m_hdrColor = 0;
		std::unique_ptr<gfx::EnvironmentMap> m_environment;
		std::string m_environmentPath;
		bool m_lit = true;
		bool m_showSky = true;
		float m_exposure = 1.0f;
		GpuMesh m_cube;
		std::unordered_map<int, GpuMesh> m_meshes;
		GLuint m_fbo = 0, m_color = 0, m_depth = 0;
		int m_width = 0, m_height = 0;

		glm::vec3 m_position{0.0f, 40.0f, 80.0f};
		float m_yaw = -90.0f;
		float m_pitch = -25.0f;
		float m_speed = 25.0f;
		bool m_showColliders = true;
		bool m_showLights = true;
		int m_op = 0;	// 0 translate, 1 rotate, 2 scale
		bool m_captured = false;
		bool m_local = false;
		bool m_snap = false;
		float m_snapMove = 1.0f;
		float m_snapRotate = 15.0f;
		float m_snapScale = 0.1f;
		GpuMesh m_sphere;
	};

} // namespace editor

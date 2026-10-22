#include "editor/PlaySession.h"
#include "eng/BuiltinComponents.h"
#include "gfx/ShaderLibrary.h"
#include "ui/UiLayout.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace editor
{

	PlaySession::PlaySession(const eng::SceneData &scene, game::Terrain &terrain, Mode mode) : m_mode(mode), m_terrain(terrain)
	{
		m_audio = std::make_unique<audio::AudioEngine>();
		m_world.audio = m_audio.get();
		m_world.SetWorldQuery(&terrain);

		auto shaders = std::make_shared<gfx::ShaderLibrary>(SHADER_DIR "materials/");
		eng::SceneLoader loader(shaders);
		m_content.world = &m_world;
		m_content.state = &m_state;
		m_content.terrain = &terrain;
		m_content.gameplay = mode == Mode::Play;
		game::RegisterGameContent(loader, m_content);

		const std::string skyPath = scene.environment.skyHdr.empty() ? std::string() : (std::filesystem::path(ASSETS_DIR) / scene.environment.skyHdr).string();
		m_environment = std::make_shared<gfx::EnvironmentMap>(skyPath);
		game::ConfigureWorld(m_world, scene, m_environment, terrain);

		loader.Instantiate(m_world, scene);
		const eng::ObjectDesc *cameraObject = nullptr;
		for (const eng::ObjectDesc &o : scene.objects)
			if (o.camera.enabled && (!cameraObject || o.camera.primary))
				cameraObject = &o;
		if (cameraObject)
		{
			m_sceneCamera.position = cameraObject->position;
			m_sceneCamera.forward = cameraObject->rotation * glm::vec3(0.0f, 0.0f, -1.0f);
			m_sceneCamera.fovDegrees = cameraObject->camera.fov;
			m_sceneCamera.nearPlane = cameraObject->camera.nearPlane;
			m_sceneCamera.farPlane = cameraObject->camera.farPlane;
			m_hasSceneCamera = true;
		}
		if (mode == Mode::Play && !m_content.player && !m_hasSceneCamera)
			throw std::runtime_error("The world has no Player or Camera object to play");

		if (mode == Mode::Play && m_content.player)
		{
			m_hud = std::make_unique<game::Hud>(m_canvas.Root());
			m_hasUi = true;
		}
		if (!scene.ui.empty())
		{
			try
			{
				const ui::UiLayout layout = ui::LoadUiLayout(std::string(ASSETS_DIR) + "ui/" + scene.ui + ui::kUiLayoutExtension);
				ui::BuildWidgets(m_canvas.Root(), layout);
				m_hasUi = true;
			}
			catch (const std::exception &e)
			{
				spdlog::warn("UI layout '{}' not loaded: {}", scene.ui, e.what());
			}
		}

		glGenFramebuffers(1, &m_fbo);
		glGenTextures(1, &m_color);
	}

	PlaySession::~PlaySession()
	{
		m_post.reset();
		if (m_fbo)
			glDeleteFramebuffers(1, &m_fbo);
		if (m_color)
			glDeleteTextures(1, &m_color);
	}

	void PlaySession::Update(float dt, const gfx::Camera &flyCamera)
	{
		m_audio->Update();
		dt = std::min(dt, 0.05f);
		if (m_mode == Mode::Play)
		{
			if (m_state.status != game::Status::Playing)
				return;
			m_state.ecology->Update(dt);
			m_world.Update(dt, ActiveCamera(flyCamera));
			if (!m_content.player)
				return;
			if (const auto *health = m_content.player->GetComponent<eng::Health>(); health && health->Dead())
				m_state.status = game::Status::Lost;
		}
		else
			m_world.Update(dt, flyCamera);
	}

	void PlaySession::EnsureTarget(int width, int height)
	{
		if (m_post && width == m_width && height == m_height)
			return;
		m_width = width;
		m_height = height;
		glBindTexture(GL_TEXTURE_2D, m_color);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		if (!m_post)
		{
			m_post = std::make_unique<gfx::PostProcess>(width, height);
			m_post->outputFbo = m_fbo;
		}
		else
			m_post->Resize(width, height);
	}

	GLuint PlaySession::Render(int width, int height, const gfx::Camera &flyCamera, float time)
	{
		EnsureTarget(width, height);
		gfx::Camera camera = ActiveCamera(flyCamera);

		m_post->time = time;
		m_post->showRaw = false;
		m_post->exposure = glm::mix(1.6f, 0.85f, m_world.DayNight().Daylight());
		m_post->nearPlane = camera.nearPlane;
		m_post->farPlane = camera.farPlane;
		m_post->BeginScene();
		m_world.Render(static_cast<float>(width) / static_cast<float>(height), m_post->DepthTexture(), width, height, camera);
		m_post->EndScene(width, height);
		if (m_hasUi)
		{
			if (m_hud && m_content.player)
				m_hud->Update(*m_content.player, m_state, time);
			glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
			glViewport(0, 0, width, height);
			m_canvas.Frame(glm::vec2(width, height), glm::vec2(-1.0f), false);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return m_color;
	}

} // namespace editor

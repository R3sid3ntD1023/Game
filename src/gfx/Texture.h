#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace gfx
{
	struct DecodedTexture
	{
		std::vector<std::byte> data;
		int width = 0;
		int height = 0;
		int channels = 0;
	};

	class Texture
	{
	public:
		static std::shared_ptr<Texture> FromFile(const std::string &path, bool srgb);
		// Decodes an encoded image (png/jpg/...) already in memory, e.g. a texture embedded in a .glb.
		static std::shared_ptr<Texture> FromMemory(const unsigned char *bytes, size_t size, bool srgb);
		static std::shared_ptr<Texture> FromColor(const glm::vec4 &color, bool srgb);
		static DecodedTexture FromFileToBytes(const std::string &path);

		Texture(const unsigned char *rgba, int width, int height, bool srgb);
		~Texture();
		Texture(const Texture &) = delete;
		Texture &operator=(const Texture &) = delete;

		void Bind(GLuint unit) const { glBindTextureUnit(unit, m_id); }
		GLuint Id() const { return m_id; }

	private:
		GLuint m_id = 0;
	};

} // namespace gfx

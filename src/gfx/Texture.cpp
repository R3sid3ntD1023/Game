#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "gfx/Texture.h"
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace gfx
{

	Texture::Texture(const unsigned char *rgba, int width, int height, bool srgb)
	{
		int levels = 1 + static_cast<int>(std::floor(std::log2(std::max(width, height))));
		glCreateTextures(GL_TEXTURE_2D, 1, &m_id);
		glTextureStorage2D(m_id, levels, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, width, height);
		glTextureSubImage2D(m_id, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
		glGenerateTextureMipmap(m_id);
		glTextureParameteri(m_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTextureParameteri(m_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTextureParameterf(m_id, GL_TEXTURE_MAX_ANISOTROPY, 8.0f);
	}

	Texture::~Texture()
	{
		if (m_id)
			glDeleteTextures(1, &m_id);
	}

	std::shared_ptr<Texture> Texture::FromFile(const std::string &path, bool srgb)
	{
		int w = 0, h = 0, n = 0;
		stbi_set_flip_vertically_on_load(true);
		unsigned char *data = stbi_load(path.c_str(), &w, &h, &n, 4);
		if (!data)
		{
			spdlog::error("Failed to load texture '{}': {}", path, stbi_failure_reason());
			return FromColor(glm::vec4(1.0f, 0.0f, 1.0f, 1.0f), srgb);
		}
		auto tex = std::make_shared<Texture>(data, w, h, srgb);
		stbi_image_free(data);
		spdlog::info("Loaded texture '{}' ({}x{})", path, w, h);
		return tex;
	}

	std::shared_ptr<Texture> Texture::FromColor(const glm::vec4 &c, bool srgb)
	{
		auto to8 = [](float v) { return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
		const unsigned char px[4] = {to8(c.r), to8(c.g), to8(c.b), to8(c.a)};
		return std::make_shared<Texture>(px, 1, 1, srgb);
	}

	DecodedTexture Texture::FromFileToBytes(const std::string &path)
	{
		DecodedTexture tex;
		int w = 0, h = 0, n = 0;
		stbi_set_flip_vertically_on_load(true);
		unsigned char *img = stbi_load(path.c_str(), &w, &h, &n, 4);
		if (img)
		{
			tex.data.resize(w * h * 4);
			std::memcpy(tex.data.data(), img, tex.data.size());
			stbi_image_free(img);
			tex.width = w;
			tex.height = h;
			tex.channels = 4;
		}
		return tex;
	}

} // namespace gfx

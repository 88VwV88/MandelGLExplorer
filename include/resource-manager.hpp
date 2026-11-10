#ifndef MANDEL_RESOURCE_MANAGER_HPP
#define MANDEL_RESOURCE_MANAGER_HPP

#include <GL/glew.h>
#include <filesystem>
#include <span>
#include <vector>

namespace gl {

struct Texture2DCreateInfo {
  GLint width;
  GLint height;
  GLenum internal_format;
  GLenum image_format{GL_RGBA};
  std::optional<std::span<GLubyte const>> image;
};

struct ShaderFiles {
  std::filesystem::path vertex;
  std::filesystem::path fragment;
  std::optional<std::filesystem::path> geometry;
};

struct ResourceManager {
  auto create_vertex_array() -> GLuint;
  auto create_buffer(std::size_t size) -> GLuint;
  auto create_vertex_buffer(std::span<GLfloat const> data) -> GLuint;
  auto create_element_buffer(std::span<GLuint const> data) -> GLuint;
  auto create_texture_2d(Texture2DCreateInfo const &info) -> GLuint;
  auto create_shader_program(ShaderFiles const &files) -> GLuint;
  auto create_compute_program(std::filesystem::path const &compute_shader_path)
      -> GLuint;

  ~ResourceManager() noexcept;

private:
  std::vector<GLuint> m_buffers;
  std::vector<GLuint> m_arrays;
  std::vector<GLuint> m_textures;
  std::vector<GLuint> m_programs;
};

}; // namespace gl

#endif
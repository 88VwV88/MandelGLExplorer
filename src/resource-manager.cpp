#include "resource-manager.hpp"
#include <filesystem>
#include <fstream>

namespace gl {

auto ResourceManager::create_buffer(std::size_t size) -> GLuint {
  GLuint buffer;
  glCreateBuffers(1, &buffer);
  glNamedBufferStorage(buffer, size, nullptr, GL_DYNAMIC_STORAGE_BIT);
  return m_buffers.emplace_back(buffer);
}

auto ResourceManager::create_vertex_array() -> GLuint {
  GLuint array;
  glCreateVertexArrays(1, &array);
  return m_arrays.emplace_back(array);
}

auto ResourceManager::create_vertex_buffer(std::span<GLfloat const> data)
    -> GLuint {
  auto const &buffer = create_buffer(data.size_bytes());
  glNamedBufferSubData(buffer, 0, data.size_bytes(), data.data());
  return buffer;
}

auto ResourceManager::create_element_buffer(std::span<GLuint const> data)
    -> GLuint {
  auto const &buffer = create_buffer(data.size_bytes());
  glNamedBufferSubData(buffer, 0, data.size_bytes(), data.data());
  return buffer;
}

auto ResourceManager::create_texture_2d(Texture2DCreateInfo const &info)
    -> GLuint {
  GLuint texture;
  glCreateTextures(GL_TEXTURE_2D, 1, &texture);
  glTextureStorage2D(texture, 1, info.internal_format, info.width, info.height);
  if (info.image.has_value()) {
    glTextureSubImage2D(texture, 0, 0, 0, info.width, info.height,
                        info.image_format, GL_UNSIGNED_BYTE,
                        info.image->data());
  }
  return m_textures.emplace_back(texture);
}

static inline auto check_file_path(std::filesystem::path const &path) -> void {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("File not found: " + path.string());
  }
  if (!std::filesystem::is_regular_file(path)) {
    throw std::runtime_error("Not a regular file: " + path.string());
  }
}
static inline auto read_file_contents(std::filesystem::path const &path)
    -> std::string {
  check_file_path(path);
  std::ifstream file{path, std::ios::binary | std::ios::ate};
  if (!file) {
    throw std::runtime_error("Failed to open file: " + path.string());
  }

  auto const size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::string contents(size, '\0');
  if (!file.read(contents.data(), size)) {
    throw std::runtime_error("Failed to read file: " + path.string());
  }
  return contents;
}
auto check_shader_compile_status(GLuint shader) -> void {
  GLint param;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &param);
  if (param == GL_TRUE) {
    return;
  }
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &param);
  std::string log(param, '\0');
  glGetShaderInfoLog(shader, param, nullptr, log.data());
  throw std::runtime_error("Shader compilation failed: " + log);
}
auto check_program_link_status(GLuint program) -> void {
  GLint param;
  glGetProgramiv(program, GL_LINK_STATUS, &param);
  if (param == GL_TRUE) {
    return;
  }
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &param);
  std::string log(param, '\0');
  glGetProgramInfoLog(program, param, nullptr, log.data());
  throw std::runtime_error("Program linking failed: " + log);
}
auto ResourceManager::create_shader_program(const ShaderFiles &files)
    -> GLuint {
  GLuint vert_shader{0};
  GLuint frag_shader{0};
  GLuint geom_shader{0};

  try {
    auto const vert_source = read_file_contents(files.vertex);
    auto const vert_shader_source = vert_source.c_str();
    vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader, 1, &vert_shader_source, nullptr);
    glCompileShader(vert_shader);
    check_shader_compile_status(vert_shader);

    auto const frag_source = read_file_contents(files.fragment);
    auto const frag_shader_source = frag_source.c_str();
    frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader, 1, &frag_shader_source, nullptr);
    glCompileShader(frag_shader);
    check_shader_compile_status(frag_shader);

    auto const program = glCreateProgram();
    glAttachShader(program, frag_shader);
    glAttachShader(program, vert_shader);

    if (files.geometry.has_value()) {
      auto const geom_source = read_file_contents(files.geometry.value());
      auto const geom_shader_source = geom_source.c_str();
      geom_shader = glCreateShader(GL_GEOMETRY_SHADER);
      glShaderSource(geom_shader, 1, &geom_shader_source, nullptr);
      glCompileShader(geom_shader);
      check_shader_compile_status(geom_shader);
      glAttachShader(program, geom_shader);
    }
    glLinkProgram(program);
    check_program_link_status(program);

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
    if (geom_shader != 0) {
      glDeleteShader(geom_shader);
    }

    return program;
  } catch (std::exception const &error) {
    if (vert_shader != 0) {
      glDeleteShader(vert_shader);
    }
    if (frag_shader != 0) {
      glDeleteShader(frag_shader);
    }
    if (geom_shader != 0) {
      glDeleteShader(geom_shader);
    }

    throw std::runtime_error("Failed to compile vertex shader: " +
                             std::string(error.what()));
  }
}

auto ResourceManager::create_compute_program(
    std::filesystem::path const &compute_shader_path) -> GLuint {
  GLuint comp_shader{0};
  try {
    auto const comp_source = read_file_contents(compute_shader_path);
    auto const comp_shader_source = comp_source.c_str();
    comp_shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(comp_shader, 1, &comp_shader_source, nullptr);
    glCompileShader(comp_shader);
    check_shader_compile_status(comp_shader);

    auto const program = glCreateProgram();
    glAttachShader(program, comp_shader);
    glLinkProgram(program);
    check_program_link_status(program);

    glDeleteShader(comp_shader);
    return program;
  } catch (std::exception const &error) {
    if (comp_shader != 0) {
      glDeleteShader(comp_shader);
    }

    throw std::runtime_error("Failed to compile compute shader: " +
                             std::string(error.what()));
  }
}

ResourceManager::~ResourceManager() noexcept {
  glDeleteTextures(m_textures.size(), m_textures.data());
  glDeleteBuffers(m_buffers.size(), m_buffers.data());
  glDeleteVertexArrays(m_arrays.size(), m_arrays.data());
  for (auto const &program : m_programs) {
    glDeleteProgram(program);
  }
}
}; // namespace gl
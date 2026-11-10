#include "resource-manager.hpp"
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/ext/vector_float2_precision.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <print>
#include <stdexcept>

static constexpr GLint WIDTH = 1280;
static constexpr GLint HEIGHT = 720;

struct State {
  glm::vec2 center{0.0f, 0.0f};
  float zoom{1.0f};
  GLuint image;
  GLuint array;
  struct {
    GLuint mandel;
    GLuint render;
  } programs;
};

static constexpr std::array<GLfloat, 4 * 4> QUAD_VERTICES{
    -1.0f, -1.0f, 0.0f, 0.0f, //
    -1.0f, +1.0f, 0.0f, 1.0f, //
    +1.0f, -1.0f, 1.0f, 0.0f, //
    +1.0f, +1.0f, 1.0f, 1.0f, //
};
static constexpr std::array<GLuint, 6> QUAD_INDICES{
    0, 2, 3, //
    3, 1, 0, //
};

static inline auto process_keyboard_input(GLFWwindow *window, State &state,
                                          double delta_time) -> void;
static inline auto scroll_callback(GLFWwindow *window, double xoffset,
                                   double yoffset) -> void;
static inline auto display_screen_quad(State const &state) -> void;
static inline auto compute_mandel_image(State const &state) -> void;
static inline auto setup_opengl_debug_context() -> void;

int main() {
  gl::ResourceManager resources;

  glfwSetErrorCallback([](int error, const char *description) -> void {
    std::println(stderr, "[ERROR({})] {}", error, description);
  });
  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  GLFWwindow *window =
      glfwCreateWindow(WIDTH, HEIGHT, "MandelGL Explorer", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    throw std::runtime_error("Failed to create GLFW window");
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(0);

  auto const &status = glewInit();
  if (status != GLEW_OK and status != GLEW_ERROR_NO_GLX_DISPLAY) {
    glfwDestroyWindow(window);
    glfwTerminate();
    throw std::runtime_error("Failed to initialize GLEW");
  }
  setup_opengl_debug_context();

  std::println("[INFO] OpenGL Version: {}",
               reinterpret_cast<const char *>(glGetString(GL_VERSION)));
  glViewport(0, 0, WIDTH, HEIGHT);

  State state;
  glfwSetWindowUserPointer(window, &state);
  glfwSetScrollCallback(window, scroll_callback);

  state.programs.render =
      resources.create_shader_program({.vertex = "shaders/default.vert",
                                       .fragment = "shaders/default.frag",
                                       .geometry = "shaders/default.geom"});
  state.programs.mandel =
      resources.create_compute_program("shaders/mandel.comp");

  state.image = resources.create_texture_2d(
      {.width = WIDTH, .height = HEIGHT, .internal_format = GL_RGBA8});

  state.array = resources.create_vertex_array();
  const auto &vertex_buffer = resources.create_vertex_buffer(QUAD_VERTICES);
  const auto &element_buffer = resources.create_element_buffer(QUAD_INDICES);

  glVertexArrayVertexBuffer(state.array, 0, vertex_buffer, 0,
                            4 * sizeof(GLfloat));
  glVertexArrayElementBuffer(state.array, element_buffer);

  glVertexArrayAttribFormat(state.array, 0, 2, GL_FLOAT, GL_FALSE, 0);
  glVertexArrayAttribBinding(state.array, 0, 0);
  glEnableVertexArrayAttrib(state.array, 0);

  glVertexArrayAttribFormat(state.array, 1, 2, GL_FLOAT, GL_FALSE,
                            2 * sizeof(GLfloat));
  glVertexArrayAttribBinding(state.array, 1, 0);
  glEnableVertexArrayAttrib(state.array, 1);

  double last_frame{0.0};
  double delta_time{0.0};
  int fps_counter{0};

  while (not glfwWindowShouldClose(window)) {
    glfwPollEvents();

    auto const &current_frame = glfwGetTime();
    delta_time = current_frame - last_frame;
    last_frame = current_frame;
    if (fps_counter >= 60) {
      glfwSetWindowTitle(
          window,
          std::format("MandelGL Explorer | {} FPS | Frametime: {:.3f}ms",
                      static_cast<int>(1.0 / delta_time), delta_time)
              .c_str());
      fps_counter = 0;
    }
    fps_counter++;

    process_keyboard_input(window, state, delta_time);
    compute_mandel_image(state);
    display_screen_quad(state);

    glfwSwapBuffers(window);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
}

auto compute_mandel_image(State const &state) -> void {
  glUseProgram(state.programs.mandel);
  glUniform2f(glGetUniformLocation(state.programs.mandel, "uResolution"), WIDTH,
              HEIGHT);
  glUniform2f(glGetUniformLocation(state.programs.mandel, "uCenter"),
              state.center.x, state.center.y);
  glUniform1f(glGetUniformLocation(state.programs.mandel, "uZoom"), state.zoom);
  glUniform1i(glGetUniformLocation(state.programs.mandel, "uMaxIter"), 512);

  glBindImageTexture(0, state.image, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
  glDispatchCompute((WIDTH + 15) / 16, (HEIGHT + 15) / 16, 1);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                  GL_TEXTURE_FETCH_BARRIER_BIT);
}

auto display_screen_quad(State const &state) -> void {
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(state.programs.render);

  glBindTextureUnit(0, state.image);
  glUniform1i(glGetUniformLocation(state.programs.render, "uQuadTexture"), 0);

  glBindVertexArray(state.array);
  glDrawElements(GL_TRIANGLES, QUAD_INDICES.size(), GL_UNSIGNED_INT, nullptr);
}

auto process_keyboard_input(GLFWwindow *window, State &state, double delta_time)
    -> void {
  auto const speed = static_cast<float>(10.0 * delta_time / state.zoom);
  if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    state.center.y += speed;
  }
  if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    state.center.y -= speed;
  }
  if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    state.center.x += speed;
  }
  if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS ||
      glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    state.center.x -= speed;
  }
}

auto scroll_callback(GLFWwindow *window, double, double yoffset) -> void {
  auto &state = *reinterpret_cast<State *>(glfwGetWindowUserPointer(window));
  state.zoom = std::clamp(state.zoom + yoffset * 5.0f, 1.0, 100000.0);
}

static inline auto APIENTRY gl_debug_output(GLenum source, GLenum type,
                                            unsigned int id, GLenum severity,
                                            GLsizei length, const char *message,
                                            const void *) -> void {
  // ignore non-significant error/warning codes
  if (id == 131169 || id == 131185 || id == 131218 || id == 131204)
    return;

  std::cerr << "---------------" << std::endl;
  std::cerr << "Debug message (" << id << "): " << message << std::endl;

  switch (source) {
  case GL_DEBUG_SOURCE_API:
    std::cerr << "Source: API";
    break;
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
    std::cerr << "Source: Window System";
    break;
  case GL_DEBUG_SOURCE_SHADER_COMPILER:
    std::cerr << "Source: Shader Compiler";
    break;
  case GL_DEBUG_SOURCE_THIRD_PARTY:
    std::cerr << "Source: Third Party";
    break;
  case GL_DEBUG_SOURCE_APPLICATION:
    std::cerr << "Source: Application";
    break;
  case GL_DEBUG_SOURCE_OTHER:
    std::cerr << "Source: Other";
    break;
  }
  std::cerr << std::endl;

  switch (type) {
  case GL_DEBUG_TYPE_ERROR:
    std::cerr << "Type: Error";
    break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
    std::cerr << "Type: Deprecated Behaviour";
    break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
    std::cerr << "Type: Undefined Behaviour";
    break;
  case GL_DEBUG_TYPE_PORTABILITY:
    std::cerr << "Type: Portability";
    break;
  case GL_DEBUG_TYPE_PERFORMANCE:
    std::cerr << "Type: Performance";
    break;
  case GL_DEBUG_TYPE_MARKER:
    std::cerr << "Type: Marker";
    break;
  case GL_DEBUG_TYPE_PUSH_GROUP:
    std::cerr << "Type: Push Group";
    break;
  case GL_DEBUG_TYPE_POP_GROUP:
    std::cerr << "Type: Pop Group";
    break;
  case GL_DEBUG_TYPE_OTHER:
    std::cerr << "Type: Other";
    break;
  }
  std::cerr << std::endl;

  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH:
    std::cerr << "Severity: high";
    break;
  case GL_DEBUG_SEVERITY_MEDIUM:
    std::cerr << "Severity: medium";
    break;
  case GL_DEBUG_SEVERITY_LOW:
    std::cerr << "Severity: low";
    break;
  case GL_DEBUG_SEVERITY_NOTIFICATION:
    std::cerr << "Severity: notification";
    break;
  }
  std::cerr << std::endl;
  std::cerr << std::endl;
}

auto setup_opengl_debug_context() -> void {
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback(gl_debug_output, nullptr);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr,
                        GL_TRUE);
}
#include "resource-manager.hpp"
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/ext/vector_float2_precision.hpp>
#include <glm/glm.hpp>
#include <print>
#include <stdexcept>

static constexpr GLint WIDTH = 1280;
static constexpr GLint HEIGHT = 720;

struct State {
  glm::vec2 center;
  float zoom{1.0f};
  float dt;
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

static inline auto key_callback(GLFWwindow *window, int key, int scancode,
                                int action, int mods) -> void;
static inline auto scroll_callback(GLFWwindow *window, double xoffset,
                                   double yoffset) -> void;
static inline auto display_screen_quad(State const &state) -> void;
static inline auto compute_mandel_image(State const &state) -> void;

int main() {
  gl::ResourceManager resources;

  glfwSetErrorCallback([](int error, const char *description) -> void {
    std::println(stderr, "[ERROR({})] {}", error, description);
  });
  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

  GLFWwindow *window =
      glfwCreateWindow(WIDTH, HEIGHT, "MandelGL Explorer", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    throw std::runtime_error("Failed to create GLFW window");
  }
  glfwMakeContextCurrent(window);
  // glfwSwapInterval(0);

  auto const &status = glewInit();
  if (status != GLEW_OK and status != GLEW_ERROR_NO_GLX_DISPLAY) {
    glfwDestroyWindow(window);
    glfwTerminate();
    throw std::runtime_error("Failed to initialize GLEW");
  }
  std::println("[INFO] OpenGL Version: {}",
               reinterpret_cast<const char *>(glGetString(GL_VERSION)));
  glViewport(0, 0, WIDTH, HEIGHT);

  State state;
  glfwSetWindowUserPointer(window, &state);
  glfwSetKeyCallback(window, key_callback);
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

    state.dt = delta_time;
    compute_mandel_image(state);
    display_screen_quad(state);

    glfwPollEvents();
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

auto key_callback(GLFWwindow *window, int key, int, int action, int) -> void {
  auto &state = *reinterpret_cast<State *>(glfwGetWindowUserPointer(window));
  const auto SPEED = 10.0 / state.zoom;
  if ((key == GLFW_KEY_UP or key == GLFW_KEY_W) and action == GLFW_PRESS) {
    state.center.y += state.dt * SPEED;
  }
  if ((key == GLFW_KEY_DOWN or key == GLFW_KEY_S) and action == GLFW_PRESS) {
    state.center.y -= state.dt * SPEED;
  }
  if ((key == GLFW_KEY_RIGHT or key == GLFW_KEY_D) and action == GLFW_PRESS) {
    state.center.x += state.dt * SPEED;
  }
  if ((key == GLFW_KEY_LEFT or key == GLFW_KEY_A) and action == GLFW_PRESS) {
    state.center.x -= state.dt * SPEED;
  }
}

auto scroll_callback(GLFWwindow *window, double, double yoffset) -> void {
  auto &state = *reinterpret_cast<State *>(glfwGetWindowUserPointer(window));
  state.zoom = std::clamp(state.zoom + yoffset * 5.0f, 1.0, 100000.0);
}
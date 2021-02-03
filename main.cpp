#include <iostream>
#include <vector>
#include "GLM/glm.hpp"
#include "GLM/gtc/constants.hpp"
#include "GLM/gtx/rotate_vector.hpp"
#include "GLM/gtc/type_ptr.hpp"
#include "glad.h"
#include "GLFW/glfw3.h"

/* Keep the global state inside this struct */
static struct {
    glm::dvec2 mouse_position;
    glm::ivec2 screen_dimensions = glm::ivec2(600, 600);
    int scene=0;
} Globals;

/* GLFW Callback functions */
static void ErrorCallback(int error, const char* description)
{
    std::cerr << "Error: " << description << std::endl;
}

static void CursorPositionCallback(GLFWwindow* window, double x, double y)
{
    Globals.mouse_position.x = x;
    Globals.mouse_position.y = y;
}


static void WindowSizeCallback(GLFWwindow* window, int width, int height)
{
    Globals.screen_dimensions.x = width;
    Globals.screen_dimensions.y = height;

    glViewport(0, 0, width, height);
}

/* OpenGL Utility Structs */
struct VAO
{
    GLuint id;

    GLuint position_buffer;
    GLuint normals_buffer;
    GLsizei vertex_count;

    GLuint element_array_buffer;
    GLsizei element_array_count;

    VAO(
        const std::vector<glm::vec3>& positions,
        const std::vector<glm::vec3>& normals,
        const std::vector<GLuint>& indices
    )
    {
        glGenVertexArrays(1, &id);
        glBindVertexArray(id);

        glGenBuffers(1, &position_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
        glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, static_cast<void *>(0));
        glEnableVertexAttribArray(0);

        vertex_count = GLsizei(positions.size());

        glGenBuffers(1, &normals_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, normals_buffer);
        glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), normals.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, static_cast<void *>(0));
        glEnableVertexAttribArray(1);


        glGenBuffers(1, &element_array_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

        element_array_count = GLsizei(indices.size());
    }
};

/* OpenGL Utility Functions */
GLuint CreateShaderFromSource(const GLenum& shader_type, const GLchar * source)
{
    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    char info_log[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        std::cout << "Error: Shader Compilation failed" << std::endl;
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        std::cout << info_log << std::endl;

        glDeleteShader(shader);
        return NULL;
    }

    return shader;
}

GLuint CreateProgramFromSources(const GLchar * vertex_shader_source, const GLchar * fragment_shader_source)
{
    GLuint program = glCreateProgram();

    GLuint vertex_shader = CreateShaderFromSource(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_shader = CreateShaderFromSource(GL_FRAGMENT_SHADER, fragment_shader_source);

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    int success;
    char info_log[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        std::cout << "Error: Program Linking failed" << std::endl;
        glGetProgramInfoLog(program, 512, NULL, info_log);
        std::cout << info_log << std::endl;

        glDeleteProgram(program);
        return NULL;
    }

    return program;
}

/* Fun Stuff */
void GenerateParametricShape(
    std::vector<glm::vec3>& positions,
    std::vector<glm::vec3>& normals,
    std::vector<GLuint>& indices,
    glm::dvec2(*parametric_line)(double),
    int vertical_segments,
    int rotation_segments
)
{
    auto parametric_surface = [parametric_line](double t, double r)
    {
        auto p = glm::dvec3(parametric_line(t), 0);

        //auto s = sin(r * glm::two_pi<double>() * 6) / 2 + 1;
        //qp *= s * 0.3; //makes bigger
        //p.y *= sin(r * glm::two_pi<double>() * 3) / 2 + 1;
        return glm::rotateY(p, r * glm::two_pi<double>());
    };

    //vertices are calculated by this resolution
    positions.reserve(vertical_segments * rotation_segments);
    for (int r = 0; r < rotation_segments; ++r)
        for (int v = 0; v < vertical_segments; ++v)
            positions.push_back(parametric_surface(v / double(vertical_segments - 1), r / double(rotation_segments)));

    normals.reserve(vertical_segments * rotation_segments);
    for (int r = 0; r < rotation_segments; ++r)
        for (int v = 0; v < vertical_segments; ++v)
        {
            auto nv = v / double(vertical_segments - 1);
            auto nr = r / double(rotation_segments);
            
            auto epsilonv = 1 / double(vertical_segments - 1);
            auto epsilonr = 1 / double(rotation_segments);
            
            auto to_next_v = parametric_surface(nv + epsilonv, nr) - parametric_surface(nv, nr);
            auto from_prev_v = parametric_surface(nv, nr) - parametric_surface(nv - epsilonv, nr);
            auto tangent_v = (to_next_v + from_prev_v) / 2.; //take average

            auto to_next_r = parametric_surface(nv, nr + epsilonr) - parametric_surface(nv, nr);
            auto from_prev_r = parametric_surface(nv, nr) - parametric_surface(nv, nr - epsilonr);
            auto tangent_r = (to_next_r + from_prev_r) / 2.; //take average
                      
            auto normal = glm::normalize(glm::cross(tangent_r, tangent_v));
            normals.push_back(normal);
        }

    auto VRtoIndex = [vertical_segments, rotation_segments](int v, int r) //2D to 1D map
    {
        return (r % rotation_segments) * vertical_segments + v;
    };
    indices.reserve(rotation_segments * (vertical_segments - 1) * 6);
    for (int r = 0; r < rotation_segments; ++r)
        for (int v = 0; v < vertical_segments-1; ++v)
        {
            indices.push_back(VRtoIndex(v + 1, r));
            indices.push_back(VRtoIndex(v, r + 1));
            indices.push_back(VRtoIndex(v, r));

            indices.push_back(VRtoIndex(v + 1, r));
            indices.push_back(VRtoIndex(v + 1, r + 1));
            indices.push_back(VRtoIndex(v, r + 1));
        }
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS){
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
    if (key == GLFW_KEY_Q && action == GLFW_PRESS){
        Globals.scene = 1;
    }
    if (key == GLFW_KEY_W && action == GLFW_PRESS){
        Globals.scene = 2;
    }
    
    if (key == GLFW_KEY_E && action == GLFW_PRESS){
        Globals.scene = 3;
    }
        
    if (key == GLFW_KEY_R && action == GLFW_PRESS){
        Globals.scene = 4;
    }
        
    if (key == GLFW_KEY_T && action == GLFW_PRESS){
        Globals.scene = 5;
    }
        
    if (key == GLFW_KEY_Y && action == GLFW_PRESS){
        Globals.scene = 6;
    }
}

int main(void)
{
    /* Set GLFW error callback */
    glfwSetErrorCallback(ErrorCallback);

    /* Initialize the library */
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    /* Create a windowed mode window and its OpenGL context */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    //glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(
        Globals.screen_dimensions.x, Globals.screen_dimensions.y,
        "Ece Alptekin", NULL, NULL
    );
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    /* Load OpenGL extensions with GLAD */
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    /* Set GLFW Callbacks */
    glfwSetCursorPosCallback(window, CursorPositionCallback);
    glfwSetWindowSizeCallback(window, WindowSizeCallback);

    /* Configure OpenGL */
    glClearColor(0, 0, 0, 0.1f);
    glEnable(GL_DEPTH_TEST);

    /* Creating OpenGL objects */
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<GLuint> indices;

    auto ParametricHalfCircle = [](double t)
    {
        // [0, 1]
        t -= 0.5;
        // [-0.5, 0.5]
        t *= glm::pi<double>();
        // [-PI*0.5, PI*0.5]
        return glm::dvec2(cos(t), sin(t));
    };

    auto ParametricCircle = [](double t)
    {
        // [0, 1]
        //t -= 0.5;
        // [-0.5, 0.5]
        t *= glm::two_pi<double>();
        // [-PI, PI]

        //glm::dvec2 c(0.5, 0);
        auto c = glm::dvec2(0.7, 0);
        double r = 0.25;

        return glm::dvec2(cos(t), sin(t)) * r + c;
    };

    auto ParametricSpikes = [](double t)
    {
        // [0, 1]
        t -= 0.5;
        // [-0.5, 0.5]
        t *= glm::two_pi<double>();
        // [-PI, PI]

        auto c = glm::dvec2(0.7, 0);
        double r = 0.25;

        int a = 2 + 4 * 4;
        return (glm::dvec2(cos(t) + sin(a*t) / a, sin(t) + cos(a*t) / a) / 2.) * r + c;
    };
    
    auto ParametricSpikyCircle = [](double t)
    {
          // [0, 1]
          t *= glm::two_pi<double>();
          // [0, 2*PI]

          glm::dvec2 c(0.6, 0);
          double r = 0.35;
          int a = 1 + 2 * 6;
        
          return glm::dvec2(cos(t) + sin(a*t) / a, sin(t) + cos(a*t) / a) * r + c;
    };

    

    
    
    //program
    /* Creating OpenGL objects */
    GenerateParametricShape(positions, normals, indices, ParametricCircle, 16, 16);
    VAO shape_VAO(positions, normals, indices);
    
    //program_1
    /* Creating OpenGL objects */
    std::vector<glm::vec3> positions_1;
    std::vector<glm::vec3> normals_1;
    std::vector<GLuint> indices_1;
    GenerateParametricShape(positions_1, normals_1, indices_1, ParametricHalfCircle, 16, 16);
    VAO shape1_VAO(positions_1, normals_1, indices_1);
    
    //program_2
    /* Creating OpenGL objects */
    std::vector<glm::vec3> positions_2;
    std::vector<glm::vec3> normals_2;
    std::vector<GLuint> indices_2;
    GenerateParametricShape(positions_2, normals_2, indices_2, ParametricSpikyCircle, 60, 20);
    VAO shape2_VAO(positions_2, normals_2, indices_2);
    
    //program_3
    /* Creating OpenGL objects */
    std::vector<glm::vec3> positions_3;
    std::vector<glm::vec3> normals_3;
    std::vector<GLuint> indices_3;
    GenerateParametricShape(positions_3, normals_3, indices_3, ParametricSpikes, 12, 6);
    VAO shape3_VAO(positions_3, normals_3, indices_3);
    
    
    
    
    
     
    /* Creating OpenGL objects */
    std::vector<glm::vec3> six_positions;
    std::vector<glm::vec3> six_normals;
    std::vector<GLuint> six_indices;
    GenerateParametricShape(six_positions, six_normals, six_indices, ParametricSpikyCircle, 1024, 1024);
    VAO sixth_VAO(six_positions, six_normals, six_indices);

    
    
    
    

    
    
//scene 1
GLuint program = CreateProgramFromSources(
        R"VERTEX(
#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

uniform mat4 u_transform;

out vec3 vertex_position;
out vec3 vertex_normal;

void main()
{
    gl_Position = u_transform * vec4(a_position, 1);
    vertex_normal = (u_transform * vec4(a_normal, 0)).xyz;
    vertex_position = gl_Position.xyz;
}
        )VERTEX",

        R"FRAGMENT(
#version 330 core

in vec3 vertex_position;
in vec3 vertex_normal;

out vec3 out_color;

void main()
{
   out_color = vec3(1, 1, 1);
}
        )FRAGMENT");
    if (program == NULL)
    {
        glfwTerminate();
        return -1;
    }
    
//scene 2
    GLuint program_6 = CreateProgramFromSources(
            R"VERTEX(
    #version 330 core

    layout(location = 0) in vec3 a_position;
    layout(location = 1) in vec3 a_normal;

    uniform mat4 u_transform;

    out vec3 vertex_position;
    out vec3 vertex_normal;

    void main()
    {
        gl_Position = u_transform * vec4(a_position, 1);
        vertex_normal = (u_transform * vec4(a_normal, 0)).xyz;
        vertex_position = gl_Position.xyz;
    }
            )VERTEX",

            R"FRAGMENT(
    #version 330 core

    in vec3 vertex_position;
    in vec3 vertex_normal;

    out vec4 out_color;

    void main()
    {
          float ambient = 2;
          vec3 ambient_color = vec3(1);

          vec3 color = (ambient_color * ambient) * vertex_normal;
          
          out_color = vec4(normalize(color), 1);

    }
            )FRAGMENT");
        if (program_6 == NULL)
        {
            glfwTerminate();
            return -1;
        }
    
    
    
    
    
    
//scene 6
GLuint program_1 = CreateProgramFromSources(
            R"VERTEX(
#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

uniform mat4 u_transform;

out vec3 vertex_position;
out vec3 vertex_normal;

void main()
{
   gl_Position = u_transform * vec4(a_position, 1);
   vertex_normal = (u_transform * vec4(a_normal, 0)).xyz;
   vertex_position = gl_Position.xyz;
}
      )VERTEX",

    R"FRAGMENT(
#version 330 core

uniform vec2 u_mouse_position;

in vec3 vertex_position;
in vec3 vertex_normal;

out vec4 out_color;

void main()
{
  vec3 color = vec3(0);
                                                                                        
  vec3 surface_color = vec3(1, 1, 1);
  vec3 surface_position = vertex_position;
  vec3 surface_normal = normalize(vertex_normal);
                                                                                        
  vec3 ambient_color = vec3(0, 1, 0);
  color += ambient_color * surface_color;

  vec3 light_direction = normalize(vec3(1,1,-1));
  vec3 light_color = vec3(0, 0, 1);
                                                                        
  float diffuse_intensity = max(0, dot(light_direction, surface_normal));
  color += diffuse_intensity * light_color * surface_color;
                                                                                        
  vec3 view_dir = vec3(0,0,-1);
  vec3 halfway_dir = normalize(view_dir + light_direction);
  
  float shininess = 64;
  float specular_intensity = max(0, dot(halfway_dir, surface_normal));
  color += pow(specular_intensity, shininess) * light_color;
                                                                                            
  vec3 point_light_position = vec3(u_mouse_position,-1);
  vec3 point_light_color = vec3(1,0,0);
  vec3 to_point_light = normalize(point_light_position - surface_position);
                                            
  diffuse_intensity = max(0, dot(to_point_light, surface_normal));
  color += diffuse_intensity * point_light_color * surface_color;
                                                                                        
  view_dir = vec3(0,0,-1);
  halfway_dir = normalize(view_dir + to_point_light);
  float shininess_1 = 64;
  specular_intensity = max(0, dot(halfway_dir, surface_normal));
  color += pow(specular_intensity, shininess_1) * light_color;
                                            
  out_color = vec4(normalize(color), 1);
}
    )FRAGMENT");
        if (program_1 == NULL)
        {
            glfwTerminate();
            return -1;
        }
    
//scene 3
GLuint program_2 = CreateProgramFromSources(
          R"VERTEX(
#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

uniform mat4 u_transform;

out vec3 vertex_position;
out vec3 vertex_normal;

void main()
{
gl_Position = u_transform * vec4(a_position, 1);
vertex_normal = (u_transform * vec4(a_normal, 0)).xyz;
vertex_position = gl_Position.xyz;
}
       )VERTEX",

      R"FRAGMENT(
#version 330 core

in vec3 vertex_position;
in vec3 vertex_normal;

out vec4 out_color;

void main()
{
   vec3 color = vec3(0);
                                            
   vec3 surface_color = vec3(0.5, 0.5, 0.5);
   vec3 surface_position = vertex_position;
   vec3 surface_normal = normalize(vertex_normal);
                                            
   vec3 ambient_color = vec3(1);
   color += ambient_color * surface_color;

   vec3 light_direction = normalize(vec3(-1,-1,1));
   vec3 light_color = vec3(0.4, 0.4, 0.4);
                            
   float diffuse_intensity = max(0, dot(light_direction, surface_normal));
   color += diffuse_intensity * light_color * surface_color;
                                            
   vec3 view_dir = vec3(0,0,-1);
   vec3 halfway_dir = normalize(view_dir + light_direction);
   float shininess = 64;
   float specular_intensity = max(0, dot(halfway_dir, surface_normal));
   color += pow(specular_intensity, shininess) * light_color;

   out_color = vec4(color, 1);
}
            )FRAGMENT");
        if (program_2 == NULL)
        {
            glfwTerminate();
            return -1;
        }

//scene 4
GLuint program_3 = CreateProgramFromSources(
           R"VERTEX(
#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

uniform mat4 u_transform;

out vec3 vertex_position;
out vec3 vertex_normal;

void main()
{
   gl_Position = u_transform * vec4(a_position, 1);
   vertex_normal = (u_transform * vec4(a_normal, 0)).xyz;
   vertex_position = gl_Position.xyz;
}
        )VERTEX",

       R"FRAGMENT(
#version 330 core

uniform vec2 u_mouse_position;
uniform vec3 u_color;

in vec3 vertex_position;
in vec3 vertex_normal;

out vec4 out_color;

void main()
{
   vec3 color = vec3(0);
                    
   vec3 surface_color = u_color;
                                            
   float shininess;
   if(vertex_position.x <= 0 && vertex_position.y >= 0){ shininess = 128; }
   if(vertex_position.x >= 0 && vertex_position.y >= 0){ shininess = 32;  }
   if(vertex_position.x <= 0 && vertex_position.y <= 0){ shininess = 64;  }
   if(vertex_position.x >= 0 && vertex_position.y <= 0){ shininess = 64;  }
                                            
   vec3 surface_position = vertex_position;
   vec3 surface_normal = normalize(vertex_normal);
                                                                
   vec3 ambient_color = vec3(1);
   color += ambient_color * surface_color;

   vec3 light_direction = normalize(vec3(-1,-1,1));
   vec3 light_color = vec3(0.4, 0.4, 0.4);
                                                
   float diffuse_intensity = max(0, dot(light_direction, surface_normal));
   color += diffuse_intensity * light_color * surface_color;
                                                                
   vec3 view_dir = vec3(0,0,-1);
   vec3 halfway_dir = normalize(view_dir + light_direction);
   float specular_intensity = max(0, dot(halfway_dir, surface_normal));
   color += pow(specular_intensity, shininess) * light_color;
                                       
   vec3 point_light_position = vec3(u_mouse_position,-1);
   vec3 point_light_color = vec3(0.5,0.5,0.5);
   vec3 to_point_light = normalize(point_light_position - surface_position);
                                                                                      
   diffuse_intensity = max(0, dot(to_point_light, surface_normal));
   color += diffuse_intensity * point_light_color * surface_color;
                                                                                    
   view_dir = vec3(0,0,-1);
   halfway_dir = normalize(view_dir + to_point_light);
   specular_intensity = max(0, dot(halfway_dir, surface_normal));
   color += pow(specular_intensity, shininess) * light_color;

   out_color = vec4((color), 1);

   
}
                )FRAGMENT");
            if (program_3 == NULL)
            {
                glfwTerminate();
                return -1;
            }
    
//scene 5
GLuint program_5 = CreateProgramFromSources(
              R"VERTEX(
    #version 330 core

    layout(location = 0) in vec3 a_position;
    layout(location = 1) in vec3 a_normal;

    uniform mat4 u_transform;

    out vec3 vertex_position;
    out vec3 vertex_normal;

    void main()
    {
    gl_Position = u_transform * vec4(a_position, 1);
    vertex_normal = (u_transform * vec4(a_normal, 0)).xyz;
    vertex_position = gl_Position.xyz;
    }
           )VERTEX",

          R"FRAGMENT(
    #version 330 core

    uniform vec2 u_mouse_position;
    uniform vec3 u_color;

    in vec3 vertex_position;
    in vec3 vertex_normal;

    out vec4 out_color;

    void main()
    {
       vec3 color = vec3(0);
                                                
       vec3 surface_color = u_color;
       vec3 surface_position = vertex_position;
       vec3 surface_normal = normalize(vertex_normal);
                                                
       vec3 ambient_color = vec3(1);
       color += ambient_color * surface_color;

       vec3 light_direction = normalize(vec3(-1,-1,1));
       vec3 light_color = vec3(0.4, 0.4, 0.4);
                                
       float diffuse_intensity = max(0, dot(light_direction, surface_normal));
       color += diffuse_intensity * light_color * surface_color;
                                                
       vec3 view_dir = vec3(0,0,-1);
       vec3 halfway_dir = normalize(view_dir + light_direction);
       float shininess = 64;
       float specular_intensity = max(0, dot(halfway_dir, surface_normal));
       color += pow(specular_intensity, shininess) * light_color;
     
       out_color = vec4(color, 1);
    }
                )FRAGMENT");
            if (program_5 == NULL)
            {
                glfwTerminate();
                return -1;
            }

    
    
    auto u_transform_location = glGetUniformLocation(program, "u_transform");
    auto u_transform_location_1 = glGetUniformLocation(program_1, "u_transform");
    auto u_transform_location_2 = glGetUniformLocation(program_2, "u_transform");
    auto u_transform_location_3 = glGetUniformLocation(program_3, "u_transform");
    auto u_transform_location_5 = glGetUniformLocation(program_5, "u_transform");
    auto u_transform_location_6 = glGetUniformLocation(program_6, "u_transform");
    auto u_mouse_position_location_3 = glGetUniformLocation(program_3, "u_mouse_position");
    auto u_mouse_position_location_1 = glGetUniformLocation(program_1, "u_mouse_position");
    
    auto color_location = glGetUniformLocation(program_3, "u_color");
    auto color_location_5 = glGetUniformLocation(program_5, "u_color");
     
    glm::vec3 chasing_pos = glm::vec3(0,0,0);
    
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        glfwSetKeyCallback(window, key_callback);
        
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Change the position of a vertex dynamically
        auto mouse_position = Globals.mouse_position / glm::dvec2(Globals.screen_dimensions);
        mouse_position.y = 1. - mouse_position.y;
        mouse_position = mouse_position * 2. - 1.;
        
    if(Globals.scene == 0)
     {
          glUseProgram(program);
          glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                         
          glm::mat4 transform(1.0);
          transform = glm::scale(transform, glm::vec3(0.4));
                     
          transform       = glm::translate(transform,  glm::vec3(1.2,1.2,0));
          auto transform3 = glm::translate(transform,  glm::vec3(0,-2.4,-0));
          auto transform2 = glm::translate(transform,  glm::vec3(-2.2,0.0,0));
          auto transform4 = glm::translate(transform,  glm::vec3(-2.1,-2.3,0));
                     
          transform  = glm::rotate(transform,  glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
          transform2 = glm::rotate(transform2, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
          transform3 = glm::rotate(transform3, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
          transform4 = glm::rotate(transform4, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
                         
          glBindVertexArray(shape_VAO.id);
          glUniformMatrix4fv(u_transform_location, 1, GL_FALSE, glm::value_ptr(transform));
          glDrawElements(GL_TRIANGLES, shape_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
                                 
          glBindVertexArray(shape1_VAO.id);
          glUniformMatrix4fv(u_transform_location, 1, GL_FALSE, glm::value_ptr(transform2));
          glDrawElements(GL_TRIANGLES, shape1_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
                                        
          glBindVertexArray(shape3_VAO.id);
          glUniformMatrix4fv(u_transform_location, 1, GL_FALSE, glm::value_ptr(transform3));
          glDrawElements(GL_TRIANGLES, shape3_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
                         
          glBindVertexArray(shape2_VAO.id);
          glUniformMatrix4fv(u_transform_location, 1, GL_FALSE, glm::value_ptr(transform4));
          glDrawElements(GL_TRIANGLES, shape2_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
     }
         
        
    if(Globals.scene == 1)
    {
         glUseProgram(program);
         glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                        
         glm::mat4 transform(1.0);
         transform = glm::scale(transform, glm::vec3(0.4));
                    
         transform       = glm::translate(transform,  glm::vec3(1.2,1.2,0));
         auto transform3 = glm::translate(transform,  glm::vec3(0,-2.4,-0));
         auto transform2 = glm::translate(transform,  glm::vec3(-2.2,0.0,0));
         auto transform4 = glm::translate(transform,  glm::vec3(-2.1,-2.3,0));
                    
         transform  = glm::rotate(transform,  glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
         transform2 = glm::rotate(transform2, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
         transform3 = glm::rotate(transform3, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
         transform4 = glm::rotate(transform4, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
                        
        
         //shape - parametricCircle, shape1 - ParametricHalfCircle, 2 - ParametricSpikyCircle, 3 - ParametricSpikes
        
         glBindVertexArray(shape_VAO.id); //ParametricCirle //sağ üst
         glUniformMatrix4fv(u_transform_location, 1, GL_FALSE, glm::value_ptr(transform));
         glDrawElements(GL_TRIANGLES, shape_VAO.element_array_count, GL_UNSIGNED_INT, NULL); // Draw the triangle
                                
         glBindVertexArray(shape1_VAO.id); //ParametricHalfCircle //sol üst
         glUniformMatrix4fv(u_transform_location, 1, GL_FALSE, glm::value_ptr(transform2));
         glDrawElements(GL_TRIANGLES, shape1_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
            
         glBindVertexArray(shape3_VAO.id); //ParametricSpikes //sağ alt
         glUniformMatrix4fv(u_transform_location, 1, GL_FALSE, glm::value_ptr(transform3));
         glDrawElements(GL_TRIANGLES, shape3_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
          
         glBindVertexArray(shape2_VAO.id); //ParametricSpikyCircle //sol alt
         glUniformMatrix4fv(u_transform_location, 1, GL_FALSE, glm::value_ptr(transform4));
         glDrawElements(GL_TRIANGLES, shape2_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
         
    }
        
        
    if(Globals.scene == 2)
    {
        glUseProgram(program_6);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                       
        glm::mat4 transform(1.0);
        transform = glm::scale(transform, glm::vec3(0.4));
                   
        transform       = glm::translate(transform,  glm::vec3(1.2,1.2,0));
        auto transform3 = glm::translate(transform,  glm::vec3(0,-2.4,-0));
        auto transform2 = glm::translate(transform,  glm::vec3(-2.2,0.0,0));
        auto transform4 = glm::translate(transform,  glm::vec3(-2.1,-2.3,0));
                   
        transform  = glm::rotate(transform,  glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
        transform2 = glm::rotate(transform2, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
        transform3 = glm::rotate(transform3, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
        transform4 = glm::rotate(transform4, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
                       
        glBindVertexArray(shape_VAO.id); //ParametricCirle
        glUniformMatrix4fv(u_transform_location_6, 1, GL_FALSE, glm::value_ptr(transform));
        glDrawElements(GL_TRIANGLES, shape_VAO.element_array_count, GL_UNSIGNED_INT, NULL); // Draw the triangle
                               
        glBindVertexArray(shape1_VAO.id); //ParametricHalfCircle
        glUniformMatrix4fv(u_transform_location_6, 1, GL_FALSE, glm::value_ptr(transform2));
        glDrawElements(GL_TRIANGLES, shape1_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
                                      
        glBindVertexArray(shape3_VAO.id); //ParametricSpikes
        glUniformMatrix4fv(u_transform_location_6, 1, GL_FALSE, glm::value_ptr(transform3));
        glDrawElements(GL_TRIANGLES, shape3_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
                       
        glBindVertexArray(shape2_VAO.id); //ParametricSpikyCircle
        glUniformMatrix4fv(u_transform_location_6, 1, GL_FALSE, glm::value_ptr(transform4));
        glDrawElements(GL_TRIANGLES, shape2_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
    }
        
            
    if(Globals.scene == 3)
    {
        
      glUseProgram(program_2);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                  
      glm::mat4 transform(1.0);
      transform = glm::scale(transform, glm::vec3(0.4));
              
      transform       = glm::translate(transform,  glm::vec3(1.2,1.2,0));
      auto transform3 = glm::translate(transform,  glm::vec3(0,-2.4,-0));
      auto transform2 = glm::translate(transform,  glm::vec3(-2.2,0.0,0));
      auto transform4 = glm::translate(transform,  glm::vec3(-2.1,-2.3,0));
              
      transform  = glm::rotate(transform,  glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
      transform2 = glm::rotate(transform2, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
      transform3 = glm::rotate(transform3, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
      transform4 = glm::rotate(transform4, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
                  
      glBindVertexArray(shape_VAO.id); //ParametricCirle
      glUniformMatrix4fv(u_transform_location_2, 1, GL_FALSE, glm::value_ptr(transform));
      glDrawElements(GL_TRIANGLES, shape_VAO.element_array_count, GL_UNSIGNED_INT, NULL); // Draw the triangle
                          
      glBindVertexArray(shape1_VAO.id); //ParametricHalfCircle
      glUniformMatrix4fv(u_transform_location_2, 1, GL_FALSE, glm::value_ptr(transform2));
      glDrawElements(GL_TRIANGLES, shape1_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
                                 
      glBindVertexArray(shape3_VAO.id); //ParametricSpikes
      glUniformMatrix4fv(u_transform_location_2, 1, GL_FALSE, glm::value_ptr(transform3));
      glDrawElements(GL_TRIANGLES, shape3_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
                  
      glBindVertexArray(shape2_VAO.id); //ParametricSpikyCircle
      glUniformMatrix4fv(u_transform_location_2, 1, GL_FALSE, glm::value_ptr(transform4));
      glDrawElements(GL_TRIANGLES, shape2_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
        
    }
            
    if(Globals.scene == 4)
    {
        
      glUseProgram(program_3);
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            
      glUniform2fv(u_mouse_position_location_3, 1, glm::value_ptr(glm::vec2(mouse_position)));
            
      glm::mat4 transform(1.0);
      transform = glm::scale(transform, glm::vec3(0.4));
        
      transform       = glm::translate(transform,  glm::vec3(1.2,1.2,0));
      auto transform3 = glm::translate(transform,  glm::vec3(0,-2.4,-0));
      auto transform2 = glm::translate(transform,  glm::vec3(-2.2,0.0,0));
      auto transform4 = glm::translate(transform,  glm::vec3(-2.1,-2.3,0));
        
      transform  = glm::rotate(transform,  glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
      transform2 = glm::rotate(transform2, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
      transform3 = glm::rotate(transform3, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
      transform4 = glm::rotate(transform4, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
            
      glBindVertexArray(shape_VAO.id); //ParametricCirle
      glUniformMatrix4fv(u_transform_location_3, 1, GL_FALSE, glm::value_ptr(transform));
      glUniform3fv(color_location, 1, glm::value_ptr(glm::vec3(1,0,0)));
      glDrawElements(GL_TRIANGLES, shape_VAO.element_array_count, GL_UNSIGNED_INT, NULL); // Draw the triangle
                    
      glBindVertexArray(shape1_VAO.id); //ParametricHalfCircle
      glUniformMatrix4fv(u_transform_location_3, 1, GL_FALSE, glm::value_ptr(transform2));
      glUniform3fv(color_location, 1, glm::value_ptr(glm::vec3(0.5,0.5,0.5)));
      glDrawElements(GL_TRIANGLES, shape1_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
                           
      glBindVertexArray(shape3_VAO.id); //ParametricSpikes
      glUniformMatrix4fv(u_transform_location_3, 1, GL_FALSE, glm::value_ptr(transform3));
      glUniform3fv(color_location, 1, glm::value_ptr(glm::vec3(0,0,1)));
      glDrawElements(GL_TRIANGLES, shape3_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
            
      glBindVertexArray(shape2_VAO.id); //ParametricSpikyCircle
      glUniformMatrix4fv(u_transform_location_3, 1, GL_FALSE, glm::value_ptr(transform4));
      glUniform3fv(color_location, 1, glm::value_ptr(glm::vec3(0,1,0)));
      glDrawElements(GL_TRIANGLES, shape2_VAO.element_array_count, GL_UNSIGNED_INT, NULL);

    }
        
    if(Globals.scene == 5)
    {
        glUseProgram(program_5);
        
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBindVertexArray(shape1_VAO.id);
            
        auto scale = glm::scale(glm::vec3(0.3));
        auto translate = glm::translate(glm::vec3(mouse_position.x, mouse_position.y,0));
        auto transform = translate * scale;
        
        glm::mat4 chasing_transform(1.0);
               
        glm::vec3 mouse_pos = glm::vec3(mouse_position.x, mouse_position.y, 0);
        chasing_pos = glm::mix(mouse_pos, chasing_pos, 0.99);
        auto chasing_translate = glm::translate(chasing_pos);
        chasing_transform = chasing_translate * scale;
        
        glUniformMatrix4fv(u_transform_location_5, 1, GL_FALSE, glm::value_ptr(chasing_transform));
        glUniform3fv(color_location_5, 1, glm::value_ptr(glm::vec3(0.5,0.5,0.5)));
        glDrawElements(GL_TRIANGLES, shape1_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
        
        glUniformMatrix4fv(u_transform_location_5, 1, GL_FALSE, glm::value_ptr(transform));
        if(glm::distance(chasing_pos, mouse_pos) > 0.6)
        {
            glUniform3fv(color_location_5, 1, glm::value_ptr(glm::vec3(0,1,0)));
        }
        
        else
        {
            glUniform3fv(color_location_5, 1, glm::value_ptr(glm::vec3(1,0,0)));
        }
        glDrawElements(GL_TRIANGLES, shape1_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
       
    }
        
    if(Globals.scene == 6)
    {
         glUseProgram(program_1);
        
         glUniform2fv(u_mouse_position_location_1, 1, glm::value_ptr(glm::vec2(mouse_position)));
                      
         glm::mat4 transform(1.0);
         transform = glm::scale(transform, glm::vec3(0.6));
         transform = glm::rotate(transform, glm::radians(float(glfwGetTime() * 10)), glm::vec3(1, 1, 0));
                                     
         glUniformMatrix4fv(u_transform_location_1, 1, GL_FALSE, glm::value_ptr(transform));
         glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            
         glBindVertexArray(sixth_VAO.id); //ParametricCirle
         glDrawElements(GL_TRIANGLES, sixth_VAO.element_array_count, GL_UNSIGNED_INT, NULL);
        
    }
        
        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

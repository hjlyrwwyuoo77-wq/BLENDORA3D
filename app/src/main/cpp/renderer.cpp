#include "renderer.h"
#include <GLES3/gl3.h>
#include <cmath>

static GLuint program = 0;
static GLuint vao = 0;
static GLuint vbo = 0;
static GLint mvpLocation = -1;

static const char* vertexShaderSource = R"(
#version 300 es
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vColor;

void main() {
    gl_Position = uMVP * vec4(aPosition, 1.0);
    vColor = aColor;
}
)";

static const char* fragmentShaderSource = R"(
#version 300 es
precision mediump float;
in vec3 vColor;
out vec4 fragColor;

void main() {
    fragColor = vec4(vColor, 1.0);
}
)";

static GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static void multiply(float* out, const float* a, const float* b) {
    float result[16];
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            result[col * 4 + row] = 0.0f;
            for (int k = 0; k < 4; ++k) {
                result[col * 4 + row] +=
                    a[k * 4 + row] * b[col * 4 + k];
            }
        }
    }
    for (int i = 0; i < 16; ++i) out[i] = result[i];
}

static void perspective(float* m, float fov, float aspect,
                        float nearPlane, float farPlane) {
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    float f = 1.0f / std::tan(fov * 0.5f);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    m[11] = -1.0f;
    m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
}

static void rotationX(float* m, float angle) {
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    float c = std::cos(angle), s = std::sin(angle);
    m[0] = 1.0f;
    m[5] = c;
    m[6] = s;
    m[9] = -s;
    m[10] = c;
    m[15] = 1.0f;
}

static void rotationY(float* m, float angle) {
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    float c = std::cos(angle), s = std::sin(angle);
    m[0] = c;
    m[2] = -s;
    m[5] = 1.0f;
    m[8] = s;
    m[10] = c;
    m[15] = 1.0f;
}

bool Renderer::init() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(program);
        program = 0;
        return false;
    }

    mvpLocation = glGetUniformLocation(program, "uMVP");

    const float vertices[] = {
        // position            
        -1,-1,-1, 1,0,0,  1,-1,-1, 0,1,0,  1,1,-1, 0,0,1,
        -1,-1,-1, 1,0,0,  1,1,-1, 0,0,1, -1,1,-1, 1,1,0,
        -1,-1, 1, 1,0,1,  1,-1, 1, 0,1,1,  1,1, 1, 1,1,1,
        -1,-1, 1, 1,0,1,  1,1, 1, 1,1,1, -1,1, 1, 1,0.5,0,
        -1,-1,-1, 1,0,0, -1,1,-1, 1,1,0, -1,1,1, 1,0.5,0,
        -1,-1,-1, 1,0,0, -1,1,1, 1,0.5,0, -1,-1,1, 1,0,1,
         1,-1,-1, 0,1,0,  1,1,-1, 0,0,1,  1,1,1, 1,1,1,
         1,-1,-1, 0,1,0,  1,1,1, 1,1,1,  1,-1,1, 0,1,1,
        -1,-1,-1, 1,0,0, -1,-1,1, 1,0,1,  1,-1,1, 0,1,1,
        -1,-1,-1, 1,0,0,  1,-1,1, 0,1,1,  1,-1,-1, 0,1,0,
        -1,1,-1, 1,1,0, -1,1,1, 1,0.5,0,  1,1,1, 1,1,1,
        -1,1,-1, 1,1,0,  1,1,1, 1,1,1,   1,1,-1, 0,0,1
    };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    return true;
}

static float aspectRatio = 1.0f;

void Renderer::resize(int width, int height) {
    if (width > 0 && height > 0) {
        aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        glViewport(0, 0, width, height);
    }
}

void Renderer::draw(float rotationXAngle, float rotationYAngle) {
    if (!program) return;

    float projection[16], rx[16], ry[16], rotation[16], model[16], mvp[16];
    const float translation[16] = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-5,1
    };

    glClearColor(0.025f, 0.04f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    perspective(projection, 1.0472f, aspectRatio, 0.1f, 100.0f);
    rotationX(rx, rotationXAngle);
    rotationY(ry, rotationYAngle);
    multiply(rotation, ry, rx);
    multiply(model, translation, rotation);
    multiply(mvp, projection, model);

    glUseProgram(program);
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, mvp);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void Renderer::shutdown() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (program) glDeleteProgram(program);
    vbo = 0;
    vao = 0;
    program = 0;
    mvpLocation = -1;
}

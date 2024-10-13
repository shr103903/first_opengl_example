#include "common.h"
#include "shader.h"
#include "image.h"

uint32_t CompileShaders();
void Render();
void RenderTriangle();
void RenderRectangle();
void Startup();
void Shutdown();

uint32_t renderingProgram;
uint32_t m_vertexBuffer;
uint32_t m_vertexArrayObject;
uint32_t m_indexBuffer;
uint32_t m_texture;

void OnFramebufferSizeChange(GLFWwindow* window, int width, int height) {
    SPDLOG_INFO("framebuffer size changed: ({} x {})", width, height);
    glViewport(0, 0, width, height);
}

void OnKeyEvent(GLFWwindow* window,
    int key, int scancode, int action, int mods) {
    SPDLOG_INFO("key: {}, scancode: {}, action: {}, mods: {}{}{}",
        key, scancode,
        action == GLFW_PRESS ? "Pressed" :
        action == GLFW_RELEASE ? "Released" :
        action == GLFW_REPEAT ? "Repeat" : "Unknown",
        mods & GLFW_MOD_CONTROL ? "C" : "-",
        mods & GLFW_MOD_SHIFT ? "S" : "-",
        mods & GLFW_MOD_ALT ? "A" : "-");
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

uint32_t CompileShaders() {
    uint32_t program = glCreateProgram();
    if (program == 0){
        SPDLOG_ERROR("failed to create program");
        return 0;
    }

    // 쉐이더 생성
    auto vertexShader = Shader::CreateFromFile("./shader/texture.vs", GL_VERTEX_SHADER);
    auto fragmentShader = Shader::CreateFromFile("./shader/texture.fs", GL_FRAGMENT_SHADER);
    SPDLOG_INFO("vertex shader id: {}", vertexShader->Get());
    SPDLOG_INFO("fragment shader id: {}", fragmentShader->Get());
    
    // 쉐이더를 프로그램에 연결
    glAttachShader(program, vertexShader->Get());
    glAttachShader(program, fragmentShader->Get());
    glLinkProgram(program);

    // 프로그램 링크 확인
    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        SPDLOG_ERROR("failed to link program");
        SPDLOG_ERROR("reason: {}", infoLog);
        return 0;
    }
    SPDLOG_INFO("program id: {}", program);
    return program;
}

void Render() {
    glClearColor(0.1f, 0.2f, 0.3f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    //glDrawArrays(GL_POINTS, 0, 1);
    //RenderTriangle();
    RenderRectangle();
}

void RenderTriangle() {
    //glDrawArrays(GL_TRIANGLES, 0, 6);
}

void RenderRectangle() {
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void Startup() {
    auto renderingProgram = CompileShaders();
    if (renderingProgram == 0) {
        SPDLOG_ERROR("failed to compile shaders");
        return;
    }
    glUseProgram(renderingProgram);

    // 이미지 로드
    auto image = Image::Load("./image/wall.jpg");
    if (!image) {
        SPDLOG_ERROR("failed to load image");
        return; // Correct usage of return in a void function
    }

    // auto image = Image::Create(512, 512);
    // image->SetCheckImage(16, 16);
    
    SPDLOG_INFO("image: {}x{}, {} channels", image->GetWidth(), image->GetHeight(), image->GetChannelCount());

    // 텍스처 생성
    glGenTextures(1, &m_texture); // texture id 생성
    glBindTexture(GL_TEXTURE_2D, m_texture); // texture id 바인딩
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // linear filter
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
        image->GetWidth(), image->GetHeight(), 0,
        GL_RGB, GL_UNSIGNED_BYTE, image->GetData());
    
    // 정점 데이터
    float vertices[] = { 
        0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // top right, red
        0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // bottom right, green
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom left, blue
        -0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, // top left, yellow
    };

    uint32_t indices[] = { // note that we start from 0!
        0, 1, 3, // first triangle
        1, 2, 3, // second triange
    };

    glGenVertexArrays(1, &m_vertexArrayObject);
    glBindVertexArray(m_vertexArrayObject);

    glGenBuffers(1, &m_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,GL_STATIC_DRAW);

    glGenBuffers(1, &m_indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, 0);

    // 꼭지점 색상 지정을 위해 3개의 float를 건너뛰고 3개의 float를 읽는다
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(sizeof(float) * 3));

    // 텍스처 좌표를 위해 6개의 float를 건너뛰고 2개의 float를 읽는다
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void*)(sizeof(float) * 6));

    // for mipmap
    glGenerateMipmap(GL_TEXTURE_2D);
    // filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // linear
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void Shutdown() {
    if(renderingProgram != 0) {
        glDeleteProgram(renderingProgram);
    }

    if(m_vertexBuffer != 0) {
        glDeleteBuffers(1, &m_vertexBuffer);
    }
    if(m_vertexArrayObject != 0) {
        glDeleteVertexArrays(1, &m_vertexArrayObject);
    }
}

int main(int argc, const char** argv) {
    SPDLOG_INFO("Start program");

    // glfw 라이브러리 초기화, 실패하면 에러 출력후 종료
    SPDLOG_INFO("Initialize glfw");
    if (!glfwInit()) {
        const char* description = nullptr;
        glfwGetError(&description);
        SPDLOG_ERROR("failed to initialize glfw: {}", description);
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw 윈도우 생성, 실패하면 에러 출력후 종료
    SPDLOG_INFO("Create glfw window");
    auto window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_NAME, nullptr, nullptr);
    if (!window) {
        SPDLOG_ERROR("failed to create glfw window");
        glfwTerminate();
        return -1;
    }
    // OpenGL 컨텍스트 생성 및 현재 컨텍스트로 설정
    glfwMakeContextCurrent(window);

    // glad 활용한 OpenGL 함수 로딩
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        SPDLOG_ERROR("failed to initialize glad");
        return -1;
    }

    auto glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    SPDLOG_INFO("OpenGL context version: {}", glVersion);


    OnFramebufferSizeChange(window, WINDOW_WIDTH, WINDOW_HEIGHT);
    glfwSetFramebufferSizeCallback(window, OnFramebufferSizeChange);
    glfwSetKeyCallback(window, OnKeyEvent);

    // shader program 생성 및 사용
    Startup();

    // glfw 루프 실행, 윈도우 close 버튼을 누르면 정상 종료
    SPDLOG_INFO("Start main loop");
    while (!glfwWindowShouldClose(window)) {
        Render(); // 렌더링 - 점을 그림
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
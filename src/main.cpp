#include "common.h"
#include "shader_m.h"
//#include "image.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "camera.h"
// #define STB_IMAGE_IMPLEMENTATION
// #include <stb/stb_image.h>
// #include "model.h"
#include "animator.h"
#include "model_animation.h"

using namespace std;


void Render();
void Init();
void Shutdown();
unsigned int loadTexture(const std::string& filepath);
void MouseMove(double x, double y);
void MouseButton(int button, int action, double x, double y);

void ProcessInput(GLFWwindow* window);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

void renderCube();
void renderQuad();
void renderScene(const Shader &shader);
void renderOurModel();

// clear color
glm::vec4 m_clearColor { glm::vec4(0.3f, 0.35f, 0.4f, 0.0f) };

//// settings - by using learnopengl.com
const unsigned int SCR_WIDTH = WINDOW_WIDTH;
const unsigned int SCR_HEIGHT = WINDOW_HEIGHT;

////  camera 
Camera camera(glm::vec3(0.0f, 2.5f, 10.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

//// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool m_cameraControl { false };  
glm::vec2 m_prevMousePos { glm::vec2(0.0f) };

// build and compile our shader zprogram
// ------------------------------------

Shader lightingShader; // 물체를 그리는 쉐이더프로그램 
Shader lightCubeShader; // 광원을 그리는 쉐이더프로그램
Shader simpleDepthShader;
Shader debugDepthQuad;
Shader shader; // 그림자 셰이더
Shader ourShader; // 애니메이션 모델 셰이더
Shader normalShader; // 노말 매핑 셰이더
//std::unique_ptr<Shader> lightingShader;
Model ourModel;

Animation danceAnimation;
Animator animator;

unsigned int diffuseMap;
unsigned int specularMap;
unsigned int emissionMap;

// normal map
unsigned int diffuseMapBlock;
unsigned int normalMapBlock;


unsigned int VBO;
// unsigned int VBO, cubeVAO;
unsigned int lightCubeVAO;

// shadow
unsigned int depthMapFBO;
// unsigned int depthMap;
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

// meshes
unsigned int planeVAO;
unsigned int woodTexture;
unsigned int depthMap;

bool m_animation = true;
// glm::vec3 m_lightPos(0.2f, 0.2f, 1.0f);
float m_materialShininess = 64.0f;
glm::vec3 lightPos(0.7f, 0.0f, 1.5f);
glm::vec3 m_lightDirection(-0.2f, -1.0f, -0.3f);

glm::vec3 m_modelRotation(0.f, 0.f, 0.f);


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

void Render() {
    if (ImGui::Begin("ui window")) {
        if (ImGui::CollapsingHeader("model", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("model rotation", glm::value_ptr(m_modelRotation), 0.01f);
          
        }
        if (ImGui::CollapsingHeader("light", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("light pos", glm::value_ptr(lightPos), 0.01f);
            ImGui::SliderFloat("materialShininess", &m_materialShininess, 0.0f, 256.0f);
            ImGui::DragFloat3("light direction", glm::value_ptr(m_lightDirection), 0.01f);
        }

        ImGui::Checkbox("animation", &m_animation);

        if (ImGui::ColorEdit4("clear color", glm::value_ptr(m_clearColor))) {
            glClearColor(m_clearColor.r, m_clearColor.g, m_clearColor.b, m_clearColor.a);
        }
        ImGui::Separator();
        ImGui::DragFloat3("camera pos", glm::value_ptr(camera.Position), 0.01f);
        if(ImGui::DragFloat("camera yaw", &(camera.Yaw), 0.5f)) {
            camera.updateCameraVectors();
        }
        if(ImGui::DragFloat("camera pitch", &(camera.Pitch), 0.5f, -89.0f, 89.0f)) {
            camera.updateCameraVectors();
        }
        ImGui::Separator();
        if (ImGui::Button("reset camera")) {
            camera.Yaw = -90.0f;
            camera.Pitch = 0.0f;
            camera.Position = glm::vec3(0.0f, 2.5f, 10.0f);
            camera.updateCameraVectors();
        }
    }
    ImGui::End();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 1. render depth of scene to texture (from light's perspective)
    // --------------------------------------------------------------
    glm::mat4 lightProjection, lightView;
    glm::mat4 lightSpaceMatrix;
    float near_plane = 1.0f, far_plane = 30.f;
    lightProjection = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, near_plane, far_plane);
    lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
    lightSpaceMatrix = lightProjection * lightView;
    // render scene from light's point of view
    simpleDepthShader.use();
    simpleDepthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, woodTexture);
        renderScene(simpleDepthShader);
        // 아래 코드는 배낭 모델을 렌더링하기 위한 것
        glm::mat4 modelB = glm::mat4(1.0f);
        modelB = glm::scale(modelB, glm::vec3(0.7f, 0.7f, 0.7f));  // 크기 조정
        simpleDepthShader.setMat4("model", modelB);
        ourModel.Draw(simpleDepthShader);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 현재 코드는 디버깅용 - depth map을 사각형에 입혀서 렌더링 결과 확인함 
    // reset viewport
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 2. render scene as normal using the generated depth/shadow map  
    // --------------------------------------------------------------
    shader.use();
    glm::mat4 projectionS = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glm::mat4 viewS = camera.GetViewMatrix();
    shader.setMat4("projection", projectionS);
    shader.setMat4("view", viewS);
    // set light uniforms
    shader.setVec3("viewPos", camera.Position);
    shader.setVec3("lightPos", lightPos);
    shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, woodTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    renderScene(shader);

    // render Depth map to quad for visual debugging
    // ---------------------------------------------
    debugDepthQuad.use();
    debugDepthQuad.setFloat("near_plane", near_plane);
    debugDepthQuad.setFloat("far_plane", far_plane);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    // renderQuad();// depth map 디버깅용이므로 여기서는 안그림

    // normal mapping 
    normalShader.use();
    glm::mat4 projectionN = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glm::mat4 viewN = camera.GetViewMatrix();
    normalShader.setMat4("projection", projectionN);
    normalShader.setMat4("view", viewN);
    glm::mat4 modelN = glm::mat4(1.0f);
    // modelN = glm::rotate(modelN, glm::radians((float)glfwGetTime() * -10.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0))); // rotate the quad to show normal mapping from multiple directions
    normalShader.setMat4("model", modelN);
    normalShader.setVec3("viewPos", camera.Position);
    normalShader.setVec3("lightPos", lightPos);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, diffuseMapBlock);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalMapBlock);
    modelN = glm::translate(modelN, glm::vec3(4.0f, 2.0f, 0.2f));
    modelN = glm::scale(modelN, glm::vec3(2.5f));
    normalShader.setMat4("model", modelN);
    renderQuad();


    // positions of the point lights
    glm::vec3 pointLightPositions[] = {
        glm::vec3( 0.7f,  0.2f,  2.0f),
        glm::vec3( 2.3f, -3.3f, -4.0f),
        glm::vec3(-4.0f,  2.0f, -12.0f),
        glm::vec3( 0.0f,  0.0f, -3.0f)
    };

     // be sure to activate shader when setting uniforms/drawing objects
    lightingShader.use();

    // directional light
    lightingShader.setVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
    lightingShader.setVec3("dirLight.ambient", 0.2f, 0.2f, 0.2f);
    lightingShader.setVec3("dirLight.diffuse", 0.9f, 0.9f, 0.9f);
    lightingShader.setVec3("dirLight.specular", 0.7f, 0.7f, 0.7f);
    // point light 1
    lightingShader.setVec3("pointLights[0].position", pointLightPositions[0]);
    lightingShader.setVec3("pointLights[0].ambient", 0.5f, 0.5f, 0.5f);
    lightingShader.setVec3("pointLights[0].diffuse", 0.8f, 0.8f, 0.8f);
    lightingShader.setVec3("pointLights[0].specular", 1.0f, 1.0f, 1.0f);
    lightingShader.setFloat("pointLights[0].constant", 1.0f);
    lightingShader.setFloat("pointLights[0].linear", 0.022f);
    lightingShader.setFloat("pointLights[0].quadratic", 0.0019f);
    // point light 2
    lightingShader.setVec3("pointLights[1].position", pointLightPositions[1]);
    lightingShader.setVec3("pointLights[1].ambient", 0.5f, 0.5f, 0.5f);
    lightingShader.setVec3("pointLights[1].diffuse", 0.8f, 0.8f, 0.8f);
    lightingShader.setVec3("pointLights[1].specular", 1.0f, 1.0f, 1.0f);
    lightingShader.setFloat("pointLights[1].constant", 1.0f);
    lightingShader.setFloat("pointLights[1].linear", 0.022f);
    lightingShader.setFloat("pointLights[1].quadratic", 0.0019f);
    // point light 3
    lightingShader.setVec3("pointLights[2].position", pointLightPositions[2]);
    lightingShader.setVec3("pointLights[2].ambient", 0.5f, 0.5f, 0.5f);
    lightingShader.setVec3("pointLights[2].diffuse", 0.8f, 0.8f, 0.8f);
    lightingShader.setVec3("pointLights[2].specular", 1.0f, 1.0f, 1.0f);
    lightingShader.setFloat("pointLights[2].constant", 1.0f);
    lightingShader.setFloat("pointLights[2].linear", 0.022f);
    lightingShader.setFloat("pointLights[2].quadratic", 0.0019f);
    // point light 4
    lightingShader.setVec3("pointLights[3].position", pointLightPositions[3]);
    lightingShader.setVec3("pointLights[3].ambient", 0.5f, 0.5f, 0.5f);
    lightingShader.setVec3("pointLights[3].diffuse", 0.8f, 0.8f, 0.8f);
    lightingShader.setVec3("pointLights[3].specular", 1.0f, 1.0f, 1.0f);
    lightingShader.setFloat("pointLights[3].constant", 1.0f);
    lightingShader.setFloat("pointLights[3].linear", 0.022f);
    lightingShader.setFloat("pointLights[3].quadratic", 0.0019f);
    // spotLight
    lightingShader.setVec3("spotLight.position", camera.Position);
    lightingShader.setVec3("spotLight.direction", camera.Front);
    lightingShader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
    lightingShader.setVec3("spotLight.diffuse", 0.3f, 0.3f, 0.3f);
    lightingShader.setVec3("spotLight.specular", 0.3f, 0.3f, 0.3f);
    lightingShader.setFloat("spotLight.constant", 1.0f);
    lightingShader.setFloat("spotLight.linear", 0.022f);
    lightingShader.setFloat("spotLight.quadratic", 0.0019f);
    lightingShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
    lightingShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));

    lightingShader.setVec3("viewPos", camera.Position);

    // material properties
    lightingShader.setFloat("material.shininess", m_materialShininess);

    // view/projection transformations
    glm::mat4 projectionL = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glm::mat4 viewL = camera.GetViewMatrix();
    lightingShader.setMat4("projection", projectionL);
    lightingShader.setMat4("view", viewL);

    renderScene(lightingShader);
    lightingShader.setMat4("model", modelN);
    // renderOurModel();

    // don't forget to enable shader before setting uniforms
	ourShader.use();

	// view/projection transformations
	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
	glm::mat4 view = camera.GetViewMatrix();
	ourShader.setMat4("projection", projection);
	ourShader.setMat4("view", view);

    auto transforms = animator.GetFinalBoneMatrices();
	for (int i = 0; i < transforms.size(); ++i)
		ourShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);

	// render the loaded model
	glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, -0.7f, 0.0f)); 
    model = glm::scale(model, glm::vec3(0.7f, 0.7f, 0.7f));
	ourShader.setMat4("model", model);
	ourModel.Draw(ourShader);


    // // bind diffuse map
    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_2D, diffuseMap);
    // // bind specular map
    // glActiveTexture(GL_TEXTURE1);
    // glBindTexture(GL_TEXTURE_2D, specularMap);
    // // bind emission map
    // glActiveTexture(GL_TEXTURE2);
    // glBindTexture(GL_TEXTURE_2D, emissionMap);

    // // render containers
    // glBindVertexArray(cubeVAO);
    // // positions all containers
    // glm::vec3 cubePositions[] = {
    //     glm::vec3( 0.0f,  0.0f,  0.0f),
    //     glm::vec3( 2.0f,  5.0f, -15.0f),
    //     glm::vec3(-1.5f, -2.2f, -2.5f),
    //     glm::vec3(-3.8f, -2.0f, -12.3f),
    //     glm::vec3( 2.4f, -0.4f, -3.5f),
    //     glm::vec3(-1.7f,  3.0f, -7.5f),
    //     glm::vec3( 1.3f, -2.0f, -2.5f),
    //     glm::vec3( 1.5f,  2.0f, -2.5f),
    //     glm::vec3( 1.5f,  0.2f, -1.5f),
    //     glm::vec3(-1.3f,  1.0f, -1.5f)
    // };
    
    // for (unsigned int i = 0; i < 10; i++)
    // {
    //     // calculate the model matrix for each object and pass it to shader before drawing
    //     glm::mat4 model = glm::mat4(1.0f);
    //     model = glm::translate(model, cubePositions[i]);
    //     float angle = 20.0f * i;
    //     model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
    //     lightingShader.setMat4("model", model);

    //     glDrawArrays(GL_TRIANGLES, 0, 36);
    // }


    // // also draw the lamp object
    // lightCubeShader.use();
    // lightCubeShader.setMat4("projection", projection);
    // lightCubeShader.setMat4("view", view);
    
    // glBindVertexArray(lightCubeVAO);
    // for (unsigned int i = 0; i < 4; i++)
    // {
    //     model = glm::mat4(1.0f);
    //     model = glm::translate(model, pointLightPositions[i]);
    //     model = glm::scale(model, glm::vec3(0.2f)); // Make it a smaller cube
    //     lightCubeShader.setMat4("model", model);
    //     glDrawArrays(GL_TRIANGLES, 0, 36);
    // }
}

void Init() {
    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);
    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);  
    glClearColor(m_clearColor.r, m_clearColor.g, m_clearColor.b, m_clearColor.a);

    lightingShader = Shader("./shader/lighting.vs", "./shader/lighting.fs");
    lightCubeShader= Shader("./shader/lighting_cube.vs", "./shader/lighting_cube.fs");

    // build and compile shaders
    // -------------------------
    shader = Shader("./shader/shadow_mapping.vs", "./shader/shadow_mapping.fs");
    simpleDepthShader = Shader("./shader/simpleDepthShader.vs", "./shader/simpleDepthShader.fs");
    debugDepthQuad = Shader("./shader/debug_quad.vs", "./shader/debug_quad_depth.fs");

    // normal mapping
    normalShader = Shader("./shader/normal_mapping.vs", "./shader/normal_mapping.fs");

    diffuseMapBlock =  loadTexture("./image/brickwall.jpg");
    normalMapBlock  = loadTexture("./image/brickwall_normal.jpg");

    // shader configuration
    normalShader.use();
    normalShader.setInt("diffuseMap", 0);
    normalShader.setInt("normalMap", 1);

    // load textures
    // -------------
    woodTexture = loadTexture("./image/wood.png");

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float planeVertices[] = {
        // positions            // normals         // texcoords
         25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
        -25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
        -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,

         25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
        -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,
         25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,  25.0f, 25.0f
    };
    // plane VAO
    unsigned int planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);

    // configure depth map FBO
    // -----------------------
 
    glGenFramebuffers(1, &depthMapFBO);
    // create depth texture
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
   
    // attach depth texture as FBO's depth buffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    // shader configuration
    // --------------------
    //shadow rendering 위해 추가 
    shader.use();
    shader.setInt("diffuseTexture", 0);
    shader.setInt("shadowMap", 1);
    
    debugDepthQuad.use();
    debugDepthQuad.setInt("depthMap", 0);

    ourShader = Shader("./shader/anim_model.vs", "./shader/anim_model.fs");

    // load models
    // -----------
    // ourModel = Model("./model/backpack/backpack.obj");
    ourModel = Model("./model/Timmy/model.dae");
	danceAnimation = Animation("./model/Timmy/model.dae", &ourModel);
	animator = Animator(&danceAnimation);

    // // second, configure the light's VAO (VBO stays the same; the vertices are the same for the light object which is also a 3D cube)
    // //unsigned int lightCubeVAO;
    // glGenVertexArrays(1, &lightCubeVAO);
    // glBindVertexArray(lightCubeVAO);

    // glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // // note that we update the lamp's position attribute's stride to reflect the updated buffer data
    // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    // glEnableVertexAttribArray(0);

    // // load textures (we now use a utility function to keep the code more organized)
    // // -----------------------------------------------------------------------------
    // diffuseMap  = loadTexture("./image/container2.png");
    // specularMap = loadTexture("./image/container2_specular.png");
    // emissionMap = loadTexture("./image/container2.png");

    // // shader configuration
    // // --------------------
    // lightingShader.use();
    // lightingShader.setInt("material.diffuse", 0);
    // lightingShader.setInt("material.specular", 1);
    // lightingShader.setInt("material.emission", 2);
}

void Shutdown() {
    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    // glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &lightCubeVAO);
    glDeleteBuffers(1, &VBO);
}

void renderOurModel() {

    // world transformation
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, -0.45f, 1.0));
    model = glm::scale(model, glm::vec3(0.01f, 0.01f, 0.01));
    model = glm::rotate(model, glm::radians(m_modelRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_modelRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(m_modelRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    lightingShader.setMat4("model", model);
    
    ourModel.Draw(lightingShader);
}

// renders the 3D scene
// --------------------
void renderScene(const Shader &shader)
{
    // floor
    glm::mat4 model = glm::mat4(1.0f);
    shader.setMat4("model", model);
    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // // cubes
    // model = glm::mat4(1.0f);
    // model = glm::translate(model, glm::vec3(-1.0f, 1.5f, 0.0));
    // model = glm::scale(model, glm::vec3(0.5f));
    // shader.setMat4("model", model);
    // renderCube();
    // model = glm::mat4(1.0f);
    // model = glm::translate(model, glm::vec3(2.0f, 0.5f, 1.0));
    // model = glm::scale(model, glm::vec3(0.5f));
    // shader.setMat4("model", model);
    // renderCube();
    // model = glm::mat4(1.0f);
    // model = glm::translate(model, glm::vec3(-1.0f, 0.5f, 2.0));
    // model = glm::rotate(model, glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
    // model = glm::scale(model, glm::vec3(0.25));
    // shader.setMat4("model", model);
    // renderCube();
}

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        // positions
        glm::vec3 pos1(-1.0f,  1.0f, 0.0f);
        glm::vec3 pos2(-1.0f, -1.0f, 0.0f);
        glm::vec3 pos3( 1.0f, -1.0f, 0.0f);
        glm::vec3 pos4( 1.0f,  1.0f, 0.0f);
        // texture coordinates
        glm::vec2 uv1(0.0f, 1.0f);
        glm::vec2 uv2(0.0f, 0.0f);
        glm::vec2 uv3(1.0f, 0.0f);  
        glm::vec2 uv4(1.0f, 1.0f);
        // normal vector
        glm::vec3 nm(0.0f, 0.0f, 1.0f);

        // calculate tangent/bitangent vectors of both triangles
        glm::vec3 tangent1, bitangent1;
        glm::vec3 tangent2, bitangent2;
        // triangle 1
        // ----------
        glm::vec3 edge1 = pos2 - pos1;
        glm::vec3 edge2 = pos3 - pos1;
        glm::vec2 deltaUV1 = uv2 - uv1;
        glm::vec2 deltaUV2 = uv3 - uv1;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        tangent1.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent1.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent1.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

        bitangent1.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
        bitangent1.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
        bitangent1.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

        // triangle 2
        // ----------
        edge1 = pos3 - pos1;
        edge2 = pos4 - pos1;
        deltaUV1 = uv3 - uv1;
        deltaUV2 = uv4 - uv1;

        f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        tangent2.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent2.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent2.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);


        bitangent2.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
        bitangent2.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
        bitangent2.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);


        float quadVertices[] = {
            // positions            // normal         // texcoords  // tangent                          // bitangent
            pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
            pos2.x, pos2.y, pos2.z, nm.x, nm.y, nm.z, uv2.x, uv2.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
            pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,

            pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
            pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
            pos4.x, pos4.y, pos4.z, nm.x, nm.y, nm.z, uv4.x, uv4.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z
        };
        // configure plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(11 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}


// utility function for loading a 2D texture from file
// ---------------------------------------------------
//unsigned int loadTexture(char const * path)
unsigned int loadTexture(const std::string& filepath)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    char const * path = filepath.c_str();
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

void ProcessInput(GLFWwindow* window) {
    if (!m_cameraControl)
        return;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
}

void MouseMove(double x, double y) {
    if (!m_cameraControl)
        return;
    auto pos = glm::vec2((float)x, (float)y);
    auto deltaPos = pos - m_prevMousePos;

    camera.ProcessMouseMovement(deltaPos.x, deltaPos.y);

    m_prevMousePos = pos;
}

void MouseButton(int button, int action, double x, double y) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
        // 마우스 조작 시작 시점에 현재 마우스 커서 위치 저장
        m_prevMousePos = glm::vec2((float)x, (float)y);
        m_cameraControl = true;
        }
        else if (action == GLFW_RELEASE) {
            m_cameraControl = false;
        }
    }
}

void OnCursorPos(GLFWwindow* window, double x, double y) {
    glfwGetWindowUserPointer(window);
    MouseMove(x, y);
}

void OnMouseButton(GLFWwindow* window, int button, int action, int modifier) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, modifier);

    glfwGetWindowUserPointer(window);
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    MouseButton(button, action, x, y);
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

    auto imguiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(imguiContext);
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init();
    ImGui_ImplOpenGL3_CreateFontsTexture();
    ImGui_ImplOpenGL3_CreateDeviceObjects();


    OnFramebufferSizeChange(window, WINDOW_WIDTH, WINDOW_HEIGHT);
    glfwSetFramebufferSizeCallback(window, OnFramebufferSizeChange);
    glfwSetKeyCallback(window, OnKeyEvent);
    // glfwSetCharCallback(window, OnCharEvent);
    glfwSetCursorPosCallback(window, OnCursorPos);
    glfwSetMouseButtonCallback(window, OnMouseButton);
    // glfwSetScrollCallback(window, OnScroll);
	// glfwSetCursorPosCallback(window, mouse_callback);
	// glfwSetScrollCallback(window, scroll_callback);

    // shader program 생성 및 사용
    Init();

    // glfw 루프 실행, 윈도우 close 버튼을 누르면 정상 종료
    SPDLOG_INFO("Start main loop");
    while (!glfwWindowShouldClose(window)) {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        ProcessInput(window);
        animator.UpdateAnimation(deltaTime);
        glfwPollEvents();

        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        Render(); // 렌더링 - 점을 그림

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    Shutdown();
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(imguiContext);

    glfwTerminate();
    return 0;
}
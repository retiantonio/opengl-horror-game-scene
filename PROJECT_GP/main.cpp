#if defined (__APPLE__)
    #define GLFW_INCLUDE_GLCOREARB
    #define GL_SILENCE_DEPRECATION
#else
    #define GLEW_STATIC
    #include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>

#include <glm/glm.hpp> //core glm functionality
#include <glm/gtc/matrix_transform.hpp> //glm extension for generating common transformation matrices
#include <glm/gtc/matrix_inverse.hpp> //glm extension for computing inverse matrices
#include <glm/gtc/type_ptr.hpp> //glm extension for accessing the internal data structure of glm types

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Window.h"
#include "Shader.hpp"
#include "Camera.hpp"
#include "Model3D.hpp"

//audio
#include <SFML/Audio.hpp>

#include <iostream>

// window
gps::Window myWindow;

// matrices
glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
glm::mat3 normalMatrix;

// light parameters
glm::vec3 lightDir;
glm::vec3 lightColor;

// shader uniform locations
GLint modelLoc;
GLint viewLoc;
GLint projectionLoc;
GLint normalMatrixLoc;
GLint lightDirLoc;
GLint lightColorLoc;

// spotlight (flashlight) parameters
glm::vec3 spotLightPos;
glm::vec3 spotLightDir;
glm::vec3 spotLightColor;
GLint spotLightPosLoc;
GLint spotLightDirLoc;
GLint spotLightColorLoc;
GLint spotLightCutOffLoc;
GLint spotLightOuterCutOffLoc;
GLint spotLightConstantLoc;
GLint spotLightLinearLoc;
GLint spotLightQuadraticLoc;
bool flashlightOn = false;

//shadows
const unsigned int SHADOW_WIDTH = 4096, SHADOW_HEIGHT = 4096;
const unsigned int SHADOW_LAYERS = 5;
GLuint shadowMapFBO;
GLuint depthMapTextureArray;

glm::mat4 lightMatrices[SHADOW_LAYERS];

gps::Shader depthMapShader;

glm::mat4 lightSpaceMatrix;
GLint lightSpaceMatrixLoc;
GLint shadowMapLoc;

//candles
struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
    float constant;
    float linear;
    float quadratic;
    bool enabled;

    PointLight(glm::vec3 pos, glm::vec3 col)
        : position(pos), color(col), constant(1.0f),
        linear(0.7f), quadratic(1.8f), enabled(true) {
    }
};

#define MAX_POINT_LIGHTS 8
std::vector<PointLight> pointLights;

GLint pointLightPosLoc[MAX_POINT_LIGHTS];
GLint pointLightColorLoc[MAX_POINT_LIGHTS];
GLint pointLightConstantLoc[MAX_POINT_LIGHTS];
GLint pointLightLinearLoc[MAX_POINT_LIGHTS];
GLint pointLightQuadraticLoc[MAX_POINT_LIGHTS];
GLint numPointLightsLoc;

// camera
gps::Camera myCamera(
    glm::vec3(0.3206f, 1.147f, 0.3556f),
    glm::vec3(0.0f, 0.0f, -10.0f),
    glm::vec3(0.0f, 1.0f, 0.0f));

struct CameraState {
    glm::vec3 position;
    glm::vec3 target;
    std::string name;

    CameraState(glm::vec3 pos, glm::vec3 targ, std::string n)
        : position(pos), target(targ), name(n) {
    }
};

std::vector<CameraState> cameraStates;
int currentCameraState = 0;
bool isTransitioning = false;
float transitionProgress = 0.0f;
float transitionSpeed = 2.0f;

glm::vec3 transitionStartPos;
glm::vec3 transitionStartTarget;
glm::vec3 transitionEndPos;
glm::vec3 transitionEndTarget;

// audio
std::unique_ptr<sf::SoundBuffer> foxyArrivalBuffer;
std::unique_ptr<sf::Sound> foxyArrivalSound;

std::unique_ptr<sf::SoundBuffer> bonnieArrivalBuffer;
std::unique_ptr<sf::Sound> bonnieArrivalSound;

std::unique_ptr<sf::SoundBuffer> screamBuffer;
std::unique_ptr<sf::Sound> screamSound;

std::unique_ptr<sf::SoundBuffer> flashlightClickBuffer;
std::unique_ptr<sf::Sound> flashlightClickSound;

std::unique_ptr<sf::SoundBuffer> closeDoorBuffer;
std::unique_ptr<sf::Sound> closeDoorSound;

std::unique_ptr<sf::SoundBuffer> runningBuffer;
std::unique_ptr<sf::Sound> runningSound;


std::unique_ptr<sf::Music> backgroundMusic;

bool foxyArrivedSoundPlayed = false;
bool bonnieArrivedSoundPlayed = false;

struct AnimatronicState {
    glm::vec3 hiddenPosition;
    glm::vec3 visiblePosition;
    bool isVisible;
    bool playedSound;
 
    float moveTimer;
    float moveInterval;

    float lightIrritation;     
    float irritationThreshold;

    std::string name;

    AnimatronicState(std::string n, glm::vec3 hidden, glm::vec3 visible)
        : name(n), hiddenPosition(hidden), visiblePosition(visible),
        isVisible(false), playedSound(false), moveTimer(0.0f),
        lightIrritation(0.0f), irritationThreshold(5.0f),
        moveInterval(5.0f + (rand() % 10)) {
    }

    glm::vec3 getCurrentPosition() const {
        return isVisible ? visiblePosition : hiddenPosition;
    }

    void processLight(float deltaTime, bool lightOn, float alignment) {
        if (!isVisible) return;

        if (flashlightOn && alignment > 0.9f) {
            if (!playedSound) {
                playedSound = true;
                screamSound->setPosition({
                      visiblePosition.x,
                      visiblePosition.y,
                      visiblePosition.z 
                });
                screamSound->play();
            }
            std::cout << "IT'S WORKING" << std::endl;
            lightIrritation += deltaTime;
        } else {
            lightIrritation = glm::max(0.0f, lightIrritation - (deltaTime * 0.5f));
            std::cout << "FIND HIM AND SCARE HIM AWAY" << std::endl;
        }

        if (lightIrritation >= irritationThreshold) {
            std::cout << "HE'S GONE.. FOR NOW" << std::endl;

            lightIrritation = 0.0f;
            moveTimer = 0.0f;

            if (name == "Foxy") {
                closeDoorSound->play();
            }

            playedSound = false;
            isVisible = false;
        }
    }
};

AnimatronicState foxyState("Foxy",
    glm::vec3(-0.76f, 10.0f, 1.07f),     // /off screen
    glm::vec3(-0.76f, -0.4f, 1.07f));   // at door

AnimatronicState bonnieState("Bonnie",
    glm::vec3(2.9f, 10.0f, -2.36f),      //off screen
    glm::vec3(2.9f, 0.0f, -2.36f));     // at window

GLfloat cameraSpeed = 0.01f;

GLboolean pressedKeys[1024];

GLfloat lastX = 512.0f;
GLfloat lastY = 384.0f;
GLfloat yaw = -90.0f;
GLfloat pitch = 0.0f;
bool firstMouse = true;
float mouseSensitivity = 0.1f;

// models
gps::Model3D bathroom;
gps::Model3D nightmareFoxy;
gps::Model3D nightmareBonnie;
gps::Model3D flashlight;
gps::Model3D candles;

gps::Model3D bathroomDoor;
float doorAngle = 0.0f;
float doorOpenSpeed = 150.0f;

GLfloat angle;

// shaders
gps::Shader myBasicShader;

GLenum glCheckError_(const char *file, int line)
{
	GLenum errorCode;
	while ((errorCode = glGetError()) != GL_NO_ERROR) {
		std::string error;
		switch (errorCode) {
            case GL_INVALID_ENUM:
                error = "INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                error = "INVALID_VALUE";
                break;
            case GL_INVALID_OPERATION:
                error = "INVALID_OPERATION";
                break;
            case GL_OUT_OF_MEMORY:
                error = "OUT_OF_MEMORY";
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                error = "INVALID_FRAMEBUFFER_OPERATION";
                break;
        }
		std::cout << error << " | " << file << " (" << line << ")" << std::endl;
	}
	return errorCode;
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)

void windowResizeCallback(GLFWwindow* window, int width, int height) {
	fprintf(stdout, "Window resized! New width: %d , and height: %d\n", width, height);
	//TODO
}

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_J:
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_POLYGON_SMOOTH);
            break;
        case GLFW_KEY_K:
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            break;
        case GLFW_KEY_L:
            glPointSize(2.5f);
            glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            break;
        }
    }

    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        flashlightOn = !flashlightOn;
        flashlightClickSound->play(); 
    }

	if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            pressedKeys[key] = true;
        } else if (action == GLFW_RELEASE) {
            pressedKeys[key] = false;
        }
    }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {

    if (isTransitioning) {
        lastX = xpos;
        lastY = ypos;
        return;
    }

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    myCamera.rotate(pitch, yaw);

    // update view matrix
    view = myCamera.getViewMatrix();
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
}

void transitionToCamera(int stateIndex) {
    if (stateIndex < 0 || stateIndex >= cameraStates.size()) {
        std::cout << "Invalid camera state: " << stateIndex << std::endl;
        return;
    }

    if (stateIndex == currentCameraState && !isTransitioning) {
        return;  // already at this state
    }

    transitionStartPos = myCamera.getCameraPosition();
    transitionStartTarget = myCamera.getCameraTarget();

    transitionEndPos = cameraStates[stateIndex].position;
    transitionEndTarget = cameraStates[stateIndex].target;

    isTransitioning = true;

    runningSound->play();

    transitionProgress = 0.0f;
    currentCameraState = stateIndex;

    std::cout << "Transitioning to: " << cameraStates[stateIndex].name << std::endl;
}

void updateCameraTransition(float deltaTime) {
    if (!isTransitioning) return;

    transitionProgress += deltaTime * transitionSpeed;

    if (transitionProgress >= 1.0f) {
        transitionProgress = 1.0f;
        isTransitioning = false;
    }

    float t = transitionProgress;
    t = t * t * (3.0f - 2.0f * t);

    glm::vec3 newPos = glm::mix(transitionStartPos, transitionEndPos, t);
    glm::vec3 newTarget = glm::mix(transitionStartTarget, transitionEndTarget, t);

    myCamera.setCameraPosition(newPos);
    myCamera.setCameraTarget(newTarget);

    glm::vec3 direction = glm::normalize(newTarget - newPos);

    pitch = glm::degrees(asin(direction.y));
    yaw = glm::degrees(atan2(direction.z, direction.x));

    myCamera.rotate(pitch, yaw);

    view = myCamera.getViewMatrix();
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
}

void updateAnimatronics(float deltaTime) {

    if (!bonnieState.isVisible) {
        bonnieArrivedSoundPlayed = false;
    }

    if (!foxyState.isVisible) {
        foxyArrivedSoundPlayed = false;
    }

    // flashlight mecanic
    glm::vec3 camPos = myCamera.getCameraPosition();
    glm::vec3 camFront = myCamera.getCameraFrontDirection();

    glm::vec3 heightOffset = glm::vec3(0.0f, 1.2f, 0.0f);

    glm::vec3 dirToFoxy = glm::normalize(foxyState.visiblePosition + heightOffset - camPos);
    float alignFoxy = glm::dot(camFront, dirToFoxy);
    foxyState.processLight(deltaTime, flashlightOn, alignFoxy);

    glm::vec3 dirToBonnie = glm::normalize(bonnieState.visiblePosition + heightOffset - camPos);
    float alignBonnie = glm::dot(camFront, dirToBonnie);
    bonnieState.processLight(deltaTime, flashlightOn, alignBonnie);

    // update Foxy
    foxyState.moveTimer += deltaTime;

    if (foxyState.moveTimer >= foxyState.moveInterval) {
        if (rand() % 100 < 50) { // 50% chance

           foxyState.isVisible = true;
           if (!foxyArrivedSoundPlayed) {
               foxyArrivedSoundPlayed = true;
               foxyArrivalSound->setPosition({
                   foxyState.visiblePosition.x,
                   foxyState.visiblePosition.y,
                   foxyState.visiblePosition.z });
               foxyArrivalSound->play();
           }
           
           std::cout << "FOXY HAS APPEARED!" << std::endl;
        }

        foxyState.moveTimer = 0.0f;
        foxyState.moveInterval = 5.0f + (rand() % 10);
    }

    // update Bonnie
    bonnieState.moveTimer += deltaTime;

    if (bonnieState.moveTimer >= bonnieState.moveInterval) {
        if (rand() % 100 < 50) {  // 50% chance
            bonnieState.isVisible = true;

            if (!bonnieArrivedSoundPlayed) {
                bonnieArrivedSoundPlayed = true;
                bonnieArrivalSound->setPosition({
                    bonnieState.visiblePosition.x,
                    bonnieState.visiblePosition.y,
                    bonnieState.visiblePosition.z });
                bonnieArrivalSound->play();
            }
          
            std::cout << "BONNIE HAS APPEARED!" << std::endl;
        }

        bonnieState.moveTimer = 0.0f;
        bonnieState.moveInterval = 5.0f + (rand() % 10);
    }
}

void processMovement() {

    if (isTransitioning) {
        return;
    }

	if (pressedKeys[GLFW_KEY_W]) {
		myCamera.move(gps::MOVE_FORWARD, cameraSpeed);
		//update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // compute normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
	}

	if (pressedKeys[GLFW_KEY_S]) {
		myCamera.move(gps::MOVE_BACKWARD, cameraSpeed);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // compute normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
	}

	if (pressedKeys[GLFW_KEY_A]) {
		myCamera.move(gps::MOVE_LEFT, cameraSpeed);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // compute normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
	}

	if (pressedKeys[GLFW_KEY_D]) {
		myCamera.move(gps::MOVE_RIGHT, cameraSpeed);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // compute normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
	}

    if (pressedKeys[GLFW_KEY_Q]) {
        angle -= 1.0f;
        // update model matrix for teapot
        model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0, 1, 0));
        // update normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
    }

    if (pressedKeys[GLFW_KEY_E]) {
        angle += 1.0f;
        // update model matrix for teapot
        model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0, 1, 0));
        // update normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
    }

    if (pressedKeys[GLFW_KEY_SPACE]) {
        myCamera.move(gps::MOVE_UP, cameraSpeed);
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    }

    if (pressedKeys[GLFW_KEY_LEFT_SHIFT]) {
        myCamera.move(gps::MOVE_DOWN, cameraSpeed);
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    }

    if (pressedKeys[GLFW_KEY_1]) {
        transitionToCamera(0);
    }

    if (pressedKeys[GLFW_KEY_2]) {
        transitionToCamera(1);
    }

    if (pressedKeys[GLFW_KEY_3]) {
        transitionToCamera(2);
    }
}

void initOpenGLWindow() {
    myWindow.Create(1920, 1080, "OpenGL Project Core");
    glfwSetInputMode(myWindow.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void setWindowCallbacks() {
	glfwSetWindowSizeCallback(myWindow.getWindow(), windowResizeCallback);
    glfwSetKeyCallback(myWindow.getWindow(), keyboardCallback);
    glfwSetCursorPosCallback(myWindow.getWindow(), mouseCallback);
}

void initOpenGLState() {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    glEnable(GL_FRAMEBUFFER_SRGB);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
}

void initModels() {
    bathroom.LoadModel("models/bathroom/bathroom1.obj");
    nightmareFoxy.LoadModel("models/nightmare_foxy/nightmare_foxy.obj");
    nightmareBonnie.LoadModel("models/nightmare_bonnie/nightmare_bonnie.obj");
    flashlight.LoadModel("models/flashlight/flashlight.obj");
    candles.LoadModel("models/candles/candles.obj");
    bathroomDoor.LoadModel("models/bathroom_door/bathroom_door.obj");
}

void initAudio() {
    foxyArrivalBuffer = std::make_unique<sf::SoundBuffer>();
    if (!foxyArrivalBuffer->loadFromFile("audio/doorcreakfast.wav")) {
        std::cout << "Failed to load sound!" << std::endl;
    }
    foxyArrivalSound = std::make_unique<sf::Sound>(*foxyArrivalBuffer);
    foxyArrivalSound->setVolume(200.0f);

    closeDoorBuffer = std::make_unique<sf::SoundBuffer>();
    if (!closeDoorBuffer->loadFromFile("audio/doormove4.wav")) {
        std::cout << "Failed to load sound!" << std::endl;
    }
    closeDoorSound = std::make_unique<sf::Sound>(*closeDoorBuffer);
    closeDoorSound->setVolume(200.0f);

    bonnieArrivalBuffer = std::make_unique<sf::SoundBuffer>();
    if (!bonnieArrivalBuffer->loadFromFile("audio/scrape-5-105787.mp3")) {
        std::cout << "Failed to load sound!" << std::endl;
    }
    bonnieArrivalSound = std::make_unique<sf::Sound>(*bonnieArrivalBuffer);
    bonnieArrivalSound->setVolume(120.0f);

    screamBuffer = std::make_unique<sf::SoundBuffer>();
    if (!screamBuffer->loadFromFile("audio/animatronicjump.wav")) {
        std::cout << "Failed to load sound!" << std::endl;
    }
    screamSound = std::make_unique<sf::Sound>(*screamBuffer);

    flashlightClickBuffer = std::make_unique<sf::SoundBuffer>();
    if (!flashlightClickBuffer->loadFromFile("audio/flashlight.wav")) {
        std::cout << "Failed to load sound!" << std::endl;
    }
    flashlightClickSound = std::make_unique<sf::Sound>(*flashlightClickBuffer);

    runningBuffer = std::make_unique<sf::SoundBuffer>();
    if (!runningBuffer->loadFromFile("audio/runrtolb_jUiHc0U3.wav")) {
        std::cout << "Failed to load sound!" << std::endl;
    }
    runningSound = std::make_unique<sf::Sound>(*runningBuffer);
    runningSound->setVolume(150.0f);

    backgroundMusic = std::make_unique<sf::Music>();
    if (!backgroundMusic->openFromFile("audio/deepamb.wav")) {
        std::cout << "Failed to load ambient music!" << std::endl;
    }
    backgroundMusic->setLooping(true);
    backgroundMusic->setVolume(30.0f);
    backgroundMusic->play();
}

void initShaders() {
	myBasicShader.loadShader(
        "shaders/basic.vert",
        "shaders/basic.frag");

    depthMapShader.loadShader(
        "shaders/depthMap.vert",
        "shaders/depthMap.frag");
}

void initCandleLights() {
    pointLights.clear();

    pointLights.push_back(PointLight(
        glm::vec3(0.75f, 1.10f, 0.468f),
        glm::vec3(1.0f, 0.6f, 0.3f)
    ));

    pointLights.push_back(PointLight(
        glm::vec3(0.64f, 1.0f, 0.54f),
        glm::vec3(1.0f, 0.6f, 0.3f)
    ));

    pointLights.push_back(PointLight(
        glm::vec3(0.662f, 1.02f, 0.427f),
        glm::vec3(1.0f, 0.6f, 0.3f)
    ));

    pointLights.push_back(PointLight(glm::vec3(-0.391f, 1.271f, 0.71f), glm::vec3(0.0f, 0.0f, 0.0f)));
    pointLights.push_back(PointLight(glm::vec3(3.339f, 1.994f, -1.566f), glm::vec3(0.1f, 0.4f, 1.0f)));

    for (int i = 0; i < 5; i++) {
        pointLights[i].linear = 2.5f;
        pointLights[i].quadratic = 5.0f;
    } 
}

void initCameraStates() {
    cameraStates.clear();

    // state 0: spawn
    cameraStates.push_back(CameraState(
        glm::vec3(0.3206f, 1.147f, 0.3556f),
        glm::vec3(0.0f, 0.0f, -10.0f),
        "spawn"
    ));

    // state 1: looking door
    cameraStates.push_back(CameraState(
        glm::vec3(-1.495f, 1.147f, -1.585f),
        glm::vec3(0.0f, 0.0f, 10.0f),
        "door"
    ));

    // state 2: looking window
    cameraStates.push_back(CameraState(
        glm::vec3(0.247f, 1.147f, -1.875f),
        glm::vec3(10.0f, 0.0f, 0.0f),
        "window"
    ));
}

void initShadowFBO() {
  
    glGenFramebuffers(1, &shadowMapFBO);

    glGenTextures(1, &depthMapTextureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthMapTextureArray);

    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT,
        SHADOW_WIDTH, SHADOW_HEIGHT, SHADOW_LAYERS, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthMapTextureArray, 0);

    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

glm::mat4 computeLightSpaceMatrix() {
    float near_plane = 0.1f, far_plane = 50.0f;
    glm::mat4 lightProjection = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, near_plane, far_plane);

    glm::vec3 lightPos = lightDir * 10.0f;
    glm::mat4 lightView = glm::lookAt(lightPos,
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));

    return lightProjection * lightView;
}

void initUniforms() {
	myBasicShader.useShaderProgram();

    // create model matrix for teapot
    model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
	modelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "model");

	// get view matrix for current camera
	view = myCamera.getViewMatrix();
	viewLoc = glGetUniformLocation(myBasicShader.shaderProgram, "view");
	// send view matrix to shader
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    // compute normal matrix for teapot
    normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
	normalMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "normalMatrix");

	// create projection matrix
	projection = glm::perspective(glm::radians(45.0f),
                               (float)myWindow.getWindowDimensions().width / (float)myWindow.getWindowDimensions().height,
                               0.1f, 20.0f);
	projectionLoc = glGetUniformLocation(myBasicShader.shaderProgram, "projection");
	// send projection matrix to shader
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));	

	//set the light direction (direction towards the light)
	lightDir = glm::vec3(0.0f, 0.0f, 0.0f);
	lightDirLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightDir");
	// send light dir to shader
	glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));

	//set light color
	lightColor = glm::vec3(0.25f, 0.0f, 0.0f);
	lightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightColor");
	// send light color to shader
	glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));

    // Initialize spotlight (flashlight)
    spotLightColor = glm::vec3(1.0f, 0.95f, 0.85f); // Slightly warm white for flashlight effect
    spotLightPosLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightPos");
    spotLightDirLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightDir");
    spotLightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightColor");
    spotLightCutOffLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightCutOff");
    spotLightOuterCutOffLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightOuterCutOff");
    spotLightConstantLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightConstant");
    spotLightLinearLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightLinear");
    spotLightQuadraticLoc = glGetUniformLocation(myBasicShader.shaderProgram, "spotLightQuadratic");

    // Set spotlight parameters
    glUniform1f(spotLightCutOffLoc, glm::cos(glm::radians(10.0f)));      // Inner cone angle
    glUniform1f(spotLightOuterCutOffLoc, glm::cos(glm::radians(12.0f))); // Outer cone angle for smooth edges
    glUniform1f(spotLightConstantLoc, 1.0f);      // Constant attenuation
    glUniform1f(spotLightLinearLoc, 0.45f);       // Linear attenuation
    glUniform1f(spotLightQuadraticLoc, 0.75f);   // Quadratic attenuation

    lightSpaceMatrix = computeLightSpaceMatrix();
    lightSpaceMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightSpaceMatrix");
    glUniformMatrix4fv(lightSpaceMatrixLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

    //Set point light - candles
    numPointLightsLoc = glGetUniformLocation(myBasicShader.shaderProgram, "numPointLights");

    // Get uniform locations for each point light
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        std::string baseName = "pointLights[" + std::to_string(i) + "]";

        pointLightPosLoc[i] = glGetUniformLocation(myBasicShader.shaderProgram,
            (baseName + ".position").c_str());
        pointLightColorLoc[i] = glGetUniformLocation(myBasicShader.shaderProgram,
            (baseName + ".color").c_str());
        pointLightConstantLoc[i] = glGetUniformLocation(myBasicShader.shaderProgram,
            (baseName + ".constant").c_str());
        pointLightLinearLoc[i] = glGetUniformLocation(myBasicShader.shaderProgram,
            (baseName + ".linear").c_str());
        pointLightQuadraticLoc[i] = glGetUniformLocation(myBasicShader.shaderProgram,
            (baseName + ".quadratic").c_str());
    }

    // Set shadow map texture unit
    shadowMapLoc = glGetUniformLocation(myBasicShader.shaderProgram, "shadowMap");
    glUniform1i(shadowMapLoc, 2);

}

void renderBathroom(gps::Shader shader) {
     // select active shader program
    shader.useShaderProgram();

    glm::mat4 bathroomModel = glm::mat4(1.0f);

    //send model matrix data to shader
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(bathroomModel));

    //send normal matrix data to shader
    glm::mat3 bathroomNormal = glm::mat3(glm::inverseTranspose(view * bathroomModel));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(bathroomNormal));

    // draw bathroom
    bathroom.Draw(shader);
}

void renderFoxy(gps::Shader shader) {
    shader.useShaderProgram();

    glm::mat4 foxyModel = glm::mat4(1.0f);
   
    foxyModel = glm::translate(foxyModel, foxyState.getCurrentPosition());

    //bl x = gl x
    //bl y = - gl z
    //bl z = gl y

    foxyModel = glm::rotate(foxyModel, glm::radians(-5.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    foxyModel = glm::rotate(foxyModel, glm::radians(5.5f), glm::vec3(1.0f, 0.0f, 0.0f));
    foxyModel = glm::rotate(foxyModel, glm::radians(-98.4f), glm::vec3(0.0f, 1.0f, 0.0f));

    foxyModel = glm::scale(foxyModel, glm::vec3(1.25f, 1.25f, 1.25f));

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(foxyModel));

    glm::mat3 foxyNormal = glm::mat3(glm::inverseTranspose(view * foxyModel));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(foxyNormal));

    nightmareFoxy.Draw(shader);
}

void renderBonnie(gps::Shader shader) {
    shader.useShaderProgram();

    glm::mat4 bonnieModel = glm::mat4(1.0f);
    bonnieModel = glm::translate(bonnieModel,bonnieState.getCurrentPosition());

    ////bl x = gl x
    ////bl y = - gl z
    ////bl z = gl y

    bonnieModel = glm::rotate(bonnieModel, glm::radians(-80.4f), glm::vec3(0.0f, 1.0f, 0.0f)); 

    bonnieModel = glm::scale(bonnieModel, glm::vec3(1.25f, 1.25f, 1.25f));

    //send teapot model matrix data to shader
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(bonnieModel));

    glm::mat3 bonnieNormal = glm::mat3(glm::inverseTranspose(view * bonnieModel));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(bonnieNormal));

    // draw teapot
    nightmareBonnie.Draw(shader);
}

void renderFlashlight(gps::Shader shader) {
    shader.useShaderProgram();

    glm::mat4 flashlightModel = glm::mat4(1.0f);

    glm::vec3 camPos = myCamera.getCameraPosition();
    glm::vec3 camFront = myCamera.getCameraFrontDirection();
    glm::vec3 camRight = myCamera.getCameraRightDirection();
    glm::vec3 camUp = myCamera.getCameraUpDirection();

    glm::vec3 flashlightPos = camPos
        + camFront * 0.3f    // forward offset
        + camRight * 0.2f    // right offset  
        - camUp * 0.15f;     // down offset

    flashlightModel = glm::translate(flashlightModel, flashlightPos);

    glm::mat4 rotationMatrix = glm::mat4(1.0f);
    rotationMatrix[0] = glm::vec4(camRight, 0.0f);
    rotationMatrix[1] = glm::vec4(camUp, 0.0f);
    rotationMatrix[2] = glm::vec4(-camFront, 0.0f);

    flashlightModel = flashlightModel * rotationMatrix;

    flashlightModel = glm::rotate(flashlightModel, glm::radians(120.4f), glm::vec3(0.0f, 1.0f, 0.0f));
    flashlightModel = glm::scale(flashlightModel, glm::vec3(4.0f, 4.0f, 4.0f));

    //send model matrix data to shader
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(flashlightModel));

    //send normal matrix data to shader
    glm::mat3 flashlightNormal = glm::mat3(glm::inverseTranspose(view * flashlightModel));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(flashlightNormal));

    // draw flashlight
    flashlight.Draw(shader);
}

void renderCandles(gps::Shader shader) {
    shader.useShaderProgram();

    glm::mat4 candlesModel = glm::mat4(1.0f);
    candlesModel = glm::translate(candlesModel, glm::vec3(0.7f, 0.85f, 0.471f));

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(candlesModel));

    glm::mat3 candlesNormal = glm::mat3(glm::inverseTranspose(view * candlesModel));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(candlesNormal));

    candles.Draw(shader);
}

void renderBathroomDoor(gps::Shader shader) {
    shader.useShaderProgram();

    glm::mat4 doorModel = glm::mat4(1.0f);

    doorModel = glm::translate(doorModel, glm::vec3(-0.1495f, 0.0479f, 0.86624f));

    doorModel = glm::rotate(doorModel, glm::radians(doorAngle), glm::vec3(0.0f, 1.0f, 0.0f));
    doorModel = glm::scale(doorModel, glm::vec3(0.91f, 0.91f, 0.91f));

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(doorModel));

    glm::mat3 doorNormal = glm::mat3(glm::inverseTranspose(view * doorModel));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(doorNormal));

    bathroomDoor.Draw(shader);
}

void renderBathroomDepth(gps::Shader shader) {
    shader.useShaderProgram();
    glm::mat4 bathroomModel = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(bathroomModel));
    bathroom.Draw(shader);
}

void renderFoxyDepth(gps::Shader shader) {
    shader.useShaderProgram();
    glm::mat4 foxyModel = glm::mat4(1.0f);
    foxyModel = glm::translate(foxyModel, foxyState.getCurrentPosition());
    foxyModel = glm::rotate(foxyModel, glm::radians(-5.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    foxyModel = glm::rotate(foxyModel, glm::radians(5.5f), glm::vec3(1.0f, 0.0f, 0.0f));
    foxyModel = glm::rotate(foxyModel, glm::radians(-98.4f), glm::vec3(0.0f, 1.0f, 0.0f));
    foxyModel = glm::scale(foxyModel, glm::vec3(1.25f, 1.25f, 1.25f));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(foxyModel));
    nightmareFoxy.Draw(shader);
}

void renderBonnieDepth(gps::Shader shader) {
    shader.useShaderProgram();
    glm::mat4 bonnieModel = glm::mat4(1.0f);
    bonnieModel = glm::translate(bonnieModel, bonnieState.getCurrentPosition());
    bonnieModel = glm::rotate(bonnieModel, glm::radians(-80.4f), glm::vec3(0.0f, 1.0f, 0.0f));
    bonnieModel = glm::scale(bonnieModel, glm::vec3(1.25f, 1.25f, 1.25f));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(bonnieModel));
    nightmareBonnie.Draw(shader);
}

void renderCandlesDepth(gps::Shader shader) {
    shader.useShaderProgram();
    glm::mat4 candlesModel = glm::mat4(1.0f);
    candlesModel = glm::translate(candlesModel, glm::vec3(0.7f, 0.85f, 0.471f));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(candlesModel));
    candles.Draw(shader);
}

void renderBathroomDoorDepth(gps::Shader shader) {
    shader.useShaderProgram();

    glm::mat4 doorModel = glm::mat4(1.0f);
    doorModel = glm::translate(doorModel, glm::vec3(-0.15f, 0.05f, 0.85f));
    doorModel = glm::rotate(doorModel, glm::radians(doorAngle), glm::vec3(0.0f, 1.0f, 0.0f));
    doorModel = glm::rotate(doorModel, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(doorModel));
    bathroomDoor.Draw(shader);
}

void renderSceneDepth(gps::Shader shader, glm::mat4 currentLightSpaceMatrix) {
    shader.useShaderProgram();

    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "lightSpaceMatrix"),
        1, GL_FALSE, glm::value_ptr(currentLightSpaceMatrix));

    renderBathroomDepth(shader);
    renderFoxyDepth(shader);
    renderBonnieDepth(shader);
    renderCandlesDepth(shader);
    renderBathroomDepth(shader);
    renderBathroomDoorDepth(shader);
}

void updateFlashlight() {
    myBasicShader.useShaderProgram();

    if (flashlightOn) {
        glm::vec3 camPos = myCamera.getCameraPosition();
        glm::vec3 camFront = myCamera.getCameraFrontDirection();
        glm::vec3 camRight = myCamera.getCameraRightDirection();
        glm::vec3 camUp = myCamera.getCameraUpDirection();

        spotLightPos = camPos
            + camFront * 0.4f
            + camRight * 0.2f 
            - camUp * 0.15f;

        spotLightDir = camFront;

        glUniform3fv(spotLightPosLoc, 1, glm::value_ptr(spotLightPos));
        glUniform3fv(spotLightDirLoc, 1, glm::value_ptr(spotLightDir));

        float time = glfwGetTime();
        float flickerIntensity = 1.0f;
        float maxIrritation = glm::max(foxyState.lightIrritation, bonnieState.lightIrritation);
        float irritationPercent = maxIrritation / 3.0f;

        if (irritationPercent > 0.5f) {
            float sineFlicker = 0.85f + 0.15f * glm::sin(time * 50.0f);
            float randomStutter = (rand() % 100 > (98 - (irritationPercent * 10))) ? 0.0f : 1.0f;

            flickerIntensity = sineFlicker * randomStutter;
        }

        glm::vec3 currentFlashlightColor = spotLightColor * flickerIntensity;
        glUniform3fv(spotLightColorLoc, 1, glm::value_ptr(currentFlashlightColor));

    } else {
        glUniform3fv(spotLightColorLoc, 1, glm::value_ptr(glm::vec3(0.0f)));
    }
}

void updateCandleLights() {
    myBasicShader.useShaderProgram();

    int numLights = std::min((int)pointLights.size(), MAX_POINT_LIGHTS);
    glUniform1i(numPointLightsLoc, numLights);

    for (int i = 0; i < numLights; i++) {
        glUniform3fv(pointLightPosLoc[i], 1, glm::value_ptr(pointLights[i].position));
        glUniform3fv(pointLightColorLoc[i], 1, glm::value_ptr(pointLights[i].color));
        glUniform1f(pointLightConstantLoc[i], pointLights[i].constant);
        glUniform1f(pointLightLinearLoc[i], pointLights[i].linear);
        glUniform1f(pointLightQuadraticLoc[i], pointLights[i].quadratic);
    }
}

void updateCandleFlicker(float deltaTime) {
    for (int i = 0; i < 3; i++) {
        if (i >= pointLights.size()) break;
        if (!pointLights[i].enabled) continue;

        float flicker = 0.85f + (rand() % 100) / 100.0f * 0.3f;
        glm::vec3 baseColor = glm::vec3(1.0f, 0.6f, 0.3f);
        pointLights[i].color = baseColor * flicker;
    }
}

void updateDoorAnimation(float deltaTime) {
    float targetAngle = foxyState.isVisible ? -42.7f : 0.0f;

    if (foxyState.isVisible) {
        pointLights[3].color = glm::vec3(1.0f, 0.05f, 0.0f);
    } else {
        pointLights[3].color = glm::vec3(0.0f, 0.0f, 0.0f);
    }

    float speed = 200.0f;
    if (doorAngle < targetAngle) {
        doorAngle = glm::min(doorAngle + speed * deltaTime, targetAngle);
    }else if (doorAngle > targetAngle) {
        doorAngle = glm::max(doorAngle - speed * deltaTime, targetAngle);
    }
}

void renderScene() {

    //moonlight light matrix
    lightMatrices[0] = computeLightSpaceMatrix();

    //flashlight light matrix
    glm::mat4 flashProj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 20.0f);
    glm::mat4 flashView = glm::lookAt(myCamera.getCameraPosition(),
        myCamera.getCameraPosition() + myCamera.getCameraFrontDirection(),
        myCamera.getCameraUpDirection());
    lightMatrices[1] = flashProj * flashView;

    //candle light matrix
    for (int i = 0; i < 3; i++) {
        glm::mat4 candleProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

        glm::vec3 lookDir;
        glm::vec3 vectorDir;

        if (i == 0) {
            lookDir = glm::vec3(1.0f, 0.0f, 0.0f); // map left wall 
            vectorDir = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        else if (i == 1) {
            lookDir = glm::vec3(0.0f, 0.0f, 1.0f); //map front wall
            vectorDir = glm::vec3(0.0f, -1.0f, 0.0f);
        }
        else {
            lookDir = glm::vec3(0.0f, -1.0f, 0.0f); // map floor
            vectorDir = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        lightMatrices[i + 2] = candleProj * glm::lookAt(pointLights[i].position,
            pointLights[i].position + lookDir,
            vectorDir);
    }
    
    depthMapShader.useShaderProgram();
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);

    for (int i = 0; i < SHADOW_LAYERS; i++) {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthMapTextureArray, 0, i);
        glClear(GL_DEPTH_BUFFER_BIT);

        glUniformMatrix4fv(glGetUniformLocation(depthMapShader.shaderProgram, "lightSpaceMatrix"),
            1, GL_FALSE, glm::value_ptr(lightMatrices[i]));
        renderSceneDepth(depthMapShader, lightMatrices[i]);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    myBasicShader.useShaderProgram();

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, depthMapTextureArray);
    glUniform1i(glGetUniformLocation(myBasicShader.shaderProgram, "shadowMapArray"), 2);

    glUniformMatrix4fv(glGetUniformLocation(myBasicShader.shaderProgram, "lightSpaceMatrices"),
        5, GL_FALSE, glm::value_ptr(lightMatrices[0]));

    //flashlight
    updateFlashlight();
    renderFlashlight(myBasicShader);

    //candles
    updateCandleLights();
    renderCandles(myBasicShader);

    //environment
	renderBathroom(myBasicShader);
    renderBathroomDoor(myBasicShader);

    //animatronics
    renderFoxy(myBasicShader);
    renderBonnie(myBasicShader);
}

void cleanup() {
    myWindow.Delete();
    //cleanup code for your own data
}

int main(int argc, const char * argv[]) {

    try {
        initOpenGLWindow();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    initOpenGLState();
	initModels();
	initShaders();
    initShadowFBO();
	initUniforms();
    initCandleLights();
    initCameraStates();
    initAudio();
    setWindowCallbacks();

    foxyState.irritationThreshold = 3.0f;
    foxyState.moveInterval = 8.0f;

    bonnieState.irritationThreshold = 6.0f;
    bonnieState.moveInterval = 15.0f;

	glCheckError();

    srand(time(NULL));
    float lastFrame = 0.0f;

	// application loop
	while (!glfwWindowShouldClose(myWindow.getWindow())) {
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        updateCandleFlicker(deltaTime);

        updateCameraTransition(deltaTime);
        updateAnimatronics(deltaTime);
        updateDoorAnimation(deltaTime);

        processMovement();
	    renderScene();

		glfwPollEvents();
		glfwSwapBuffers(myWindow.getWindow());

		glCheckError();
	}

	cleanup();

    return EXIT_SUCCESS;
}

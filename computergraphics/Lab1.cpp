/*
    과제 - 컴퓨터그래픽스
    =========================
    일부 실행에 필요한 라이브러리는 제거해둔 상태입니다.
    따라서 본 소스만으로는 실행 불가능합니다.
*/
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

// =========================
// v 필요한 외부 라이브러리. 과제 수행 당시에 주어진 비공개 라이브러리가 있기에 미포함되었습니다.
#define STB_IMAGE_IMPLEMENTATION
#include "toys.h" // for this assignment
#include "j3a.hpp" // for this assignment
#include "stb_image.h" // for this assignment
// =========================

#include <iostream> // debug output
#include <ctime> // time 
#include <vector>

using namespace glm;


/// Boilerplate for future assignments
/// This could be separated to another file, but for ease of uploading to assignments page I've inlined it
#pragma pack (push, 1)
struct Vert {
    const static int stride = 12; // stride per vertices -> # of floats per vert.
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 col;

    Vert(vec3 pos, vec3 normal, vec2 uv, vec4 col) {
        this->pos = pos;
        this->normal = normal;
        this->uv = uv;
        this->col = col;
    }
    // Transpose the data into 1D array of floats
    void pushDataTo(std::vector<float> &dst) {
        dst.insert(dst.end(), {pos.x, pos.y, pos.z, normal.x, normal.y, normal.z, uv.s, uv.t, col.r, col.g, col.b, col.a});
    }
    void pushDataTo(std::vector<float>& dst, int idx) {
        float tmp[] = { pos.x, pos.y, pos.z, normal.x, normal.y, normal.z, uv.s, uv.t, col.r, col.g, col.b, col.a };
        for (int i = 0; i < sizeof(tmp)/sizeof(float); i++)
        {
            int vidx = idx * stride + i;
            dst[vidx] = tmp[i];
        }
    }
}; 
#pragma pack (pop)

struct Mesh {
    GLuint	vaoID = 0,
            vbPosID = 0, // vb: position
            vbNormalID = 0, // vb: normal
            vbColID = 0, // vb: colour
            vbUVID = 0, // vb: uv
            eboID = 0;
    // std::vector<Vert> verts;
    int vertsNum;
    std::vector<vec3> vp; // position
    std::vector<vec3> vn; // normal
    std::vector<vec2> vt; // uv
    std::vector<vec4> vc; // colour

    // textures
    bool hasTextures;
    std::string texPathAlbedo; // path
    std::string texPathBump;
    int texWidAlbedo, texHeiAlbedo, texCompAlbedo; // dimensions
    int texWidBump, texHeiBump, texCompBump;
    GLuint texIDAlbedo, texIDBump; // texture IDs

    // material
    float matShininess;

    std::vector<u32vec3> vidx; // u16vec3: u16 for each index, with 3 index forming one triangle
    // array of vertex attributes (for data copying, don't modify this as this is used internally)
    std::vector<float> _vb;

    Mesh() {
        hasTextures = false;
        texPathAlbedo = "";
        texPathBump = "";
        matShininess = 4.0;
        texIDAlbedo = 0;
        texIDBump = 0;
    }
    ~Mesh() {
        // Cleanup the buffers
        //glDeleteBuffers(1, &vboID);
        glDeleteBuffers(1, &vbPosID);
        glDeleteBuffers(1, &vbNormalID);
        glDeleteBuffers(1, &vbUVID);
        glDeleteBuffers(1, &vbColID);
        glDeleteBuffers(1, &eboID);
        glDeleteVertexArrays(1, &vaoID);

        glDeleteTextures(1, &texIDAlbedo);
        glDeleteTextures(1, &texIDBump);
    }

    // Loads the texture using stb_image
    bool loadTextures(std::string pathAlbedo, std::string pathBump) {
        void *texBuffAlbedo = NULL, *texBuffBump = NULL;
        texIDAlbedo = 0; texIDBump = 0;
        // Set the texture path
        texPathAlbedo = pathAlbedo;
        texPathBump = pathBump;
        // Try to load
        texBuffAlbedo = stbi_load(pathAlbedo.c_str(), &texWidAlbedo, &texHeiAlbedo, &texCompAlbedo, 4); // RGBA, 4 channels
        texBuffBump = stbi_load(pathBump.c_str(), &texWidBump, &texHeiBump, &texCompBump, 4); // RGBA, 4 channels
        //std::cout << "(Albedo: " << pathAlbedo << "|" << texWidAlbedo << "x" << texHeiAlbedo << "::" << texCompAlbedo << "channels), ";
        //std::cout << "(Bump: " << pathBump << "|" << texWidBump << "x" << texHeiBump << "::" << texCompBump << "channels)" << std::endl;

        // Check if the file were loaded correctly
        if (texBuffAlbedo == NULL && texBuffBump == NULL)
            return false; // no texture

        // Send albedo map to GPU
        if (texBuffAlbedo)
        {
            glGenTextures(1, &texIDAlbedo);
            glBindTexture(GL_TEXTURE_2D, texIDAlbedo);
                // (parameter)
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                // (send)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, texWidAlbedo, texHeiAlbedo, 0, GL_RGBA, GL_UNSIGNED_BYTE, texBuffAlbedo);
                // (generate mipmap)
                glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(texBuffAlbedo); // Free the buffers
            hasTextures = true;
        }

        // Send bump map to GPU
        if (texBuffBump)
        {
            glGenTextures(1, &texIDBump);
            glBindTexture(GL_TEXTURE_2D, texIDBump);
                // (parameter)
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                // (send)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, texWidBump, texHeiBump, 0, GL_RGBA, GL_UNSIGNED_BYTE, texBuffBump);
                // (generate mipmap)
                glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(texBuffBump);
            hasTextures = true;
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        return (texIDAlbedo != 0 || texIDBump != 0);
    }

    // Call this after you set up the windows/OpenGL context (otherwise it will crash)
    void init() {
        clear();
        // Generate buffers on GPU
        //glGenBuffers(1, &vboID); // Buffer for VBO
        glGenBuffers(1, &vbPosID);
        glGenBuffers(1, &vbNormalID);
        glGenBuffers(1, &vbColID);
        glGenBuffers(1, &vbUVID);

        glGenBuffers(1, &eboID); // Buffer for Index buffer
        glGenVertexArrays(1, &vaoID); // Buffer for VAO

        // Assign buffers to VAO
        // Set vertex array assigned at index vaTest to current context
        glBindVertexArray(vaoID);
        // Bind GL_ELEMENT_ARRAY_BUFFER to ebo on this context
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboID);
        // Attribute #0 (pos)
        glEnableVertexAttribArray(0); // Enable attribute #0
        glBindBuffer(GL_ARRAY_BUFFER, vbPosID);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), 0);
        // Attribute #1 (normal)
        glEnableVertexAttribArray(1); // Enable attribute #1
        glBindBuffer(GL_ARRAY_BUFFER, vbNormalID);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), 0);
        // Attribute #2 (uv)
        glEnableVertexAttribArray(2); // Enable attribute #2
        glBindBuffer(GL_ARRAY_BUFFER, vbUVID);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), 0);
        // Attribute #3 (colour)
        glEnableVertexAttribArray(3); // Enable attribute #3
        glBindBuffer(GL_ARRAY_BUFFER, vbColID);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(vec4), 0);
        glBindVertexArray(0); // reset context
    }

    void clear() {
        //verts.clear();
        vp.clear();
        vn.clear();
        vt.clear();
        vc.clear();
        vidx.clear();
        vertsNum = 0;
    }

    // VB wrangling functions
    // Add a vertex to mesh and returns the index of new vertex
    int addVert(Vert vert) {
        //verts.push_back(vert);
        vp.push_back(vert.pos);
        vn.push_back(vert.normal);
        vt.push_back(vert.uv);
        vc.push_back(vert.col);
        return vertsNum++;
    }
    // Add a triangle to mesh with previously added vertices
    void addTriFromVerts(u32vec3 triidx) {
        vidx.push_back(triidx);
    }
    void addTriFromVerts(GLuint v1, GLuint v2, GLuint v3) {
        vidx.push_back(u32vec3(v1, v2, v3));
    }
    // Add a triangle to mesh with new vertices
    void addTri(Vert v1, Vert v2, Vert v3) {
        GLuint idx1 = addVert(v1), idx2 = addVert(v2), idx3 = addVert(v3);
        addTriFromVerts(idx1, idx2, idx3);
    }
    // Build the vertex buffer and send to GPU
    void build() {
        // Build array of vertex data
        std::cout << "(mesh: build begin, ";

        // Send data to GPU
        std::cout << "sending to GPU...";
        std::cout << "vbo.";
        glBindBuffer(GL_ARRAY_BUFFER, vbPosID); // Bind ARRAY_BUFFER
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * vertsNum, vp.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, vbNormalID);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * vertsNum, vn.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, vbUVID);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * vertsNum, vt.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, vbColID);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec4) * vertsNum, vc.data(), GL_STATIC_DRAW);
        std::cout << "ebo.";
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboID); // Bind GL_ELEMENT_ARRAY_BUFFER to ebo on this context
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32vec3) * vidx.size(), vidx.data(), GL_STATIC_DRAW); // copy indices to buffer
        std::cout << "... done.)";
    }

    // Sets vertices and triangles from list of vertices formed by j3a.hpp
    void setJ3a(int vnum, int trinum, vec3* vp, vec3* vn, vec2* vt, vec4 tint, u32vec3* tris) {
        clear();

        // Copy vertex data
        for (int i = 0; i < vnum; i++)
        {
            addVert(Vert(vp[i], vn[i], vt[i], tint));
            //verts.push_back(Vert(vp[i], vn[i], vt[i], tint));
        }

        // Copy triangle data
        for (int i = 0; i < trinum; i++)
        {
            vidx.push_back(tris[i]);
        }
    }

    // Submit the mesh onto GPU
    void render(GLuint shaderProgramID, GLuint shadowMapTex) {
        GLuint loc;

        // Texture indicator
        loc = glGetUniformLocation(shaderProgramID, "uTextured");
        glUniform1i(loc, hasTextures);

        // albedo texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texIDAlbedo);
        loc = glGetUniformLocation(shaderProgramID, "sTexAlbedo");
        glUniform1i(loc, 0);
        // bumpmap texture
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texIDBump);
        loc = glGetUniformLocation(shaderProgramID, "sTexBump");
        glUniform1i(loc, 1);
        

        // shininess
        loc = glGetUniformLocation(shaderProgramID, "uShininess");
        glUniform1f(loc, matShininess);

        // VAO
        glBindVertexArray(vaoID);
        glDrawElements(GL_TRIANGLES, (GLsizei)vidx.size() * 3, GL_UNSIGNED_INT, 0);
        //glBindVertexArray(0); // reset context
    }
};

// Convert hex colour to vec4
glm::vec4 colHexToVec4(int col)
{
    return vec4(((col & 0xFF000000) >> 24) / 255.0, ((col & 0x00FF0000) >> 16) / 255.0, ((col & 0x0000FF00) >> 8) / 255.0, ((col & 0x000000FF) >> 0) / 255.0);
}

// Load J3A, all the objects into a list of meshes
bool loadMesh(const char* dir, const char* filename, std::vector<Mesh*> *meshes)
{
    // convert directory & filename to full directory
    std::string file = dir + std::string(filename);

    // Load J3A, create mesh from loaded data for each objects
    bool res = loadJ3A(file.c_str());

    if (!res)
    {
        std::cout << "loadMesh: Load failed! File " << file.c_str() << " does not exist?" << std::endl;
        return res;
    }

    for (int i = 0; i < nObjects; i++)
    {
        Mesh *mesh = new Mesh();
        mesh->init();

        std::cout << "loadMesh: Loading object " << i << "/" << nObjects << " (verts: " << nVertices[i] << ", tris: " << nTriangles[i] << ") ...";
        mesh->setJ3a(nVertices[i], nTriangles[i], vertices[i], normals[i], texCoords[i], diffuseColor[i], triangles[i]);
        std::cout << "... Building...";
        mesh->build();
        std::cout << "... Reading textures...";
        if (!mesh->loadTextures(dir + diffuseMap[i], dir + bumpMap[i]))
        {
            std::cout << std::endl << "ERROR! can't read textures." << std::endl;
        }
        // set materials
        mesh->matShininess = shininess[i];
        std::cout << "... Done!" << std::endl;
        meshes->push_back(mesh);
    }

    std::cout << "loadMesh: Loaded " << nObjects << " objects mesh (result: " << res << ")" << std::endl;
    
    return res;
}
/// Boilerplate for future assignments ///

// Vars decl.
#define PI 3.14159265359f
// (2*pi)
#define TAU 6.28318530718f
long timeCurrent, timePrev;
double timeDelta, timeElapsed;
// camera
double oldMouseX = 0, oldMouseY = 0;
float   viewDist = 1, viewRotH = 0, viewRotV = 0,
        viewFov = 90;
vec3 viewBasePos = vec3(0, 0, 5);
// mesh
std::vector<std::vector<Mesh*>> meshes;
Mesh floorMesh;

// Shadow mapping
#define SHADOW_MAP_WIDTH 1024
#define SHADOW_MAP_HEIGHT 1024
GLuint shadowMapTex, shadowMapDepthTex, shadowMapFBO;
Program shadowMapProgram, renderingProgram;
bool shadowMapDebug = false, shadowMapShowMap = false;

bool error = false;

GLuint texID;

// Function decl.
int initWindow(int w, int h, const char* title, GLFWwindow** window);
void prepVB();
void render(GLFWwindow* window);
void onDraw(GLFWwindow* window);

void cursorCallback(GLFWwindow* win, double x, double y)
{
    int w, h;
    glfwGetFramebufferSize(win, &w, &h);
    // Only drag the view around
    if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_1))
    {
        viewRotH += (oldMouseX - x) * 1.0f / w * 360;
        viewRotV += (oldMouseY - y) * 1.0f / h * 360;
    }
    oldMouseX = x;
    oldMouseY = y;
}
void scrollCallback(GLFWwindow* win, double x, double y)
{
    //viewDist = clamp(viewDist * (float)(pow(0.98f, y)), 0.1f, 10.0f);
    viewFov = viewFov * (float)(pow(0.98f, y));
}
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_E)
    {
        shadowMapDebug = (action == GLFW_RELEASE) ? false : true;
    }
    if (key == GLFW_KEY_R)
    {
        shadowMapShowMap = (action == GLFW_RELEASE) ? false : true;
    }
}
// Debug
// https://www.khronos.org/opengl/wiki/OpenGL_Error
void GLAPIENTRY
MessageCallback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
        (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
        type, severity, message);
    if (severity == GL_DEBUG_SEVERITY_HIGH) error = true;
}

int main(void)
{
    /// Initialize
    error = false;
    std::cout << "Graphics Programming/2021-2 Assignment #6: Shadow mapping" << std::endl << "Ahn yubin(202021088)@MMXXI" << std::endl;
    std::cout << "Window init...";
    GLFWwindow* window = NULL;
    if (!initWindow(640, 480, "Assignment #6 - 202021088 Ahn Yubin", &window))
        return -1; // Got error while initializing -- quit
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);
    std::cout << "... done" << std::endl;

    // During init, enable debug output
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    // Initialize data
    std::cout << "Loading shaders...";
    renderingProgram.loadShaders("render.vert", "render.frag");
    shadowMapProgram.loadShaders("shadowmap.vert", "shadowmap.frag");
    std::cout << "... done" << std::endl;
    // Setup texture loading
    stbi_set_flip_vertically_on_load(1);
    std::cout << "Loading VB and data...";
    // Generate ground 
    // Load models
    prepVB();
    //program.loadShaders("shader.vert", "shader.frag");
    // Generate floor model
    floorMesh.init();
    floorMesh.addTri({ {-2048, -1, -2048}, {0, 1, 0}, {0, 0}, {1,1,1,1} },
                    { {2048, -1, -2048}, {0, 1, 0}, {0, 0}, {1,1,1,1} },
                    { {-2048, -1, 2048}, {0, 1, 0}, {0, 0}, {1,1,1,1} });
    floorMesh.addTri({ {-2048, -1, 2048}, {0, 1, 0}, {0, 0}, {1,1,1,1} },
                    { {2048, -1, -2048}, {0, 1, 0}, {0, 0}, {1,1,1,1} },
                    { {2048, -1, 2048}, {0, 1, 0}, {0, 0}, {1,1,1,1} });
    floorMesh.build();
    std::cout << "... done" << std::endl;
    // Setup textures
    std::cout << "Generating textures..." << std::endl;
    // (shadow texture)
    glGenTextures(1, &shadowMapTex);
    glBindTexture(GL_TEXTURE_2D, shadowMapTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, 0, GL_RGB, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // (depth texture)
    glGenTextures(1, &shadowMapDepthTex);
    glBindTexture(GL_TEXTURE_2D, shadowMapDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // (frame buffer)
    glGenFramebuffers(1, &shadowMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, shadowMapTex, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowMapDepthTex, 0);
    GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, drawBuffers);
    // (check)
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "SHADOW MAP FBO ERROR!" << std::endl;
        return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);


    std::cout << "... done" << std::endl;
    //

    timeCurrent = timePrev = clock(); // get time elapsed since start
    ///

    /// Render loop
    std::cout << "Entering main loop..." << std::endl;
    while (!error && !glfwWindowShouldClose(window)) {
        //std::cout << "----------------------------------------STEP BEGIN" << std::endl;
        render(window);
        glfwPollEvents();
    }
    std::cout << "...Loop ended. Goodbye!" << std::endl;
    ///

    // Free mesh list
    for (int i = 0; i < meshes.size(); i++)
    {
        for (int j = 0; j < meshes[i].size(); j++)
        {
            delete meshes[i][j];
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    // Free textures
    glDeleteTextures(1, &shadowMapTex);
    glDeleteTextures(1, &shadowMapDepthTex);
}

// Builds vertex buffers
void prepVB()
{
    std::vector<Mesh*> mesh1, mesh2;
    if (!loadMesh("./j3amdls/", "dwarf.j3a", &mesh1))
        loadMesh("./", "dwarf.j3a", &mesh1); // alternative model path
    meshes.push_back(mesh1);
    
    if (!loadMesh("./j3amdls/", "Trex_m.j3a", &mesh2))
        loadMesh("./", "Trex_m.j3a", &mesh2); // alternative model path
    meshes.push_back(mesh2);
}

// Initializes window
int initWindow(int w, int h, const char* title, GLFWwindow** window)
{
    if (!glfwInit()) // Check for error
        return -1;

    // set antialiasing
    glfwWindowHint(GLFW_SAMPLES, 8);

    *window = glfwCreateWindow(w, h, title, NULL, NULL);
    if (*window == NULL) // Check for error
        return -1;

    glfwMakeContextCurrent(*window);
    glewInit();
    glfwSwapInterval(0);
    return 1;
}

// Render loop
void render(GLFWwindow* window)
{
    // Update delta time
    timePrev = timeCurrent;
    timeCurrent = clock();
    timeDelta = (timeCurrent - timePrev) / (1.0f * CLOCKS_PER_SEC);
    timeElapsed = timeCurrent / (1.0f * CLOCKS_PER_SEC);

    // Get window size
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    // Use it to set the viewport
    glViewport(0, 0, width, height);

    // Draw
    onDraw(window);

    // Update
    glfwSwapBuffers(window);
}

void onDraw(GLFWwindow* window)
{
    GLclampf t = fmod(timeElapsed, 1.0);

    // Update view
    viewRotV = clamp(viewRotV, -89.99f, 89.99f);
    viewFov = clamp(viewFov, 10.0f, 170.0f);

    vec3 lightPos = vec3(rotate((float)radians(timeElapsed * 45.0), vec3(0, 1, 0)) *
                    rotate(radians(10.0f), vec3(1, 0, 0)) *
                    vec4(vec3(20.0f), 1.0f));

    // Calculate matrix
    int winWid, winHei;
    glfwGetFramebufferSize(window, &winWid, &winHei);

    vec3 viewFrom = vec3(   rotate(radians(viewRotH), vec3(0, 1, 0)) *
                            rotate(radians(viewRotV), vec3(1, 0, 0)) *
                            vec4(viewBasePos * viewDist, 1.0f) ),
         viewTo = vec3(0, 0, 0);
    mat4    matView = lookAt(viewFrom, viewTo, vec3(0, 1, 0)),
            matProj = perspective(radians(viewFov), winWid / (float)winHei, 0.1f, 512.0f);
    // list of model matrices
    mat4* matModels = new mat4[meshes.size()];
    for (int i = 0; i < meshes.size(); i++)
        matModels[i] = mat4(1);
    // offset models a bit
    //matModels[0] = translate(vec3(3, 0, 0));
    matModels[1] = translate(vec3(-3, 0, 0));

    // Calculate (global) light matrix
    mat4 //shadowMatProj = ortho(-5.0, 5.0, -5.0, 5.0, 0.01, 256.0),
        shadowMatProj = ortho(-5.0, 5.0, -5.0, 5.0, 0.01, 64.0),
        shadowMatView = lookAt(lightPos, vec3(0, 0, 0), vec3(0, 1, 0)),
        shadowMatVP = shadowMatProj * shadowMatView;

    glEnable(GL_DEPTH_TEST);

    // Pass #1: shadow map
    // =====================================
    GLuint uMAT_MVP;
    // Draw to shadow map FBO
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    // Background
    glViewport(0, 0, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //glDisable(GL_FRAMEBUFFER_SRGB); // Disable SRGB

    // Set shader & fetch uniform location
    glUseProgram(shadowMapProgram.programID);
    uMAT_MVP = glGetUniformLocation(shadowMapProgram.programID, "MAT_MVP");

    // Draw floor first
    glUniformMatrix4fv(uMAT_MVP, 1, false, value_ptr(shadowMatVP));
    floorMesh.render(shadowMapProgram.programID, shadowMapTex);
    // Draw all meshes
    for (int i = 0; i < meshes.size(); i++)
    {
        // Set uniform: shadow MVP (calculated)
        mat4 shadowMatMVP = shadowMatVP * matModels[i];
        glUniformMatrix4fv(uMAT_MVP, 1, false, value_ptr(shadowMatVP * matModels[i]));

        for (int j = 0; j < meshes[i].size(); j++)
            meshes[i][j]->render(shadowMapProgram.programID, shadowMapTex);
    }
    
    // Pass #2: normal render
    // =====================================
    GLuint uMAT_M, uMAT_V, uMAT_P, uMAT_LIGHT,
            uTime, uShininess, uLightColour, uAmbColour, uColour,
            uCameraPos, uLightPos;
    // Draw to default FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // Background
    glViewport(0, 0, winWid, winHei);
    //glClearColor(0, 0, 0, 1);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB); // SRGB
    glClearColor(0.16, 0.34, 0.4, 1); // same as uAmbientColour
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set shader
    glUseProgram(renderingProgram.programID);
    
    // Set uniform
    uMAT_M = glGetUniformLocation(renderingProgram.programID, "MAT_M");
    uMAT_V = glGetUniformLocation(renderingProgram.programID, "MAT_V");
    uMAT_P = glGetUniformLocation(renderingProgram.programID, "MAT_P");
    uMAT_LIGHT = glGetUniformLocation(renderingProgram.programID, "MAT_LIGHT");

    uTime = glGetUniformLocation(renderingProgram.programID, "uTime");

    uLightColour = glGetUniformLocation(renderingProgram.programID, "uLightColour");
    uAmbColour = glGetUniformLocation(renderingProgram.programID, "uAmbColour");
    uColour = glGetUniformLocation(renderingProgram.programID, "uColour");

    uCameraPos = glGetUniformLocation(renderingProgram.programID, "uCameraPos");
    uLightPos = glGetUniformLocation(renderingProgram.programID, "uLightPos");
    
    glUniform1f(uTime, timeElapsed);

    glUniform3f(uLightColour, 1.0, 0.85, 0.4);
    glUniform3f(uAmbColour, 0.16, 0.34, 0.4);
    glUniform3f(uColour, 1.0, 1.0, 1.0);

    glUniform3f(uCameraPos, viewFrom.x, viewFrom.y, viewFrom.z);
    glUniform3f(uLightPos, lightPos.x, lightPos.y, lightPos.z);

    // Shadow mapping
    // (pass shadowmap texture)
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, shadowMapTex);
    GLuint loc = glGetUniformLocation(renderingProgram.programID, "sTexShadowmap");
    glUniform1i(loc, 2);

    glUniform1i(glGetUniformLocation(renderingProgram.programID, "uDebugMode"), shadowMapShowMap);
    
    
    // (prepare bias matrix)
    mat4 biasMatrix = mat4(1);//  translate(vec3(0.5))* scale(vec3(0.5));
    
    // Draw meshes
    // Set transform mat
    glUniformMatrix4fv(uMAT_V, 1, false, value_ptr(matView));
    glUniformMatrix4fv(uMAT_P, 1, false, value_ptr(matProj));
    
    // Draw floor first
    glUniformMatrix4fv(uMAT_M, 1, false, value_ptr(identity<mat4>()));
    glUniformMatrix4fv(uMAT_LIGHT, 1, false, value_ptr(biasMatrix * shadowMatVP));
    // (set floor colour)
    glUniform3f(uColour, 0.55, 0.95, 0.24);
    floorMesh.render(renderingProgram.programID, shadowMapTex);

    // Draw all meshes
    // (set default colour)
    glUniform3f(uColour, 1.0, 1.0, 1.0);
    for (int i = 0; i < meshes.size(); i++)
    {
        // set current meshes transformation and light matrix
        glUniformMatrix4fv(uMAT_M, 1, false, value_ptr(matModels[i]));
        glUniformMatrix4fv(uMAT_LIGHT, 1, false, value_ptr(biasMatrix * shadowMatVP * matModels[i]));

        for (int j = 0; j < meshes[i].size(); j++)
            meshes[i][j]->render(renderingProgram.programID, shadowMapTex);
    }
    

    // Debug: FBO
    if (shadowMapDebug)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, shadowMapFBO);// makes OpenGL reading data from your “framebuffer”
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);// makes OpenGL drawing into “0”, which is the default framebuffer
        // from now on all draw operations affect the default framebuffer (screen) and all read operations will get the data from your “framebuffer” …
        glReadBuffer(GL_COLOR_ATTACHMENT0);// tells openGL from which attachment you want to read the data

        glViewport(0, 0, winWid, winHei);
        glBlitFramebuffer(// makes OpenGL copying the framebuffer data
            0, 0, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT,
            0, 0, 1024, 1024,
            GL_COLOR_BUFFER_BIT,// you only care about the color data, not the depth data …
            GL_NEAREST);// filter parameter
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    delete matModels;
}
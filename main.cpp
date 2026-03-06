#include <GL/glut.h>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

constexpr float kPi = 3.14159265359f;
constexpr int kRainDropCount = 900;

struct Camera {
    float x = 0.0f;
    float y = 3.2f;
    float z = 11.0f;
    float yaw = 0.0f;
    float pitch = -8.0f;
};

Camera gCamera;
GLUquadric* gQuadric = nullptr;

bool gKeyDown[256] = {false};
bool gAnimateBlades = true;
bool gDayMode = true;
bool gRainMode = false;

float gBladeAngle = 0.0f;
float gCloudOffsetNear = 0.0f;
float gCloudOffsetFar = 0.0f;

int gWindowWidth = 1280;
int gWindowHeight = 720;

struct RainDrop {
    float x;
    float y;
    float z;
    float speed;
};

RainDrop gRainDrops[kRainDropCount];

float degToRad(float degrees) {
    return degrees * kPi / 180.0f;
}

float randomRange(float minValue, float maxValue) {
    return minValue + (maxValue - minValue) *
                          (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
}

void resetRaindrop(RainDrop& drop, bool spawnAtTop) {
    drop.x = randomRange(-32.0f, 32.0f);
    drop.z = randomRange(-38.0f, 18.0f);
    drop.y = spawnAtTop ? randomRange(10.0f, 22.0f) : randomRange(0.6f, 22.0f);
    drop.speed = randomRange(10.0f, 18.0f);
}

void initRainSystem() {
    for (int i = 0; i < kRainDropCount; ++i) {
        resetRaindrop(gRainDrops[i], false);
    }
}

void drawCube(float w, float h, float d) {
    glPushMatrix();
    glScalef(w, h, d);
    glutSolidCube(1.0f);
    glPopMatrix();
}

void drawCylinderY(float baseRadius, float topRadius, float height, int slices = 32) {
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    gluCylinder(gQuadric, baseRadius, topRadius, height, slices, 1);
    glPopMatrix();
}

void drawDiskY(float radius, bool flipNormal = false, int slices = 32) {
    glPushMatrix();
    glRotatef(flipNormal ? 90.0f : -90.0f, 1.0f, 0.0f, 0.0f);
    gluDisk(gQuadric, 0.0f, radius, slices, 1);
    glPopMatrix();
}

void drawTriangle3D(float ax, float ay, float az,
                    float bx, float by, float bz,
                    float cx, float cy, float cz) {
    float ux = bx - ax;
    float uy = by - ay;
    float uz = bz - az;
    float vx = cx - ax;
    float vy = cy - ay;
    float vz = cz - az;

    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 0.0001f) {
        nx /= len;
        ny /= len;
        nz /= len;
    }

    glNormal3f(nx, ny, nz);
    glVertex3f(ax, ay, az);
    glVertex3f(bx, by, bz);
    glVertex3f(cx, cy, cz);
}

void drawMountainPyramid(float halfWidth, float height, float halfDepth) {
    glBegin(GL_TRIANGLES);
    drawTriangle3D(-halfWidth, 0.0f, halfDepth, halfWidth, 0.0f, halfDepth, 0.0f, height, 0.0f);
    drawTriangle3D(halfWidth, 0.0f, halfDepth, halfWidth, 0.0f, -halfDepth, 0.0f, height, 0.0f);
    drawTriangle3D(halfWidth, 0.0f, -halfDepth, -halfWidth, 0.0f, -halfDepth, 0.0f, height, 0.0f);
    drawTriangle3D(-halfWidth, 0.0f, -halfDepth, -halfWidth, 0.0f, halfDepth, 0.0f, height, 0.0f);
    glEnd();
}

void drawGround() {
    const int extent = 30;
    for (int x = -extent; x < extent; ++x) {
        for (int z = -extent; z < extent; ++z) {
            const bool dark = ((x + z) & 1) == 0;
            if (dark) {
                glColor3f(0.26f, 0.52f, 0.24f);
            } else {
                glColor3f(0.22f, 0.48f, 0.20f);
            }

            glBegin(GL_QUADS);
            glNormal3f(0.0f, 1.0f, 0.0f);
            glVertex3f(static_cast<float>(x), 0.0f, static_cast<float>(z));
            glVertex3f(static_cast<float>(x + 1), 0.0f, static_cast<float>(z));
            glVertex3f(static_cast<float>(x + 1), 0.0f, static_cast<float>(z + 1));
            glVertex3f(static_cast<float>(x), 0.0f, static_cast<float>(z + 1));
            glEnd();
        }
    }

    glColor3f(0.40f, 0.36f, 0.30f);
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-2.0f, 0.01f, 12.0f);
    glVertex3f(2.0f, 0.01f, 12.0f);
    glVertex3f(1.0f, 0.01f, 1.0f);
    glVertex3f(-1.0f, 0.01f, 1.0f);
    glEnd();
}

void drawHills() {
    struct Hill {
        float x;
        float y;
        float z;
        float width;
        float height;
        float depth;
        float yaw;
        float r;
        float g;
        float b;
    };

    const float rainTint = gRainMode ? 0.72f : 1.0f;
    const Hill hills[] = {
        {-27.0f, -0.2f, -35.0f, 11.0f, 11.5f, 7.5f, -7.0f, 0.22f, 0.42f, 0.21f},
        {-14.0f, -0.2f, -33.0f, 9.8f, 10.2f, 6.8f, 5.0f, 0.24f, 0.45f, 0.22f},
        {-1.0f, -0.2f, -36.0f, 13.5f, 12.8f, 8.9f, -2.0f, 0.21f, 0.41f, 0.20f},
        {14.0f, -0.2f, -34.0f, 10.0f, 10.0f, 6.8f, 8.0f, 0.24f, 0.44f, 0.22f},
        {29.0f, -0.2f, -37.5f, 14.0f, 13.2f, 9.0f, -4.0f, 0.20f, 0.38f, 0.19f},
    };

    for (const Hill& hill : hills) {
        glPushMatrix();
        glTranslatef(hill.x, hill.y, hill.z);
        glRotatef(hill.yaw, 0.0f, 1.0f, 0.0f);
        glColor3f(hill.r * rainTint, hill.g * rainTint, hill.b * rainTint);
        drawMountainPyramid(hill.width, hill.height, hill.depth);
        glPopMatrix();

        float iceR = gDayMode ? 0.96f : 0.84f;
        float iceG = gDayMode ? 0.98f : 0.88f;
        float iceB = gDayMode ? 1.00f : 0.96f;
        if (gRainMode) {
            iceR *= 0.90f;
            iceG *= 0.90f;
            iceB *= 0.92f;
        }

        glPushMatrix();
        glTranslatef(hill.x, hill.y + hill.height * 0.60f, hill.z);
        glRotatef(hill.yaw, 0.0f, 1.0f, 0.0f);
        glColor3f(iceR, iceG, iceB);
        drawMountainPyramid(hill.width * 0.34f, hill.height * 0.40f, hill.depth * 0.34f);
        glPopMatrix();
    }
}

void drawTree(float x, float z, float scale) {
    glPushMatrix();
    glTranslatef(x, 0.0f, z);
    glScalef(scale, scale, scale);

    glColor3f(0.45f, 0.28f, 0.16f);
    glPushMatrix();
    glTranslatef(0.0f, 0.7f, 0.0f);
    drawCylinderY(0.18f, 0.14f, 1.4f, 20);
    glTranslatef(0.0f, 1.4f, 0.0f);
    drawDiskY(0.14f);
    glPopMatrix();

    glColor3f(0.10f, 0.45f, 0.18f);
    glPushMatrix();
    glTranslatef(0.0f, 2.25f, 0.0f);
    glutSolidSphere(0.75f, 20, 20);
    glTranslatef(-0.35f, -0.25f, 0.2f);
    glutSolidSphere(0.55f, 16, 16);
    glTranslatef(0.75f, 0.0f, -0.4f);
    glutSolidSphere(0.55f, 16, 16);
    glPopMatrix();

    glPopMatrix();
}

void drawHouse(float x, float z, float rotY, float scale) {
    glPushMatrix();
    glTranslatef(x, 0.0f, z);
    glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glScalef(scale, scale, scale);

    glColor3f(0.78f, 0.70f, 0.57f);
    glPushMatrix();
    glTranslatef(0.0f, 1.1f, 0.0f);
    drawCube(3.4f, 2.2f, 2.4f);
    glPopMatrix();

    glColor3f(0.52f, 0.16f, 0.14f);
    glPushMatrix();
    glTranslatef(0.0f, 2.45f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glutSolidCone(2.25f, 1.25f, 4, 1);
    glPopMatrix();

    glColor3f(0.30f, 0.18f, 0.10f);
    glPushMatrix();
    glTranslatef(0.0f, 0.75f, 1.21f);
    drawCube(0.60f, 1.10f, 0.10f);
    glPopMatrix();

    glColor3f(0.20f, 0.35f, 0.55f);
    glPushMatrix();
    glTranslatef(-0.95f, 1.35f, 1.21f);
    drawCube(0.55f, 0.55f, 0.10f);
    glTranslatef(1.9f, 0.0f, 0.0f);
    drawCube(0.55f, 0.55f, 0.10f);
    glPopMatrix();

    glPopMatrix();
}

void drawFenceRing(float radius, int posts) {
    glColor3f(0.58f, 0.30f, 0.16f);

    for (int i = 0; i < posts; ++i) {
        float a = (2.0f * kPi * i) / static_cast<float>(posts);
        float x = std::cos(a) * radius;
        float z = std::sin(a) * radius;

        glPushMatrix();
        glTranslatef(x, 0.45f, z);
        drawCube(0.11f, 0.90f, 0.11f);
        glPopMatrix();
    }

    for (int i = 0; i < posts; ++i) {
        int j = (i + 1) % posts;

        float a0 = (2.0f * kPi * i) / static_cast<float>(posts);
        float a1 = (2.0f * kPi * j) / static_cast<float>(posts);

        float x0 = std::cos(a0) * radius;
        float z0 = std::sin(a0) * radius;
        float x1 = std::cos(a1) * radius;
        float z1 = std::sin(a1) * radius;

        float mx = (x0 + x1) * 0.5f;
        float mz = (z0 + z1) * 0.5f;
        float dx = x1 - x0;
        float dz = z1 - z0;
        float len = std::sqrt(dx * dx + dz * dz);
        float angle = std::atan2(dz, dx) * (180.0f / kPi);

        for (int rail = 0; rail < 2; ++rail) {
            glPushMatrix();
            glTranslatef(mx, rail == 0 ? 0.42f : 0.68f, mz);
            glRotatef(angle, 0.0f, 1.0f, 0.0f);
            drawCube(len, 0.07f, 0.07f);
            glPopMatrix();
        }
    }
}

void drawWindmill() {
    glPushMatrix();

    glColor3f(0.93f, 0.93f, 0.92f);
    glPushMatrix();
    glTranslatef(0.0f, 0.05f, 0.0f);
    drawCylinderY(1.05f, 0.72f, 4.2f, 36);
    drawDiskY(1.05f, true, 36);
    glTranslatef(0.0f, 4.2f, 0.0f);
    drawDiskY(0.72f, false, 36);
    glPopMatrix();

    glColor3f(0.38f, 0.24f, 0.14f);
    glPushMatrix();
    glTranslatef(0.0f, 4.25f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glutSolidCone(0.95f, 1.4f, 36, 8);
    glPopMatrix();

    glColor3f(0.64f, 0.19f, 0.15f);
    glPushMatrix();
    glTranslatef(0.0f, 1.1f, 0.74f);
    drawCube(0.62f, 1.35f, 0.12f);
    glPopMatrix();

    glColor3f(0.23f, 0.33f, 0.54f);
    glPushMatrix();
    glTranslatef(-0.35f, 2.2f, 0.73f);
    drawCube(0.32f, 0.32f, 0.10f);
    glTranslatef(0.70f, 0.0f, 0.0f);
    drawCube(0.32f, 0.32f, 0.10f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 3.15f, 0.86f);
    glRotatef(gBladeAngle, 0.0f, 0.0f, 1.0f);

    glColor3f(0.28f, 0.20f, 0.13f);
    glutSolidSphere(0.14f, 24, 24);

    glColor3f(0.97f, 0.97f, 0.97f);
    for (int i = 0; i < 4; ++i) {
        glPushMatrix();
        glRotatef(i * 90.0f, 0.0f, 0.0f, 1.0f);
        glTranslatef(0.0f, 1.1f, 0.0f);
        drawCube(0.28f, 2.2f, 0.08f);

        glColor3f(0.25f, 0.25f, 0.25f);
        glTranslatef(0.0f, 0.72f, 0.0f);
        drawCube(0.28f, 0.12f, 0.10f);
        glColor3f(0.97f, 0.97f, 0.97f);
        glPopMatrix();
    }

    glPopMatrix();

    drawFenceRing(2.9f, 24);

    glPopMatrix();
}

void drawCloud(float x, float y, float z, float scale, float offset) {
    glPushMatrix();
    glTranslatef(x + offset, y, z);
    glScalef(scale, scale, scale);

    if (gRainMode) {
        glColor3f(0.64f, 0.68f, 0.74f);
    } else if (gDayMode) {
        glColor3f(0.98f, 0.98f, 0.99f);
    } else {
        glColor3f(0.72f, 0.74f, 0.82f);
    }

    glutSolidSphere(0.65f, 20, 20);
    glTranslatef(0.6f, 0.1f, 0.2f);
    glutSolidSphere(0.55f, 20, 20);
    glTranslatef(-1.2f, -0.05f, 0.05f);
    glutSolidSphere(0.50f, 20, 20);

    glPopMatrix();
}

void drawClouds() {
    drawCloud(-22.0f, 8.4f, -7.0f, 1.3f, gCloudOffsetNear);
    drawCloud(-14.0f, 9.1f, -11.8f, 1.5f, gCloudOffsetNear);
    drawCloud(-6.0f, 8.7f, -9.2f, 1.2f, gCloudOffsetNear);
    drawCloud(2.0f, 9.0f, -8.5f, 1.4f, gCloudOffsetNear);
    drawCloud(10.5f, 8.8f, -10.2f, 1.5f, gCloudOffsetNear);
    drawCloud(18.0f, 9.3f, -12.5f, 1.6f, gCloudOffsetNear);

    drawCloud(-26.0f, 10.3f, -18.0f, 1.7f, gCloudOffsetFar);
    drawCloud(-15.0f, 10.8f, -20.5f, 1.9f, gCloudOffsetFar);
    drawCloud(-2.0f, 10.1f, -19.5f, 1.6f, gCloudOffsetFar);
    drawCloud(12.0f, 10.5f, -21.0f, 1.8f, gCloudOffsetFar);
    drawCloud(24.0f, 10.2f, -19.0f, 1.6f, gCloudOffsetFar);
}

void updateRain(float dt) {
    if (!gRainMode) {
        return;
    }

    for (int i = 0; i < kRainDropCount; ++i) {
        RainDrop& drop = gRainDrops[i];
        drop.x += 1.45f * dt;
        drop.y -= drop.speed * dt;

        if (drop.y < 0.15f || drop.x > 34.0f) {
            resetRaindrop(drop, true);
        }
    }
}

void drawRain() {
    if (!gRainMode) {
        return;
    }

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(0.72f, 0.80f, 1.0f, 0.72f);
    glLineWidth(1.2f);
    glBegin(GL_LINES);
    for (int i = 0; i < kRainDropCount; ++i) {
        const RainDrop& drop = gRainDrops[i];
        glVertex3f(drop.x, drop.y, drop.z);
        glVertex3f(drop.x + 0.08f, drop.y - 0.55f, drop.z + 0.04f);
    }
    glEnd();
    glLineWidth(1.0f);

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void updateLights() {
    GLfloat ambient[4];
    GLfloat diffuse[4];
    GLfloat specular[4] = {0.80f, 0.80f, 0.80f, 1.0f};
    GLfloat sunPos[4];

    if (gDayMode) {
        GLfloat a[4] = {0.34f, 0.34f, 0.34f, 1.0f};
        GLfloat d[4] = {0.96f, 0.95f, 0.90f, 1.0f};
        GLfloat p[4] = {16.0f, 24.0f, 10.0f, 1.0f};
        std::memcpy(ambient, a, sizeof(ambient));
        std::memcpy(diffuse, d, sizeof(diffuse));
        std::memcpy(sunPos, p, sizeof(sunPos));
    } else {
        GLfloat a[4] = {0.11f, 0.11f, 0.18f, 1.0f};
        GLfloat d[4] = {0.30f, 0.34f, 0.50f, 1.0f};
        GLfloat p[4] = {12.0f, 18.0f, -16.0f, 1.0f};
        std::memcpy(ambient, a, sizeof(ambient));
        std::memcpy(diffuse, d, sizeof(diffuse));
        std::memcpy(sunPos, p, sizeof(sunPos));
    }

    if (gRainMode) {
        for (int i = 0; i < 3; ++i) {
            ambient[i] *= 1.15f;
            diffuse[i] *= 0.58f;
        }
    }

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, sunPos);

    if (gDayMode) {
        glDisable(GL_LIGHT1);
    } else {
        GLfloat lampDiffuse[4] = {1.00f, 0.82f, 0.56f, 1.0f};
        GLfloat lampAmbient[4] = {0.14f, 0.11f, 0.08f, 1.0f};
        GLfloat lampPos[4] = {0.0f, 1.2f, 1.4f, 1.0f};

        if (gRainMode) {
            lampDiffuse[0] = 0.85f;
            lampDiffuse[1] = 0.72f;
            lampDiffuse[2] = 0.50f;
        }

        glLightfv(GL_LIGHT1, GL_AMBIENT, lampAmbient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, lampDiffuse);
        glLightfv(GL_LIGHT1, GL_POSITION, lampPos);
        glEnable(GL_LIGHT1);
    }
}

void renderBitmapText(float x, float y, const char* text) {
    glRasterPos2f(x, y);
    for (const char* p = text; *p; ++p) {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *p);
    }
}

void drawOverlay() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, static_cast<GLdouble>(gWindowWidth), 0.0, static_cast<GLdouble>(gWindowHeight));

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glColor3f(1.0f, 1.0f, 1.0f);
    renderBitmapText(14.0f, static_cast<float>(gWindowHeight - 22), "WASD: move | Q/E: up/down | IJKL: look | Space: blades | N: day/night | P: rain | R: reset");

    if (gDayMode) {
        renderBitmapText(14.0f, static_cast<float>(gWindowHeight - 42), "Environment: DAY");
    } else {
        renderBitmapText(14.0f, static_cast<float>(gWindowHeight - 42), "Environment: NIGHT");
    }

    if (gRainMode) {
        renderBitmapText(14.0f, static_cast<float>(gWindowHeight - 62), "Weather: RAIN");
    } else {
        renderBitmapText(14.0f, static_cast<float>(gWindowHeight - 62), "Weather: CLEAR");
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void display() {
    if (gRainMode && gDayMode) {
        glClearColor(0.44f, 0.51f, 0.60f, 1.0f);
    } else if (gRainMode) {
        glClearColor(0.05f, 0.07f, 0.12f, 1.0f);
    } else if (gDayMode) {
        glClearColor(0.53f, 0.79f, 0.93f, 1.0f);
    } else {
        glClearColor(0.06f, 0.07f, 0.14f, 1.0f);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    float yawRad = degToRad(gCamera.yaw);
    float pitchRad = degToRad(gCamera.pitch);

    float dirX = std::cos(pitchRad) * std::sin(yawRad);
    float dirY = std::sin(pitchRad);
    float dirZ = -std::cos(pitchRad) * std::cos(yawRad);

    gluLookAt(gCamera.x, gCamera.y, gCamera.z,
              gCamera.x + dirX, gCamera.y + dirY, gCamera.z + dirZ,
              0.0f, 1.0f, 0.0f);

    updateLights();

    drawGround();
    drawHills();
    drawWindmill();

    drawHouse(-8.5f, -6.0f, 25.0f, 1.0f);
    drawHouse(9.5f, -8.0f, -20.0f, 0.9f);

    drawTree(-5.0f, 3.5f, 1.0f);
    drawTree(4.5f, 4.0f, 1.1f);
    drawTree(-10.0f, -2.5f, 1.2f);
    drawTree(8.8f, -1.2f, 1.0f);
    drawTree(2.0f, -10.5f, 1.25f);

    drawClouds();
    drawRain();
    drawOverlay();

    glutSwapBuffers();
}

void resetCamera() {
    gCamera.x = 0.0f;
    gCamera.y = 3.2f;
    gCamera.z = 11.0f;
    gCamera.yaw = 0.0f;
    gCamera.pitch = -8.0f;
}

void updateMovement(float dt) {
    const float moveSpeed = 5.0f;
    const float lookSpeed = 70.0f;

    float yawRad = degToRad(gCamera.yaw);
    float forwardX = std::sin(yawRad);
    float forwardZ = -std::cos(yawRad);
    float rightX = std::cos(yawRad);
    float rightZ = std::sin(yawRad);

    if (gKeyDown['w']) {
        gCamera.x += forwardX * moveSpeed * dt;
        gCamera.z += forwardZ * moveSpeed * dt;
    }
    if (gKeyDown['s']) {
        gCamera.x -= forwardX * moveSpeed * dt;
        gCamera.z -= forwardZ * moveSpeed * dt;
    }
    if (gKeyDown['a']) {
        gCamera.x -= rightX * moveSpeed * dt;
        gCamera.z -= rightZ * moveSpeed * dt;
    }
    if (gKeyDown['d']) {
        gCamera.x += rightX * moveSpeed * dt;
        gCamera.z += rightZ * moveSpeed * dt;
    }
    if (gKeyDown['q']) {
        gCamera.y += moveSpeed * dt;
    }
    if (gKeyDown['e']) {
        gCamera.y -= moveSpeed * dt;
        if (gCamera.y < 0.7f) {
            gCamera.y = 0.7f;
        }
    }

    if (gKeyDown['j']) {
        gCamera.yaw -= lookSpeed * dt;
    }
    if (gKeyDown['l']) {
        gCamera.yaw += lookSpeed * dt;
    }
    if (gKeyDown['i']) {
        gCamera.pitch += lookSpeed * dt;
    }
    if (gKeyDown['k']) {
        gCamera.pitch -= lookSpeed * dt;
    }

    if (gCamera.pitch > 80.0f) {
        gCamera.pitch = 80.0f;
    }
    if (gCamera.pitch < -80.0f) {
        gCamera.pitch = -80.0f;
    }
}

void timer(int) {
    const float dt = 1.0f / 60.0f;

    updateMovement(dt);

    if (gAnimateBlades) {
        gBladeAngle += 72.0f * dt;
        if (gBladeAngle >= 360.0f) {
            gBladeAngle -= 360.0f;
        }
    }

    gCloudOffsetNear += 0.55f * dt;
    if (gCloudOffsetNear > 30.0f) {
        gCloudOffsetNear = -30.0f;
    }

    gCloudOffsetFar += 0.35f * dt;
    if (gCloudOffsetFar > 34.0f) {
        gCloudOffsetFar = -34.0f;
    }

    updateRain(dt);

    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

void reshape(int width, int height) {
    if (height <= 0) {
        height = 1;
    }

    gWindowWidth = width;
    gWindowHeight = height;

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, static_cast<double>(width) / static_cast<double>(height), 0.1, 200.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboardDown(unsigned char key, int, int) {
    unsigned char lower = static_cast<unsigned char>(std::tolower(key));

    if (lower == 27) {
        if (gQuadric) {
            gluDeleteQuadric(gQuadric);
            gQuadric = nullptr;
        }
        std::exit(0);
    }

    gKeyDown[lower] = true;

    if (lower == ' ') {
        gAnimateBlades = !gAnimateBlades;
    } else if (lower == 'n') {
        gDayMode = !gDayMode;
    } else if (lower == 'p') {
        gRainMode = !gRainMode;
    } else if (lower == 'r') {
        resetCamera();
    }
}

void keyboardUp(unsigned char key, int, int) {
    unsigned char lower = static_cast<unsigned char>(std::tolower(key));
    gKeyDown[lower] = false;
}

void init() {
    std::srand(7);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    GLfloat specular[4] = {0.35f, 0.35f, 0.35f, 1.0f};
    GLfloat shininess[1] = {24.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);

    glShadeModel(GL_SMOOTH);

    gQuadric = gluNewQuadric();
    gluQuadricNormals(gQuadric, GLU_SMOOTH);

    initRainSystem();
}

}  // namespace

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(gWindowWidth, gWindowHeight);
    glutCreateWindow("Complete 3D Windmill Project");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutTimerFunc(16, timer, 0);

    glutMainLoop();
    return 0;
}

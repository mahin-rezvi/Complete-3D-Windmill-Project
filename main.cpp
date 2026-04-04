// ============================================================================
// SEGMENT 1: CORE DATA + SHARED HELPERS
// ============================================================================

#include <GL/glut.h>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

constexpr float kPi = 3.14159265359f;
constexpr int kRainDropCount = 900;
constexpr int kStarCount = 140;
constexpr float kSunX = 0.0f;
constexpr float kSunY = 18.0f;
constexpr float kSunZ = 18.0f;
constexpr float kMoonX = 0.0f;
constexpr float kMoonY = 18.0f;
constexpr float kMoonZ = -18.0f;

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
float gSceneTime = 0.0f;

int gWindowWidth = 1280;
int gWindowHeight = 720;

struct RainDrop {
    float x;
    float y;
    float z;
    float speed;
};

RainDrop gRainDrops[kRainDropCount];

struct Star {
    float x;
    float y;
    float size;
    float phase;
};

Star gStars[kStarCount];

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

void initStars() {
    for (int i = 0; i < kStarCount; ++i) {
        gStars[i].x = randomRange(-0.98f, 0.98f);
        gStars[i].y = randomRange(0.10f, 0.98f);
        gStars[i].size = randomRange(1.0f, 2.5f);
        gStars[i].phase = randomRange(0.0f, 2.0f * kPi);
    }
}

void setMaterial(float specularStrength, float shininess) {
    GLfloat specular[4] = {specularStrength, specularStrength, specularStrength, 1.0f};
    GLfloat shiny[1] = {shininess};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shiny);
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

// ============================================================================
// SEGMENT 2: SKY + TERRAIN FOUNDATION
// ============================================================================

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

void drawQuad3D(float ax, float ay, float az,
                float bx, float by, float bz,
                float cx, float cy, float cz,
                float dx, float dy, float dz) {
    drawTriangle3D(ax, ay, az, bx, by, bz, cx, cy, cz);
    drawTriangle3D(ax, ay, az, cx, cy, cz, dx, dy, dz);
}

void drawMountainPyramid(float halfWidth, float height, float halfDepth) {
    glBegin(GL_TRIANGLES);
    drawTriangle3D(-halfWidth, 0.0f, halfDepth, halfWidth, 0.0f, halfDepth, 0.0f, height, 0.0f);
    drawTriangle3D(halfWidth, 0.0f, halfDepth, halfWidth, 0.0f, -halfDepth, 0.0f, height, 0.0f);
    drawTriangle3D(halfWidth, 0.0f, -halfDepth, -halfWidth, 0.0f, -halfDepth, 0.0f, height, 0.0f);
    drawTriangle3D(-halfWidth, 0.0f, -halfDepth, -halfWidth, 0.0f, halfDepth, 0.0f, height, 0.0f);
    glEnd();
}

void drawScreenGlow(float cx, float cy, float radius,
                    float r, float g, float b,
                    float alphaCenter, int slices = 40) {
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(r, g, b, alphaCenter);
    glVertex2f(cx, cy);
    glColor4f(r, g, b, 0.0f);
    for (int i = 0; i <= slices; ++i) {
        float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(slices);
        glVertex2f(cx + std::cos(angle) * radius, cy + std::sin(angle) * radius);
    }
    glEnd();
}

void drawSkyGradient() {
    GLfloat top[3];
    GLfloat horizon[3];

    if (gRainMode && gDayMode) {
        top[0] = 0.36f; top[1] = 0.42f; top[2] = 0.50f;
        horizon[0] = 0.62f; horizon[1] = 0.68f; horizon[2] = 0.74f;
    } else if (gRainMode) {
        top[0] = 0.04f; top[1] = 0.06f; top[2] = 0.11f;
        horizon[0] = 0.18f; horizon[1] = 0.21f; horizon[2] = 0.30f;
    } else if (gDayMode) {
        top[0] = 0.33f; top[1] = 0.58f; top[2] = 0.88f;
        horizon[0] = 0.80f; horizon[1] = 0.89f; horizon[2] = 0.97f;
    } else {
        top[0] = 0.03f; top[1] = 0.06f; top[2] = 0.14f;
        horizon[0] = 0.16f; horizon[1] = 0.20f; horizon[2] = 0.32f;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBegin(GL_QUADS);
    glColor3f(horizon[0], horizon[1], horizon[2]);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f(1.0f, -1.0f);
    glColor3f(top[0], top[1], top[2]);
    glVertex2f(1.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!gDayMode && !gRainMode) {
        for (int i = 0; i < kStarCount; ++i) {
            const Star& star = gStars[i];
            float twinkle = 0.45f + 0.55f * std::sin(gSceneTime * 1.6f + star.phase);
            float alpha = 0.20f + 0.65f * twinkle;
            glPointSize(star.size);
            glBegin(GL_POINTS);
            glColor4f(0.92f, 0.94f, 1.0f, alpha);
            glVertex2f(star.x, star.y);
            glEnd();
        }
    }

    glDisable(GL_BLEND);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

void drawSun() {
    if (!gDayMode) {
        return;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();
    glTranslatef(kSunX, kSunY, kSunZ);
    if (gRainMode) {
        glColor4f(0.96f, 0.94f, 0.86f, 0.22f);
        glutSolidSphere(2.20f, 28, 28);
        glColor4f(0.98f, 0.96f, 0.90f, 0.52f);
        glutSolidSphere(1.05f, 24, 24);
    } else {
        glColor4f(1.00f, 0.90f, 0.62f, 0.36f);
        glutSolidSphere(4.20f, 34, 34);
        glColor4f(1.00f, 0.95f, 0.78f, 0.92f);
        glutSolidSphere(1.85f, 28, 28);
    }
    glPopMatrix();

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}

void drawMoon() {
    if (gDayMode) {
        return;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPushMatrix();
    glTranslatef(kMoonX, kMoonY, kMoonZ);
    if (gRainMode) {
        glColor4f(0.84f, 0.88f, 0.98f, 0.18f);
        glutSolidSphere(1.85f, 24, 24);
        glColor4f(0.90f, 0.93f, 1.00f, 0.44f);
        glutSolidSphere(0.88f, 22, 22);
    } else {
        glColor4f(0.80f, 0.86f, 1.00f, 0.26f);
        glutSolidSphere(2.05f, 26, 26);
        glColor4f(0.93f, 0.96f, 1.00f, 0.82f);
        glutSolidSphere(0.98f, 24, 24);
    }
    glPopMatrix();

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);
}

void updateFog() {
    GLfloat fogColor[4];
    if (gRainMode && gDayMode) {
        fogColor[0] = 0.46f; fogColor[1] = 0.54f; fogColor[2] = 0.62f; fogColor[3] = 1.0f;
    } else if (gRainMode) {
        fogColor[0] = 0.10f; fogColor[1] = 0.12f; fogColor[2] = 0.18f; fogColor[3] = 1.0f;
    } else if (gDayMode) {
        fogColor[0] = 0.77f; fogColor[1] = 0.86f; fogColor[2] = 0.94f; fogColor[3] = 1.0f;
    } else {
        fogColor[0] = 0.11f; fogColor[1] = 0.14f; fogColor[2] = 0.23f; fogColor[3] = 1.0f;
    }

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glHint(GL_FOG_HINT, GL_NICEST);
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_START, gRainMode ? 11.0f : (gDayMode ? 26.0f : 20.0f));
    glFogf(GL_FOG_END, gRainMode ? 62.0f : (gDayMode ? 120.0f : 88.0f));
}

void drawGroundShadow(float x, float z, float radiusX, float radiusZ, float alpha) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float adjustedAlpha = alpha;
    if (gDayMode) {
        adjustedAlpha *= 1.40f;
    } else {
        adjustedAlpha *= 0.85f;
    }
    if (gRainMode) {
        adjustedAlpha *= 0.65f;
    }
    if (adjustedAlpha > 1.0f) {
        adjustedAlpha = 1.0f;
    }
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.05f, 0.05f, 0.05f, adjustedAlpha);
    glVertex3f(x, 0.015f, z);
    glColor4f(0.05f, 0.05f, 0.05f, 0.0f);
    const int segments = 28;
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segments);
        glVertex3f(x + std::cos(angle) * radiusX, 0.015f, z + std::sin(angle) * radiusZ);
    }
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void drawGround() {
    setMaterial(gRainMode ? 0.20f : 0.06f, gRainMode ? 34.0f : 8.0f);

    const int extent = 48;
    for (int x = -extent; x < extent; ++x) {
        for (int z = -extent; z < extent; ++z) {
            float fx = static_cast<float>(x);
            float fz = static_cast<float>(z);
            float noise = 0.5f + 0.5f * (std::sin(fx * 0.37f + fz * 0.13f) *
                                         std::cos(fz * 0.41f - fx * 0.16f));
            float shade = 0.80f + noise * 0.25f;
            if (gRainMode) {
                shade *= 0.86f;
            }

            glColor3f(0.23f * shade, 0.50f * shade, 0.22f * shade);

            glBegin(GL_QUADS);
            glNormal3f(0.0f, 1.0f, 0.0f);
            glVertex3f(fx, 0.0f, fz);
            glVertex3f(fx + 1.0f, 0.0f, fz);
            glVertex3f(fx + 1.0f, 0.0f, fz + 1.0f);
            glVertex3f(fx, 0.0f, fz + 1.0f);
            glEnd();
        }
    }

    setMaterial(gRainMode ? 0.14f : 0.03f, gRainMode ? 24.0f : 6.0f);
    glColor3f(gRainMode ? 0.31f : 0.40f, gRainMode ? 0.30f : 0.36f, gRainMode ? 0.29f : 0.30f);
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
        setMaterial(0.08f, 14.0f);
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

        setMaterial(0.40f, 60.0f);
        glPushMatrix();
        glTranslatef(hill.x, hill.y + hill.height * 0.60f, hill.z);
        glRotatef(hill.yaw, 0.0f, 1.0f, 0.0f);
        glColor3f(iceR, iceG, iceB);
        drawMountainPyramid(hill.width * 0.34f, hill.height * 0.40f, hill.depth * 0.34f);
        glPopMatrix();
    }
}

// ============================================================================
// SEGMENT 3: WORLD STRUCTURES (TREE / HOUSE / WINDMILL)
// ============================================================================

void drawTree(float x, float z, float scale) {
    glPushMatrix();
    glTranslatef(x, 0.0f, z);
    glScalef(scale, scale, scale);

    setMaterial(0.12f, 14.0f);
    glColor3f(0.42f, 0.27f, 0.15f);
    glPushMatrix();
    drawCylinderY(0.20f, 0.13f, 1.95f, 24);
    glTranslatef(0.0f, 1.95f, 0.0f);
    drawDiskY(0.13f);
    glPopMatrix();

    setMaterial(0.10f, 10.0f);
    glColor3f(0.38f, 0.24f, 0.14f);
    glPushMatrix();
    glTranslatef(0.0f, 1.45f, 0.0f);
    glRotatef(36.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(24.0f, 1.0f, 0.0f, 0.0f);
    drawCylinderY(0.07f, 0.04f, 0.95f, 16);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 1.35f, 0.0f);
    glRotatef(-34.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(-18.0f, 1.0f, 0.0f, 0.0f);
    drawCylinderY(0.065f, 0.035f, 0.90f, 16);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 1.68f, -0.02f);
    glRotatef(70.0f, 1.0f, 0.0f, 0.0f);
    drawCylinderY(0.055f, 0.03f, 0.75f, 16);
    glPopMatrix();

    setMaterial(0.05f, 8.0f);
    glPushMatrix();
    glTranslatef(0.0f, 1.80f, 0.0f);
    glColor3f(0.12f, 0.38f, 0.14f);
    glutSolidSphere(0.28f, 16, 16);

    glTranslatef(0.0f, 0.20f, 0.0f);
    glColor3f(0.13f, 0.40f, 0.15f);
    glutSolidSphere(0.78f, 20, 20);

    glTranslatef(-0.52f, -0.18f, 0.30f);
    glColor3f(0.11f, 0.46f, 0.18f);
    glutSolidSphere(0.58f, 18, 18);

    glTranslatef(0.98f, 0.02f, -0.56f);
    glColor3f(0.10f, 0.43f, 0.16f);
    glutSolidSphere(0.54f, 18, 18);

    glTranslatef(-0.26f, 0.52f, 0.36f);
    glColor3f(0.15f, 0.49f, 0.20f);
    glutSolidSphere(0.46f, 16, 16);

    glTranslatef(-0.34f, -0.04f, -0.58f);
    glColor3f(0.09f, 0.37f, 0.14f);
    glutSolidSphere(0.42f, 16, 16);
    glPopMatrix();

    glPopMatrix();
}

void drawGarageCar() {
    glPushMatrix();

    float bodyR = gRainMode ? 0.55f : 0.78f;
    float bodyG = gRainMode ? 0.17f : 0.20f;
    float bodyB = gRainMode ? 0.16f : 0.18f;
    if (!gDayMode) {
        bodyR *= 0.90f;
        bodyG *= 0.92f;
        bodyB *= 1.06f;
    }

    setMaterial(gRainMode ? 0.30f : 0.22f, gRainMode ? 42.0f : 32.0f);
    glColor3f(bodyR, bodyG, bodyB);
    glPushMatrix();
    glTranslatef(0.0f, 0.28f, 0.0f);
    drawCube(2.20f, 0.56f, 1.16f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-0.12f, 0.58f, 0.0f);
    drawCube(1.12f, 0.34f, 0.94f);
    glPopMatrix();

    setMaterial(0.50f, 86.0f);
    glColor3f(gDayMode ? 0.30f : 0.20f, gDayMode ? 0.48f : 0.30f, gDayMode ? 0.64f : 0.42f);
    glPushMatrix();
    glTranslatef(-0.34f, 0.60f, 0.0f);
    drawCube(0.50f, 0.18f, 0.88f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.18f, 0.56f, 0.0f);
    drawCube(0.28f, 0.14f, 0.84f);
    glPopMatrix();

    setMaterial(0.05f, 10.0f);
    glColor3f(0.08f, 0.08f, 0.08f);
    const float wheelCenters[4][3] = {
        {-0.68f, 0.18f, -0.62f},
        {-0.68f, 0.18f, 0.62f},
        {0.68f, 0.18f, -0.62f},
        {0.68f, 0.18f, 0.62f},
    };

    for (const auto& wheel : wheelCenters) {
        glPushMatrix();
        glTranslatef(wheel[0], wheel[1], wheel[2]);
        glutSolidTorus(0.05f, 0.17f, 12, 18);
        glPopMatrix();
    }

    setMaterial(0.42f, 72.0f);
    glColor3f(0.96f, 0.92f, 0.66f);
    glPushMatrix();
    glTranslatef(-1.04f, 0.24f, -0.30f);
    drawCube(0.06f, 0.14f, 0.16f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-1.04f, 0.24f, 0.30f);
    drawCube(0.06f, 0.14f, 0.16f);
    glPopMatrix();

    glColor3f(0.78f, 0.20f, 0.16f);
    glPushMatrix();
    glTranslatef(1.04f, 0.24f, -0.30f);
    drawCube(0.06f, 0.14f, 0.16f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(1.04f, 0.24f, 0.30f);
    drawCube(0.06f, 0.14f, 0.16f);
    glPopMatrix();

    glPopMatrix();
}

void drawHouse(float x, float z, float rotY, float scale, bool addGarage = false) {
    glPushMatrix();
    glTranslatef(x, 0.0f, z);
    glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glScalef(scale, scale, scale);

    setMaterial(0.08f, 10.0f);
    glColor3f(0.78f, 0.70f, 0.57f);
    glPushMatrix();
    glTranslatef(0.0f, 1.1f, 0.0f);
    drawCube(3.4f, 2.2f, 2.4f);
    glPopMatrix();

    setMaterial(0.12f, 20.0f);
    glColor3f(0.52f, 0.16f, 0.14f);
    const float roofHalfW = 1.90f;
    const float roofHalfD = 1.35f;
    const float ridgeHalfW = 1.05f;
    const float roofBaseY = 2.22f;
    const float roofRidgeY = 3.05f;

    glBegin(GL_TRIANGLES);
    drawQuad3D(-roofHalfW, roofBaseY, roofHalfD,
                roofHalfW, roofBaseY, roofHalfD,
                ridgeHalfW, roofRidgeY, 0.0f,
               -ridgeHalfW, roofRidgeY, 0.0f);
    drawQuad3D( roofHalfW, roofBaseY, -roofHalfD,
               -roofHalfW, roofBaseY, -roofHalfD,
               -ridgeHalfW, roofRidgeY, 0.0f,
                ridgeHalfW, roofRidgeY, 0.0f);
    drawTriangle3D(roofHalfW, roofBaseY,  roofHalfD,
                   roofHalfW, roofBaseY, -roofHalfD,
                   ridgeHalfW, roofRidgeY, 0.0f);
    drawTriangle3D(-roofHalfW, roofBaseY, -roofHalfD,
                   -roofHalfW, roofBaseY,  roofHalfD,
                   -ridgeHalfW, roofRidgeY, 0.0f);
    glEnd();

    setMaterial(0.05f, 8.0f);
    glColor3f(0.30f, 0.18f, 0.10f);
    glPushMatrix();
    glTranslatef(0.0f, 0.75f, 1.21f);
    drawCube(0.60f, 1.10f, 0.10f);
    glPopMatrix();

    setMaterial(0.45f, 64.0f);
    glColor3f(0.20f, 0.35f, 0.55f);
    glPushMatrix();
    glTranslatef(-0.95f, 1.35f, 1.21f);
    drawCube(0.55f, 0.55f, 0.10f);
    glTranslatef(1.9f, 0.0f, 0.0f);
    drawCube(0.55f, 0.55f, 0.10f);
    glPopMatrix();

    if (addGarage) {
        const float garageX = 4.35f;
        const float garageZ = 0.0f;
        const float garageWidth = 3.20f;
        const float garageHeight = 2.20f;
        const float garageDepth = 2.90f;
        const float wallThickness = 0.12f;

        setMaterial(gRainMode ? 0.14f : 0.08f, gRainMode ? 18.0f : 10.0f);
        glColor3f(gRainMode ? 0.58f : 0.74f,
                  gRainMode ? 0.58f : 0.72f,
                  gRainMode ? 0.60f : 0.70f);

        glPushMatrix();
        glTranslatef(garageX, 0.03f, garageZ);
        drawCube(garageWidth, 0.06f, garageDepth);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(garageX, garageHeight * 0.5f, -garageDepth * 0.5f + wallThickness * 0.5f);
        drawCube(garageWidth, garageHeight, wallThickness);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(garageX - garageWidth * 0.5f + wallThickness * 0.5f,
                     garageHeight * 0.5f,
                     0.0f);
        drawCube(wallThickness, garageHeight, garageDepth);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(garageX + garageWidth * 0.5f - wallThickness * 0.5f,
                     garageHeight * 0.5f,
                     0.0f);
        drawCube(wallThickness, garageHeight, garageDepth);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(garageX, garageHeight - wallThickness * 0.5f, 0.0f);
        drawCube(garageWidth, wallThickness, garageDepth);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(garageX, 0.0f, 0.10f);
        drawGarageCar();
        glPopMatrix();
    }


    glPopMatrix();
}

void drawFenceRing(float radius, int posts) {
    setMaterial(0.10f, 12.0f);
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

    setMaterial(0.20f, 30.0f);
    glColor3f(0.93f, 0.93f, 0.92f);
    glPushMatrix();
    glTranslatef(0.0f, 0.05f, 0.0f);
    drawCylinderY(1.05f, 0.72f, 4.2f, 36);
    drawDiskY(1.05f, true, 36);
    glTranslatef(0.0f, 4.2f, 0.0f);
    drawDiskY(0.72f, false, 36);
    glPopMatrix();

    float coneR = gDayMode ? 0.52f : 0.44f;
    float coneG = gDayMode ? 0.32f : 0.27f;
    float coneB = gDayMode ? 0.19f : 0.20f;
    if (gRainMode) {
        coneR *= 0.92f;
        coneG *= 0.92f;
        coneB *= 0.95f;
    }
    setMaterial(gDayMode ? 0.26f : 0.22f, gDayMode ? 42.0f : 34.0f);
    glColor3f(coneR, coneG, coneB);
    glPushMatrix();
    glTranslatef(0.0f, 4.25f, 0.0f);
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glutSolidCone(0.95f, 1.4f, 36, 8);
    glPopMatrix();

    setMaterial(0.08f, 10.0f);
    glColor3f(0.64f, 0.19f, 0.15f);
    glPushMatrix();
    glTranslatef(0.0f, 1.1f, 0.74f);
    drawCube(0.62f, 1.35f, 0.12f);
    glPopMatrix();

    setMaterial(0.38f, 60.0f);
    glColor3f(0.23f, 0.33f, 0.54f);
    glPushMatrix();
    glTranslatef(-0.35f, 2.2f, 0.73f);
    drawCube(0.32f, 0.32f, 0.10f);
    glTranslatef(0.70f, 0.0f, 0.0f);
    drawCube(0.32f, 0.32f, 0.10f);
    glPopMatrix();

    setMaterial(0.15f, 5.0f);
    glColor3f(0.35f, 0.25f, 0.15f);
    const float hubY = 3.15f;
    const float bodyFrontZ = 0.80f;
    const float hubZ = 1.35f;
    const float shaftLength = (hubZ - bodyFrontZ);

    glPushMatrix();
    glTranslatef(0.0f, hubY, bodyFrontZ);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    drawCylinderY(0.10f, 0.10f, shaftLength, 24);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 3.15f, 1.35f);
    glRotatef(gBladeAngle, 0.0f, 0.0f, 1.0f);

    setMaterial(0.18f, 24.0f);
    glColor3f(0.28f, 0.20f, 0.13f);
    glutSolidSphere(0.14f, 24, 24);

    setMaterial(0.24f, 36.0f);
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

// ============================================================================
// SEGMENT 4: ATMOSPHERE + LIGHTING + HUD
// ============================================================================

void drawCloud(float x, float y, float z, float scale, float offset) {
    glPushMatrix();
    glTranslatef(x + offset, y, z);
    glScalef(scale, scale, scale);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (gRainMode) {
        glColor4f(0.64f, 0.68f, 0.74f, 0.88f);
    } else if (gDayMode) {
        glColor4f(0.98f, 0.98f, 0.99f, 0.90f);
    } else {
        glColor4f(0.72f, 0.74f, 0.82f, 0.86f);
    }

    glutSolidSphere(0.65f, 20, 20);
    glTranslatef(0.6f, 0.1f, 0.2f);
    glutSolidSphere(0.55f, 20, 20);
    glTranslatef(-1.2f, -0.05f, 0.05f);
    glutSolidSphere(0.50f, 20, 20);

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);

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
        GLfloat a[4] = {0.20f, 0.20f, 0.19f, 1.0f};
        GLfloat d[4] = {1.05f, 1.00f, 0.92f, 1.0f};
        GLfloat p[4] = {kSunX, kSunY, kSunZ, 0.0f};
        std::memcpy(ambient, a, sizeof(ambient));
        std::memcpy(diffuse, d, sizeof(diffuse));
        std::memcpy(sunPos, p, sizeof(sunPos));
    } else {
        GLfloat a[4] = {0.11f, 0.11f, 0.18f, 1.0f};
        GLfloat d[4] = {0.30f, 0.34f, 0.50f, 1.0f};
        GLfloat p[4] = {kMoonX, kMoonY, kMoonZ, 0.0f};
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

    glDisable(GL_LIGHT1);
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
    glDisable(GL_FOG);
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
    glEnable(GL_FOG);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ============================================================================
// SEGMENT 5: FRAME LOOP + INPUT + APP BOOTSTRAP
// ============================================================================

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
    drawSkyGradient();
    glLoadIdentity();

    float yawRad = degToRad(gCamera.yaw);
    float pitchRad = degToRad(gCamera.pitch);

    float dirX = std::cos(pitchRad) * std::sin(yawRad);
    float dirY = std::sin(pitchRad);
    float dirZ = -std::cos(pitchRad) * std::cos(yawRad);

    gluLookAt(gCamera.x, gCamera.y, gCamera.z,
              gCamera.x + dirX, gCamera.y + dirY, gCamera.z + dirZ,
              0.0f, 1.0f, 0.0f);

    drawSun();
    drawMoon();
    updateLights();
    updateFog();

    drawGround();
    drawHills();

    drawGroundShadow(0.0f, 0.0f, 3.0f, 2.3f, 0.22f);
    drawGroundShadow(-8.5f, -6.0f, 2.8f, 1.9f, 0.20f);
    drawGroundShadow(9.5f, -8.0f, 2.5f, 1.8f, 0.18f);
    drawGroundShadow(-5.0f, 3.5f, 1.3f, 1.0f, 0.17f);
    drawGroundShadow(4.5f, 4.0f, 1.4f, 1.1f, 0.17f);
    drawGroundShadow(-10.0f, -2.5f, 1.5f, 1.2f, 0.18f);
    drawGroundShadow(8.8f, -1.2f, 1.3f, 1.0f, 0.17f);
    drawGroundShadow(2.0f, -10.5f, 1.5f, 1.2f, 0.18f);

    drawWindmill();

    drawHouse(-8.5f, -6.0f, 25.0f, 1.0f, true);
    drawHouse(9.5f, -8.0f, -20.0f, 0.9f);

    drawTree(-5.0f, 3.5f, 1.0f);
    drawTree(4.5f, 4.0f, 1.1f);
    drawTree(6.5f, 4.0f, 1.1f);
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
    gSceneTime += dt;

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
    initStars();
}

}  // namespace

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(gWindowWidth, gWindowHeight);
    glutCreateWindow("Complete 3D Windmill Project Made By, Mahin, Anik, Mithila, Illin, faria");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutTimerFunc(16, timer, 0);

    glutMainLoop();
    return 0;
}

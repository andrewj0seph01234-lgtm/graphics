#define GLUT_DISABLE_ATEXIT_HACK
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <ctime>
#include <chrono>
#include <glut.h>

#define DEG2RAD(a) (a *0.0174532925f)

///////////////
// Globals (player/camera kept from your version)
///////////////
float playerX = 5.0f;
float playerY = 0.05f / 2 + 0.1f;
float playerZ = 5.0f;
float playerAngleY = 0.0f; // rotation to face movement
float playerPitch = 0.0f; // tilt on x-axis when airborne
float playerSpeed = 0.12f; // movement speed

float prevPlayerX = 5.0f;
float prevPlayerY = 0.05f / 2 + 0.1f;
float prevPlayerZ = 5.0f;

float groundY = 0.05f / 2 + 0.1f; // ground level
float maxPlayerY = 3.0f; // maximum allowed height

int collectedGoals = 0;
int totalGoals = 3;

int cameraViewMode = 1; //1=behind,2=top,3=side

// Timer
float gameTime = 90.0f; // seconds
bool gameOver = false;
bool gameWin = false;
std::chrono::steady_clock::time_point lastTime;

///////////////
// Simple Vector
///////////////
class Vector3f {
public:
	float x, y, z;
	Vector3f(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f) {
		x = _x; y = _y; z = _z;
	}
	Vector3f operator+(Vector3f v) const { return Vector3f(x + v.x, y + v.y, z + v.z); }
	Vector3f operator-(Vector3f v) const { return Vector3f(x - v.x, y - v.y, z - v.z); }
	Vector3f operator*(float n) const { return Vector3f(x * n, y * n, z * n); }
	Vector3f operator/(float n) const { return Vector3f(x / n, y / n, z / n); }
	Vector3f unit() const {
		float L = sqrtf(x * x + y * y + z * z);
		if (L == 0) return Vector3f(0, 0, 0);
		return *this / L;
	}
	Vector3f cross(Vector3f v) const {
		return Vector3f(
			y * v.z - z * v.y,
			z * v.x - x * v.z,
			x * v.y - y * v.x
		);
	}
};

///////////////
// Camera (kept as in lab code)
///////////////
class Camera {
public:
	Vector3f eye, center, up;
	Camera(float eyeX = 1.0f, float eyeY = 1.0f, float eyeZ = 1.0f,
		float centerX = 0.0f, float centerY = 0.0f, float centerZ = 0.0f,
		float upX = 0.0f, float upY = 1.0f, float upZ = 0.0f)
	{
		eye = Vector3f(eyeX, eyeY, eyeZ);
		center = Vector3f(centerX, centerY, centerZ);
		up = Vector3f(upX, upY, upZ);
	}
	void moveX(float d) { // strafe left/right
		Vector3f right = up.cross(center - eye).unit();
		eye = eye + right * d;
		center = center + right * d;
	}
	void moveY(float d) { // move up/down
		eye = eye + up.unit() * d;
		center = center + up.unit() * d;
	}
	void moveZ(float d) { // move forward/back
		Vector3f view = (center - eye).unit();
		eye = eye + view * d;
		center = center + view * d;
	}
	void rotateX(float a) { // pitch
		Vector3f view = (center - eye).unit();
		Vector3f right = up.cross(view).unit();
		float s = sinf(DEG2RAD(a)), c = cosf(DEG2RAD(a));
		view = view * c + up * s;
		up = view.cross(right);
		center = eye + view;
	}
	void rotateY(float a) { // yaw
		Vector3f view = (center - eye).unit();
		Vector3f right = up.cross(view).unit();
		float s = sinf(DEG2RAD(a)), c = cosf(DEG2RAD(a));
		view = view * c + right * s;
		right = view.cross(up);
		center = eye + view;
	}
	void look() {
		gluLookAt(eye.x, eye.y, eye.z,
			center.x, center.y, center.z,
			up.x, up.y, up.z);
	}
};

Camera camera;

///////////////
// Lighting & primitives (minor aesthetic changes)
///////////////
void setupLights() {
	// Soft underwater ambient
	GLfloat ambient[] = { 0.05f,0.12f,0.18f,1.0f };
	GLfloat diffuse[] = { 0.2f,0.4f,0.6f,1.0f };
	GLfloat specular[] = { 0.3f,0.6f,0.8f,1.0f };
	GLfloat shininess[] = { 30.0f };

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);

	GLfloat lightColor[] = { 0.4f,0.6f,0.9f,1.0f };
	GLfloat lightPos[] = { -4.0f,6.0f,3.0f,1.0f };

	glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor);
	glLightfv(GL_LIGHT0, GL_SPECULAR, lightColor);
}

void setupCameraProjection() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, 640.0 / 480.0, 0.1, 200.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	camera.look();
}

static void drawUnitCube() { glutSolidCube(1.0); }
static void drawUnitSphere() { glutSolidSphere(0.5, 20, 12); }
static void drawUnitCylinder() {
	GLUquadric* q = gluNewQuadric();
	glPushMatrix();
	gluCylinder(q, 0.5, 0.5, 1.0, 16, 2);
	glPopMatrix();
	gluDeleteQuadric(q);
}

///////////////
// AABB collision helper
///////////////
struct AABB {
	float minx, miny, minz;
	float maxx, maxy, maxz;
};

bool aabbIntersects(const AABB& a, const AABB& b) {
	return (a.minx <= b.maxx && a.maxx >= b.minx) &&
		(a.miny <= b.maxy && a.maxy >= b.miny) &&
		(a.minz <= b.maxz && a.maxz >= b.minz);
}

///////////////
// Scene objects (structures)
///////////////
struct SceneObj {
	float x, y, z;
	float sx, sy, sz;
	bool visible;
	bool animating;
	float animPhase;
	SceneObj() : x(0), y(0), z(0), sx(1), sy(1), sz(1), visible(true), animating(false), animPhase(0) {}
};

std::vector<SceneObj> majorObjs(2);
std::vector<SceneObj> regObjs(3);

struct GoalObj {
	float x, y, z;
	bool visible;
	float phase;
};
std::vector<GoalObj> goals;

struct CoralSegment {
	float x, y, z;
	float w, h, d; // box dims
	bool visible;
};
std::vector<CoralSegment> coralSegments;

///////////////
// Arena & drawing helpers
///////////////
const float arenaSize = 10.0f;
const float wallHeight = 1.0f;
const float wallTh = 0.2f;

float colorPhase = 0.0f;

void DrawSeabed(float width, float depth) {
	glPushMatrix();
	glColor3f(0.06f, 0.2f, 0.12f); // deep seabed
	glTranslatef(width / 2.0f, 0.0f, depth / 2.0f);
	glScalef(width, 0.05f, depth);
	glutSolidCube(1.0f);
	glPopMatrix();
}

void DrawBoundaryWallFixed(float x, float y, float z, float width, float height, float depth, float colorPhaseLocal) {
	glPushMatrix();
	float r = 0.4f + 0.2f * sinf(colorPhaseLocal + x + z);
	float g = 0.2f + 0.15f * cosf(colorPhaseLocal * 1.1f + x - z);
	float b = 0.25f + 0.15f * sinf(colorPhaseLocal * 0.7f - x + z);
	glColor3f(r, g, b);
	glTranslatef(x + width / 2.0f, y + height / 2.0f, z + depth / 2.0f);
	glScalef(width, height, depth);
	glutSolidCube(1.0f);
	glPopMatrix();
}

void DrawCoral(const CoralSegment& c, float timePhase) {
	if (!c.visible) return;
	glPushMatrix();
	glTranslatef(c.x + c.w / 2.0f, c.y + c.h / 2.0f, c.z + c.d / 2.0f);
	glColor3f(0.9f, 0.35f, 0.5f);
	glPushMatrix();
	glScalef(c.w, c.h, c.d);
	drawUnitCube();
	glPopMatrix();

	// small tubes (non-colliding decoration)
	int tubes = 3;
	for (int i = 0; i < tubes; i++) {
		glPushMatrix();
		float dx = (i - 1) * 0.15f;
		float dz = sinf(timePhase + i) * 0.03f;
		glTranslatef(dx, c.h / 2.0f + 0.12f, dz);
		glRotatef(-90, 1, 0, 0);
		glScalef(0.18f, 0.18f, 0.4f);
		drawUnitCylinder();
		glPopMatrix();
	}
	glPopMatrix();
}

void DrawRock(float x, float y, float z, float s) {
	glPushMatrix();
	glColor3f(0.2f, 0.2f, 0.25f);
	glTranslatef(x, y, z);
	glScalef(s, s * 0.6f, s);
	drawUnitSphere();
	glPopMatrix();
}

void DrawSeaweed(float x, float y, float z, float height, float tphase) {
	glPushMatrix();
	glTranslatef(x, y, z);
	glRotatef(sinf(tphase + x + z) * 20.0f, 0, 1, 0);
	glColor3f(0.05f, 0.6f, 0.2f);
	glBegin(GL_TRIANGLES);
	glVertex3f(0, 0, 0);
	glVertex3f(-0.08f, height / 2.0f, 0);
	glVertex3f(0.08f, height, 0);
	glEnd();
	glPopMatrix();
}

///////////////
// Diver
///////////////
void DrawDiverModel(float x, float y, float z, float angleY, float scale = 0.22f) {
	glPushMatrix();
	glTranslatef(x, y, z);
	// apply yaw (y-axis) then pitch (x-axis) so airborne tilt keeps facing direction
	glRotatef(angleY, 0, 1, 0);
	glRotatef(playerPitch, 1, 0, 0);
	glScalef(scale, scale, scale);

	// Torso
	glPushMatrix();
	glColor3f(0.15f, 0.45f, 0.7f);
	glTranslatef(0.0f, 0.6f, 0.0f);
	glScalef(0.6f, 0.9f, 0.35f);
	drawUnitCube();
	glPopMatrix();

	// Head
	glPushMatrix();
	glColor3f(0.95f, 0.85f, 0.75f);
	glTranslatef(0.0f, 1.15f, 0.0f);
	glScalef(0.45f, 0.45f, 0.45f);
	drawUnitSphere();
	glPopMatrix();

	// Left Arm
	glPushMatrix();
	glColor3f(0.15f, 0.45f, 0.7f);
	glTranslatef(-0.5f, 0.7f, 0);
	glScalef(0.18f, 0.6f, 0.18f);
	drawUnitCube();
	glPopMatrix();

	// Right Arm
	glPushMatrix();
	glColor3f(0.15f, 0.45f, 0.7f);
	glTranslatef(0.5f, 0.7f, 0);
	glScalef(0.18f, 0.6f, 0.18f);
	drawUnitCube();
	glPopMatrix();

	// Left Leg
	glPushMatrix();
	glColor3f(0.1f, 0.1f, 0.2f);
	glTranslatef(-0.18f, 0.1f, 0.0f);
	glScalef(0.18f, 0.6f, 0.18f);
	drawUnitCube();
	glPopMatrix();

	// Right Leg
	glPushMatrix();
	glColor3f(0.1f, 0.1f, 0.2f);
	glTranslatef(0.18f, 0.1f, 0.0f);
	glScalef(0.18f, 0.6f, 0.18f);
	drawUnitCube();
	glPopMatrix();

	// Oxygen tank
	glPushMatrix();
	glColor3f(0.02f, 0.45f, 0.25f);
	glTranslatef(0.0f, 0.6f, -0.35f);
	glRotatef(-90, 1, 0, 0);
	glScalef(0.35f, 0.35f, 0.8f);
	drawUnitCylinder();
	glPopMatrix();
	glPopMatrix();
}

///////////////
// Goal portal (visible & always non-blocking)
///////////////
void DrawGoalPortal(const GoalObj& g, float tphase) {
	if (!g.visible) return;
	glPushMatrix();
	glTranslatef(g.x, g.y + 0.18f * sinf(tphase * 2.0f), g.z);
	glRotatef(tphase * 40.0f, 0, 1, 0);

	glPushMatrix();
	glColor3f(0.9f, 0.5f, 0.05f);
	glutSolidTorus(0.03f, 0.20f, 16, 30);
	glPopMatrix();

	glPushMatrix();
	glColor3f(1.0f, 0.8f, 0.1f);
	glScalef(0.24f, 0.24f, 0.24f);
	drawUnitSphere();
	glPopMatrix();

	glPushMatrix();
	glColor3f(0.95f, 0.7f, 0.15f);
	glTranslatef(0, -0.55f, 0);
	glRotatef(-90, 1, 0, 0);
	glScalef(0.05f, 0.05f, 1.0f);
	drawUnitCylinder();
	glPopMatrix();

	glPopMatrix();
}

///////////////
// Major object (>=5 primitives)
///////////////
void DrawMajorObj(const SceneObj& o) {
	if (!o.visible) return;
	glPushMatrix();
	glTranslatef(o.x, o.y, o.z);
	glRotatef(o.animPhase * 60.0f, 0, 1, 0);

	// base 
	glPushMatrix();
	glColor3f(0.6f, 0.6f, 0.6f);
	glTranslatef(0, 0.10f, 0);
	glScalef(0.7f, 0.20f, 0.7f);
	drawUnitCube();
	glPopMatrix();

	// mast 
	glPushMatrix();
	glColor3f(0.45f, 0.45f, 0.5f);
	glTranslatef(0, 0.24f, 0);
	glRotatef(-90, 1, 0, 0);
	glScalef(0.12f, 0.12f, 0.9f);
	drawUnitCylinder();
	glPopMatrix();

	// arms 
	glPushMatrix();
	glColor3f(0.8f, 0.4f, 0.2f);
	glTranslatef(0.20f, 0.72f, 0);
	glRotatef(20, 0, 0, 1);
	glScalef(0.40f, 0.09f, 0.09f);
	drawUnitCube();
	glPopMatrix();

	glPushMatrix();
	glColor3f(0.7f, 0.35f, 0.2f);
	glTranslatef(0.40f, 0.82f, 0);
	glRotatef(10, 0, 0, 1);
	glScalef(0.28f, 0.08f, 0.08f);
	drawUnitCube();
	glPopMatrix();

	// hook
	glPushMatrix();
	glColor3f(0.95f, 0.9f, 0.3f);
	glTranslatef(0.48f, 0.90f, 0);
	glScalef(0.08f, 0.08f, 0.08f);
	drawUnitSphere();
	glPopMatrix();

	glPopMatrix();
}

///////////////
// Regular object (>=3 primitives)
///////////////
void DrawRegularObj(const SceneObj& o, float t) {
	if (!o.visible) return;
	glPushMatrix();
	glTranslatef(o.x, o.y, o.z);
	glRotatef(o.animPhase * 90.0f, 0, 1, 0);

	// rock base 
	glPushMatrix();
	glColor3f(0.25f, 0.25f, 0.28f);
	glTranslatef(0, 0, 0);
	glScalef(0.5f, 0.28f, 0.5f);
	drawUnitSphere();
	glPopMatrix();

	// seaweed1
	glPushMatrix();
	DrawSeaweed(0.12f, 0.0f, 0.0f, 0.9f, t + o.x);
	glPopMatrix();

	// seaweed2
	glPushMatrix();
	DrawSeaweed(-0.12f, 0.0f, 0.0f, 0.7f, t - o.x);
	glPopMatrix();

	glPopMatrix();
}

///////////////
// Build a tidy maze layout with no overlaps and clear collectible spots
///////////////
void buildMazeLayout() {
	coralSegments.clear();

	auto addBox = [&](float x, float z, float w, float d) {
		CoralSegment s;
		s.x = x; s.z = z; s.y = 0.0f;
		s.w = w; s.d = d; s.h = 0.9f;
		s.visible = true;
		coralSegments.push_back(s);
		};

	// Layout chosen to create a few corridors and visible collectible spots
	// Coordinates are chosen not to overlap with majors/regObjs or rocks

	// vertical wall left
	addBox(1.0f, 1.0f, 0.4f, 6.5f); // from z=1 to z=7.5
	// vertical wall right
	addBox(8.6f, 2.0f, 0.4f, 6.0f); // from z=2 to z=8
	// horizontal middle bar
	addBox(2.0f, 4.0f, 4.8f, 0.4f); // from x=2 to x=6.8 at z=4
	// small divider near start
	addBox(4.2f, 1.4f, 0.4f, 1.8f);
	// another small divider near end
	addBox(6.0f, 6.0f, 0.4f, 1.8f);

	// pillars (clear, not overlapping)
	addBox(3.0f, 7.6f, 0.6f, 0.6f);
	addBox(5.6f, 7.6f, 0.6f, 0.6f);
}

///////////////
// Initialize objects tidily (no overlaps)
///////////////
void initSceneObjects() {
	srand((unsigned)time(NULL));
	buildMazeLayout();

	// majors (large, blocking).
	majorObjs[0].x = 2.0f; majorObjs[0].y = 0.0f; majorObjs[0].z = 1.8f;
	majorObjs[0].visible = true; majorObjs[0].animPhase = 0; majorObjs[0].animating = true;

	majorObjs[1].x = 7.2f; majorObjs[1].y = 0.0f; majorObjs[1].z = 6.8f;
	majorObjs[1].visible = true; majorObjs[1].animPhase = 0; majorObjs[1].animating = true;

	// regular (minor) objects (seaweed/rock clusters).
	regObjs[0].x = 3.4f; regObjs[0].y = 0.0f; regObjs[0].z = 3.2f; regObjs[0].visible = true; regObjs[0].animating = true;
	regObjs[1].x = 6.4f; regObjs[1].y = 0.0f; regObjs[1].z = 2.2f; regObjs[1].visible = true; regObjs[1].animating = true;
	regObjs[2].x = 2.2f; regObjs[2].y = 0.0f; regObjs[2].z = 8.4f; regObjs[2].visible = true; regObjs[2].animating = true;

	// goals (collectibles)
	goals.clear();
	// visible near corridor junction
	GoalObj g1; g1.x = 4.5f; g1.z = 3.7f; g1.y = 0.6f; g1.visible = true; g1.phase = 0.5f; goals.push_back(g1);
	// visible in small alcove (raised to require floating)
	GoalObj g2; g2.x = 7.8f; g2.z = 7.8f; g2.y = 1.6f; g2.visible = true; g2.phase = 1.5f; goals.push_back(g2);
	// visible near pillar cluster (easy to spot)
	GoalObj g3; g3.x = 1.5f; g3.z = 9.0f; g3.y = 0.65f; g3.visible = true; g3.phase = 2.5f; goals.push_back(g3);
	colorPhase = 0.0f;
}

///////////////
// AABB factories (player, coral, major, regular, goal)
///////////////
AABB getPlayerAABB() {
	// tighter player box so player can touch walls/objects closely
	const float halfx = 0.14f;
	const float halfy = 0.48f;
	const float halfz = 0.14f;
	return AABB{ playerX - halfx, playerY - halfy, playerZ - halfz,
	playerX + halfx, playerY + halfy, playerZ + halfz };
}
AABB getGoalAABB(const GoalObj& g) {

	float r = 0.20f;
	return AABB{ g.x - r, g.y - r, g.z - r, g.x + r, g.y + r, g.z + r };
}
AABB getCoralAABB(const CoralSegment& c) {
	return AABB{ c.x, c.y, c.z, c.x + c.w, c.y + c.h, c.z + c.d };
}
AABB getMajorAABB(const SceneObj& o) {
	const float extent = 0.50f;
	const float height = 1.10f;
	return AABB{ o.x - extent, o.y, o.z - extent, o.x + extent, o.y + height, o.z + extent };
}
AABB getRegAABB(const SceneObj& o) {
	// regular objects (rock + seaweed) extents match drawing
	return AABB{ o.x - 0.60f, o.y, o.z - 0.60f, o.x + 0.60f, o.y + 0.90f, o.z + 0.60f };
}

///////////////
// Camera functions (kept; added top/side view)
///////////////
void SetCameraBehindPlayer()
{
	float distance = 3.0f;
	float height = 1.5f;

	float rad = DEG2RAD(playerAngleY);
	camera.eye.x = playerX + sin(rad) * distance;
	camera.eye.z = playerZ + cos(rad) * distance;
	camera.eye.y = height;

	camera.center.x = playerX;
	camera.center.y = playerY + 0.8f;
	camera.center.z = playerZ;


	camera.up = Vector3f(0, 1, 0);
}

void SetCameraTopView() {
	camera.eye = Vector3f(arenaSize / 2.0f, 20.0f, arenaSize / 2.0f);
	camera.center = Vector3f(arenaSize / 2.0f, 0.0f, arenaSize / 2.0f);
	camera.up = Vector3f(0.0f, 0.0f, -1.0f);
}

void SetCameraSideView() {
	camera.eye = Vector3f(-8.0f, 3.0f, arenaSize / 2.0f);
	camera.center = Vector3f(arenaSize / 2.0f, 0.8f, arenaSize / 2.0f);
	camera.up = Vector3f(0.0f, 1.0f, 0.0f);
}

void SetCameraFrontView() {
	camera.eye = Vector3f(arenaSize / 2.0f, 6.0f, arenaSize + 12.0f);
	camera.center = Vector3f(playerX, playerY + 0.8f, playerZ);
	camera.up = Vector3f(0.0f, 1.0f, 0.0f);
}

void SetCameraFreeView() {
	// leave camera where it is but allow free controls (WASD/QE, arrows)
	// if starting from a fixed view, move camera to a reasonable default behind the map center
	camera.eye = Vector3f(arenaSize / 2.0f, 3.0f, arenaSize + 2.0f);
	camera.center = Vector3f(arenaSize / 2.0f, 0.8f, arenaSize / 2.0f);
	camera.up = Vector3f(0.0f, 1.0f, 0.0f);
}

///////////////
// Input handlers - preserved player & camera keys
///////////////
void Keyboard(unsigned char key, int x, int y) {
	if (gameOver) {
		if (key == 'r' || key == 'R') {
			// restart
			gameOver = false;
			gameWin = false;
			collectedGoals = 0;
			playerX = 5.0f; playerZ = 5.0f; playerY = 0.05 / 2 + 0.1f; playerAngleY = 0.0f; playerPitch = 0.0f;
			gameTime = 90.0f;
			lastTime = std::chrono::steady_clock::now();
			initSceneObjects();
			glutPostRedisplay();
		}
		return;
	}

	float d = 0.1f; // camera movement
	float pSpeed = playerSpeed; // player movement speed

	// Save previous for safe revert on collision
	prevPlayerX = playerX;
	prevPlayerZ = playerZ;
	prevPlayerY = playerY;

	switch (key) {
		// camera moves (kept)
	case 'w': camera.moveY(d); break;
	case 's': camera.moveY(-d); break;
	case 'a': camera.moveX(d); break;
	case 'd': camera.moveX(-d); break;
	case 'q': camera.moveZ(d); break;
	case 'e': camera.moveZ(-d); break;

		// player movement (kept EXACT keys: I/J/K/L)
	case 'i': playerZ -= pSpeed; playerAngleY = 0.0f; break; // forward (decreasing Z)
	case 'k': playerZ += pSpeed; playerAngleY = 180.0f; break; // backward
	case 'j': playerX -= pSpeed; playerAngleY = 90.0f; break; // left
	case 'l': playerX += pSpeed; playerAngleY = -90.0f; break; // right

		// vertical movement: u = up, o = down
	case 'u': playerY += pSpeed; break;
	case 'o': playerY -= pSpeed; break;

		// animation toggles: M/N control majors anim start/stop
	case 'm':
		for (auto& mo : majorObjs) mo.animating = true;
		break;
	case 'n':
		for (auto& mo : majorObjs) mo.animating = false;
		break;

		// animation toggles: ,/. control regulars anim start/stop
	case 'v':
		for (auto& r : regObjs) r.animating = true;
		break;
	case 'b':
		for (auto& r : regObjs) r.animating = false;
		break;

		// camera view switching keys (remapped:1=back fixed,2=top,3=side,4=free)
	case '1': cameraViewMode = 1; SetCameraFrontView(); break; // fixed back-side view
	case '2': cameraViewMode = 2; SetCameraTopView(); break; // top view
	case '3': cameraViewMode = 3; SetCameraSideView(); break; // side view
	case '4': cameraViewMode = 4; SetCameraFreeView(); break; // free movement view

	case27: exit(EXIT_SUCCESS);
	default: break;
	}

	// clamp player to arena bounds based on player half-extents so touching walls is possible but not passing through
	const float phalfx = 0.14f;
	const float phalfz = 0.14f;
	if (playerX - phalfx < wallTh) playerX = wallTh + phalfx;
	if (playerX + phalfx > arenaSize - wallTh) playerX = arenaSize - wallTh - phalfx;
	if (playerZ - phalfz < wallTh) playerZ = wallTh + phalfz;
	if (playerZ + phalfz > arenaSize - wallTh) playerZ = arenaSize - wallTh - phalfz;

	// clamp player Y to allowed range
	if (playerY < groundY) playerY = groundY;
	if (playerY > maxPlayerY) playerY = maxPlayerY;

	glutPostRedisplay();
}

void Special(int key, int x, int y) {
	float a = 2.0f;
	switch (key) {
	case GLUT_KEY_UP: camera.rotateX(a); break;
	case GLUT_KEY_DOWN: camera.rotateX(-a); break;
	case GLUT_KEY_LEFT: camera.rotateY(a); break;
	case GLUT_KEY_RIGHT: camera.rotateY(-a); break;
	}
	glutPostRedisplay();
}

///////////////
// Update loop (animations, collisions, timer)
// - Collision: revert to prev pos on colliding with visible majors/corals/major rocks
// - Minor objects do NOT block
///////////////
void updateScene() {
	if (gameOver) return;

	auto now = std::chrono::steady_clock::now();
	std::chrono::duration<float> elapsed = now - lastTime;
	float dt = elapsed.count();
	if (dt <= 0) return;
	lastTime = now;

	// update timer
	gameTime -= dt;
	if (gameTime <= 0.0f) {
		gameOver = true;
		gameWin = (collectedGoals >= totalGoals);
		glutPostRedisplay();
		return;
	}

	colorPhase += dt * 1.0f;

	// animate majors if enabled (animation property unaffected by show/hide)
	for (auto& m : majorObjs) {
		if (m.animating) m.animPhase += dt;
	}
	for (auto& r : regObjs) {
		if (r.animating) r.animPhase += dt * 1.2f;
	}
	for (auto& g : goals) g.phase += dt * 1.5f;

	// airborne detection and pitch (positive tilts head forward around x-axis)
	bool airborne = (playerY > groundY + 0.01f);
	playerPitch = airborne ? 20.0f : 0.0f;

	// save current pos in case we need revert due to collision
	AABB pbox = getPlayerAABB();

	// Check collisions with visible coral segments
	bool collided = false;
	for (const auto& c : coralSegments) {
		if (!c.visible) continue;
		AABB cab = getCoralAABB(c);
		if (aabbIntersects(pbox, cab)) { collided = true; }
	}

	// majors
	if (!collided) {
		for (const auto& m : majorObjs) {
			if (!m.visible) continue;
			if (aabbIntersects(pbox, getMajorAABB(m))) { collided = true; break; }
		}
	}

	// large rocks (we'll test a couple of major rock positions)
	if (!collided) {
		{
			float rx = 1.0f, ry = 0.08f, rz = 1.2f, s = 0.35f;
			float xr = 0.5f * s;
			float yr = 0.5f * s * 0.6f;
			AABB rock1 = AABB{ rx - xr, ry - yr, rz - xr, rx + xr, ry + yr, rz + xr };
			if (aabbIntersects(pbox, rock1)) collided = true;
		}
		if (!collided) {
			float rx = 8.2f, ry = 0.08f, rz = 1.6f, s = 0.45f;
			float xr = 0.5f * s;
			float yr = 0.5f * s * 0.6f;
			AABB rock2 = AABB{ rx - xr, ry - yr, rz - xr, rx + xr, ry + yr, rz + xr };
			if (aabbIntersects(pbox, rock2)) collided = true;
		}
		if (!collided) {
			float rx = 4.0f, ry = 0.08f, rz = 8.2f, s = 0.30f;
			float xr = 0.5f * s;
			float yr = 0.5f * s * 0.6f;
			AABB rock3 = AABB{ rx - xr, ry - yr, rz - xr, rx + xr, ry + yr, rz + xr };
			if (aabbIntersects(pbox, rock3)) collided = true;
		}
	}

	// If collided with visible major or coral, revert to previous position (safe)
	if (collided) {
		playerX = prevPlayerX;
		playerZ = prevPlayerZ;
		playerY = prevPlayerY;
		pbox = getPlayerAABB();
	}

	// Check goals (collect) - goals are always visible or hidden but do not block
	for (auto& g : goals) {
		if (!g.visible) continue;
		if (aabbIntersects(pbox, getGoalAABB(g))) {
			g.visible = false;
			collectedGoals++;
			if (collectedGoals >= totalGoals) {
				gameOver = true;
				gameWin = true;
			}
		}
	}

	glutPostRedisplay();
}

///////////////
// HUD & display
///////////////
void renderHUD() {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	int w = glutGet(GLUT_WINDOW_WIDTH);
	int h = glutGet(GLUT_WINDOW_HEIGHT);

	gluOrtho2D(0, w, 0, h);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glDisable(GL_LIGHTING);

	// Top status line: Timer, Collected goals, View mode and Animations state
	char buf[256];
	sprintf(buf,
		"Time: %.0f Collected: %d/%d View:%d MajAnim:%s RegAnim:%s",
		gameTime, collectedGoals, totalGoals, cameraViewMode,
		(majorObjs[0].animating || majorObjs[1].animating) ? "ON" : "OFF",
		(regObjs[0].animating || regObjs[1].animating || regObjs[2].animating) ? "ON" : "OFF"
	);

	glColor3f(1.0f, 1.0f, 1.0f);
	glRasterPos2i(10, h - 20);

	for (char* p = buf; *p; ++p)
		glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *p);

	// Controls (only the important keys requested)
	auto printLine = [&](int y, const char* text) {
		glRasterPos2i(10, y);
		for (const char* c = text; *c; ++c)
			glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
		};

	printLine(h - 40, "Player: I/J/K/L move | U=up O=down (float)");
	printLine(h - 55, "Camera:1=behind  2=top  3=side");
	printLine(h - 70, "Animations: M=start majors N=stop majors | v=start regulars b=stop regulars");

	if (gameOver) {
		std::string msg = gameWin ?
			"GAME WIN - Press R to restart" :
			"GAME LOSE - Press R to restart";

		glRasterPos2i(w / 2 - 120, h / 2);
		for (char c : msg)
			glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
	}

	glEnable(GL_LIGHTING);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
}


void Display(void) {
	setupCameraProjection();
	setupLights();

	glClearColor(0.02f, 0.07f, 0.12f, 1.0f); // underwater blue
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Draw seabed
	DrawSeabed(arenaSize, arenaSize);

	// Boundary walls
	DrawBoundaryWallFixed(0.0f, 0.0f, 0.0f, arenaSize, wallHeight, wallTh, colorPhase);
	DrawBoundaryWallFixed(0.0f, 0.0f, arenaSize - wallTh, arenaSize, wallHeight, wallTh, colorPhase + 1.0f);
	DrawBoundaryWallFixed(0.0f, 0.0f, 0.0f, wallTh, wallHeight, arenaSize, colorPhase + 2.0f);
	DrawBoundaryWallFixed(arenaSize - wallTh, 0.0f, 0.0f, wallTh, wallHeight, arenaSize, colorPhase + 3.0f);

	// Coral maze
	for (const auto& c : coralSegments) {
		if (c.visible)
			DrawCoral(c, colorPhase);
	}

	// Major objects
	for (const auto& m : majorObjs) {
		if (m.visible)
			DrawMajorObj(m);
	}

	// Major rocks
	DrawRock(1.0f, 0.08f, 1.2f, 0.35f);
	DrawRock(8.2f, 0.08f, 1.6f, 0.45f);
	DrawRock(4.0f, 0.08f, 8.2f, 0.30f);

	// Regular objects
	for (const auto& r : regObjs)
		DrawRegularObj(r, colorPhase);

	// Seaweed
	DrawSeaweed(2.2f, 0.0f, 3.5f, 0.9f, colorPhase + 0.3f);
	DrawSeaweed(6.8f, 0.0f, 2.2f, 0.7f, colorPhase - 0.6f);
	DrawSeaweed(4.5f, 0.0f, 6.0f, 0.8f, colorPhase + 1.2f);

	// Goals
	for (const auto& g : goals)
		DrawGoalPortal(g, g.phase);

	// Player
	DrawDiverModel(playerX, playerY, playerZ, playerAngleY + 180.0f, 0.22f);

	// HUD
	renderHUD();

	glFlush();
	glutSwapBuffers();
}


///////////////////////
// main & initialization
///////////////////////
int main(int argc, char** argv) {
	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(640, 480);
	glutInitWindowPosition(50, 50);
	glutCreateWindow("Assignment2 - Coral Maze Escape (Fixed)");

	glutDisplayFunc(Display);
	glutKeyboardFunc(Keyboard);
	glutSpecialFunc(Special);
	glutIdleFunc(updateScene);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glEnable(GL_COLOR_MATERIAL);
	glShadeModel(GL_SMOOTH);

	// init scene
	initSceneObjects();
	lastTime = std::chrono::steady_clock::now();
	SetCameraFrontView();
	cameraViewMode = 1;

	glutMainLoop();
	return 0;
}

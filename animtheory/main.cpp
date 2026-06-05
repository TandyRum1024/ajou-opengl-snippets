/*
    과제 - 애니메이션이론
    =========================
    일부 실행에 필요한 라이브러리는 제거해둔 상태이며,
	본인이 작성하지 않은 스켈레톤 코드가 섞인 구간도 또한 제거해둔 상태입니다.
    따라서 본 소스만으로는 실행 불가능합니다.
*/

#include <stdio.h>
#include <iostream>
// =========================
// v 필요한 외부 라이브러리. 과제 수행 당시에 주어진 비공개 라이브러리가 있기에 미포함되었습니다.
#include <JGL/JGL_Window.hpp>
#include "AnimView.hpp"
#include <glm/gtx/quaternion.hpp>
// =========================

#include <fstream> // file read

// See init() and loadFromBVH()
const std::string FILES[] = {
	"DNCMODRNA.bvh",
	"WaveA.bvh",
	"jogCurve.bvh",
	"SledgeHammerA.bvh",
	"WhipA.bvh",
	"SpinBackKickA.bvh",
	"SpinKickHiA.bvh",
	"BigKick.bvh",
	"smoothWalk.bvh",
	"StrutLoopA.bvh",
	"Example1.bvh"
};
int FILE_IDX = 0; // "DNCMODRNA.bvh"
const int FILE_NUM = std::size(FILES); // wow!
glm::vec3 modelImportScale = glm::vec3(2.0); // model import scale
glm::vec3 modelOff = glm::vec3(0.0, 0.0, 0.0); // model offset

int iblCoeffIdx = 0;
glm::vec3 iblCoeffNow[9];

// uncomment for debug mode
// (more BVH related debug prints)
//#define BVH_DEBUG_LOAD 
// (allows to modify spherical harmonics coeffectients in runtime, printing them in console to be used for coefficient constants)
//#define SH_DEBUG_EDIT

/* 중략. 과제시 주어진 비공개 스켈레톤 코드. */

/////////// Spring-mass system
// Length: 1cm
// Mass: 1kg
vec3 worldGrav = vec3(0.0, -980, 0.0f);
float WORLD_EPS = 0.1f;
float	worldTimescale = 1.0f,
		worldDamping = 0.005f,
		worldFriction = 0.3f,
		worldDefaultKs = 0.9f, worldDefaultKd = 0.01f;
int worldIterationPerFrame = 15;

struct Point {
	vec3 p; // pos
	float m; // mass
	vec3 v; // vel
	vec3 f; // force
	bool fixed = false;
	Point(const vec3& pos, float mass = 0.001f, const vec3& vel = vec3(0), bool isFixed=false)
		: p(pos), m(mass), v(vel), f(0), fixed(isFixed) {}

	void clearForce() {
		f = vec3(0);
	}
	void addForce(const vec3& force) {
		f += force;
	}
	void integrate(float dt) {
		if (fixed) v = vec3(0);
		// Explicit euler method
		p += v * dt;
		v += f / m * dt;
	}
	void draw() const {
		drawSphere(p, 1, fixed ? vec4(0.1, 0, 1, 1) : vec4(1, 1, 0, 1));
	}
};

struct Spring {
	Point &a, &b;
	float k_s, k_d, r;
	bool hide = false;
	Spring(Point& p1, Point& p2, float ks=worldDefaultKs, bool hideRendering=false)
		: a(p1), b(p2), k_s(ks), r(length(p1.p - p2.p)), hide(hideRendering) {}

	void applyForce() {
		vec3	deltaPos = a.p - b.p, dir = normalize(deltaPos),
				deltaVel = a.v - b.v;
		vec3 f = -(k_s * (length(deltaPos) - r) + worldDefaultKd * dot(deltaVel, dir)) * dir;
		a.addForce(f);
		b.addForce(-f);
	}
	void draw() const {
		if (!hide)
			drawCylinder(a.p, b.p, 0.8, vec4(1, 0.25, 0, 1));
	}
};

struct WorldPlane {
	vec3 p;
	vec3 n;
	bool hide = false;

	void draw() {
		if (!hide)
			drawQuad(p, n, vec2(1000, 1000));
	}

	bool check(const Point point) {
		return (dot(point.p - p, n) < WORLD_EPS && dot(point.v, n) < 0);
	}

	void resolve(Point& point, float alpha=0.5) {
		vec3	vn = dot(point.v, n) * n,
				vt = point.v - vn;
		point.v = vt - (alpha*vn);

		// friction
		vec3 velProjected = vt;
		point.addForce(-velProjected * worldFriction);
	}
};

struct WorldSphere {
	vec3 p;
	float r;
	bool hide = false;

	WorldSphere(vec3 pos, float rad)
		: r(rad), p(pos) {}

	void draw() const {
		if (!hide)
			drawSphere(p, r, vec4(0.5, 0.5, 0.5, 1));
	}

	bool check(const Point point) {
		return length(point.p - p) < r;
	}

	void resolve(Point& point, float alpha = 0.5) {
		vec3	delta = point.p - p,
				n = normalize(delta),
				vn = dot(point.v, n) * n,
				vt = point.v - vn;
		float penetration = r - length(delta);
		point.v = vt - (alpha * vn);

		// non-impulse based collision response to make sure that the points are not clipping through moving spheres
		point.p += n * penetration;

		// friction
		vec3 velProjected = vt;
		point.addForce(-velProjected * worldFriction);
		//point.v += n * penetration;
	}
};

vector<WorldSphere> spheres;
vector<WorldPlane> planes;
vector<Point> points;
vector<Spring> springs;

int bvh_trail_idx = 0;

int curtain_top_off = 0, curtain_top_off_2 = 0;
float curtain_size = 75.0f, curtain_z = -120.0f, curtain_y_start = 150.0f, curtain_y_end = 50.0f, curtain_y = curtain_y_start, curtain_rot = 0.0f;
int curtain_div = 10;

// Helper routine for generating cloth from spring-masses
void generateCloth(vec3 origin, vec3 step, int sz_x, int sz_y, float mass = 0.002f, float k_s = worldDefaultKs) {
	vec3 p = origin;
	int idx_off = points.size();
	for (auto i = 0; i <= sz_y; i++)
	{
		p.x = origin.x;
		p.z = origin.z;
		for (auto j = 0; j <= sz_x; j++)
		{
			points.push_back(Point(p, mass));
			if (i == 0) points.back().fixed = true;
			p.x += step.x;
			p.z += step.z;
		}
		p.y += step.y;
	}
	// Stretch spring
	for (auto i = 0; i <= sz_y; i++) {
		for (auto j = 0; j < sz_x; j++) {
			springs.push_back(Spring(points[idx_off + j + i * (sz_x + 1)], points[idx_off + j + 1 + i * (sz_x + 1)], k_s));
		}
	}
	for (auto i = 0; i < sz_y; i++) {
		for (auto j = 0; j <= sz_x; j++) {
			springs.push_back(Spring(points[idx_off + j + i * (sz_x + 1)], points[idx_off + j + (i + 1) * (sz_x + 1)], k_s));
		}
	}
	// Shear spring
	for (auto i = 0; i < sz_y; i++) {
		for (auto j = 0; j < sz_x; j++) {
			springs.push_back(Spring(points[idx_off + j + i * (sz_x + 1)], points[idx_off + j + 1 + (i + 1) * (sz_x + 1)], k_s));
			springs.push_back(Spring(points[idx_off + j + (i + 1) * (sz_x + 1)], points[idx_off + j + 1 + i * (sz_x + 1)], k_s));
		}
	}
	// Bend spring
	for (auto i = 0; i <= sz_y; i++) {
		for (auto j = 0; j <= sz_x - 2; j+=2) {
			springs.push_back(Spring(points[idx_off + j + i * (sz_x + 1)], points[idx_off + j + 2 + i * (sz_x + 1)], k_s, true));
		}
	}
	for (auto j = 0; j <= sz_x; j++) {
		for (auto i = 0; i <= sz_y - 2; i += 2) {
			springs.push_back(Spring(points[idx_off + j + i * (sz_x + 1)], points[idx_off + j + (i + 2) * (sz_x + 1)], k_s, true));
		}
	}
}
///////////

quat SLERP(const quat& x, const quat& y, float t);
/*
	중략. 과제시 주어진 비공개 스켈레톤 코드가 섞인 부분이 있어, 본 분량은 제외합니다.
	Skeletal Animation의 bone 역할을 하는, 트리 구조의 Joint, 그리고 이의 목록을 담고 업데이트 및 드로우하는 Body struct가 정의되었었습니다.
*/

/////////// BVH ANIMATION
struct Animation {
	vector<int> jointIdxTable;
	vector<int> jointIdxTable2;

	// Animation data
	int frameNum;
	float frameTime;

	int _currentFrame; // currently modifying frame

	// Each frame contains all offset data for each joints in the body
	struct JointData {
		vec3 pos; // XYZ
		vec3 rot; // (debug) Euler rot
		quat ori;
	};
	struct Frame {
		vector<JointData> jointData; // For all joints
	};

	vector<Frame> frames;

	/*
		Usage:
			for each frame:
				anim.addFrame(joint_num) // prepares new frame
				for each joint:
					anim.setJointFrame(joint_idx, vec3_pos, vec3_rot) // sets joint frame data
	*/
	// Add & prepare new frame
	void addFrame(int joint_num) {
		Frame frame;
		for (int i = 0; i < joint_num; i++)
		{
			frame.jointData.push_back({ vec3(0.0), vec3(0.0), quat(1, 0, 0, 0) });
		}
		_currentFrame = frames.size();
		frames.push_back(frame);
		frameNum++;
	};
	// Sets joint frame data
	// vec3 pos = offset, vec3 rot = local euler angles
	void setJointFrame(int jointIdx, vec3 pos, vec3 rot) {
		Frame& frame = frames[_currentFrame];

		// Convert degrees to radians
		vec3 radRot = vec3(radians(rot.x), radians(rot.y), radians(rot.z));

		// Convert euler rotation to combined quaternion
		quat q;
		q = quat(vec3(0, 0, radRot.z)) * quat(vec3(radRot.x, 0, 0)) * quat(vec3(0, radRot.y, 0)); // ???

		int idx = jointIdx;
		frame.jointData[idx].pos = pos;
		frame.jointData[idx].rot = rot; // original euler angles for debugging
		frame.jointData[idx].ori = normalize(q); // ori = q_parent * (animationQ * q)
	};

	JointData eval(int jointIdx, float time) {
		// Find the frame index from time; Using the fact that all frames are uniformly distributed
		int idx1 = int(floor(time / frameTime)),
			idx2 = int(floor(time / frameTime));
		// Process frame index so that they won't overflow
		idx1 %= frameNum;
		idx2 %= frameNum;
		Frame&	f1 = frames[idx1],
				f2 = frames[idx2];
		// For now just return the data
		return f1.jointData[jointIdx];
	};

	void evalTo(Body& body, float time)
	{
		// Find the frame index from time; Using the fact that all frames are uniformly distributed
		int idx1 = int(floor(time / frameTime)),
			idx2 = idx1 + 1;
		// Process frame index so that they won't overflow
		idx1 %= frameNum;
		idx2 %= frameNum;

		// Process the fraction time / interpolation factor
		float interp = fract(time / frameTime);

		// Update the joints
		Frame&	f1 = frames[idx1],
				f2 = frames[idx2];
		for (int i=0; i<body.joints.size(); i++)
		{
			body.joints[i].animationLink	= f1.jointData[i].pos + (f2.jointData[i].pos - f1.jointData[i].pos) * interp;
			body.joints[i].animationQ		= SLERP(f1.jointData[i].ori, f2.jointData[i].ori, interp);
		}
	}

	// Converts between BVH index (excludes End sites) to Body struct's joint index (includes End sites)
	int convertBVHJointIndex(int bvh_idx)
	{
		return jointIdxTable[bvh_idx];
	}
	// Converts between Body struct's joint index (includes End sites) to BVH index (excludes End sites)
	int convertJointIndex(int joint_idx)
	{
		return jointIdxTable2[joint_idx];
	}
};
/////
float animT = 0.0; // current time for animation
Animation anim;
bool animIsLoaded = false;
/////////// BVH ANIMATION

/////////// BVH
int bvhHeadIdx = 0;
// Slave function for recursion
bool _parseBVHJoint(Body& body, ifstream& FILE, int jointParent, int& currentLine, string& line, string& word, string indent="")
{
	string name = "";
	vec3 offset;
	bool isEndsite = false;

	//string line, word;
	stringstream ls;

	// Check for ROOT/JOINT/End Site
	getline(FILE, line); currentLine++;
	ls = stringstream(line); ls >> word;
	if (word.compare("ROOT") == 0 || word.compare("JOINT") == 0 || word.compare("End") == 0)
	{
		ls >> name;
#ifdef BVH_DEBUG_LOAD
		cout << indent + "JOINT " << name << endl;
#endif
		isEndsite = (word.compare("End") == 0);
	}
	else
	{
		if (word.compare("}") != 0 && word.compare("MOTION") != 0)
			cerr << "loadBVHTo:: Expected `JOINT` or `ROOT` or `End Site` but got `" << word << "` (@ line " << currentLine << ")" << endl;
		return false;
	}

	// Check for opening brace
	getline(FILE, line); currentLine++;
	ls = stringstream(line); ls >> word;
	if (word.compare("{") != 0)
	{
		cerr << "loadBVHTo:: INVALID FORMAT - Expected `{` (@ line " << currentLine << ")" << endl;
		return false;
	}

	// Parse offset & channel
	getline(FILE, line); currentLine++;
	ls = stringstream(line); ls >> word;
	if (word.compare("OFFSET") == 0)
		ls >> offset.x >> offset.y >> offset.z;
#ifdef BVH_DEBUG_LOAD
	cout << indent + " > OFFSET " << offset.x << ", " << offset.y << ", " << offset.z << endl;
#endif

	// (skip the channels for now)
	if (!isEndsite)
	{
		getline(FILE, line); currentLine++;
	}

	// Add new joint
	int jointIdx = body.joints.size();
	body.emplace(modelImportScale * offset, jointParent, name, isEndsite);

	// Recursively parse the joint
	if (!isEndsite)
	{
		// First parse the recursive part
		while (_parseBVHJoint(body, FILE, jointIdx, currentLine, line, word, indent + "\t")) {};

		// Check for the matching brace at the end
		if (word.compare("}") != 0)
		{
			cerr << "loadBVHTo:: INVALID FORMAT - Expected `}` BUT GOT `" << line << "` (@ line " << currentLine << ")" << endl;
			return false;
		}
#ifdef BVH_DEBUG_LOAD
		else
		{
			cout << indent + "END " << name << endl;
		}
#endif
	}
	else
	{
		// Check for matching brace at the end
		getline(FILE, line); currentLine++;
		ls = stringstream(line); ls >> word;
		if (word.compare("}") != 0)
		{
			cerr << "loadBVHTo:: INVALID FORMAT - Expected `}` AFTER End Site BUT GOT `" << line << "` (@ line " << currentLine << ")" << endl;
			return false;
		}
		else
			return true;
	}
}

bool loadBVHTo(Body& body, Animation& anim, string filepath) {
	ifstream FILE;
	FILE.open(filepath, ios::in);
	if (!FILE.is_open()) // File failed to open
		return false;

	// Parse BVH file
	cout << "loadBVHTo:: LOADING FILE `" << filepath << "`" << endl;
	string line, word;
	int currentLine = 0;
	int jointParent = -1, jointCurrent = -1; // parent & currently modifying joint

	// Check for correct header
	getline(FILE, line); currentLine++;
	if (line.compare("HIERARCHY") != 0)
	{
		cerr << "loadBVHTo:: INVALID FORMAT - Expectecd `HIERARCHY` (@ line " << currentLine << ")" << endl;
		return false;
	}

	// Recursively parse the joints
	while (_parseBVHJoint(body, FILE, -1, currentLine, line, word)) {};
	
	// Check for successful parse
	if (word.compare("}") != 0 && word.compare("MOTION") != 0)
	{
		cerr << "loadBVHTo:: INVALID FORMAT - Expected `}` (@ line " << currentLine << ")" << endl;
		return false;
	}

	// Update the joint index conversion table
	for (auto i = 0; i < body.joints.size(); i++)
	{
		// Check for end site
		int j = i;
		while (j < body.joints.size() && body.joints[j].isEndsite) // Find new index that is not an endsite
			j++;
		anim.jointIdxTable.push_back(j);
	}
	int idx = 0;
	for (auto i = 0; i < body.joints.size(); i++)
	{
		if (body.joints[i].isEndsite)
			anim.jointIdxTable2.push_back(-1);
		else
			anim.jointIdxTable2.push_back(idx);
		if (!body.joints[i].isEndsite)
			idx++;
	}

	// Parse the animation data
	int frameNum;
	float frameTime;
	getline(FILE, line); currentLine++;
	sscanf_s(line.c_str(), "Frames: %d", &frameNum);
	getline(FILE, line); currentLine++;
	sscanf_s(line.c_str(), "Frame Time: %f", &frameTime);

	//anim.frameNum = frameNum;
	anim.frameTime = frameTime;
#ifdef BVH_DEBUG_LOAD
	cout << "\tFRAME NUM: " << frameNum << ", FRAME TIME: " << frameTime << endl;
#endif

	int jointNum = body.joints.size();
	stringstream ls;
	vec3 pos, rot;
	for (int i = 0; i < frameNum; i++)
	{
		getline(FILE, line); currentLine++;
		ls = stringstream(line);
		
		anim.addFrame(jointNum);

		// First load the root joint's data: X Y Z, ZROT, XROT, YROT
		ls >> pos.x >> pos.y >> pos.z;
		ls >> rot.z >> rot.x >> rot.y;
		anim.setJointFrame(0, modelImportScale * pos, rot);
		
		// Load the rest of the joints data: ZROT, XROT, YROT
		for (int j = 1; j < jointNum; j++)
		{
			// Skip if end site
			if (body.joints[j].isEndsite)
				continue;
			ls >> rot.z >> rot.x >> rot.y;
			anim.setJointFrame(anim.convertBVHJointIndex(j), vec3(0.0), rot);
		}
	}

#ifdef BVH_DEBUG_LOAD
	stringstream jointOffset;
	vector<stringstream> jointRot;
	for (int j = 0; j < jointNum; j++)
		jointRot.push_back(stringstream(""));

	for (int i = 0; i < frameNum; i++)
	{
		Animation::Frame f = anim.frames[i];
		jointOffset << " | " << f.jointData[0].pos.x << "," << f.jointData[0].pos.y << "," << f.jointData[0].pos.z;
		for (int j = 0; j < jointNum; j++)
			jointRot[j] << " | " << f.jointData[j].rot.x << "," << f.jointData[j].rot.y << "," << f.jointData[j].rot.z;
	}
	cout << "==================== MOTION ====================" << endl << "JOINT " << ((body.joints[0].name == "") ? "0" : body.joints[0].name + "(0)") << " POS" << jointOffset.str() << endl;
	for (int j = 0; j < jointNum; j++)
	{
		string id = (body.joints[j].name == "") ? to_string(anim.convertJointIndex(j)) : body.joints[j].name + "(" + to_string(anim.convertJointIndex(j)) + ")";
		if (body.joints[j].isEndsite)
			id = "ENDSITE_" + ((body.joints[j].name == "") ? to_string(anim.convertJointIndex(j)) : body.joints[j].name);
		cout << "JOINT " << id << " ROT" << jointRot[j].str() << endl;
	}
#endif

	cout << "loadBVHTo:: LOADING SUCCESS!" << endl;
	FILE.close();
	return true;
}

void loadFromBVH(string filename) {
	// Clear previous data
	bvhHeadIdx = 0;

	body.joints.clear();
	anim.frames.clear();
	anim._currentFrame = 0;
	anim.frameNum = 0;

	// Load from file
	animIsLoaded = true;
	if (!loadBVHTo(body, anim, "BVH/" + filename)) // failed to load from ./BVH/..., try loading directly from the root folder
	{
		if (!loadBVHTo(body, anim, filename))
		{
			animIsLoaded = false;
			cerr << "FILE LOAD ERROR! `" << filename << "` DOES NOT EXIST?" << endl;
		}
	}
	if (animIsLoaded)
	{
		body.update();

		// Find head index
		for (auto i = 0; i < body.joints.size(); i++)
		{
			if (body.joints[i].name == "Head" || body.joints[i].name == "head")
			{
				bvhHeadIdx = i;
				continue;
			}
		}

		// Calculate automatic scaling and offset
		float maxY = 0;
		for (auto& joint : body.joints)
			maxY = std::max(maxY, abs(joint.getPosition().y));

		float scale = 20.0 / maxY;
		for (auto& joint : body.joints)
		{
			joint.link *= scale;
		}
		for (auto& frame : anim.frames)
		{
			for (auto& joint : frame.jointData)
			{
				joint.pos *= scale;
			}
		}

		// Initial update
		anim.evalTo(body, animT);
		body.update();
	}
}
/////////// BVH

void init() {
	cout << "PERFORM INIT()" << endl;
	cout << "__FILE__ `" << __FILE__ << "`" << endl;

	// Prepare BVH
	animIsLoaded = false;
	string filename = FILES[FILE_IDX];
	animT = 0;
	loadFromBVH(filename);

	// Prepare spring-mass system
	curtain_rot = 0;

	spheres.clear();
	planes.clear();
	points.clear();
	springs.clear();
	
	// Background spinning curtain
	curtain_y = curtain_y_start;
	curtain_top_off = points.size();
	generateCloth(vec3(-curtain_size*0.5, curtain_y, curtain_z), vec3(curtain_size / curtain_div, -curtain_size / curtain_div * 0.5, 0), curtain_div, curtain_div);

	// Small piece of cloth colliding with jugglers and floating spheres
	curtain_top_off_2 = points.size();
	generateCloth(vec3(0.0f, 52, -16), vec3(0, -curtain_size / curtain_div * 0.3, curtain_size / curtain_div * 0.3), curtain_div / 2, curtain_div / 2, 0.0005f, 2.0f);
	// (only make the corner point fixed)
	for (auto i = 1; i <= curtain_div; i++)
		points[i + curtain_top_off_2].fixed = false;

	// Spheres definition
	spheres.push_back(WorldSphere({ 0, -8, curtain_z }, 32));
	spheres.push_back(WorldSphere({ -42, 16, -32 }, 6));
	spheres.push_back(WorldSphere({ 42, 16, 32 }, 6));

	spheres.push_back(WorldSphere({ 4, 16, -16 }, 6));
	spheres.push_back(WorldSphere({ -4, 4, -16 }, 6));

	planes.push_back({ { 0,0.5,0 }, { 0,1,0 } });
	planes[0].hide = true;

	if (animIsLoaded)
	{
		vec3 origin = body.joints[bvhHeadIdx].getPosition();

		bvh_trail_idx = points.size();
		for (auto i = 0; i < 16; i++)
		{
			points.push_back(Point(origin + vec3(-i, 0, 0), 0.005f - i * 0.0001f));
			if (i > 0)
				springs.push_back(Spring(points.rbegin()[1], points.back(), 1.5f));
		}
		points[bvh_trail_idx].fixed = true;
		points[bvh_trail_idx+1].fixed = true;
	}
}

quat SLERP(const quat& x, const quat& y, float t) {
	quat q;
	if (dot(x, y) < 0)
		q = inverse(x) * -y;
	else q = inverse(x) * y;
	quat v = log(q); v.w = 0;
	return x * exp(v * t);
}

// dt = in seconds
void frame(float dt) {
	// Sanitize dt to prevent the simulation from getting too spastic on freeze
	dt = std::min(dt, 0.04f);

	// Update BVH
	animT += dt * worldTimescale;
	if (animIsLoaded)
	{
		anim.evalTo(body, animT);
		body.update();
	}

	// Update spring-mass
	// (Sphere)
	spheres[1].p = vec3(cos(animT * 0.5f * pi<float>()) * 38, 26 + abs(sin(animT * 0.5f * pi<float>()) * 8), sin(animT * 0.5f * pi<float>()) * 16);
	spheres[2].p = vec3(cos((animT * 0.5f + 1.0) * pi<float>()) * 38, 26 + abs(sin((animT * 0.5f + 1.0) * pi<float>()) * 8), sin((animT * 0.5f + 1.0) * pi<float>()) * 16);
	// (curtain)
	// SPIN
	curtain_y = mix(curtain_y, 60.0f, 1.0f * dt);
	curtain_rot += dt * pi<float>();
	for (auto i = 0; i <= curtain_div; i++)
	{
		if (points[curtain_top_off + i].fixed)
		{
			float interp = float(i) / curtain_div;
			points[curtain_top_off + i].p = vec3(-curtain_size * 0.5 * cos(curtain_rot) + curtain_size * interp * cos(curtain_rot), curtain_y, -curtain_size * 0.5 * sin(curtain_rot) + curtain_size * interp * sin(curtain_rot) + curtain_z);
		}
	}
	// (bvh trail thing)
	if (animIsLoaded)
	{
		quat p = body.joints[bvhHeadIdx].getOrientation() * quat(0, vec3(0, 0, -16)) * inverse(body.joints[bvhHeadIdx].getOrientation());
		points[bvh_trail_idx].v = (body.joints[bvhHeadIdx].getPosition() - points[bvh_trail_idx].p);
		points[bvh_trail_idx].p = body.joints[bvhHeadIdx].getPosition();
		points[bvh_trail_idx + 1].v = (body.joints[bvhHeadIdx].getPosition() + vec3(p.x, p.y, p.z) - points[bvh_trail_idx + 1].p);
		points[bvh_trail_idx + 1].p = body.joints[bvhHeadIdx].getPosition() + vec3(p.x, p.y, p.z);

		// fake lift simulation according to horizontal velocity
		for (auto i = 0; i < body.joints.size(); i++)
			points[bvh_trail_idx + i].v.y += length(vec3(points[bvh_trail_idx + i].v.x, 0, points[bvh_trail_idx + i].v.z)) * 0.1;
	}

	// (drag)
	if (picked >= 0)
	{
		points[picked].v += (targetPt - points[picked].p) * 10.0f;
		//pickPt = points[picked].p;
	}

	for (auto i = 0; i < worldIterationPerFrame; i++)
	{
		for (auto& p : points)	p.clearForce();
		for (auto& p : points)	p.addForce(p.m * worldGrav);
		for (auto& p : points)	p.addForce(-p.v * worldDamping);
		for (auto& s : springs) s.applyForce();
		//for (auto& s : spheres) s.updatePos(1.0f / worldIterationPerFrame);
		for (auto& p : points)
		{ 
			for (auto& s : planes)
			{
				if (s.check(p))
					s.resolve(p);
			}
			for (auto& s : spheres)
			{
				if (s.check(p))
					s.resolve(p);
			}
		}
		for (auto& p : points)	p.integrate(dt * worldTimescale / worldIterationPerFrame);
	}

}

// Used to mimic the weird spherical limbs of old Commodore Amiga Juggler demo
void drawSphereRod(vec3 p1, vec3 p2, float rStart, float rEnd, int num, vec4 col) {
	int interpDiv = num - 1;
	for (auto i = 0; i < num; i++)
	{
		float interp = float(i) / interpDiv;
		drawSphere(mix(p1, p2, interp), mix(rStart, rEnd, interp), col);
	}
}
void drawSphereLimbs(vec3 p1, vec3 p2, vec3 p3, float rStart, float rEnd, int num, vec4 col) {
	int interpHalf = num / 2,
		interpDiv = interpHalf - 1;
	for (auto i = 0; i < interpHalf; i++)
	{
		float interp = float(i) / interpHalf;
		drawSphere(mix(p1, p2, interp), mix(rStart, rEnd, interp * 0.5), col);
	}
	for (auto i = 0; i < interpHalf; i++)
	{
		float interp = float(i) / interpDiv;
		drawSphere(mix(p2, p3, interp), mix(rStart, rEnd, 0.5+interp * 0.5), col);
	}
}

void render() {
	// Hijack shader uniforms (sorry professor!)
	// Setup Spherical harmonics
#ifdef SH_DEBUG_EDIT
	// SH debug
	for (auto i = 0; i < 9; i++)
		animView->iblCoeffs[i] = iblCoeffNow[i]; //  vec3(0.2, -0.82, -0.5);

	setUniform(animView->renderProg, "shadowEnabled", 0);
	setUniform(animView->renderProg, "iblCoeffs", animView->iblCoeffs, 9);
	setUniform(animView->renderProg, "iblIntensityFactor", 1.0f);
	drawSphere(vec3(0, 32, 0), 32, vec4(1, 1, 1, 1));
	setUniform(animView->renderProg, "shadowEnabled", 1);
#else
	// Clear SH, and set to certain values that looks pretty
	for (auto i=0; i<9; i++)
		animView->iblCoeffs[i] = vec3(0, 0, 0);
	animView->iblCoeffs[0] = vec3(1.2, 1.7, 0.24);
	animView->iblCoeffs[1] = vec3(-0.8, -0.8, 1.6) * 0.25f;
	//animView->iblCoeffs[2] = vec3(1.0, -0.1, 0);
	setUniform(animView->renderProg, "iblCoeffs", animView->iblCoeffs, 9);
#endif

	// Pseudo-skybox
	//setUniform(animView->renderProg, "iblCoeffs", animView->iblCoeffs, 9);
	//setUniform(animView->renderProg, "shadowEnabled", 0);
	//setUniform(animView->renderProg, "iblIntensityFactor", 1.0f);
	//setUniform(animView->renderProg, "roughness", 0.0f);
	//setUniform(animView->renderProg, "specularFactor", 0.0f);
	//drawSphere(vec3(0, 200, 0), -500, vec4(1, 1, 1, 1));

	setUniform(animView->renderProg, "iblIntensityFactor", 0.3f);
	setUniform(animView->renderProg, "roughness", 0.01f);
	setUniform(animView->renderProg, "specularFactor", 16.0f);

	// BVH body
	if (animIsLoaded)
		body.draw();

	// Jugglers
	float	head_y_off = abs(sin(animT * 0.5f * pi<float>() + 0.25f) * 4),
			torso_y_off = abs(sin(animT * 0.5f * pi<float>() + 0.5f) * 2),
			hand_y_off = torso_y_off + abs(sin(animT * 0.5f * pi<float>() + 0.4f) * 3);
	drawSphere(vec3(-48, 30 + head_y_off, 0), 3, vec4(1.0, 0.6, 0.7, 1.0)); // head
	drawSphere(vec3(-49, 30.5 + head_y_off, 0), 3, vec4(0.025, 0.025, 0.1, 1.0)); // hair
	drawSphere(vec3(-45.5, 30.5 + head_y_off, -1.5), 1, vec4(0.025, 0.025, 0.5, 1.0)); // eyeball
	drawSphere(vec3(-45.5, 30.5 + head_y_off, 1.5), 1, vec4(0.025, 0.025, 0.5, 1.0)); // eyeball
	drawSphereRod(vec3(-48, 16 + torso_y_off, 0), vec3(-48, 22 + torso_y_off, 0), 4, 5, 4, vec4(1.0, 0.0, 0.0, 1.0)); // torso
	drawSphereLimbs(vec3(-48, 24 + torso_y_off, -5), vec3(-40, 22 + hand_y_off*0.5, -6), vec3(-36, 22 + hand_y_off, -4), 1, 0.75, 16, vec4(1.0, 0.6, 0.7, 1.0)); // arm
	drawSphereLimbs(vec3(-48, 24 + torso_y_off, 5), vec3(-40, 22 + hand_y_off * 0.5, 6), vec3(-36, 22 + hand_y_off, 4), 1, 0.75, 16, vec4(1.0, 0.6, 0.7, 1.0)); // arm
	drawSphereLimbs(vec3(-48, 14 + torso_y_off, -3), vec3(-44, 9 + torso_y_off * 0.5, -(4 + torso_y_off * 0.5)), vec3(-48, 0, -2), 1, 0.6, 16, vec4(1.0, 0.6, 0.7, 1.0)); // leg
	drawSphereLimbs(vec3(-48, 14 + torso_y_off, 3), vec3(-44, 9 + torso_y_off * 0.5, (4 + torso_y_off * 0.5)), vec3(-48, 0, 2), 1, 0.6, 16, vec4(1.0, 0.6, 0.7, 1.0)); // leg

	//drawSphereLimbs(vec3(48, 16, 0), vec3(44, 24, 0), 5, 7, 4, vec4(0.8, 0.25, 0.0, 1.0));
	drawSphere(vec3(48, 30 + head_y_off, 0), 3, vec4(1.0, 0.6, 0.7, 1.0)); // head
	drawSphere(vec3(49, 30.5 + head_y_off, 0), 3, vec4(0.025, 0.025, 0.1, 1.0)); // hair
	drawSphere(vec3(45.5, 30.5 + head_y_off, -1.5), 1, vec4(0.025, 0.025, 0.5, 1.0)); // eyeball
	drawSphere(vec3(45.5, 30.5 + head_y_off, 1.5), 1, vec4(0.025, 0.025, 0.5, 1.0)); // eyeball
	drawSphereRod(vec3(48, 16 + torso_y_off, 0), vec3(48, 22 + torso_y_off, 0), 4, 5, 4, vec4(0.0, 0.0, 1.0, 1.0)); // torso
	drawSphereLimbs(vec3(48, 24 + torso_y_off, -5), vec3(40, 22 + hand_y_off * 0.5, -6), vec3(36, 22 + hand_y_off, -4), 1, 0.75, 16, vec4(1.0, 0.6, 0.7, 1.0)); // arm
	drawSphereLimbs(vec3(48, 24 + torso_y_off, 5), vec3(40, 22 + hand_y_off * 0.5, 6), vec3(36, 22 + hand_y_off, 4), 1, 0.75, 16, vec4(1.0, 0.6, 0.7, 1.0)); // arm
	drawSphereLimbs(vec3(48, 14 + torso_y_off, -3), vec3(44, 9 + torso_y_off * 0.5, -(4 + torso_y_off * 0.5)), vec3(48, 0, -2), 1, 0.6, 16, vec4(1.0, 0.6, 0.7, 1.0)); // leg
	drawSphereLimbs(vec3(48, 14 + torso_y_off, 3), vec3(44, 9 + torso_y_off * 0.5, (4 + torso_y_off * 0.5)), vec3(48, 0, 2), 1, 0.6, 16, vec4(1.0, 0.6, 0.7, 1.0)); // leg
	

	// Spring-mass
	setUniform(animView->renderProg, "specularFactor", 0.0f);
	for (auto& p : points) p.draw();
	for (auto& s : springs) s.draw();

	// World constraints
	for (auto& s : planes) s.draw();
	setUniform(animView->renderProg, "iblIntensityFactor", 1.0f);
	setUniform(animView->renderProg, "roughness", 0.01f);
	setUniform(animView->renderProg, "specularFactor", 16.0f);
	for (auto& s : spheres) s.draw();
	
	// Debug axis
	drawSphere(vec3(0, 0, 0), 1, vec4(1,1,1,1));
	drawSphere(vec3(10, 0, 0), 1, vec4(1, 0, 0, 1));
	drawSphere(vec3(0, 10, 0), 1, vec4(0, 1, 0, 1));
	drawSphere(vec3(0, 0, 10), 1, vec4(0, 0, 1, 1));
	drawCylinder(vec3(0, 0, 0), vec3(10, 0, 0), 0.5, vec4(1, 1, 1, 1));
	drawCylinder(vec3(0, 0, 0), vec3(0, 10, 0), 0.5, vec4(1, 1, 1, 1));
	drawCylinder(vec3(0, 0, 0), vec3(0, 0, 10), 0.5, vec4(1, 1, 1, 1));

	setUniform(animView->renderProg, "iblIntensityFactor", 0.0f);
	setUniform(animView->renderProg, "roughness", 0.3f);
	setUniform(animView->renderProg, "specularFactor", 1.0f);

	// Picked
	if (picked >= 0)
	{
		drawSphere(points[picked].p, 1.5, vec4(1, 0, 0, 1));
		drawCylinder(points[picked].p, targetPt, 0.1, vec4(1, 0, 0, 1));
		drawSphere(targetPt, 1.5, vec4(1, 0, 0, 1));
	}

	// Checkerboard floor
	int quads = 10, quadSize = 16;
	vec3 quadOff = vec3(0, 0, -quadSize*(quads/2 + 3));
	for (auto j = 0; j < quads; j++)
	{
		for (auto i = 0; i < quads / 1.5; i++)
		{
			vec4	col1 = (((i+j) & 1) == 0) ? vec4(1.0, 1.0, 0.0, 1.0) : vec4(0.0, 1.0, 0.0, 1.0),
					col2 = (((i+j) & 1) == 0) ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 1.0, 0.0, 1.0);
			drawQuad(quadOff + vec3(quadSize * (i * 2.0f + 1.0f), 0, quadSize * (j * 2.0f + 1.0f)), vec3(0, 1, 0), vec3(quadSize), col1); // x++ y++
			drawQuad(quadOff + vec3(quadSize * (i * 2.0f + 1.0f), 0, quadSize * -(j * 2.0f + 1.0f)), vec3(0, 1, 0), vec3(quadSize), col2); // x++ y--
			drawQuad(quadOff + vec3(quadSize * -(i * 2.0f + 1.0f), 0, quadSize * (j * 2.0f + 1.0f)), vec3(0, 1, 0), vec3(quadSize), col2); // x-- y++
			drawQuad(quadOff + vec3(quadSize * -(i * 2.0f + 1.0f), 0, quadSize * -(j * 2.0f + 1.0f)), vec3(0, 1, 0), vec3(quadSize), col1); // x-- y--
		}
	}
}
/*****************************************************************
 * File:      Physics2D.hpp
 * Category:  include/FrameworkAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Lightweight 2D physics engine for game development.
 *    Provides collision detection, rigid body dynamics, gravity,
 *    and physics world management optimized for embedded systems.
 * 
 * Features:
 *    - Collision shapes: AABB, Circle, Point
 *    - Collision detection with manifold generation
 *    - Rigid body dynamics with velocity/acceleration
 *    - Configurable gravity and drag
 *    - Collision layers and masks
 *    - Trigger zones (non-physical colliders)
 *    - Simple spatial partitioning for performance
 * 
 * Constraints:
 *    - Max 64 bodies per world (configurable)
 *    - Fixed-point friendly design
 *    - No heap allocation after init
 *    - Cache-friendly memory layout
 *****************************************************************/

#ifndef ARCOS_INCLUDE_FRAMEWORKAPI_PHYSICS_2D_HPP_
#define ARCOS_INCLUDE_FRAMEWORKAPI_PHYSICS_2D_HPP_

#include <cstdint>
#include <cmath>
#include <cstring>
#include <functional>
#include <algorithm>

namespace arcos::framework {

// ============================================================
// Configuration
// ============================================================

#ifndef PHYSICS_MAX_BODIES
#define PHYSICS_MAX_BODIES 64
#endif

#ifndef PHYSICS_MAX_CONTACTS
#define PHYSICS_MAX_CONTACTS 128
#endif

// ============================================================
// 2D Vector
// ============================================================

struct Vec2 {
  float x, y;
  
  constexpr Vec2() : x(0), y(0) {}
  constexpr Vec2(float x, float y) : x(x), y(y) {}
  
  // Arithmetic operators
  Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
  Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
  Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
  Vec2 operator/(float s) const { return Vec2(x / s, y / s); }
  Vec2 operator-() const { return Vec2(-x, -y); }
  
  Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
  Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
  Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
  Vec2& operator/=(float s) { x /= s; y /= s; return *this; }
  
  // Vector operations
  float dot(const Vec2& o) const { return x * o.x + y * o.y; }
  float cross(const Vec2& o) const { return x * o.y - y * o.x; }  // 2D cross = scalar
  float lengthSq() const { return x * x + y * y; }
  float length() const { return sqrtf(lengthSq()); }
  
  Vec2 normalized() const {
    float len = length();
    return len > 0.0001f ? Vec2(x / len, y / len) : Vec2(0, 0);
  }
  
  Vec2 perpendicular() const { return Vec2(-y, x); }
  
  // Reflect vector off normal
  Vec2 reflect(const Vec2& normal) const {
    return *this - normal * (2.0f * dot(normal));
  }
  
  // Rotate vector by angle (radians)
  Vec2 rotated(float angle) const {
    float c = cosf(angle), s = sinf(angle);
    return Vec2(x * c - y * s, x * s + y * c);
  }
  
  // Distance between points
  float distanceTo(const Vec2& o) const { return (*this - o).length(); }
  float distanceToSq(const Vec2& o) const { return (*this - o).lengthSq(); }
  
  // Lerp between vectors
  static Vec2 lerp(const Vec2& a, const Vec2& b, float t) {
    return a + (b - a) * t;
  }
  
  // Common vectors
  static constexpr Vec2 zero() { return Vec2(0, 0); }
  static constexpr Vec2 one() { return Vec2(1, 1); }
  static constexpr Vec2 up() { return Vec2(0, -1); }      // Screen coords: -Y is up
  static constexpr Vec2 down() { return Vec2(0, 1); }
  static constexpr Vec2 left() { return Vec2(-1, 0); }
  static constexpr Vec2 right() { return Vec2(1, 0); }
};

// ============================================================
// Axis-Aligned Bounding Box
// ============================================================

struct AABB {
  Vec2 min;  // Top-left corner
  Vec2 max;  // Bottom-right corner
  
  constexpr AABB() : min(), max() {}
  constexpr AABB(Vec2 min, Vec2 max) : min(min), max(max) {}
  
  static AABB fromCenter(Vec2 center, Vec2 halfSize) {
    return AABB(center - halfSize, center + halfSize);
  }
  
  static AABB fromPosSize(Vec2 pos, Vec2 size) {
    return AABB(pos, pos + size);
  }
  
  Vec2 center() const { return (min + max) * 0.5f; }
  Vec2 size() const { return max - min; }
  Vec2 halfSize() const { return size() * 0.5f; }
  float width() const { return max.x - min.x; }
  float height() const { return max.y - min.y; }
  
  bool contains(const Vec2& point) const {
    return point.x >= min.x && point.x <= max.x &&
           point.y >= min.y && point.y <= max.y;
  }
  
  bool overlaps(const AABB& other) const {
    return !(max.x < other.min.x || min.x > other.max.x ||
             max.y < other.min.y || min.y > other.max.y);
  }
  
  // Expand AABB to include point
  void include(const Vec2& point) {
    min.x = fminf(min.x, point.x);
    min.y = fminf(min.y, point.y);
    max.x = fmaxf(max.x, point.x);
    max.y = fmaxf(max.y, point.y);
  }
  
  // Expand AABB by margin
  AABB expanded(float margin) const {
    return AABB(min - Vec2(margin, margin), max + Vec2(margin, margin));
  }
};

// ============================================================
// Collision Shape Types
// ============================================================

enum class ShapeType : uint8_t {
  NONE = 0,
  AABB,       // Axis-aligned bounding box
  CIRCLE,     // Circle
  SEGMENT,    // Line segment (for raycasting, thin platforms)
};

/**
 * Collision shape definition
 */
struct CollisionShape {
  ShapeType type = ShapeType::NONE;
  
  union {
    struct { float halfWidth, halfHeight; } box;   // AABB half-extents
    struct { float radius; } circle;                // Circle
    struct { float startX, startY, endX, endY; } segment;  // Line segment (raw floats for trivial union)
  };
  
  Vec2 offset;  // Offset from body position
  
  // Default constructor
  CollisionShape() : type(ShapeType::NONE), offset(0, 0) {
    box.halfWidth = 0;
    box.halfHeight = 0;
  }
  
  // Constructors
  static CollisionShape makeBox(float width, float height, Vec2 off = Vec2::zero()) {
    CollisionShape s;
    s.type = ShapeType::AABB;
    s.box.halfWidth = width * 0.5f;
    s.box.halfHeight = height * 0.5f;
    s.offset = off;
    return s;
  }
  
  static CollisionShape makeCircle(float radius, Vec2 off = Vec2::zero()) {
    CollisionShape s;
    s.type = ShapeType::CIRCLE;
    s.circle.radius = radius;
    s.offset = off;
    return s;
  }
  
  static CollisionShape makeSegment(Vec2 start, Vec2 end) {
    CollisionShape s;
    s.type = ShapeType::SEGMENT;
    s.segment.startX = start.x;
    s.segment.startY = start.y;
    s.segment.endX = end.x;
    s.segment.endY = end.y;
    s.offset = Vec2::zero();
    return s;
  }
  
  // Get world-space AABB for broad phase
  AABB getWorldAABB(Vec2 position) const {
    Vec2 pos = position + offset;
    switch (type) {
      case ShapeType::AABB:
        return AABB(Vec2(pos.x - box.halfWidth, pos.y - box.halfHeight),
                   Vec2(pos.x + box.halfWidth, pos.y + box.halfHeight));
      case ShapeType::CIRCLE:
        return AABB(Vec2(pos.x - circle.radius, pos.y - circle.radius),
                   Vec2(pos.x + circle.radius, pos.y + circle.radius));
      case ShapeType::SEGMENT: {
        AABB aabb;
        aabb.min.x = fminf(segment.startX, segment.endX) + position.x;
        aabb.min.y = fminf(segment.startY, segment.endY) + position.y;
        aabb.max.x = fmaxf(segment.startX, segment.endX) + position.x;
        aabb.max.y = fmaxf(segment.startY, segment.endY) + position.y;
        return aabb;
      }
      default:
        return AABB(pos, pos);
    }
  }
};

// ============================================================
// Collision Layers
// ============================================================

/**
 * Collision layer bitmask (16 layers available)
 */
using LayerMask = uint16_t;

namespace Layer {
  constexpr LayerMask NONE     = 0x0000;
  constexpr LayerMask DEFAULT  = 0x0001;
  constexpr LayerMask PLAYER   = 0x0002;
  constexpr LayerMask ENEMY    = 0x0004;
  constexpr LayerMask GROUND   = 0x0008;
  constexpr LayerMask OBSTACLE = 0x0010;
  constexpr LayerMask TRIGGER  = 0x0020;
  constexpr LayerMask PICKUP   = 0x0040;
  constexpr LayerMask BULLET   = 0x0080;
  constexpr LayerMask UI       = 0x0100;
  constexpr LayerMask ALL      = 0xFFFF;
  
  // User-defined layers (custom1-custom7)
  constexpr LayerMask CUSTOM1  = 0x0200;
  constexpr LayerMask CUSTOM2  = 0x0400;
  constexpr LayerMask CUSTOM3  = 0x0800;
  constexpr LayerMask CUSTOM4  = 0x1000;
  constexpr LayerMask CUSTOM5  = 0x2000;
  constexpr LayerMask CUSTOM6  = 0x4000;
  constexpr LayerMask CUSTOM7  = 0x8000;
}

// ============================================================
// Rigid Body Definition
// ============================================================

/**
 * Body type determines physics behavior
 */
enum class BodyType : uint8_t {
  STATIC,     // Never moves (walls, ground)
  KINEMATIC,  // Moves via velocity, ignores forces (moving platforms)
  DYNAMIC,    // Full physics simulation
};

/**
 * Material properties
 */
struct PhysicsMaterial {
  float friction = 0.3f;     // 0 = frictionless, 1 = max friction
  float restitution = 0.0f;  // 0 = no bounce, 1 = perfect bounce
  float density = 1.0f;      // Affects mass calculation
  
  static PhysicsMaterial rubber() { return {0.8f, 0.7f, 1.0f}; }
  static PhysicsMaterial ice() { return {0.02f, 0.1f, 1.0f}; }
  static PhysicsMaterial wood() { return {0.4f, 0.2f, 0.6f}; }
  static PhysicsMaterial metal() { return {0.3f, 0.1f, 7.8f}; }
  static PhysicsMaterial bouncy() { return {0.2f, 0.95f, 1.0f}; }
};

/**
 * Rigid body flags
 */
struct BodyFlags {
  bool isTrigger : 1;        // No physical response, only callbacks
  bool fixedRotation : 1;    // Prevent rotation
  bool isBullet : 1;         // Enable continuous collision detection
  bool isEnabled : 1;        // Active in simulation
  bool isSleeping : 1;       // Temporarily disabled for optimization
  bool gravityEnabled : 1;   // Affected by world gravity
  uint8_t reserved : 2;
  
  BodyFlags() : isTrigger(false), fixedRotation(false), isBullet(false),
                isEnabled(true), isSleeping(false), gravityEnabled(true), reserved(0) {}
};

/**
 * Physics body ID (handle)
 */
using BodyId = int16_t;
constexpr BodyId INVALID_BODY = -1;

/**
 * Rigid body structure
 */
struct RigidBody {
  // Identity
  BodyId id = INVALID_BODY;
  BodyType type = BodyType::DYNAMIC;
  void* userData = nullptr;  // Application-specific data
  
  // Transform
  Vec2 position;
  float rotation = 0.0f;     // Radians
  
  // Physics state
  Vec2 velocity;
  Vec2 acceleration;         // Applied forces / mass
  float angularVelocity = 0.0f;
  
  // Properties
  float mass = 1.0f;
  float invMass = 1.0f;      // 1/mass (0 for static)
  float inertia = 1.0f;      // Rotational inertia
  float invInertia = 1.0f;
  float linearDamping = 0.1f;  // Air resistance
  float angularDamping = 0.1f;
  
  // Collision
  CollisionShape shape;
  LayerMask layer = Layer::DEFAULT;
  LayerMask collisionMask = Layer::ALL;  // What layers to collide with
  PhysicsMaterial material;
  BodyFlags flags;
  
  // Default constructor
  RigidBody() : position(0, 0), velocity(0, 0), acceleration(0, 0) {}
  
  // Methods
  bool isValid() const { return id != INVALID_BODY; }
  bool isStatic() const { return type == BodyType::STATIC; }
  bool isDynamic() const { return type == BodyType::DYNAMIC; }
  bool isKinematic() const { return type == BodyType::KINEMATIC; }
  
  void setMass(float m) {
    mass = m;
    invMass = (m > 0.0f && type == BodyType::DYNAMIC) ? 1.0f / m : 0.0f;
  }
  
  // Apply forces
  void applyForce(Vec2 force) {
    if (type == BodyType::DYNAMIC && flags.isEnabled) {
      acceleration += force * invMass;
    }
  }
  
  void applyImpulse(Vec2 impulse) {
    if (type == BodyType::DYNAMIC && flags.isEnabled) {
      velocity += impulse * invMass;
    }
  }
  
  void applyTorque(float torque) {
    if (type == BodyType::DYNAMIC && flags.isEnabled && !flags.fixedRotation) {
      angularVelocity += torque * invInertia;
    }
  }
  
  // Get world-space AABB
  AABB getWorldAABB() const {
    return shape.getWorldAABB(position);
  }
};

// ============================================================
// Collision Detection Results
// ============================================================

/**
 * Contact point between two bodies
 */
struct ContactPoint {
  Vec2 point;       // World-space contact point
  Vec2 normal;      // Contact normal (from A to B)
  float penetration; // Overlap depth
};

/**
 * Collision manifold (collision result)
 */
struct CollisionManifold {
  BodyId bodyA = INVALID_BODY;
  BodyId bodyB = INVALID_BODY;
  ContactPoint contacts[2];  // Max 2 contacts for 2D
  uint8_t contactCount = 0;
  bool isTrigger = false;    // True if either body is a trigger
  
  bool isValid() const { return contactCount > 0; }
};

/**
 * Raycast result
 */
struct RaycastHit {
  BodyId bodyId = INVALID_BODY;
  Vec2 point;      // Hit point in world space
  Vec2 normal;     // Surface normal at hit
  float distance;  // Distance along ray
  
  bool hit() const { return bodyId != INVALID_BODY; }
};

// ============================================================
// Collision Callbacks
// ============================================================

using CollisionCallback = std::function<void(BodyId bodyA, BodyId bodyB, const CollisionManifold& manifold)>;
using TriggerCallback = std::function<void(BodyId trigger, BodyId other)>;

// ============================================================
// Collision Detection Functions
// ============================================================

namespace Collision {

/**
 * Test AABB vs AABB collision
 */
inline bool testAABBvsAABB(const AABB& a, const AABB& b, ContactPoint* contact = nullptr) {
  float dx = (a.center().x) - (b.center().x);
  float dy = (a.center().y) - (b.center().y);
  float px = (a.halfSize().x + b.halfSize().x) - fabsf(dx);
  float py = (a.halfSize().y + b.halfSize().y) - fabsf(dy);
  
  if (px <= 0 || py <= 0) return false;
  
  if (contact) {
    if (px < py) {
      contact->normal = Vec2(dx > 0 ? 1.0f : -1.0f, 0);
      contact->penetration = px;
      contact->point = Vec2(a.center().x + contact->normal.x * a.halfSize().x, a.center().y);
    } else {
      contact->normal = Vec2(0, dy > 0 ? 1.0f : -1.0f);
      contact->penetration = py;
      contact->point = Vec2(a.center().x, a.center().y + contact->normal.y * a.halfSize().y);
    }
  }
  return true;
}

/**
 * Test Circle vs Circle collision
 */
inline bool testCirclevsCircle(Vec2 posA, float radiusA, Vec2 posB, float radiusB, ContactPoint* contact = nullptr) {
  Vec2 delta = posB - posA;
  float distSq = delta.lengthSq();
  float radiusSum = radiusA + radiusB;
  
  if (distSq >= radiusSum * radiusSum) return false;
  
  if (contact) {
    float dist = sqrtf(distSq);
    contact->normal = dist > 0.0001f ? delta / dist : Vec2(1, 0);
    contact->penetration = radiusSum - dist;
    contact->point = posA + contact->normal * radiusA;
  }
  return true;
}

/**
 * Test AABB vs Circle collision
 */
inline bool testAABBvsCircle(const AABB& box, Vec2 circlePos, float radius, ContactPoint* contact = nullptr) {
  // Find closest point on AABB to circle center
  Vec2 closest;
  closest.x = fmaxf(box.min.x, fminf(circlePos.x, box.max.x));
  closest.y = fmaxf(box.min.y, fminf(circlePos.y, box.max.y));
  
  Vec2 delta = circlePos - closest;
  float distSq = delta.lengthSq();
  
  if (distSq >= radius * radius) return false;
  
  if (contact) {
    float dist = sqrtf(distSq);
    if (dist > 0.0001f) {
      contact->normal = delta / dist;
      contact->penetration = radius - dist;
    } else {
      // Circle center inside box - push out via shortest axis
      Vec2 center = box.center();
      Vec2 half = box.halfSize();
      float dx = circlePos.x - center.x;
      float dy = circlePos.y - center.y;
      float px = half.x - fabsf(dx);
      float py = half.y - fabsf(dy);
      
      if (px < py) {
        contact->normal = Vec2(dx > 0 ? 1.0f : -1.0f, 0);
        contact->penetration = px + radius;
      } else {
        contact->normal = Vec2(0, dy > 0 ? 1.0f : -1.0f);
        contact->penetration = py + radius;
      }
    }
    contact->point = closest;
  }
  return true;
}

/**
 * Test Circle vs AABB (swapped version)
 */
inline bool testCirclevsAABB(Vec2 circlePos, float radius, const AABB& box, ContactPoint* contact = nullptr) {
  bool result = testAABBvsCircle(box, circlePos, radius, contact);
  if (result && contact) {
    contact->normal = -contact->normal;  // Flip normal
  }
  return result;
}

/**
 * Test point inside shape
 */
inline bool testPointInAABB(Vec2 point, const AABB& box) {
  return box.contains(point);
}

inline bool testPointInCircle(Vec2 point, Vec2 circlePos, float radius) {
  return (point - circlePos).lengthSq() <= radius * radius;
}

/**
 * Raycast against AABB
 */
inline bool raycastAABB(Vec2 origin, Vec2 direction, const AABB& box, float maxDist, RaycastHit* hit) {
  Vec2 invDir = Vec2(1.0f / direction.x, 1.0f / direction.y);
  
  float t1 = (box.min.x - origin.x) * invDir.x;
  float t2 = (box.max.x - origin.x) * invDir.x;
  float t3 = (box.min.y - origin.y) * invDir.y;
  float t4 = (box.max.y - origin.y) * invDir.y;
  
  float tmin = fmaxf(fminf(t1, t2), fminf(t3, t4));
  float tmax = fminf(fmaxf(t1, t2), fmaxf(t3, t4));
  
  if (tmax < 0 || tmin > tmax || tmin > maxDist) return false;
  
  float t = tmin >= 0 ? tmin : tmax;
  if (t > maxDist) return false;
  
  if (hit) {
    hit->distance = t;
    hit->point = origin + direction * t;
    
    // Calculate hit normal
    Vec2 p = hit->point;
    float epsilon = 0.001f;
    if (fabsf(p.x - box.min.x) < epsilon) hit->normal = Vec2(-1, 0);
    else if (fabsf(p.x - box.max.x) < epsilon) hit->normal = Vec2(1, 0);
    else if (fabsf(p.y - box.min.y) < epsilon) hit->normal = Vec2(0, -1);
    else hit->normal = Vec2(0, 1);
  }
  return true;
}

/**
 * Raycast against Circle
 */
inline bool raycastCircle(Vec2 origin, Vec2 direction, Vec2 circlePos, float radius, float maxDist, RaycastHit* hit) {
  Vec2 oc = origin - circlePos;
  float a = direction.dot(direction);
  float b = 2.0f * oc.dot(direction);
  float c = oc.dot(oc) - radius * radius;
  float discriminant = b * b - 4 * a * c;
  
  if (discriminant < 0) return false;
  
  float t = (-b - sqrtf(discriminant)) / (2.0f * a);
  if (t < 0 || t > maxDist) {
    t = (-b + sqrtf(discriminant)) / (2.0f * a);
    if (t < 0 || t > maxDist) return false;
  }
  
  if (hit) {
    hit->distance = t;
    hit->point = origin + direction * t;
    hit->normal = (hit->point - circlePos).normalized();
  }
  return true;
}

} // namespace Collision

// ============================================================
// Physics World
// ============================================================

/**
 * Physics world configuration
 */
struct PhysicsConfig {
  Vec2 gravity = Vec2(0, 980.0f);  // Pixels/sec² (positive Y = down)
  float fixedTimeStep = 1.0f / 60.0f;  // 60 Hz physics
  int maxSubSteps = 4;              // Max iterations per frame
  int velocityIterations = 4;       // Collision resolution iterations
  float sleepVelocityThreshold = 5.0f;  // Auto-sleep below this velocity
  float sleepTimeThreshold = 0.5f;      // Seconds before sleeping
  bool allowSleep = true;
  AABB worldBounds = AABB(Vec2(-1000, -1000), Vec2(2000, 2000));
};

/**
 * Physics World - manages all bodies and simulation
 */
class PhysicsWorld {
public:
  PhysicsWorld() { clear(); }
  
  /**
   * Initialize the physics world
   */
  void init(const PhysicsConfig& config = PhysicsConfig()) {
    config_ = config;
    clear();
  }
  
  /**
   * Clear all bodies and reset world
   */
  void clear() {
    bodyCount_ = 0;
    manifoldCount_ = 0;
    for (int i = 0; i < PHYSICS_MAX_BODIES; i++) {
      bodies_[i].id = INVALID_BODY;
    }
    accumulator_ = 0.0f;
  }
  
  /**
   * Create a new rigid body
   */
  BodyId createBody(BodyType type = BodyType::DYNAMIC) {
    if (bodyCount_ >= PHYSICS_MAX_BODIES) return INVALID_BODY;
    
    // Find free slot
    for (int i = 0; i < PHYSICS_MAX_BODIES; i++) {
      if (bodies_[i].id == INVALID_BODY) {
        bodies_[i] = RigidBody();
        bodies_[i].id = static_cast<BodyId>(i);
        bodies_[i].type = type;
        if (type == BodyType::STATIC) {
          bodies_[i].invMass = 0.0f;
          bodies_[i].invInertia = 0.0f;
        }
        bodyCount_++;
        return bodies_[i].id;
      }
    }
    return INVALID_BODY;
  }
  
  /**
   * Destroy a body
   */
  void destroyBody(BodyId id) {
    if (id >= 0 && id < PHYSICS_MAX_BODIES && bodies_[id].id != INVALID_BODY) {
      bodies_[id].id = INVALID_BODY;
      bodyCount_--;
    }
  }
  
  /**
   * Get body by ID
   */
  RigidBody* getBody(BodyId id) {
    if (id >= 0 && id < PHYSICS_MAX_BODIES && bodies_[id].id != INVALID_BODY) {
      return &bodies_[id];
    }
    return nullptr;
  }
  
  const RigidBody* getBody(BodyId id) const {
    if (id >= 0 && id < PHYSICS_MAX_BODIES && bodies_[id].id != INVALID_BODY) {
      return &bodies_[id];
    }
    return nullptr;
  }
  
  /**
   * Step the simulation
   * @param dt Delta time in seconds
   */
  void step(float dt) {
    if (dt <= 0) return;
    
    accumulator_ += dt;
    int steps = 0;
    
    while (accumulator_ >= config_.fixedTimeStep && steps < config_.maxSubSteps) {
      fixedStep(config_.fixedTimeStep);
      accumulator_ -= config_.fixedTimeStep;
      steps++;
    }
  }
  
  /**
   * Set collision callback
   */
  void setCollisionCallback(CollisionCallback callback) {
    collisionCallback_ = callback;
  }
  
  /**
   * Set trigger callback
   */
  void setTriggerCallback(TriggerCallback callback) {
    triggerCallback_ = callback;
  }
  
  /**
   * Set world gravity
   */
  void setGravity(Vec2 gravity) {
    config_.gravity = gravity;
  }
  
  Vec2 getGravity() const { return config_.gravity; }
  
  /**
   * Raycast into the world
   */
  RaycastHit raycast(Vec2 origin, Vec2 direction, float maxDistance = 1000.0f, 
                     LayerMask mask = Layer::ALL) const {
    RaycastHit bestHit;
    bestHit.bodyId = INVALID_BODY;
    bestHit.distance = maxDistance;
    
    direction = direction.normalized();
    
    for (int i = 0; i < PHYSICS_MAX_BODIES; i++) {
      const RigidBody& body = bodies_[i];
      if (body.id == INVALID_BODY || !body.flags.isEnabled) continue;
      if (!(body.layer & mask)) continue;
      
      RaycastHit hit;
      bool didHit = false;
      
      switch (body.shape.type) {
        case ShapeType::AABB:
          didHit = Collision::raycastAABB(origin, direction, body.getWorldAABB(), 
                                          bestHit.distance, &hit);
          break;
        case ShapeType::CIRCLE:
          didHit = Collision::raycastCircle(origin, direction, 
                                            body.position + body.shape.offset,
                                            body.shape.circle.radius,
                                            bestHit.distance, &hit);
          break;
        default:
          break;
      }
      
      if (didHit && hit.distance < bestHit.distance) {
        bestHit = hit;
        bestHit.bodyId = body.id;
      }
    }
    
    return bestHit;
  }
  
  /**
   * Query all bodies overlapping an AABB
   */
  int queryAABB(const AABB& aabb, BodyId* results, int maxResults, 
                LayerMask mask = Layer::ALL) const {
    int count = 0;
    for (int i = 0; i < PHYSICS_MAX_BODIES && count < maxResults; i++) {
      const RigidBody& body = bodies_[i];
      if (body.id == INVALID_BODY || !body.flags.isEnabled) continue;
      if (!(body.layer & mask)) continue;
      
      if (body.getWorldAABB().overlaps(aabb)) {
        results[count++] = body.id;
      }
    }
    return count;
  }
  
  /**
   * Query all bodies overlapping a circle
   */
  int queryCircle(Vec2 center, float radius, BodyId* results, int maxResults,
                  LayerMask mask = Layer::ALL) const {
    int count = 0;
    AABB queryBounds(Vec2(center.x - radius, center.y - radius),
                     Vec2(center.x + radius, center.y + radius));
    
    for (int i = 0; i < PHYSICS_MAX_BODIES && count < maxResults; i++) {
      const RigidBody& body = bodies_[i];
      if (body.id == INVALID_BODY || !body.flags.isEnabled) continue;
      if (!(body.layer & mask)) continue;
      
      // Broad phase
      if (!body.getWorldAABB().overlaps(queryBounds)) continue;
      
      // Narrow phase
      bool overlap = false;
      switch (body.shape.type) {
        case ShapeType::AABB:
          overlap = Collision::testAABBvsCircle(body.getWorldAABB(), center, radius);
          break;
        case ShapeType::CIRCLE:
          overlap = Collision::testCirclevsCircle(
            body.position + body.shape.offset, body.shape.circle.radius,
            center, radius);
          break;
        default:
          break;
      }
      
      if (overlap) {
        results[count++] = body.id;
      }
    }
    return count;
  }
  
  /**
   * Get body count
   */
  int getBodyCount() const { return bodyCount_; }
  
  /**
   * Get configuration
   */
  const PhysicsConfig& getConfig() const { return config_; }
  
  /**
   * Iterate all active bodies
   */
  template<typename Func>
  void forEachBody(Func func) {
    for (int i = 0; i < PHYSICS_MAX_BODIES; i++) {
      if (bodies_[i].id != INVALID_BODY) {
        func(bodies_[i]);
      }
    }
  }

private:
  RigidBody bodies_[PHYSICS_MAX_BODIES];
  CollisionManifold manifolds_[PHYSICS_MAX_CONTACTS];
  int bodyCount_ = 0;
  int manifoldCount_ = 0;
  float accumulator_ = 0.0f;
  
  PhysicsConfig config_;
  CollisionCallback collisionCallback_;
  TriggerCallback triggerCallback_;
  
  /**
   * Fixed timestep physics update
   */
  void fixedStep(float dt) {
    // 1. Apply gravity and integrate velocities
    for (int i = 0; i < PHYSICS_MAX_BODIES; i++) {
      RigidBody& body = bodies_[i];
      if (body.id == INVALID_BODY || !body.flags.isEnabled || body.flags.isSleeping) continue;
      if (body.type != BodyType::DYNAMIC) continue;
      
      // Apply gravity
      if (body.flags.gravityEnabled) {
        body.velocity += config_.gravity * dt;
      }
      
      // Apply acceleration (from forces)
      body.velocity += body.acceleration * dt;
      body.acceleration = Vec2::zero();  // Clear accumulated forces
      
      // Apply damping
      body.velocity *= 1.0f / (1.0f + body.linearDamping * dt);
      body.angularVelocity *= 1.0f / (1.0f + body.angularDamping * dt);
    }
    
    // 2. Detect collisions
    detectCollisions();
    
    // 3. Resolve collisions
    for (int iter = 0; iter < config_.velocityIterations; iter++) {
      resolveCollisions();
    }
    
    // 4. Integrate positions
    for (int i = 0; i < PHYSICS_MAX_BODIES; i++) {
      RigidBody& body = bodies_[i];
      if (body.id == INVALID_BODY || !body.flags.isEnabled || body.flags.isSleeping) continue;
      if (body.type == BodyType::STATIC) continue;
      
      body.position += body.velocity * dt;
      if (!body.flags.fixedRotation) {
        body.rotation += body.angularVelocity * dt;
      }
    }
    
    // 5. Fire callbacks
    for (int i = 0; i < manifoldCount_; i++) {
      CollisionManifold& m = manifolds_[i];
      if (m.isTrigger) {
        if (triggerCallback_) {
          triggerCallback_(m.bodyA, m.bodyB);
        }
      } else {
        if (collisionCallback_) {
          collisionCallback_(m.bodyA, m.bodyB, m);
        }
      }
    }
  }
  
  /**
   * Broad + narrow phase collision detection
   */
  void detectCollisions() {
    manifoldCount_ = 0;
    
    for (int i = 0; i < PHYSICS_MAX_BODIES; i++) {
      RigidBody& a = bodies_[i];
      if (a.id == INVALID_BODY || !a.flags.isEnabled) continue;
      
      for (int j = i + 1; j < PHYSICS_MAX_BODIES; j++) {
        RigidBody& b = bodies_[j];
        if (b.id == INVALID_BODY || !b.flags.isEnabled) continue;
        
        // Skip if both static
        if (a.type == BodyType::STATIC && b.type == BodyType::STATIC) continue;
        
        // Layer filtering
        if (!(a.collisionMask & b.layer) || !(b.collisionMask & a.layer)) continue;
        
        // Broad phase: AABB test
        AABB aabbA = a.getWorldAABB();
        AABB aabbB = b.getWorldAABB();
        if (!aabbA.overlaps(aabbB)) continue;
        
        // Narrow phase: shape-specific test
        ContactPoint contact;
        bool colliding = testShapes(a, b, &contact);
        
        if (colliding && manifoldCount_ < PHYSICS_MAX_CONTACTS) {
          CollisionManifold& m = manifolds_[manifoldCount_++];
          m.bodyA = a.id;
          m.bodyB = b.id;
          m.contacts[0] = contact;
          m.contactCount = 1;
          m.isTrigger = a.flags.isTrigger || b.flags.isTrigger;
        }
      }
    }
  }
  
  /**
   * Test two shapes for collision
   */
  bool testShapes(const RigidBody& a, const RigidBody& b, ContactPoint* contact) {
    Vec2 posA = a.position + a.shape.offset;
    Vec2 posB = b.position + b.shape.offset;
    
    if (a.shape.type == ShapeType::AABB && b.shape.type == ShapeType::AABB) {
      return Collision::testAABBvsAABB(a.getWorldAABB(), b.getWorldAABB(), contact);
    }
    else if (a.shape.type == ShapeType::CIRCLE && b.shape.type == ShapeType::CIRCLE) {
      return Collision::testCirclevsCircle(posA, a.shape.circle.radius, 
                                           posB, b.shape.circle.radius, contact);
    }
    else if (a.shape.type == ShapeType::AABB && b.shape.type == ShapeType::CIRCLE) {
      return Collision::testAABBvsCircle(a.getWorldAABB(), posB, b.shape.circle.radius, contact);
    }
    else if (a.shape.type == ShapeType::CIRCLE && b.shape.type == ShapeType::AABB) {
      bool result = Collision::testCirclevsAABB(posA, a.shape.circle.radius, b.getWorldAABB(), contact);
      return result;
    }
    
    return false;
  }
  
  /**
   * Resolve collisions with impulse-based response
   */
  void resolveCollisions() {
    for (int i = 0; i < manifoldCount_; i++) {
      CollisionManifold& m = manifolds_[i];
      if (m.isTrigger) continue;  // Skip triggers
      
      RigidBody* a = getBody(m.bodyA);
      RigidBody* b = getBody(m.bodyB);
      if (!a || !b) continue;
      
      for (int c = 0; c < m.contactCount; c++) {
        ContactPoint& contact = m.contacts[c];
        
        // Relative velocity at contact
        Vec2 relVel = b->velocity - a->velocity;
        float velAlongNormal = relVel.dot(contact.normal);
        
        // Don't resolve if separating
        if (velAlongNormal > 0) continue;
        
        // Restitution (bounciness)
        float e = fminf(a->material.restitution, b->material.restitution);
        
        // Impulse magnitude
        float invMassSum = a->invMass + b->invMass;
        if (invMassSum <= 0) continue;
        
        float j = -(1.0f + e) * velAlongNormal / invMassSum;
        
        // Apply impulse
        Vec2 impulse = contact.normal * j;
        a->velocity -= impulse * a->invMass;
        b->velocity += impulse * b->invMass;
        
        // Friction
        Vec2 tangent = relVel - contact.normal * velAlongNormal;
        if (tangent.lengthSq() > 0.0001f) {
          tangent = tangent.normalized();
          float jt = -relVel.dot(tangent) / invMassSum;
          float mu = sqrtf(a->material.friction * b->material.friction);
          
          // Clamp friction impulse (Coulomb's law)
          Vec2 frictionImpulse;
          if (fabsf(jt) < j * mu) {
            frictionImpulse = tangent * jt;
          } else {
            frictionImpulse = tangent * (-j * mu);
          }
          
          a->velocity -= frictionImpulse * a->invMass;
          b->velocity += frictionImpulse * b->invMass;
        }
        
        // Positional correction (prevent sinking)
        const float slop = 0.01f;
        const float percent = 0.2f;
        float correction = fmaxf(contact.penetration - slop, 0.0f) * percent / invMassSum;
        Vec2 correctionVec = contact.normal * correction;
        a->position -= correctionVec * a->invMass;
        b->position += correctionVec * b->invMass;
      }
    }
  }
};

// ============================================================
// Helper Functions
// ============================================================

/**
 * Create a static ground platform
 */
inline BodyId createGround(PhysicsWorld& world, float x, float y, float width, float height) {
  BodyId id = world.createBody(BodyType::STATIC);
  if (id != INVALID_BODY) {
    RigidBody* body = world.getBody(id);
    body->position = Vec2(x + width * 0.5f, y + height * 0.5f);
    body->shape = CollisionShape::makeBox(width, height);
    body->layer = Layer::GROUND;
  }
  return id;
}

/**
 * Create a dynamic box
 */
inline BodyId createDynamicBox(PhysicsWorld& world, float x, float y, float width, float height, float mass = 1.0f) {
  BodyId id = world.createBody(BodyType::DYNAMIC);
  if (id != INVALID_BODY) {
    RigidBody* body = world.getBody(id);
    body->position = Vec2(x, y);
    body->shape = CollisionShape::makeBox(width, height);
    body->setMass(mass);
  }
  return id;
}

/**
 * Create a dynamic circle
 */
inline BodyId createDynamicCircle(PhysicsWorld& world, float x, float y, float radius, float mass = 1.0f) {
  BodyId id = world.createBody(BodyType::DYNAMIC);
  if (id != INVALID_BODY) {
    RigidBody* body = world.getBody(id);
    body->position = Vec2(x, y);
    body->shape = CollisionShape::makeCircle(radius);
    body->setMass(mass);
  }
  return id;
}

/**
 * Create a trigger zone
 */
inline BodyId createTrigger(PhysicsWorld& world, float x, float y, float width, float height) {
  BodyId id = world.createBody(BodyType::STATIC);
  if (id != INVALID_BODY) {
    RigidBody* body = world.getBody(id);
    body->position = Vec2(x + width * 0.5f, y + height * 0.5f);
    body->shape = CollisionShape::makeBox(width, height);
    body->flags.isTrigger = true;
    body->layer = Layer::TRIGGER;
  }
  return id;
}

} // namespace arcos::framework

#endif // ARCOS_INCLUDE_FRAMEWORKAPI_PHYSICS_2D_HPP_

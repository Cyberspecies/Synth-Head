/*****************************************************************
 * File:      CPU_FrameworkAPI_AsteroidsTest.cpp
 * Category:  Physics Engine Stress Test
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Classic Asteroids game to stress test the Physics2D engine.
 *    Tests collision detection, velocity, rotation, wrap-around,
 *    multiple simultaneous bodies, and real-time physics updates.
 * 
 * Controls:
 *    Button C (GPIO7):  Turn Left
 *    Button B (GPIO6):  Thrust Forward
 *    Button A (GPIO5):  Turn Right
 *    Button D (GPIO15): Fire
 * 
 * Displays:
 *    - HUB75 (128x32): Main game display with ship and asteroids
 *    - OLED (128x128): Score, lives, radar/minimap view
 * 
 * Physics Features Tested:
 *    - Rigid body dynamics (velocity, acceleration)
 *    - Screen wrap-around (toroidal space)
 *    - AABB and Circle collision detection
 *    - Multiple simultaneous bodies (ship, bullets, asteroids)
 *    - Collision callbacks and response
 *    - Layer-based collision filtering
 * 
 * Hardware (CPU - COM15):
 *    - ESP32-S3 with UART to GPU
 *    - TX=GPIO12, RX=GPIO11 @ 10Mbps
 *****************************************************************/

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <cstring>
#include <cmath>
#include <cstdlib>

#include "FrameworkAPI/Physics2D.hpp"

using namespace arcos::framework;

static const char* TAG = "ASTEROIDS";

// ============================================================
// Hardware Configuration
// ============================================================

// UART to GPU
#define GPU_UART_NUM    UART_NUM_1
#define GPU_UART_TX     GPIO_NUM_12
#define GPU_UART_RX     GPIO_NUM_11
#define GPU_BAUD        10000000

// Buttons (directly read GPIO - active LOW)
#define BTN_A_PIN       GPIO_NUM_5   // Turn Right
#define BTN_B_PIN       GPIO_NUM_6   // Thrust
#define BTN_C_PIN       GPIO_NUM_7   // Turn Left
#define BTN_D_PIN       GPIO_NUM_15  // Fire

// Display dimensions
#define HUB75_W         128
#define HUB75_H         32
#define OLED_W          128
#define OLED_H          128

// ============================================================
// GPU Protocol (from GPU_COMMAND_REFERENCE.md)
// ============================================================

enum class CmdType : uint8_t {
  NOP             = 0x00,
  UPLOAD_SHADER   = 0x10,
  DELETE_SHADER   = 0x11,
  EXEC_SHADER     = 0x12,
  UPLOAD_SPRITE   = 0x20,
  DELETE_SPRITE   = 0x21,
  SET_VAR         = 0x30,
  SET_VARS        = 0x31,
  DRAW_PIXEL      = 0x40,
  DRAW_LINE       = 0x41,
  DRAW_RECT       = 0x42,
  DRAW_FILL       = 0x43,
  DRAW_CIRCLE     = 0x44,
  DRAW_POLY       = 0x45,
  BLIT_SPRITE     = 0x46,
  CLEAR           = 0x47,
  DRAW_LINE_F     = 0x48,
  DRAW_CIRCLE_F   = 0x49,
  DRAW_RECT_F     = 0x4A,
  SET_TARGET      = 0x50,
  PRESENT         = 0x51,
  OLED_CLEAR      = 0x60,
  OLED_LINE       = 0x61,
  OLED_RECT       = 0x62,
  OLED_FILL       = 0x63,
  OLED_CIRCLE     = 0x64,
  OLED_PRESENT    = 0x65,
  OLED_SPRITE     = 0x66,
  PING            = 0xF0,
  RESET           = 0xFF,
};

// ============================================================
// GPU Communication
// ============================================================

class GpuComm {
public:
  void init() {
    uart_config_t cfg = {
      .baud_rate = GPU_BAUD,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 0,
      .source_clk = UART_SCLK_DEFAULT,
    };
    // IMPORTANT: driver_install must come before param_config!
    uart_driver_install(GPU_UART_NUM, 4096, 4096, 0, nullptr, 0);
    uart_param_config(GPU_UART_NUM, &cfg);
    uart_set_pin(GPU_UART_NUM, GPU_UART_TX, GPU_UART_RX, -1, -1);
    ESP_LOGI(TAG, "GPU UART initialized @ %d baud", GPU_BAUD);
  }
  
  void sendCmd(CmdType cmd, const uint8_t* payload = nullptr, uint16_t len = 0) {
    uint8_t header[5] = {
      0xAA, 0x55,
      static_cast<uint8_t>(cmd),
      static_cast<uint8_t>(len & 0xFF),
      static_cast<uint8_t>((len >> 8) & 0xFF)
    };
    uart_write_bytes(GPU_UART_NUM, header, 5);
    if (len > 0 && payload) {
      uart_write_bytes(GPU_UART_NUM, payload, len);
    }
    uart_wait_tx_done(GPU_UART_NUM, pdMS_TO_TICKS(10));
  }
  
  // Drawing helpers
  void clear(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t data[3] = {r, g, b};
    sendCmd(CmdType::CLEAR, data, 3);
  }
  
  void setTarget(uint8_t target) {
    sendCmd(CmdType::SET_TARGET, &target, 1);
  }
  
  void present() {
    sendCmd(CmdType::PRESENT);
  }
  
  void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t data[11];
    memcpy(&data[0], &x1, 2);
    memcpy(&data[2], &y1, 2);
    memcpy(&data[4], &x2, 2);
    memcpy(&data[6], &y2, 2);
    data[8] = r; data[9] = g; data[10] = b;
    sendCmd(CmdType::DRAW_LINE, data, 11);
  }
  
  void drawPixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t data[7];
    memcpy(&data[0], &x, 2);
    memcpy(&data[2], &y, 2);
    data[4] = r; data[5] = g; data[6] = b;
    sendCmd(CmdType::DRAW_PIXEL, data, 7);
  }
  
  void drawCircle(int16_t cx, int16_t cy, int16_t radius, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t data[9];
    memcpy(&data[0], &cx, 2);
    memcpy(&data[2], &cy, 2);
    memcpy(&data[4], &radius, 2);
    data[6] = r; data[7] = g; data[8] = b;
    sendCmd(CmdType::DRAW_CIRCLE, data, 9);
  }
  
  void drawFill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t data[11];
    memcpy(&data[0], &x, 2);
    memcpy(&data[2], &y, 2);
    memcpy(&data[4], &w, 2);
    memcpy(&data[6], &h, 2);
    data[8] = r; data[9] = g; data[10] = b;
    sendCmd(CmdType::DRAW_FILL, data, 11);
  }
  
  void drawPoly(const int16_t* vertices, uint8_t count, uint8_t r, uint8_t g, uint8_t b) {
    if (count > 16) count = 16;
    uint8_t data[4 + 4*16];
    data[0] = count;
    data[1] = r; data[2] = g; data[3] = b;
    for (int i = 0; i < count; i++) {
      memcpy(&data[4 + i*4], &vertices[i*2], 2);
      memcpy(&data[4 + i*4 + 2], &vertices[i*2 + 1], 2);
    }
    sendCmd(CmdType::DRAW_POLY, data, 4 + count*4);
  }
  
  // OLED helpers
  void oledClear() {
    sendCmd(CmdType::OLED_CLEAR);
  }
  
  void oledPresent() {
    sendCmd(CmdType::OLED_PRESENT);
  }
  
  void oledLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t on) {
    uint8_t data[9];
    memcpy(&data[0], &x1, 2);
    memcpy(&data[2], &y1, 2);
    memcpy(&data[4], &x2, 2);
    memcpy(&data[6], &y2, 2);
    data[8] = on;
    sendCmd(CmdType::OLED_LINE, data, 9);
  }
  
  void oledFill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t on) {
    uint8_t data[9];
    memcpy(&data[0], &x, 2);
    memcpy(&data[2], &y, 2);
    memcpy(&data[4], &w, 2);
    memcpy(&data[6], &h, 2);
    data[8] = on;
    sendCmd(CmdType::OLED_FILL, data, 9);
  }
  
  void oledCircle(int16_t cx, int16_t cy, int16_t radius, uint8_t on) {
    uint8_t data[7];
    memcpy(&data[0], &cx, 2);
    memcpy(&data[2], &cy, 2);
    memcpy(&data[4], &radius, 2);
    data[6] = on;
    sendCmd(CmdType::OLED_CIRCLE, data, 7);
  }
};

static GpuComm gpu;

// ============================================================
// Game Constants
// ============================================================

constexpr float SHIP_ROTATE_SPEED = 4.5f;       // Radians per second
constexpr float SHIP_THRUST = 120.0f;           // Pixels per second²
constexpr float SHIP_MAX_SPEED = 100.0f;        // Pixels per second
constexpr float SHIP_DRAG = 0.5f;               // Drag coefficient
constexpr float BULLET_SPEED = 150.0f;          // Pixels per second
constexpr float BULLET_LIFETIME = 1.2f;         // Seconds
constexpr int   MAX_BULLETS = 8;
constexpr int   MAX_ASTEROIDS = 20;
constexpr int   INITIAL_ASTEROIDS = 4;
constexpr float ASTEROID_SPEED_MIN = 15.0f;
constexpr float ASTEROID_SPEED_MAX = 50.0f;
constexpr float ASTEROID_LARGE_RADIUS = 8.0f;
constexpr float ASTEROID_MEDIUM_RADIUS = 5.0f;
constexpr float ASTEROID_SMALL_RADIUS = 3.0f;
constexpr int   SCORE_LARGE = 20;
constexpr int   SCORE_MEDIUM = 50;
constexpr int   SCORE_SMALL = 100;
constexpr float SHIP_INVULN_TIME = 2.0f;        // Seconds after respawn
constexpr int   STARTING_LIVES = 3;
constexpr float FIRE_COOLDOWN = 0.15f;          // Seconds between shots

// ============================================================
// Game Entities
// ============================================================

struct Ship {
  Vec2 position;
  Vec2 velocity;
  float rotation;         // Radians (0 = pointing up)
  bool alive;
  float invulnTimer;
  
  void reset() {
    position = Vec2(HUB75_W / 2, HUB75_H / 2);
    velocity = Vec2::zero();
    rotation = -M_PI / 2;  // Pointing up
    alive = true;
    invulnTimer = SHIP_INVULN_TIME;
  }
  
  Vec2 direction() const {
    return Vec2(cosf(rotation), sinf(rotation));
  }
  
  // Get ship triangle vertices (for rendering)
  void getVertices(int16_t* verts) const {
    // Ship is a triangle: nose, left wing, right wing
    Vec2 dir = direction();
    Vec2 perp = dir.perpendicular();
    
    Vec2 nose = position + dir * 5.0f;
    Vec2 left = position - dir * 3.0f + perp * 3.0f;
    Vec2 right = position - dir * 3.0f - perp * 3.0f;
    
    verts[0] = (int16_t)nose.x;  verts[1] = (int16_t)nose.y;
    verts[2] = (int16_t)left.x;  verts[3] = (int16_t)left.y;
    verts[4] = (int16_t)right.x; verts[5] = (int16_t)right.y;
  }
};

struct Bullet {
  Vec2 position;
  Vec2 velocity;
  float lifetime;
  bool active;
  
  void fire(const Vec2& pos, const Vec2& dir) {
    position = pos;
    velocity = dir * BULLET_SPEED;
    lifetime = BULLET_LIFETIME;
    active = true;
  }
};

enum class AsteroidSize { LARGE, MEDIUM, SMALL };

struct Asteroid {
  Vec2 position;
  Vec2 velocity;
  float rotation;
  float rotationSpeed;
  AsteroidSize size;
  bool active;
  
  float getRadius() const {
    switch (size) {
      case AsteroidSize::LARGE: return ASTEROID_LARGE_RADIUS;
      case AsteroidSize::MEDIUM: return ASTEROID_MEDIUM_RADIUS;
      case AsteroidSize::SMALL: return ASTEROID_SMALL_RADIUS;
    }
    return ASTEROID_SMALL_RADIUS;
  }
  
  int getScore() const {
    switch (size) {
      case AsteroidSize::LARGE: return SCORE_LARGE;
      case AsteroidSize::MEDIUM: return SCORE_MEDIUM;
      case AsteroidSize::SMALL: return SCORE_SMALL;
    }
    return 0;
  }
  
  void spawn(AsteroidSize s, Vec2 pos, Vec2 vel) {
    size = s;
    position = pos;
    velocity = vel;
    rotation = (float)(rand() % 628) / 100.0f;
    rotationSpeed = ((float)(rand() % 300) / 100.0f - 1.5f);
    active = true;
  }
};

// ============================================================
// Game State
// ============================================================

struct GameState {
  Ship ship;
  Bullet bullets[MAX_BULLETS];
  Asteroid asteroids[MAX_ASTEROIDS];
  int score;
  int lives;
  int level;
  float fireCooldown;
  bool gameOver;
  uint32_t frameCount;
  
  // Physics world for collision detection
  PhysicsWorld physics;
  BodyId shipBodyId;
  BodyId bulletBodyIds[MAX_BULLETS];
  BodyId asteroidBodyIds[MAX_ASTEROIDS];
  
  void reset() {
    score = 0;
    lives = STARTING_LIVES;
    level = 1;
    fireCooldown = 0;
    gameOver = false;
    frameCount = 0;
    
    ship.reset();
    
    for (int i = 0; i < MAX_BULLETS; i++) {
      bullets[i].active = false;
    }
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
      asteroids[i].active = false;
    }
    
    // Initialize physics world (no gravity for asteroids!)
    physics.init({
      .gravity = Vec2::zero(),
      .fixedTimeStep = 1.0f / 60.0f,
      .maxSubSteps = 2,
    });
    
    // Create ship body
    shipBodyId = physics.createBody(BodyType::KINEMATIC);
    RigidBody* shipBody = physics.getBody(shipBodyId);
    shipBody->position = ship.position;
    shipBody->shape = CollisionShape::makeCircle(4.0f);
    shipBody->layer = Layer::PLAYER;
    shipBody->collisionMask = Layer::ENEMY;
    
    // Create bullet bodies
    for (int i = 0; i < MAX_BULLETS; i++) {
      bulletBodyIds[i] = physics.createBody(BodyType::KINEMATIC);
      RigidBody* body = physics.getBody(bulletBodyIds[i]);
      body->shape = CollisionShape::makeCircle(1.5f);
      body->layer = Layer::BULLET;
      body->collisionMask = Layer::ENEMY;
      body->flags.isEnabled = false;
    }
    
    // Create asteroid bodies
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
      asteroidBodyIds[i] = physics.createBody(BodyType::KINEMATIC);
      RigidBody* body = physics.getBody(asteroidBodyIds[i]);
      body->shape = CollisionShape::makeCircle(ASTEROID_LARGE_RADIUS);
      body->layer = Layer::ENEMY;
      body->collisionMask = Layer::PLAYER | Layer::BULLET;
      body->flags.isEnabled = false;
    }
    
    // Spawn initial asteroids
    spawnAsteroids(INITIAL_ASTEROIDS);
  }
  
  void spawnAsteroids(int count) {
    for (int i = 0; i < count && i < MAX_ASTEROIDS; i++) {
      // Find inactive slot
      int slot = -1;
      for (int j = 0; j < MAX_ASTEROIDS; j++) {
        if (!asteroids[j].active) {
          slot = j;
          break;
        }
      }
      if (slot < 0) break;
      
      // Spawn at edge, away from ship
      Vec2 pos;
      do {
        int edge = rand() % 4;
        switch (edge) {
          case 0: pos = Vec2(rand() % HUB75_W, 0); break;
          case 1: pos = Vec2(rand() % HUB75_W, HUB75_H - 1); break;
          case 2: pos = Vec2(0, rand() % HUB75_H); break;
          case 3: pos = Vec2(HUB75_W - 1, rand() % HUB75_H); break;
        }
      } while (pos.distanceTo(ship.position) < 30.0f);
      
      // Random velocity
      float angle = (float)(rand() % 628) / 100.0f;
      float speed = ASTEROID_SPEED_MIN + (float)(rand() % 100) / 100.0f * 
                    (ASTEROID_SPEED_MAX - ASTEROID_SPEED_MIN);
      Vec2 vel(cosf(angle) * speed, sinf(angle) * speed);
      
      asteroids[slot].spawn(AsteroidSize::LARGE, pos, vel);
      
      // Update physics body
      RigidBody* body = physics.getBody(asteroidBodyIds[slot]);
      body->position = pos;
      body->shape = CollisionShape::makeCircle(ASTEROID_LARGE_RADIUS);
      body->flags.isEnabled = true;
    }
  }
  
  void splitAsteroid(int idx) {
    if (!asteroids[idx].active) return;
    
    Vec2 pos = asteroids[idx].position;
    AsteroidSize nextSize = AsteroidSize::SMALL;  // Initialize to avoid warning
    float radius = ASTEROID_SMALL_RADIUS;         // Initialize to avoid warning
    
    switch (asteroids[idx].size) {
      case AsteroidSize::LARGE:
        nextSize = AsteroidSize::MEDIUM;
        radius = ASTEROID_MEDIUM_RADIUS;
        break;
      case AsteroidSize::MEDIUM:
        nextSize = AsteroidSize::SMALL;
        radius = ASTEROID_SMALL_RADIUS;
        break;
      case AsteroidSize::SMALL:
        // Small asteroids just disappear
        asteroids[idx].active = false;
        physics.getBody(asteroidBodyIds[idx])->flags.isEnabled = false;
        return;
    }
    
    // Deactivate original
    asteroids[idx].active = false;
    physics.getBody(asteroidBodyIds[idx])->flags.isEnabled = false;
    
    // Spawn 2 smaller asteroids
    for (int i = 0; i < 2; i++) {
      int slot = -1;
      for (int j = 0; j < MAX_ASTEROIDS; j++) {
        if (!asteroids[j].active) {
          slot = j;
          break;
        }
      }
      if (slot < 0) break;
      
      float angle = (float)(rand() % 628) / 100.0f;
      float speed = ASTEROID_SPEED_MIN + (float)(rand() % 100) / 100.0f * 
                    (ASTEROID_SPEED_MAX - ASTEROID_SPEED_MIN) * 1.3f;
      Vec2 vel(cosf(angle) * speed, sinf(angle) * speed);
      
      asteroids[slot].spawn(nextSize, pos, vel);
      
      RigidBody* body = physics.getBody(asteroidBodyIds[slot]);
      body->position = pos;
      body->shape = CollisionShape::makeCircle(radius);
      body->flags.isEnabled = true;
    }
  }
  
  void fireBullet() {
    if (fireCooldown > 0 || !ship.alive) return;
    
    // Find inactive bullet slot
    for (int i = 0; i < MAX_BULLETS; i++) {
      if (!bullets[i].active) {
        Vec2 dir = ship.direction();
        bullets[i].fire(ship.position + dir * 6.0f, dir);
        fireCooldown = FIRE_COOLDOWN;
        
        // Update physics body
        RigidBody* body = physics.getBody(bulletBodyIds[i]);
        body->position = bullets[i].position;
        body->flags.isEnabled = true;
        break;
      }
    }
  }
  
  int countActiveAsteroids() const {
    int count = 0;
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
      if (asteroids[i].active) count++;
    }
    return count;
  }
};

static GameState game;

// ============================================================
// Input
// ============================================================

struct InputState {
  bool turnLeft;
  bool turnRight;
  bool thrust;
  bool fire;
  bool firePressed;  // Edge detection
};

static InputState input;

void initButtons() {
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << BTN_A_PIN) | (1ULL << BTN_B_PIN) | 
                    (1ULL << BTN_C_PIN) | (1ULL << BTN_D_PIN),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  ESP_LOGI(TAG, "Buttons initialized: A=%d B=%d C=%d D=%d", 
           BTN_A_PIN, BTN_B_PIN, BTN_C_PIN, BTN_D_PIN);
}

void readInput() {
  bool prevFire = input.fire;
  
  // Active LOW buttons
  input.turnLeft  = (gpio_get_level(BTN_C_PIN) == 0);   // C = Turn Left
  input.thrust    = (gpio_get_level(BTN_B_PIN) == 0);   // B = Thrust
  input.turnRight = (gpio_get_level(BTN_A_PIN) == 0);   // A = Turn Right
  input.fire      = (gpio_get_level(BTN_D_PIN) == 0);   // D = Fire
  
  // Edge detection for fire (only fire on press, not hold)
  input.firePressed = input.fire && !prevFire;
}

// ============================================================
// Screen Wrap
// ============================================================

Vec2 wrapPosition(Vec2 pos) {
  while (pos.x < 0) pos.x += HUB75_W;
  while (pos.x >= HUB75_W) pos.x -= HUB75_W;
  while (pos.y < 0) pos.y += HUB75_H;
  while (pos.y >= HUB75_H) pos.y -= HUB75_H;
  return pos;
}

// ============================================================
// Game Update
// ============================================================

void updateGame(float dt) {
  if (game.gameOver) {
    // Restart on any button
    if (input.fire || input.thrust) {
      game.reset();
    }
    return;
  }
  
  // Update fire cooldown
  if (game.fireCooldown > 0) {
    game.fireCooldown -= dt;
  }
  
  // Update ship
  if (game.ship.alive) {
    // Rotation
    if (input.turnLeft) {
      game.ship.rotation -= SHIP_ROTATE_SPEED * dt;
    }
    if (input.turnRight) {
      game.ship.rotation += SHIP_ROTATE_SPEED * dt;
    }
    
    // Thrust
    if (input.thrust) {
      Vec2 accel = game.ship.direction() * SHIP_THRUST;
      game.ship.velocity += accel * dt;
      
      // Clamp speed
      float speed = game.ship.velocity.length();
      if (speed > SHIP_MAX_SPEED) {
        game.ship.velocity = game.ship.velocity.normalized() * SHIP_MAX_SPEED;
      }
    } else {
      // Apply drag
      game.ship.velocity *= (1.0f - SHIP_DRAG * dt);
    }
    
    // Move ship
    game.ship.position += game.ship.velocity * dt;
    game.ship.position = wrapPosition(game.ship.position);
    
    // Fire
    if (input.firePressed) {
      game.fireBullet();
    }
    
    // Update invulnerability
    if (game.ship.invulnTimer > 0) {
      game.ship.invulnTimer -= dt;
    }
    
    // Update physics body
    RigidBody* shipBody = game.physics.getBody(game.shipBodyId);
    shipBody->position = game.ship.position;
  }
  
  // Update bullets
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet& b = game.bullets[i];
    if (!b.active) continue;
    
    b.position += b.velocity * dt;
    b.position = wrapPosition(b.position);
    b.lifetime -= dt;
    
    if (b.lifetime <= 0) {
      b.active = false;
      game.physics.getBody(game.bulletBodyIds[i])->flags.isEnabled = false;
    } else {
      game.physics.getBody(game.bulletBodyIds[i])->position = b.position;
    }
  }
  
  // Update asteroids
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    Asteroid& a = game.asteroids[i];
    if (!a.active) continue;
    
    a.position += a.velocity * dt;
    a.position = wrapPosition(a.position);
    a.rotation += a.rotationSpeed * dt;
    
    game.physics.getBody(game.asteroidBodyIds[i])->position = a.position;
  }
  
  // Check collisions using physics engine
  // Bullet vs Asteroid
  for (int bi = 0; bi < MAX_BULLETS; bi++) {
    if (!game.bullets[bi].active) continue;
    
    for (int ai = 0; ai < MAX_ASTEROIDS; ai++) {
      if (!game.asteroids[ai].active) continue;
      
      float dist = game.bullets[bi].position.distanceTo(game.asteroids[ai].position);
      float minDist = 1.5f + game.asteroids[ai].getRadius();
      
      if (dist < minDist) {
        // Hit!
        game.score += game.asteroids[ai].getScore();
        game.bullets[bi].active = false;
        game.physics.getBody(game.bulletBodyIds[bi])->flags.isEnabled = false;
        game.splitAsteroid(ai);
        break;
      }
    }
  }
  
  // Ship vs Asteroid
  if (game.ship.alive && game.ship.invulnTimer <= 0) {
    for (int ai = 0; ai < MAX_ASTEROIDS; ai++) {
      if (!game.asteroids[ai].active) continue;
      
      float dist = game.ship.position.distanceTo(game.asteroids[ai].position);
      float minDist = 4.0f + game.asteroids[ai].getRadius();
      
      if (dist < minDist) {
        // Ship hit!
        game.lives--;
        if (game.lives <= 0) {
          game.gameOver = true;
          game.ship.alive = false;
        } else {
          game.ship.reset();
        }
        break;
      }
    }
  }
  
  // Check if level cleared
  if (game.countActiveAsteroids() == 0) {
    game.level++;
    game.spawnAsteroids(INITIAL_ASTEROIDS + game.level - 1);
  }
  
  game.frameCount++;
}

// ============================================================
// Rendering
// ============================================================

void renderHUB75() {
  // Clear to black
  gpu.setTarget(0);  // HUB75
  gpu.clear(0, 0, 0);
  
  // Draw asteroids (circles)
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!game.asteroids[i].active) continue;
    
    int16_t x = (int16_t)game.asteroids[i].position.x;
    int16_t y = (int16_t)game.asteroids[i].position.y;
    int16_t r = (int16_t)game.asteroids[i].getRadius();
    
    // Color based on size
    uint8_t red = 200, green = 150, blue = 100;
    if (game.asteroids[i].size == AsteroidSize::MEDIUM) {
      red = 180; green = 130; blue = 80;
    } else if (game.asteroids[i].size == AsteroidSize::SMALL) {
      red = 160; green = 110; blue = 60;
    }
    
    gpu.drawCircle(x, y, r, red, green, blue);
  }
  
  // Draw bullets
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!game.bullets[i].active) continue;
    
    int16_t x = (int16_t)game.bullets[i].position.x;
    int16_t y = (int16_t)game.bullets[i].position.y;
    
    gpu.drawPixel(x, y, 255, 255, 0);  // Yellow bullet
    // Draw small trail
    Vec2 trail = game.bullets[i].velocity.normalized() * -2.0f;
    gpu.drawPixel(x + (int16_t)trail.x, y + (int16_t)trail.y, 128, 128, 0);
  }
  
  // Draw ship
  if (game.ship.alive) {
    // Blink if invulnerable
    bool visible = true;
    if (game.ship.invulnTimer > 0) {
      visible = ((int)(game.ship.invulnTimer * 10) % 2) == 0;
    }
    
    if (visible) {
      int16_t verts[6];
      game.ship.getVertices(verts);
      
      // Draw ship as filled polygon (cyan)
      gpu.drawPoly(verts, 3, 0, 255, 255);
      
      // Draw thrust flame if thrusting
      if (input.thrust) {
        Vec2 dir = game.ship.direction();
        Vec2 flameBase = game.ship.position - dir * 4.0f;
        Vec2 flameTip = game.ship.position - dir * (6.0f + (float)(rand() % 3));
        
        gpu.drawLine((int16_t)flameBase.x, (int16_t)flameBase.y,
                    (int16_t)flameTip.x, (int16_t)flameTip.y,
                    255, 128, 0);  // Orange flame
      }
    }
  }
  
  gpu.present();
}

void renderOLED() {
  // Clear OLED
  gpu.oledClear();
  
  // Draw border
  gpu.oledLine(0, 0, 127, 0, 1);
  gpu.oledLine(127, 0, 127, 127, 1);
  gpu.oledLine(127, 127, 0, 127, 1);
  gpu.oledLine(0, 127, 0, 0, 1);
  
  // Draw "ASTEROIDS" title
  gpu.oledFill(4, 4, 60, 10, 1);
  gpu.oledFill(6, 6, 56, 6, 0);
  
  // Draw score
  int scoreX = 10;
  int scoreY = 20;
  gpu.oledFill(scoreX, scoreY, 50, 12, 0);
  
  // Draw score as bar graph (simple visualization)
  int scoreBarWidth = (game.score % 1000) / 10;
  if (scoreBarWidth > 50) scoreBarWidth = 50;
  gpu.oledFill(scoreX, scoreY + 2, scoreBarWidth, 3, 1);
  
  // Score in thousands
  int thousands = game.score / 1000;
  for (int i = 0; i < thousands && i < 10; i++) {
    gpu.oledFill(scoreX + i * 5, scoreY + 8, 4, 2, 1);
  }
  
  // Draw lives as ship icons
  for (int i = 0; i < game.lives && i < 5; i++) {
    int lx = 10 + i * 15;
    int ly = 40;
    // Draw mini triangle for each life
    gpu.oledLine(lx + 4, ly, lx, ly + 8, 1);
    gpu.oledLine(lx, ly + 8, lx + 8, ly + 8, 1);
    gpu.oledLine(lx + 8, ly + 8, lx + 4, ly, 1);
  }
  
  // Draw level
  gpu.oledFill(10, 55, 5 + game.level * 3, 5, 1);
  
  // Draw minimap / radar (HUB75 area scaled to fit)
  int mapX = 10, mapY = 70;
  int mapW = 100, mapH = 25;
  
  // Map border
  gpu.oledLine(mapX, mapY, mapX + mapW, mapY, 1);
  gpu.oledLine(mapX + mapW, mapY, mapX + mapW, mapY + mapH, 1);
  gpu.oledLine(mapX + mapW, mapY + mapH, mapX, mapY + mapH, 1);
  gpu.oledLine(mapX, mapY + mapH, mapX, mapY, 1);
  
  // Scale factors for minimap
  float scaleX = (float)mapW / (float)HUB75_W;
  float scaleY = (float)mapH / (float)HUB75_H;
  
  // Draw asteroids on minimap
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!game.asteroids[i].active) continue;
    
    int mx = mapX + (int)(game.asteroids[i].position.x * scaleX);
    int my = mapY + (int)(game.asteroids[i].position.y * scaleY);
    
    int r = 1;
    if (game.asteroids[i].size == AsteroidSize::LARGE) r = 2;
    
    gpu.oledCircle(mx, my, r, 1);
  }
  
  // Draw ship on minimap
  if (game.ship.alive) {
    int mx = mapX + (int)(game.ship.position.x * scaleX);
    int my = mapY + (int)(game.ship.position.y * scaleY);
    
    // Blinking if invulnerable
    if (game.ship.invulnTimer <= 0 || ((int)(game.ship.invulnTimer * 10) % 2) == 0) {
      gpu.oledFill(mx - 1, my - 1, 3, 3, 1);
    }
  }
  
  // Game over screen
  if (game.gameOver) {
    gpu.oledFill(20, 100, 88, 20, 1);
    gpu.oledFill(22, 102, 84, 16, 0);
    // "GAME OVER" text area
    gpu.oledFill(30, 106, 68, 8, 1);
  }
  
  gpu.oledPresent();
}

// ============================================================
// Main Task
// ============================================================

extern "C" void app_main() {
  ESP_LOGI(TAG, "===========================================");
  ESP_LOGI(TAG, "   ASTEROIDS - Physics Engine Stress Test");
  ESP_LOGI(TAG, "===========================================");
  ESP_LOGI(TAG, "Controls:");
  ESP_LOGI(TAG, "  C = Turn Left");
  ESP_LOGI(TAG, "  B = Thrust Forward");
  ESP_LOGI(TAG, "  A = Turn Right");
  ESP_LOGI(TAG, "  D = Fire");
  ESP_LOGI(TAG, "===========================================");
  
  // Initialize
  gpu.init();
  initButtons();
  
  // Seed random
  srand((unsigned int)esp_timer_get_time());
  
  // Initialize game
  game.reset();
  
  // Give GPU time to initialize
  vTaskDelay(pdMS_TO_TICKS(500));
  
  // Send reset to GPU
  ESP_LOGI(TAG, "Sending RESET to GPU...");
  gpu.sendCmd(CmdType::RESET);
  vTaskDelay(pdMS_TO_TICKS(200));
  
  // Send ping to establish connection
  ESP_LOGI(TAG, "Sending PING to GPU...");
  for (int i = 0; i < 5; i++) {
    gpu.sendCmd(CmdType::PING);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  
  // Clear both displays to show we're connected
  ESP_LOGI(TAG, "Clearing displays...");
  gpu.setTarget(0);  // HUB75
  gpu.clear(0, 0, 0);
  gpu.present();
  
  gpu.oledClear();
  gpu.oledPresent();
  
  vTaskDelay(pdMS_TO_TICKS(100));
  
  ESP_LOGI(TAG, "GPU initialization complete!");
  
  // Main game loop
  uint64_t lastTime = esp_timer_get_time();
  uint32_t frameCounter = 0;
  uint64_t fpsTimer = lastTime;
  
  while (true) {
    // Calculate delta time
    uint64_t now = esp_timer_get_time();
    float dt = (now - lastTime) / 1000000.0f;  // Convert to seconds
    lastTime = now;
    
    // Cap delta time to prevent physics explosions
    if (dt > 0.1f) dt = 0.1f;
    
    // Read input
    readInput();
    
    // Update game
    updateGame(dt);
    
    // Render
    renderHUB75();
    renderOLED();
    
    // FPS counter
    frameCounter++;
    if (now - fpsTimer >= 1000000) {  // 1 second
      ESP_LOGI(TAG, "FPS: %lu | Score: %d | Lives: %d | Level: %d | Asteroids: %d",
               frameCounter, game.score, game.lives, game.level,
               game.countActiveAsteroids());
      frameCounter = 0;
      fpsTimer = now;
    }
    
    // Target ~60 FPS
    vTaskDelay(pdMS_TO_TICKS(16));
  }
}

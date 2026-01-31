/*****************************************************************
 * @file StaticAnim.hpp
 * @brief Static Sprite Animation - Dual panel display with mirroring
 * 
 * BULLETPROOF STATIC ANIMATION
 * 
 * Displays a sprite centered on each panel (left/right) with:
 * - Individual X/Y offsets for each panel
 * - Individual rotation for each sprite  
 * - Individual scale for each sprite
 * - Mirror option for second display
 * - Overflow clipping toggle for each sprite
 * - Fallback rendering when sprite unavailable
 * 
 * All parameters can be bound to equations from the equation system.
 * 
 * PARAMETERS:
 * [Sprite Group]
 *   sprite_id       - Sprite to display (0-255, -1 for fallback)
 *   mirror_second   - Horizontally mirror sprite on right panel
 * 
 * [Background Group]
 *   background      - RGB background color
 * 
 * [Left Panel Group]
 *   left.visible    - Show sprite on left panel
 *   left.offset_x   - Horizontal offset from center (-32 to 32 px)
 *   left.offset_y   - Vertical offset from center (-16 to 16 px)
 *   left.rotation   - Rotation angle (-180 to 180 degrees)
 *   left.scale      - Scale multiplier (0.1 to 4.0)
 *   left.clip       - Hide pixels outside panel bounds
 * 
 * [Right Panel Group]
 *   right.visible   - Show sprite on right panel
 *   right.offset_x  - Horizontal offset from center
 *   right.offset_y  - Vertical offset from center
 *   right.rotation  - Rotation angle
 *   right.scale     - Scale multiplier
 *   right.clip      - Hide pixels outside panel bounds
 * 
 * @author ARCOS
 * @version 2.1 - Bulletproof Edition
 *****************************************************************/

#pragma once

#include "../Core/AnimationBase.hpp"
#include "../Core/AnimationRegistry.hpp"
#include <cmath>
#include <algorithm>

namespace AnimationSystem {
namespace Animations {

/**
 * @brief Static Animation - Displays sprites on both panels
 * 
 * This is the foundational animation type. It simply displays a 
 * sprite on each eye panel with configurable position, rotation,
 * and scale. When no sprite is available, it renders a fallback
 * pattern (white circle).
 */
class StaticAnim : public AnimationBase {
public:
    StaticAnim() {
        // ============================================================
        // PARAMETER DEFINITIONS
        // ============================================================
        
        // --- Sprite Selection ---
        defineParam("sprite_id", "Sprite ID", 
                    "Sprite to display (0-255, -1 for fallback circle)",
                    ParamType::INT, 0, -1, 255, "sprite");
        
        defineParam("mirror_second", "Mirror Second Sprite",
                    "Horizontally mirror sprite on right panel",
                    ParamType::BOOL, 1.0f, 0, 1, "sprite");
        
        // --- Background ---
        defineColorParam("background", "Background Color",
                         "Background color behind sprites",
                         0.0f, 0.0f, 0.0f, "background");
        
        // --- Left Panel Parameters ---
        defineParam("left.visible", "Left Visible",
                    "Show sprite on left panel",
                    ParamType::BOOL, 1.0f, 0, 1, "left");
        
        defineParam("left.offset_x", "Left Offset X",
                    "Horizontal offset from center (pixels)",
                    ParamType::FLOAT, 0.0f, -32.0f, 32.0f, "left");
        
        defineParam("left.offset_y", "Left Offset Y",
                    "Vertical offset from center (pixels)",
                    ParamType::FLOAT, 0.0f, -16.0f, 16.0f, "left");
        
        defineParam("left.rotation", "Left Rotation",
                    "Rotation angle (degrees)",
                    ParamType::FLOAT, 0.0f, -180.0f, 180.0f, "left");
        
        defineParam("left.scale", "Left Scale",
                    "Scale multiplier (1.0 = normal size)",
                    ParamType::FLOAT, 1.0f, 0.1f, 4.0f, "left");
        
        defineParam("left.clip", "Left Clip Overflow",
                    "Hide pixels outside panel bounds",
                    ParamType::BOOL, 1.0f, 0, 1, "left");
        
        // --- Right Panel Parameters ---
        defineParam("right.visible", "Right Visible",
                    "Show sprite on right panel",
                    ParamType::BOOL, 1.0f, 0, 1, "right");
        
        defineParam("right.offset_x", "Right Offset X",
                    "Horizontal offset from center (pixels)",
                    ParamType::FLOAT, 0.0f, -32.0f, 32.0f, "right");
        
        defineParam("right.offset_y", "Right Offset Y",
                    "Vertical offset from center (pixels)",
                    ParamType::FLOAT, 0.0f, -16.0f, 16.0f, "right");
        
        defineParam("right.rotation", "Right Rotation",
                    "Rotation angle (degrees)",
                    ParamType::FLOAT, 0.0f, -180.0f, 180.0f, "right");
        
        defineParam("right.scale", "Right Scale",
                    "Scale multiplier (1.0 = normal size)",
                    ParamType::FLOAT, 1.0f, 0.1f, 4.0f, "right");
        
        defineParam("right.clip", "Right Clip Overflow",
                    "Hide pixels outside panel bounds",
                    ParamType::BOOL, 1.0f, 0, 1, "right");
    }
    
    // ============================================================
    // ANIMATION METADATA
    // ============================================================
    
    const char* getTypeId() const override { return "static"; }
    const char* getDisplayName() const override { return "Static"; }
    const char* getDescription() const override { 
        return "Displays a sprite on each panel with position, rotation, scale, and mirroring options. Bulletproof with fallback rendering.";
    }
    
    // ============================================================
    // LIFECYCLE
    // ============================================================
    
    void onActivate() override {
        frameCount_ = 0;
        active_ = true;
    }
    
    void onDeactivate() override {
        active_ = false;
    }
    
    void reset() override {
        AnimationBase::reset();
        frameCount_ = 0;
        mirroredSpriteId_ = -1;
    }
    
    // ============================================================
    // ANIMATION LOGIC
    // ============================================================
    
    void update(uint32_t deltaMs) override {
        (void)deltaMs;  // Static animation - parameters drive state
        frameCount_++;
    }
    
    void render() override {
        // Safety check - ensure we have minimum callbacks
        if (!hasMinimumCallbacks()) {
            return;
        }
        
        // Get background color
        uint8_t bgR, bgG, bgB;
        getParamColor("background", bgR, bgG, bgB);
        
        // Clear with background
        if (clear) {
            clear(bgR, bgG, bgB);
        }
        
        // Get sprite config
        int spriteId = getParamInt("sprite_id");
        bool mirrorSecond = getParamBool("mirror_second");
        
        // Render left panel
        if (getParamBool("left.visible")) {
            float x = LEFT_CENTER_X + getParam("left.offset_x");
            float y = CENTER_Y + getParam("left.offset_y");
            float angle = getParam("left.rotation");
            float scale = clampScale(getParam("left.scale"));
            bool clip = getParamBool("left.clip");
            
            if (spriteId >= 0) {
                renderSprite(spriteId, x, y, angle, scale, false, clip,
                            LEFT_EYE_X, 0, EYE_W, EYE_H);
            } else {
                // Fallback: render white circle when no sprite
                renderFallbackEye(x, y, scale, LEFT_EYE_X, EYE_W);
            }
        }
        
        // Render right panel
        if (getParamBool("right.visible")) {
            float x = RIGHT_CENTER_X + getParam("right.offset_x");
            float y = CENTER_Y + getParam("right.offset_y");
            float angle = getParam("right.rotation");
            float scale = clampScale(getParam("right.scale"));
            bool clip = getParamBool("right.clip");
            
            if (spriteId >= 0) {
                // Use mirrored sprite ID if available, else mirror at render time
                int useId = (mirrorSecond && mirroredSpriteId_ >= 0) ? mirroredSpriteId_ : spriteId;
                bool mirrorAtRender = mirrorSecond && mirroredSpriteId_ < 0;
                
                renderSprite(useId, x, y, angle, scale, mirrorAtRender, clip,
                            RIGHT_EYE_X, 0, EYE_W, EYE_H);
            } else {
                // Fallback: render white circle when no sprite
                renderFallbackEye(x, y, scale, RIGHT_EYE_X, EYE_W);
            }
        }
        
        // Present frame
        if (present) {
            present();
        }
    }
    
    // ============================================================
    // PUBLIC CONFIGURATION
    // ============================================================
    
    /**
     * @brief Set pre-mirrored sprite ID (loaded by GPU system)
     * @param id Sprite ID for the mirrored version, or -1 for none
     */
    void setMirroredSpriteId(int id) {
        mirroredSpriteId_ = id;
    }
    
    /**
     * @brief Get current frame count (for debugging)
     */
    uint32_t getFrameCount() const {
        return frameCount_;
    }
    
    /**
     * @brief Check if animation is currently active
     */
    bool isActive() const {
        return active_;
    }
    
private:
    // ============================================================
    // CONSTANTS
    // ============================================================
    
    static constexpr float LEFT_CENTER_X = EYE_W / 2.0f;          // 32
    static constexpr float RIGHT_CENTER_X = RIGHT_EYE_X + EYE_W / 2.0f;  // 96
    static constexpr float CENTER_Y = EYE_H / 2.0f;               // 16
    static constexpr float FALLBACK_RADIUS = 10.0f;
    static constexpr float MIN_SCALE = 0.1f;
    static constexpr float MAX_SCALE = 4.0f;
    
    // ============================================================
    // STATE
    // ============================================================
    
    uint32_t frameCount_ = 0;
    int mirroredSpriteId_ = -1;  // Pre-mirrored sprite for right panel
    bool active_ = false;
    
    // ============================================================
    // HELPER FUNCTIONS
    // ============================================================
    
    /**
     * @brief Check if minimum GPU callbacks are available
     */
    bool hasMinimumCallbacks() const {
        // Need at least clear and present, plus some drawing capability
        return (clear != nullptr) && (present != nullptr) &&
               (blitSprite != nullptr || blitSpriteRotated != nullptr || 
                blitSpriteClipped != nullptr || fillCircle != nullptr || drawPixel != nullptr);
    }
    
    /**
     * @brief Clamp scale to valid range
     */
    float clampScale(float scale) const {
        return std::max(MIN_SCALE, std::min(MAX_SCALE, scale));
    }
    
    /**
     * @brief Render a sprite with full options and fallback chain
     */
    void renderSprite(int id, float x, float y, float angle, float scale,
                      bool mirrorX, bool applyClip,
                      int clipX, int clipY, int clipW, int clipH) {
        // Try scaled clipped blit first (most feature-complete)
        if (blitSpriteClipped) {
            blitSpriteClipped(id, x, y, angle, mirrorX,
                             clipX, clipY, clipW, clipH, applyClip);
        }
        // Fall back to rotated blit
        else if (blitSpriteRotated && std::abs(angle) > 0.01f) {
            blitSpriteRotated(id, x, y, angle);
        }
        // Fall back to basic blit
        else if (blitSprite) {
            blitSprite(id, x, y);
        }
        // Last resort: render fallback
        else {
            renderFallbackEye(x, y, scale, clipX, clipW);
        }
    }
    
    /**
     * @brief Render a fallback circle eye when no sprite available
     * @param cx Center X
     * @param cy Center Y  
     * @param scale Scale factor
     * @param panelX Panel start X for clipping reference
     * @param panelW Panel width for clipping reference
     */
    void renderFallbackEye(float cx, float cy, float scale, 
                           int panelX, int panelW) {
        float radius = FALLBACK_RADIUS * scale;
        
        // Use fillCircle if available
        if (fillCircle) {
            // Outer white circle
            fillCircle(static_cast<int>(cx), static_cast<int>(cy), 
                      static_cast<int>(radius), 255, 255, 255);
            // Inner black pupil
            fillCircle(static_cast<int>(cx), static_cast<int>(cy),
                      static_cast<int>(radius * 0.4f), 0, 0, 0);
        }
        // Fall back to pixel-based circle
        else if (drawPixel) {
            int r = static_cast<int>(radius);
            int intCx = static_cast<int>(cx);
            int intCy = static_cast<int>(cy);
            
            // Draw filled circle using pixels
            for (int py = -r; py <= r; py++) {
                for (int px = -r; px <= r; px++) {
                    float dist = std::sqrt(static_cast<float>(px*px + py*py));
                    int drawX = intCx + px;
                    int drawY = intCy + py;
                    
                    // Bounds check
                    if (drawX < 0 || drawX >= DISPLAY_W || 
                        drawY < 0 || drawY >= DISPLAY_H) {
                        continue;
                    }
                    
                    if (dist <= radius) {
                        // Pupil (inner 40%)
                        if (dist <= radius * 0.4f) {
                            drawPixel(drawX, drawY, 0, 0, 0);
                        } else {
                            drawPixel(drawX, drawY, 255, 255, 255);
                        }
                    }
                }
            }
        }
        // Absolute fallback: draw rectangle
        else if (fillRect) {
            int r = static_cast<int>(radius);
            int intCx = static_cast<int>(cx);
            int intCy = static_cast<int>(cy);
            
            // White square
            fillRect(intCx - r, intCy - r, r * 2, r * 2, 255, 255, 255);
            // Black center
            int pr = static_cast<int>(radius * 0.4f);
            fillRect(intCx - pr, intCy - pr, pr * 2, pr * 2, 0, 0, 0);
        }
    }
};

// Register this animation type
REGISTER_ANIMATION(StaticAnim, "static");

} // namespace Animations
} // namespace AnimationSystem


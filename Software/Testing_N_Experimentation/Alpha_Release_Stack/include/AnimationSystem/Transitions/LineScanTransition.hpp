/*****************************************************************
 * @file LineScanTransition.hpp
 * @brief White Line Scan Transition - Top to Bottom Reveal
 * 
 * A clean transition effect where a bright scan line sweeps from
 * top to bottom, revealing the new animation behind it. Used as
 * an intro transition when entering a new scene.
 * 
 * EFFECT:
 * - A bright horizontal line starts at the top of the display
 * - As it moves down, it reveals the new scene behind it
 * - The area above the line shows the new animation
 * - The area below shows black (or captured old frame)
 * - Optional glow/trail effect on the line
 * 
 * PARAMETERS:
 * - duration_ms: Total transition duration in milliseconds
 * - line_thickness: Width of the scan line in pixels
 * - line_color_r/g/b: Color of the scan line (default white)
 * - trail_length: Glow trail behind the line (0 = no trail)
 * - easing: Easing function (0=linear, 1=ease-in, 2=ease-out, 3=ease-in-out)
 * - direction: Scan direction (0=top-down, 1=bottom-up, 2=left-right, 3=right-left)
 * 
 * @author ARCOS
 * @version 1.0
 *****************************************************************/

#pragma once

#include "../Core/TransitionBase.hpp"
#include "../Core/TransitionRegistry.hpp"
#include <cmath>
#include <algorithm>

namespace AnimationSystem {

/**
 * @brief Line Scan Transition - Reveals new scene with scanning line
 * 
 * Creates a professional-looking "wipe" effect with a bright scan line
 * that reveals the new animation. Works well for scene transitions
 * and gives a tech/digital aesthetic.
 */
class LineScanTransition : public TransitionBase {
public:
    LineScanTransition() {
        // ============================================================
        // PARAMETER DEFINITIONS
        // ============================================================
        
        // --- Timing ---
        defineParam("duration_ms", "Duration", 
                    "Transition duration in milliseconds",
                    ParamType::FLOAT, 500.0f, 100.0f, 3000.0f, "Timing");
        
        defineParam("easing", "Easing", 
                    "Easing function (0=linear, 1=ease-in, 2=ease-out, 3=ease-in-out)",
                    ParamType::INT, 2.0f, 0.0f, 3.0f, "Timing");
        
        // --- Line Appearance ---
        defineParam("line_thickness", "Line Thickness", 
                    "Width of the scan line in pixels",
                    ParamType::INT, 2.0f, 1.0f, 8.0f, "Line");
        
        defineParam("line_color_r", "Line Red", 
                    "Red component of line color (0-255)",
                    ParamType::INT, 255.0f, 0.0f, 255.0f, "Line");
        
        defineParam("line_color_g", "Line Green", 
                    "Green component of line color (0-255)",
                    ParamType::INT, 255.0f, 0.0f, 255.0f, "Line");
        
        defineParam("line_color_b", "Line Blue", 
                    "Blue component of line color (0-255)",
                    ParamType::INT, 255.0f, 0.0f, 255.0f, "Line");
        
        // --- Trail Effect ---
        defineParam("trail_length", "Trail Length", 
                    "Glow trail length in pixels (0=no trail)",
                    ParamType::INT, 4.0f, 0.0f, 16.0f, "Trail");
        
        defineParam("trail_brightness", "Trail Brightness", 
                    "Trail glow intensity (0-1)",
                    ParamType::FLOAT, 0.5f, 0.0f, 1.0f, "Trail");
        
        // --- Direction ---
        defineParam("direction", "Direction", 
                    "Scan direction (0=top-down, 1=bottom-up, 2=left-right, 3=right-left)",
                    ParamType::INT, 0.0f, 0.0f, 3.0f, "Direction");
        
        // --- Reveal Mode ---
        defineParam("reveal_new", "Reveal New Scene", 
                    "If true, line reveals new scene. If false, line covers old scene.",
                    ParamType::BOOL, 1.0f, 0.0f, 1.0f, "Mode");
    }
    
    // ============================================================
    // TRANSITION METADATA
    // ============================================================
    
    const char* getTypeId() const override { return "line_scan"; }
    const char* getDisplayName() const override { return "Line Scan"; }
    const char* getDescription() const override { 
        return "White scan line sweeps to reveal new scene (intro transition)";
    }
    
    // ============================================================
    // TRANSITION LIFECYCLE
    // ============================================================
    
    void start(uint32_t durationMs = 0) override {
        // Use parameter duration or override
        uint32_t useDuration = durationMs > 0 ? durationMs : 
                              static_cast<uint32_t>(getParam("duration_ms"));
        TransitionBase::start(useDuration);
    }
    
    void update(uint32_t deltaMs) override {
        updateProgress(deltaMs);
    }
    
    void render() override {
        if (!isRunning() && !isComplete()) return;
        
        // Get parameters
        int thickness = getParamInt("line_thickness");
        uint8_t lineR = static_cast<uint8_t>(getParamInt("line_color_r"));
        uint8_t lineG = static_cast<uint8_t>(getParamInt("line_color_g"));
        uint8_t lineB = static_cast<uint8_t>(getParamInt("line_color_b"));
        int trailLen = getParamInt("trail_length");
        float trailBright = getParam("trail_brightness");
        int direction = getParamInt("direction");
        int easingType = getParamInt("easing");
        bool revealNew = getParamBool("reveal_new");
        
        // Get eased progress
        float progress = getProgress();
        switch (easingType) {
            case 1: progress = easeIn(progress); break;
            case 2: progress = easeOut(progress); break;
            case 3: progress = easeInOut(progress); break;
            default: break; // Linear
        }
        
        // Determine scan axis size
        int axisSize = isHorizontalScan(direction) ? DISPLAY_W : DISPLAY_H;
        
        // Calculate line position (with sub-pixel for smoothness)
        float linePos = progress * static_cast<float>(axisSize);
        int lineStart = static_cast<int>(linePos);
        int lineEnd = lineStart + thickness;
        
        // Clamp to bounds
        lineStart = std::max(0, std::min(axisSize - 1, lineStart));
        lineEnd = std::max(0, std::min(axisSize, lineEnd));
        
        // Render each pixel
        for (int y = 0; y < DISPLAY_H; y++) {
            for (int x = 0; x < DISPLAY_W; x++) {
                // Get position along scan axis
                int axisPos = isHorizontalScan(direction) ? x : y;
                
                // Apply direction reversal if needed
                if (direction == 1 || direction == 3) {
                    axisPos = axisSize - 1 - axisPos;
                }
                
                uint8_t outR, outG, outB;
                
                // Determine which frame to show based on position relative to line
                bool showNew = revealNew ? (axisPos < lineStart) : (axisPos >= lineEnd);
                
                if (showNew) {
                    // Show new scene (what will be after transition)
                    // For now, get from captured "new" frame or black
                    getNewPixel(x, y, outR, outG, outB);
                } else {
                    // Show old scene (captured frame before transition)
                    getCapturedPixel(x, y, outR, outG, outB);
                }
                
                // Check if we're on the scan line
                if (axisPos >= lineStart && axisPos < lineEnd) {
                    // Main line - full brightness
                    outR = lineR;
                    outG = lineG;
                    outB = lineB;
                }
                // Check if we're in the trail zone (behind the line)
                else if (trailLen > 0) {
                    int distFromLine;
                    bool inTrail = false;
                    
                    if (revealNew) {
                        // Trail is behind the line (in revealed area)
                        distFromLine = lineStart - axisPos;
                        inTrail = distFromLine > 0 && distFromLine <= trailLen;
                    } else {
                        // Trail is ahead of the line
                        distFromLine = axisPos - lineEnd;
                        inTrail = distFromLine >= 0 && distFromLine < trailLen;
                    }
                    
                    if (inTrail) {
                        // Calculate trail falloff (linear fade)
                        float trailFactor = 1.0f - (static_cast<float>(distFromLine) / 
                                                    static_cast<float>(trailLen));
                        trailFactor *= trailBright;
                        
                        // Blend trail glow with scene
                        outR = blendAdd(outR, lineR, trailFactor);
                        outG = blendAdd(outG, lineG, trailFactor);
                        outB = blendAdd(outB, lineB, trailFactor);
                    }
                }
                
                // Draw final pixel
                if (drawPixel) {
                    drawPixel(x, y, outR, outG, outB);
                }
            }
        }
        
        // Note: Pipeline is responsible for calling present()
    }
    
    // ============================================================
    // NEW FRAME BUFFER (for "to" animation)
    // ============================================================
    
    /**
     * @brief Capture a pixel from the "new" animation
     * Call this for each pixel of the new scene before rendering
     */
    void captureNewPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x >= 0 && x < DISPLAY_W && y >= 0 && y < DISPLAY_H) {
            int idx = (y * DISPLAY_W + x) * 3;
            newFrame_[idx + 0] = r;
            newFrame_[idx + 1] = g;
            newFrame_[idx + 2] = b;
        }
    }
    
    /**
     * @brief Get new frame pixel
     */
    void getNewPixel(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b) const {
        if (x >= 0 && x < DISPLAY_W && y >= 0 && y < DISPLAY_H) {
            int idx = (y * DISPLAY_W + x) * 3;
            r = newFrame_[idx + 0];
            g = newFrame_[idx + 1];
            b = newFrame_[idx + 2];
        } else {
            r = g = b = 0;
        }
    }
    
    /**
     * @brief Clear new frame buffer (to black)
     */
    void clearNewFrame() {
        std::memset(newFrame_, 0, sizeof(newFrame_));
    }
    
    /**
     * @brief Clear old/captured frame buffer (to black)
     */
    void clearCapturedFrameBuffer() {
        std::memset(oldFrame_, 0, sizeof(oldFrame_));
    }
    
    void reset() override {
        TransitionBase::reset();
        clearNewFrame();
    }
    
private:
    // New frame buffer (the scene we're transitioning TO)
    uint8_t newFrame_[DISPLAY_W * DISPLAY_H * 3] = {0};
    // Old frame buffer (the scene we're transitioning FROM)
    uint8_t oldFrame_[DISPLAY_W * DISPLAY_H * 3] = {0};
    
    // ============================================================
    // HELPERS
    // ============================================================
    
    /**
     * @brief Check if direction is horizontal (left/right)
     */
    bool isHorizontalScan(int direction) const {
        return direction == 2 || direction == 3;
    }
    
    /**
     * @brief Check if transition is still running
     */
    bool isRunning() const {
        return !isComplete() && elapsed_ > 0;
    }
    
    /**
     * @brief Additive blend (clamped to 255)
     */
    static uint8_t blendAdd(uint8_t base, uint8_t add, float amount) {
        float result = base + (add * amount);
        return static_cast<uint8_t>(std::min(255.0f, result));
    }
};

// Register this transition type with line icon
REGISTER_TRANSITION_WITH_ICON(LineScanTransition, "&#x2500;")

} // namespace AnimationSystem

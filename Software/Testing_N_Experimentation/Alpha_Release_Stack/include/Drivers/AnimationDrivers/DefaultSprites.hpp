/*****************************************************************
 * @file DefaultSprites.hpp
 * @brief Default vector sprites for the system
 * 
 * Contains built-in vector sprite data that is always available
 * even without SD card or uploaded sprites.
 * 
 * Default Eye Design:
 * - Circle (iris)
 * - Organic eye shape (outer)
 * - Highlight/reflection detail
 *****************************************************************/

#pragma once

#include <cstdint>
#include <vector>

namespace Drivers {
namespace Sprites {

/**
 * @brief SVG path command types
 */
enum class PathCmd : uint8_t {
    MOVE_TO = 'M',
    LINE_TO = 'L',
    CURVE_TO = 'C',  // Cubic bezier
    QUAD_TO = 'Q',   // Quadratic bezier
    ARC = 'A',
    CLOSE = 'Z',
    CIRCLE = 'O',    // Custom: circle with cx, cy, r
    ELLIPSE = 'E'    // Custom: ellipse with cx, cy, rx, ry
};

/**
 * @brief Vector path point
 */
struct PathPoint {
    float x;
    float y;
};

/**
 * @brief Vector path segment
 */
struct PathSegment {
    PathCmd cmd;
    std::vector<float> params;  // Parameters depend on command type
};

/**
 * @brief Complete vector sprite definition
 */
struct VectorSprite {
    int id;
    const char* name;
    float width;
    float height;
    float viewBoxX;
    float viewBoxY;
    float viewBoxW;
    float viewBoxH;
    std::vector<PathSegment> paths;
    uint8_t strokeR = 255;
    uint8_t strokeG = 255;
    uint8_t strokeB = 255;
    uint8_t fillR = 0;
    uint8_t fillG = 0;
    uint8_t fillB = 0;
    bool hasFill = false;
    bool hasStroke = true;
    float strokeWidth = 1.0f;
};

/**
 * @brief Default eye sprite - organic eye shape with iris
 * 
 * Based on SVG:
 * - Circle at (216, 114) r=39.5 (iris)
 * - Outer eye path (organic curved shape)
 * - Highlight/detail path
 * 
 * ViewBox: 0 0 445 308
 */
inline VectorSprite getDefaultEyeSprite() {
    VectorSprite sprite;
    sprite.id = 0;  // ID 0 = default sprite
    sprite.name = "default_eye";
    sprite.width = 445.0f;
    sprite.height = 308.0f;
    sprite.viewBoxX = 0.0f;
    sprite.viewBoxY = 0.0f;
    sprite.viewBoxW = 445.0f;
    sprite.viewBoxH = 308.0f;
    sprite.strokeR = 255;
    sprite.strokeG = 255;
    sprite.strokeB = 255;
    sprite.hasStroke = true;
    sprite.hasFill = false;
    sprite.strokeWidth = 1.0f;
    
    // Path 1: Iris circle at (216, 114) r=39.5
    sprite.paths.push_back({PathCmd::CIRCLE, {216.0f, 114.0f, 39.5f}});
    
    // Path 2: Outer highlight/detail (tear duct area)
    // M384.5 130.5 L347.5 77.5 L346 76 L343.5 76.5 L342 78 V81 ...
    sprite.paths.push_back({PathCmd::MOVE_TO, {384.5f, 130.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {347.5f, 77.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {346.0f, 76.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {343.5f, 76.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {342.0f, 78.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {342.0f, 81.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {343.5f, 88.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {345.5f, 99.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {345.5f, 112.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {345.0f, 127.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {342.5f, 140.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {338.5f, 156.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {332.0f, 171.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {322.5f, 188.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {311.5f, 203.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {297.5f, 216.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {285.5f, 225.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {284.0f, 230.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {285.0f, 235.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {289.0f, 240.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {302.0f, 242.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {320.0f, 245.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {339.0f, 251.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {355.0f, 257.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {372.0f, 266.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {404.5f, 287.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {433.0f, 305.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {439.5f, 307.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {442.5f, 307.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {444.0f, 305.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {444.0f, 290.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {441.5f, 272.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {434.0f, 240.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {419.5f, 198.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {405.0f, 166.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {384.5f, 130.5f}});
    sprite.paths.push_back({PathCmd::CLOSE, {}});
    
    // Path 3: Main eye outline (organic curved shape)
    // M238 3 L221.5 0.5 H161 L142 1.5 ... Z
    sprite.paths.push_back({PathCmd::MOVE_TO, {238.0f, 3.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {221.5f, 0.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {161.0f, 0.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {142.0f, 1.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {106.0f, 4.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {89.0f, 6.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {72.5f, 10.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {58.5f, 16.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {48.5f, 21.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {35.5f, 30.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {27.0f, 39.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {20.0f, 47.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {14.0f, 57.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {7.0f, 75.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {1.0f, 98.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {0.5f, 109.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {0.5f, 116.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {2.0f, 122.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {5.0f, 126.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {8.5f, 128.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {21.5f, 132.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {38.0f, 137.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {58.5f, 144.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {75.0f, 151.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {90.0f, 159.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {101.5f, 167.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {117.0f, 177.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {131.0f, 189.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {139.5f, 197.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {149.0f, 205.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {158.5f, 212.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {170.5f, 218.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {186.0f, 223.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {201.0f, 226.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {216.0f, 227.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {230.0f, 226.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {242.0f, 223.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {258.5f, 218.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {278.5f, 208.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {292.0f, 198.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {302.0f, 188.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {312.0f, 176.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {319.0f, 163.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {323.0f, 153.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {327.0f, 138.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {328.5f, 122.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {328.5f, 106.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {326.5f, 89.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {321.5f, 72.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {316.5f, 61.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {310.5f, 51.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {303.5f, 42.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {293.5f, 31.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {281.0f, 22.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {267.5f, 14.5f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {255.5f, 9.0f}});
    sprite.paths.push_back({PathCmd::LINE_TO, {238.0f, 3.0f}});
    sprite.paths.push_back({PathCmd::CLOSE, {}});
    
    return sprite;
}

/**
 * @brief Get default eye as simplified polygon points for GPU
 * Returns the main eye outline as a series of x,y coordinate pairs
 * Normalized to 0-1 range for easy scaling
 */
inline std::vector<float> getDefaultEyePolygon() {
    // Main eye outline normalized to 0-1 (divide by 445 for x, 308 for y)
    const float w = 445.0f;
    const float h = 308.0f;
    
    return {
        238.0f/w, 3.0f/h,
        221.5f/w, 0.5f/h,
        161.0f/w, 0.5f/h,
        142.0f/w, 1.5f/h,
        106.0f/w, 4.5f/h,
        89.0f/w, 6.0f/h,
        72.5f/w, 10.5f/h,
        58.5f/w, 16.0f/h,
        48.5f/w, 21.0f/h,
        35.5f/w, 30.5f/h,
        27.0f/w, 39.0f/h,
        20.0f/w, 47.5f/h,
        14.0f/w, 57.5f/h,
        7.0f/w, 75.0f/h,
        1.0f/w, 98.5f/h,
        0.5f/w, 109.0f/h,
        0.5f/w, 116.0f/h,
        2.0f/w, 122.0f/h,
        5.0f/w, 126.0f/h,
        8.5f/w, 128.5f/h,
        21.5f/w, 132.5f/h,
        38.0f/w, 137.5f/h,
        58.5f/w, 144.5f/h,
        75.0f/w, 151.0f/h,
        90.0f/w, 159.0f/h,
        101.5f/w, 167.0f/h,
        117.0f/w, 177.5f/h,
        131.0f/w, 189.0f/h,
        139.5f/w, 197.5f/h,
        149.0f/w, 205.5f/h,
        158.5f/w, 212.0f/h,
        170.5f/w, 218.0f/h,
        186.0f/w, 223.5f/h,
        201.0f/w, 226.5f/h,
        216.0f/w, 227.5f/h,
        230.0f/w, 226.5f/h,
        242.0f/w, 223.5f/h,
        258.5f/w, 218.5f/h,
        278.5f/w, 208.5f/h,
        292.0f/w, 198.5f/h,
        302.0f/w, 188.5f/h,
        312.0f/w, 176.0f/h,
        319.0f/w, 163.5f/h,
        323.0f/w, 153.5f/h,
        327.0f/w, 138.5f/h,
        328.5f/w, 122.0f/h,
        328.5f/w, 106.0f/h,
        326.5f/w, 89.0f/h,
        321.5f/w, 72.5f/h,
        316.5f/w, 61.0f/h,
        310.5f/w, 51.0f/h,
        303.5f/w, 42.5f/h,
        293.5f/w, 31.5f/h,
        281.0f/w, 22.5f/h,
        267.5f/w, 14.5f/h,
        255.5f/w, 9.0f/h,
        238.0f/w, 3.0f/h
    };
}

/**
 * @brief Get iris circle parameters (normalized)
 * Returns: cx, cy, r (all normalized 0-1)
 */
inline void getDefaultEyeIris(float& cx, float& cy, float& r) {
    cx = 216.0f / 445.0f;  // ~0.485
    cy = 114.0f / 308.0f;  // ~0.370
    r = 39.5f / 308.0f;    // ~0.128 (relative to height)
}

/**
 * @brief SVG string for the default eye (for file storage)
 */
inline const char* getDefaultEyeSVG() {
    return R"(<svg width="445" height="308" viewBox="0 0 445 308" fill="none" xmlns="http://www.w3.org/2000/svg">
<circle cx="216" cy="114" r="39.5" stroke="white"/>
<path d="M384.5 130.5L347.5 77.5L346 76L343.5 76.5L342 78V81L343.5 88L345.5 99.5V112L345 127L342.5 140L338.5 156L332 171L322.5 188.5L311.5 203.5L297.5 216.5L285.5 225L284 230L285 235.5L289 240L302 242L320 245L339 251L355 257.5L372 266.5L404.5 287.5L433 305L439.5 307.5H442.5L444 305.5V290L441.5 272L434 240L419.5 198.5L405 166L384.5 130.5Z" stroke="white"/>
<path d="M238 3L221.5 0.5H161L142 1.5L106 4.5L89 6L72.5 10.5L58.5 16L48.5 21L35.5 30.5L27 39L20 47.5L14 57.5L7 75L1 98.5L0.5 109V116L2 122L5 126L8.5 128.5L21.5 132.5L38 137.5L58.5 144.5L75 151L90 159L101.5 167L117 177.5L131 189L139.5 197.5L149 205.5L158.5 212L170.5 218L186 223.5L201 226.5L216 227.5L230 226.5L242 223.5L258.5 218.5L278.5 208.5L292 198.5L302 188.5L312 176L319 163.5L323 153.5L327 138.5L328.5 122V106L326.5 89L321.5 72.5L316.5 61L310.5 51L303.5 42.5L293.5 31.5L281 22.5L267.5 14.5L255.5 9L238 3Z" stroke="white"/>
</svg>)";
}

} // namespace Sprites
} // namespace Drivers

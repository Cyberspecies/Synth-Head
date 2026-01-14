/*****************************************************************
 * @file AnimationClip.hpp
 * @brief Self-contained animation sequences
 * 
 * Combines multiple tracks into a single playable animation unit.
 *****************************************************************/

#pragma once

#include "../Core/Types.hpp"
#include "KeyframeTrack.hpp"
#include <cstring>

namespace AnimationDriver {

// ============================================================
// Animation Clip - Container for multiple tracks
// ============================================================

class AnimationClip {
public:
    static constexpr int MAX_FLOAT_TRACKS = 8;
    static constexpr int MAX_VEC2_TRACKS = 4;
    static constexpr int MAX_COLOR_TRACKS = 4;
    static constexpr int MAX_NAME_LEN = 24;
    
    AnimationClip() : floatTrackCount_(0), vec2TrackCount_(0), colorTrackCount_(0),
                      state_(AnimationState::STOPPED), time_(0.0f), speed_(1.0f) {
        name_[0] = '\0';
    }
    
    AnimationClip(const char* name) : AnimationClip() {
        setName(name);
    }
    
    // Naming
    AnimationClip& setName(const char* name) {
        strncpy(name_, name, MAX_NAME_LEN - 1);
        name_[MAX_NAME_LEN - 1] = '\0';
        return *this;
    }
    
    const char* getName() const { return name_; }
    
    // Add tracks
    FloatTrack& addFloatTrack(const char* name) {
        if (floatTrackCount_ < MAX_FLOAT_TRACKS) {
            strncpy(floatTrackNames_[floatTrackCount_], name, MAX_NAME_LEN - 1);
            floatTrackNames_[floatTrackCount_][MAX_NAME_LEN - 1] = '\0';
            return floatTracks_[floatTrackCount_++];
        }
        // Return last track as fallback
        return floatTracks_[MAX_FLOAT_TRACKS - 1];
    }
    
    Vec2Track& addVec2Track(const char* name) {
        if (vec2TrackCount_ < MAX_VEC2_TRACKS) {
            strncpy(vec2TrackNames_[vec2TrackCount_], name, MAX_NAME_LEN - 1);
            vec2TrackNames_[vec2TrackCount_][MAX_NAME_LEN - 1] = '\0';
            return vec2Tracks_[vec2TrackCount_++];
        }
        return vec2Tracks_[MAX_VEC2_TRACKS - 1];
    }
    
    ColorTrack& addColorTrack(const char* name) {
        if (colorTrackCount_ < MAX_COLOR_TRACKS) {
            strncpy(colorTrackNames_[colorTrackCount_], name, MAX_NAME_LEN - 1);
            colorTrackNames_[colorTrackCount_][MAX_NAME_LEN - 1] = '\0';
            return colorTracks_[colorTrackCount_++];
        }
        return colorTracks_[MAX_COLOR_TRACKS - 1];
    }
    
    // Get tracks by name
    FloatTrack* getFloatTrack(const char* name) {
        for (int i = 0; i < floatTrackCount_; i++) {
            if (strncmp(floatTrackNames_[i], name, MAX_NAME_LEN) == 0) {
                return &floatTracks_[i];
            }
        }
        return nullptr;
    }
    
    Vec2Track* getVec2Track(const char* name) {
        for (int i = 0; i < vec2TrackCount_; i++) {
            if (strncmp(vec2TrackNames_[i], name, MAX_NAME_LEN) == 0) {
                return &vec2Tracks_[i];
            }
        }
        return nullptr;
    }
    
    ColorTrack* getColorTrack(const char* name) {
        for (int i = 0; i < colorTrackCount_; i++) {
            if (strncmp(colorTrackNames_[i], name, MAX_NAME_LEN) == 0) {
                return &colorTracks_[i];
            }
        }
        return nullptr;
    }
    
    // Get tracks by index
    FloatTrack* getFloatTrack(int index) {
        return (index >= 0 && index < floatTrackCount_) ? &floatTracks_[index] : nullptr;
    }
    
    // Evaluate tracks
    float evaluateFloat(const char* trackName, float defaultVal = 0.0f) const {
        for (int i = 0; i < floatTrackCount_; i++) {
            if (strncmp(floatTrackNames_[i], trackName, MAX_NAME_LEN) == 0) {
                return floatTracks_[i].evaluate(time_);
            }
        }
        return defaultVal;
    }
    
    float evaluateFloat(int trackIndex, float defaultVal = 0.0f) const {
        return (trackIndex >= 0 && trackIndex < floatTrackCount_)
            ? floatTracks_[trackIndex].evaluate(time_)
            : defaultVal;
    }
    
    Vec2 evaluateVec2(const char* trackName) const {
        for (int i = 0; i < vec2TrackCount_; i++) {
            if (strncmp(vec2TrackNames_[i], trackName, MAX_NAME_LEN) == 0) {
                return vec2Tracks_[i].evaluate(time_);
            }
        }
        return Vec2(0, 0);
    }
    
    RGB evaluateColor(const char* trackName) const {
        for (int i = 0; i < colorTrackCount_; i++) {
            if (strncmp(colorTrackNames_[i], trackName, MAX_NAME_LEN) == 0) {
                return colorTracks_[i].evaluate(time_);
            }
        }
        return RGB::Black();
    }
    
    // Playback control
    AnimationClip& play() {
        state_ = AnimationState::PLAYING;
        return *this;
    }
    
    AnimationClip& pause() {
        state_ = AnimationState::PAUSED;
        return *this;
    }
    
    AnimationClip& stop() {
        state_ = AnimationState::STOPPED;
        time_ = 0.0f;
        return *this;
    }
    
    AnimationClip& setSpeed(float speed) {
        speed_ = speed;
        return *this;
    }
    
    AnimationClip& setTime(float time) {
        time_ = time;
        return *this;
    }
    
    AnimationClip& setLoopMode(LoopMode mode) {
        loopMode_ = mode;
        // Apply to all tracks
        for (int i = 0; i < floatTrackCount_; i++) {
            floatTracks_[i].setLoop(mode);
        }
        for (int i = 0; i < vec2TrackCount_; i++) {
            vec2Tracks_[i].setLoop(mode);
        }
        for (int i = 0; i < colorTrackCount_; i++) {
            colorTracks_[i].setLoop(mode);
        }
        return *this;
    }
    
    // Update (call each frame)
    void update(float deltaTime) {
        if (state_ != AnimationState::PLAYING) return;
        
        time_ += deltaTime * speed_;
        
        // Handle completion for non-looping
        if (loopMode_ == LoopMode::ONCE && time_ >= getDuration()) {
            time_ = getDuration();
            state_ = AnimationState::FINISHED;
        }
    }
    
    // State queries
    AnimationState getState() const { return state_; }
    float getTime() const { return time_; }
    float getSpeed() const { return speed_; }
    bool isPlaying() const { return state_ == AnimationState::PLAYING; }
    bool isFinished() const { return state_ == AnimationState::FINISHED; }
    
    // Get total duration (max of all tracks)
    float getDuration() const {
        float maxDuration = 0.0f;
        for (int i = 0; i < floatTrackCount_; i++) {
            maxDuration = std::max(maxDuration, floatTracks_[i].getDuration());
        }
        for (int i = 0; i < vec2TrackCount_; i++) {
            maxDuration = std::max(maxDuration, vec2Tracks_[i].getDuration());
        }
        for (int i = 0; i < colorTrackCount_; i++) {
            maxDuration = std::max(maxDuration, colorTracks_[i].getDuration());
        }
        return maxDuration;
    }
    
    // Get normalized progress (0-1)
    float getProgress() const {
        float duration = getDuration();
        return (duration > 0.0f) ? (time_ / duration) : 0.0f;
    }
    
private:
    char name_[MAX_NAME_LEN];
    
    FloatTrack floatTracks_[MAX_FLOAT_TRACKS];
    char floatTrackNames_[MAX_FLOAT_TRACKS][MAX_NAME_LEN];
    int floatTrackCount_;
    
    Vec2Track vec2Tracks_[MAX_VEC2_TRACKS];
    char vec2TrackNames_[MAX_VEC2_TRACKS][MAX_NAME_LEN];
    int vec2TrackCount_;
    
    ColorTrack colorTracks_[MAX_COLOR_TRACKS];
    char colorTrackNames_[MAX_COLOR_TRACKS][MAX_NAME_LEN];
    int colorTrackCount_;
    
    AnimationState state_;
    float time_;
    float speed_;
    LoopMode loopMode_ = LoopMode::ONCE;
};

} // namespace AnimationDriver

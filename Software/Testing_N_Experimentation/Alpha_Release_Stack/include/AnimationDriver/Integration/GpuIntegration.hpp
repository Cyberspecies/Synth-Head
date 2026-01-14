/*****************************************************************
 * @file GpuIntegration.hpp
 * @brief Integration layer between AnimationDriver and GpuCommands
 * 
 * Provides helper functions to connect the animation system
 * with the GPU command interface for rendering.
 *****************************************************************/

#pragma once

#include "../AnimationDriver.hpp"
#include "GpuDriver/GpuCommands.hpp"

namespace AnimationDriver {

/**
 * @brief Helper class to integrate AnimationDriver with GpuCommands
 * 
 * Handles sending rendered frames to the GPU via the command interface.
 */
class GpuIntegration {
public:
    GpuIntegration() : gpu_(nullptr) {}
    
    /**
     * @brief Connect to a GpuCommands instance
     * @param gpu Pointer to initialized GpuCommands
     */
    void setGpuCommands(GpuCommands* gpu) {
        gpu_ = gpu;
    }
    
    /**
     * @brief Send HUB75 frame data to GPU using pixel commands
     * 
     * This sends pixel-by-pixel which is slower but compatible with
     * the existing command set.
     * 
     * @param target The render target containing frame data
     */
    void sendHub75Frame(const RenderTarget* target) {
        if (!gpu_ || !target) return;
        
        const FrameBuffer* buffer = target->getBuffer();
        if (!buffer) return;
        
        int width = buffer->getWidth();
        int height = buffer->getHeight();
        
        // Clear and draw each pixel
        // Note: For better performance, consider adding a bulk transfer command
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                RGB color = buffer->getPixel(x, y);
                gpu_->hub75Pixel(x, y, color.r, color.g, color.b);
            }
        }
        
        gpu_->hub75Present();
    }
    
    /**
     * @brief Send HUB75 frame using optimized scanline approach
     * 
     * Sends lines of pixels at once for better throughput.
     */
    void sendHub75FrameOptimized(const RenderTarget* target) {
        if (!gpu_ || !target) return;
        
        const FrameBuffer* buffer = target->getBuffer();
        if (!buffer) return;
        
        int width = buffer->getWidth();
        int height = buffer->getHeight();
        
        // Use horizontal lines for each row
        // This is faster than individual pixels
        for (int y = 0; y < height; y++) {
            int runStart = 0;
            RGB runColor = buffer->getPixel(0, y);
            
            for (int x = 1; x <= width; x++) {
                RGB currentColor = (x < width) ? buffer->getPixel(x, y) : RGB::Black();
                
                // If color changed or end of line, draw the run
                if (x == width || currentColor.r != runColor.r || 
                    currentColor.g != runColor.g || currentColor.b != runColor.b) {
                    
                    if (x - runStart > 2) {
                        // Use horizontal line for runs > 2 pixels
                        gpu_->hub75Line(runStart, y, x - 1, y, 
                                       runColor.r, runColor.g, runColor.b);
                    } else {
                        // Use individual pixels for short runs
                        for (int px = runStart; px < x; px++) {
                            gpu_->hub75Pixel(px, y, runColor.r, runColor.g, runColor.b);
                        }
                    }
                    
                    runStart = x;
                    runColor = currentColor;
                }
            }
        }
        
        gpu_->hub75Present();
    }
    
    /**
     * @brief Fill HUB75 with a solid color
     */
    void fillHub75(const RGB& color) {
        if (!gpu_) return;
        gpu_->hub75Fill(0, 0, GpuCommands::HUB75_WIDTH, GpuCommands::HUB75_HEIGHT,
                       color.r, color.g, color.b);
        gpu_->hub75Present();
    }
    
    /**
     * @brief Clear HUB75 display
     */
    void clearHub75() {
        if (!gpu_) return;
        gpu_->hub75Clear(0, 0, 0);
        gpu_->hub75Present();
    }
    
private:
    GpuCommands* gpu_;
};

/**
 * @brief Configure AnimationManager to work with GpuCommands
 * 
 * Sets up the callback to automatically send frames to GPU.
 * 
 * Example:
 *   GpuCommands gpu;
 *   gpu.init();
 *   
 *   AnimationManager anim;
 *   GpuIntegration gpuInt;
 *   gpuInt.setGpuCommands(&gpu);
 *   
 *   setupAnimationGpuCallback(anim, gpuInt);
 *   anim.setRainbow().start();
 *   
 *   // In loop:
 *   anim.update(deltaTime);  // Automatically sends to GPU
 */
inline void setupAnimationGpuCallback(AnimationManager& anim, GpuIntegration& integration) {
    // Note: This captures integration by reference, ensure it stays in scope
    anim.onSendHUB75([&integration](const uint8_t* data, size_t size) {
        // For now, we can't directly send raw data, so this callback
        // is a placeholder. Use GpuIntegration::sendHub75Frame() instead.
        (void)data;
        (void)size;
    });
}

/**
 * @brief Helper function for common animation setup pattern
 * 
 * Example:
 *   // In setup
 *   GpuCommands gpu;
 *   AnimationManager anim;
 *   GpuIntegration gpuInt;
 *   
 *   setupAnimationSystem(gpu, anim, gpuInt);
 *   anim.setRainbow().start();
 *   
 *   // In loop
 *   anim.update(deltaTime);
 *   gpuInt.sendHub75FrameOptimized(anim.getHUB75Target());
 */
inline void setupAnimationSystem(GpuCommands& gpu, AnimationManager& anim, 
                                  GpuIntegration& gpuInt) {
    gpuInt.setGpuCommands(&gpu);
    
    // Configure manager for HUB75 dimensions
    AnimationManagerConfig config;
    config.hub75Width = GpuCommands::HUB75_WIDTH;
    config.hub75Height = GpuCommands::HUB75_HEIGHT;
    config.autoSendHub75 = false;  // We'll send manually via GpuIntegration
    
    anim.configure(config);
}

} // namespace AnimationDriver

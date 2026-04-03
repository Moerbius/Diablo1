#pragma once

#include <cstdint>

struct SDL_Window;
struct SDL_Renderer;

class Video {
public:
	Video();
	~Video();

	Video(const Video&) = delete;
	Video& operator=(const Video&) = delete;

	bool Init(const char* title, int width, int height);
	bool HandleInput(bool& isRunning);
	bool RenderFrame(int rectX, int rectY, int rectW, int rectH);
	bool RenderPCXImage(const std::uint32_t* imageRgba, int width, int height);
	bool RenderPCXImageScaled(const std::uint32_t* imageRgba, int width, int height,
	                           int scaledWidth, int scaledHeight);
	bool RenderPCXImageAt(const std::uint32_t* imageRgba, int width, int height,
	                      int x, int y, int scaledWidth, int scaledHeight);
	bool RenderLogoOverlay(const std::uint32_t* logoRgba, int logoWidth, int logoHeight,
	                        int x, int y);
	bool RenderLogoScaled(const std::uint32_t* logoRgba, int logoWidth, int logoHeight,
	                       int scaledWidth, int scaledHeight, int x, int y);
	bool RenderComposite(const std::uint32_t* bgRgba, int bgWidth, int bgHeight,
	                      const std::uint32_t* fgRgba, int fgWidth, int fgHeight,
	                      int fgX, int fgY);
	void ToggleFullscreen();
	SDL_Renderer* GetRenderer() const { return renderer_; }
	SDL_Window* GetWindow() const { return window_; }

private:
	void Shutdown();

	SDL_Window* window_;
	SDL_Renderer* renderer_;
	bool initialized_;
};
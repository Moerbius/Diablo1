#include "video.hpp"

#include <SDL3/SDL.h>

#include <cstdio>

Video::Video() : window_(nullptr), renderer_(nullptr), initialized_(false)
{
}

Video::~Video()
{
	Shutdown();
}

bool Video::Init(const char* title, int width, int height)
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return false;
	}

	std::fprintf(stderr, "SDL video driver: %s\n", SDL_GetCurrentVideoDriver());

	window_ = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
	if (window_ == nullptr) {
		std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		Shutdown();
		return false;
	}

	if (!SDL_ShowWindow(window_)) {
		std::fprintf(stderr, "SDL_ShowWindow failed: %s\n", SDL_GetError());
		Shutdown();
		return false;
	}

	renderer_ = SDL_CreateRenderer(window_, nullptr);
	if (renderer_ == nullptr) {
		std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
		Shutdown();
		return false;
	}

	initialized_ = true;
	return true;
}
bool Video::HandleInput(bool& isRunning)
{
	if (!initialized_) {
		std::fprintf(stderr, "Video::HandleInput called before successful Video::Init\n");
		return false;
	}

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT) {
			isRunning = false;
		}
	}

	return true;
}

bool Video::RenderFrame(int rectX, int rectY, int rectW, int rectH)
{
	if (!initialized_) {
		std::fprintf(stderr, "Video::RenderFrame called before successful Video::Init\n");
		return false;
	}

	SDL_SetRenderDrawColor(renderer_, 15, 18, 24, 255);
	if (!SDL_RenderClear(renderer_)) {
		std::fprintf(stderr, "SDL_RenderClear failed: %s\n", SDL_GetError());
		return false;
	}

	SDL_FRect testRect;
	testRect.x = static_cast<float>(rectX);
	testRect.y = static_cast<float>(rectY);
	testRect.w = static_cast<float>(rectW);
	testRect.h = static_cast<float>(rectH);

	SDL_SetRenderDrawColor(renderer_, 192, 118, 24, 255);
	if (!SDL_RenderFillRect(renderer_, &testRect)) {
		std::fprintf(stderr, "SDL_RenderFillRect failed: %s\n", SDL_GetError());
		return false;
	}

	if (!SDL_RenderPresent(renderer_)) {
		std::fprintf(stderr, "SDL_RenderPresent failed: %s\n", SDL_GetError());
		return false;
	}

	return true;
}

bool Video::RenderPCXImage(const std::uint32_t* imageRgba, int width, int height)
{
	if (!initialized_) {
		std::fprintf(stderr, "Video::RenderPCXImage called before successful Video::Init\n");
		return false;
	}

	SDL_Texture* texture = SDL_CreateTexture(
		renderer_,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STATIC,
		width,
		height);
	if (texture == nullptr) {
		std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
		return false;
	}

	if (!SDL_UpdateTexture(texture, nullptr, imageRgba, width * 4)) {
		std::fprintf(stderr, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(texture);
		return false;
	}

	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
	if (!SDL_RenderClear(renderer_)) {
		std::fprintf(stderr, "SDL_RenderClear failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(texture);
		return false;
	}

	SDL_FRect dest;
	dest.x = 0.0f;
	dest.y = 0.0f;
	dest.w = static_cast<float>(width);
	dest.h = static_cast<float>(height);

	if (!SDL_RenderTexture(renderer_, texture, nullptr, &dest)) {
		std::fprintf(stderr, "SDL_RenderTexture failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(texture);
		return false;
	}

	if (!SDL_RenderPresent(renderer_)) {
		std::fprintf(stderr, "SDL_RenderPresent failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(texture);
		return false;
	}

	SDL_DestroyTexture(texture);
	return true;
}

bool Video::RenderPCXImageScaled(const std::uint32_t* imageRgba, int width, int height,
                                 int scaledWidth, int scaledHeight)
{
	return RenderPCXImageAt(imageRgba, width, height, 0, 0, scaledWidth, scaledHeight);
}

bool Video::RenderPCXImageAt(const std::uint32_t* imageRgba, int width, int height,
                             int x, int y, int scaledWidth, int scaledHeight)
{
	if (!initialized_) {
		std::fprintf(stderr, "Video::RenderPCXImageAt called before successful Video::Init\n");
		return false;
	}

	SDL_Texture* texture = SDL_CreateTexture(
		renderer_,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STATIC,
		width,
		height);
	if (texture == nullptr) {
		std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
		return false;
	}

	if (!SDL_UpdateTexture(texture, nullptr, imageRgba, width * 4)) {
		std::fprintf(stderr, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(texture);
		return false;
	}

	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
	if (!SDL_RenderClear(renderer_)) {
		std::fprintf(stderr, "SDL_RenderClear failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(texture);
		return false;
	}

	SDL_FRect dest;
	dest.x = static_cast<float>(x);
	dest.y = static_cast<float>(y);
	dest.w = static_cast<float>(scaledWidth);
	dest.h = static_cast<float>(scaledHeight);

	if (!SDL_RenderTexture(renderer_, texture, nullptr, &dest)) {
		std::fprintf(stderr, "SDL_RenderTexture failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(texture);
		return false;
	}

	SDL_DestroyTexture(texture);
	return true;
}

bool Video::RenderLogoOverlay(const std::uint32_t* logoRgba, int logoWidth, int logoHeight,
                              int x, int y)
{
	if (!initialized_) {
		std::fprintf(stderr, "Video::RenderLogoOverlay called before successful Video::Init\n");
		return false;
	}

	SDL_Texture* logoTexture = SDL_CreateTexture(
		renderer_,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STATIC,
		logoWidth,
		logoHeight);
	if (logoTexture == nullptr) {
		std::fprintf(stderr, "SDL_CreateTexture (logo) failed: %s\n", SDL_GetError());
		return false;
	}

	if (!SDL_UpdateTexture(logoTexture, nullptr, logoRgba, logoWidth * 4)) {
		std::fprintf(stderr, "SDL_UpdateTexture (logo) failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(logoTexture);
		return false;
	}

	if (!SDL_SetTextureScaleMode(logoTexture, SDL_SCALEMODE_NEAREST)) {
		std::fprintf(stderr, "SDL_SetTextureScaleMode failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(logoTexture);
		return false;
	}

	if (!SDL_SetTextureBlendMode(logoTexture, SDL_BLENDMODE_BLEND)) {
		std::fprintf(stderr, "SDL_SetTextureBlendMode failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(logoTexture);
		return false;
	}

	SDL_FRect dest;
	dest.x = static_cast<float>(x);
	dest.y = static_cast<float>(y);
	dest.w = static_cast<float>(logoWidth);
	dest.h = static_cast<float>(logoHeight);

	if (!SDL_RenderTexture(renderer_, logoTexture, nullptr, &dest)) {
		std::fprintf(stderr, "SDL_RenderTexture (logo) failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(logoTexture);
		return false;
	}

	SDL_DestroyTexture(logoTexture);
	return true;
}

bool Video::RenderLogoScaled(const std::uint32_t* logoRgba, int logoWidth, int logoHeight,
                             int scaledWidth, int scaledHeight, int x, int y)
{
	if (!initialized_) {
		std::fprintf(stderr, "Video::RenderLogoScaled called before successful Video::Init\n");
		return false;
	}

	SDL_Texture* logoTexture = SDL_CreateTexture(
		renderer_,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STATIC,
		logoWidth,
		logoHeight);
	if (logoTexture == nullptr) {
		std::fprintf(stderr, "SDL_CreateTexture (logo) failed: %s\n", SDL_GetError());
		return false;
	}

	if (!SDL_UpdateTexture(logoTexture, nullptr, logoRgba, logoWidth * 4)) {
		std::fprintf(stderr, "SDL_UpdateTexture (logo) failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(logoTexture);
		return false;
	}

	if (!SDL_SetTextureScaleMode(logoTexture, SDL_SCALEMODE_NEAREST)) {
		std::fprintf(stderr, "SDL_SetTextureScaleMode failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(logoTexture);
		return false;
	}

	if (!SDL_SetTextureBlendMode(logoTexture, SDL_BLENDMODE_BLEND)) {
		std::fprintf(stderr, "SDL_SetTextureBlendMode failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(logoTexture);
		return false;
	}

	SDL_FRect dest;
	dest.x = static_cast<float>(x);
	dest.y = static_cast<float>(y);
	dest.w = static_cast<float>(scaledWidth);
	dest.h = static_cast<float>(scaledHeight);

	if (!SDL_RenderTexture(renderer_, logoTexture, nullptr, &dest)) {
		std::fprintf(stderr, "SDL_RenderTexture (scaled logo) failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(logoTexture);
		return false;
	}

	SDL_DestroyTexture(logoTexture);
	return true;
}

bool Video::RenderComposite(const std::uint32_t* bgRgba, int bgWidth, int bgHeight,
                            const std::uint32_t* fgRgba, int fgWidth, int fgHeight,
                            int fgX, int fgY)
{
	if (!initialized_) {
		std::fprintf(stderr, "Video::RenderComposite called before successful Video::Init\n");
		return false;
	}

	SDL_Texture* bgTexture = SDL_CreateTexture(
		renderer_,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STATIC,
		bgWidth,
		bgHeight);
	if (bgTexture == nullptr) {
		std::fprintf(stderr, "SDL_CreateTexture (bg) failed: %s\n", SDL_GetError());
		return false;
	}

	if (!SDL_UpdateTexture(bgTexture, nullptr, bgRgba, bgWidth * 4)) {
		std::fprintf(stderr, "SDL_UpdateTexture (bg) failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(bgTexture);
		return false;
	}

	SDL_Texture* fgTexture = SDL_CreateTexture(
		renderer_,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STATIC,
		fgWidth,
		fgHeight);
	if (fgTexture == nullptr) {
		std::fprintf(stderr, "SDL_CreateTexture (fg) failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(bgTexture);
		return false;
	}

	if (!SDL_UpdateTexture(fgTexture, nullptr, fgRgba, fgWidth * 4)) {
		std::fprintf(stderr, "SDL_UpdateTexture (fg) failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(fgTexture);
		SDL_DestroyTexture(bgTexture);
		return false;
	}

	if (!SDL_SetTextureBlendMode(fgTexture, SDL_BLENDMODE_BLEND)) {
		std::fprintf(stderr, "SDL_SetTextureBlendMode failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(fgTexture);
		SDL_DestroyTexture(bgTexture);
		return false;
	}

	SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
	if (!SDL_RenderClear(renderer_)) {
		std::fprintf(stderr, "SDL_RenderClear failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(fgTexture);
		SDL_DestroyTexture(bgTexture);
		return false;
	}

	SDL_FRect bgDest;
	bgDest.x = 0.0f;
	bgDest.y = 0.0f;
	bgDest.w = static_cast<float>(bgWidth);
	bgDest.h = static_cast<float>(bgHeight);

	if (!SDL_RenderTexture(renderer_, bgTexture, nullptr, &bgDest)) {
		std::fprintf(stderr, "SDL_RenderTexture (bg) failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(fgTexture);
		SDL_DestroyTexture(bgTexture);
		return false;
	}

	SDL_FRect fgDest;
	fgDest.x = static_cast<float>(fgX);
	fgDest.y = static_cast<float>(fgY);
	fgDest.w = static_cast<float>(fgWidth);
	fgDest.h = static_cast<float>(fgHeight);

	if (!SDL_RenderTexture(renderer_, fgTexture, nullptr, &fgDest)) {
		std::fprintf(stderr, "SDL_RenderTexture (fg) failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(fgTexture);
		SDL_DestroyTexture(bgTexture);
		return false;
	}

	if (!SDL_RenderPresent(renderer_)) {
		std::fprintf(stderr, "SDL_RenderPresent failed: %s\n", SDL_GetError());
		SDL_DestroyTexture(fgTexture);
		SDL_DestroyTexture(bgTexture);
		return false;
	}

	SDL_DestroyTexture(fgTexture);
	SDL_DestroyTexture(bgTexture);
	return true;
}



void Video::Shutdown()
{
	if (renderer_ != nullptr) {
		SDL_DestroyRenderer(renderer_);
		renderer_ = nullptr;
	}

	if (window_ != nullptr) {
		SDL_DestroyWindow(window_);
		window_ = nullptr;
	}

	if (SDL_WasInit(SDL_INIT_VIDEO)) {
		SDL_Quit();
	}

	initialized_ = false;
}
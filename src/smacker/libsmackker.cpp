#include "libsmackker.hpp"

#include "audio/music.hpp"
#include "storm/stormlib.hpp"

#include "libsmackerdec/SmackerDecoder.h"

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

libSmackker::libSmackker()
{
}

libSmackker::~libSmackker()
{
}

bool libSmackker::Play(StormLib& mpq, const std::string& smkPath, SDL_Renderer* renderer)
{
	const PlaybackResult result = PlayWithControl(mpq, smkPath, false, renderer);
	return result == PlaybackResult::Completed || result == PlaybackResult::Skipped;
}

libSmackker::PlaybackResult libSmackker::PlayWithControl(StormLib& mpq, const std::string& smkPath, bool allowSkip, SDL_Renderer* renderer)
{
	if (renderer == nullptr) {
		std::fprintf(stderr, "libSmackker: renderer is null\n");
		return PlaybackResult::Error;
	}

	std::vector<std::byte> smkData;
	if (!mpq.ReadFile(smkPath, smkData)) {
		std::fprintf(stderr, "libSmackker: failed to read %s from MPQ\n", smkPath.c_str());
		return PlaybackResult::Error;
	}

	SDL_IOStream* stream = SDL_IOFromConstMem(smkData.data(), smkData.size());
	if (stream == nullptr) {
		std::fprintf(stderr, "libSmackker: SDL_IOFromConstMem failed: %s\n", SDL_GetError());
		return PlaybackResult::Error;
	}

	SmackerDecoder decoder;
	if (!decoder.Open(stream)) {
		std::fprintf(stderr, "libSmackker: failed to open smacker stream: %s\n", smkPath.c_str());
		return PlaybackResult::Error;
	}

	const std::uint32_t width = decoder.frameWidth;
	const std::uint32_t height = decoder.frameHeight;
	const std::uint32_t frameCount = decoder.GetNumFrames();
	const float timingValue = decoder.GetFrameRate();
	float frameDelayMs = 0.0f;
	if (timingValue <= 0.0f) {
		frameDelayMs = 1000.0f / 15.0f;
	} else if (timingValue <= 30.0f) {
		// libsmackerdec commonly reports FPS for SMK files like Diablo's logo.
		frameDelayMs = 1000.0f / timingValue;
	} else {
		// Fallback for streams where timing is already expressed as ms/frame.
		frameDelayMs = timingValue;
	}

	std::fprintf(stderr,
		"libSmackker: decoding %s %ux%u frames=%u\n",
		smkPath.c_str(),
		width,
		height,
		frameCount);

	SDL_Texture* texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STREAMING,
		static_cast<int>(width),
		static_cast<int>(height));
	if (texture == nullptr) {
		std::fprintf(stderr, "libSmackker: SDL_CreateTexture failed: %s\n", SDL_GetError());
		return PlaybackResult::Error;
	}

	std::vector<std::uint8_t> frameIndices(width * height);
	std::vector<std::uint8_t> palette(768, 0);
	std::vector<std::uint8_t> rgba(width * height * 4, 0);

	Audio smkAudio;
	std::vector<std::uint8_t> audioFrame;
	bool hasSmkAudio = false;
	const SmackerAudioInfo smkAudioInfo = decoder.GetAudioTrackDetails(0);
	if (smkAudioInfo.sampleRate > 0 && smkAudioInfo.nChannels > 0 && smkAudioInfo.idealBufferSize > 0) {
		const SDL_AudioFormat audioFormat = (smkAudioInfo.bitsPerSample == 8) ? SDL_AUDIO_S8 : SDL_AUDIO_S16LE;
		hasSmkAudio = smkAudio.Init(
			static_cast<int>(smkAudioInfo.sampleRate),
			static_cast<int>(smkAudioInfo.nChannels),
			audioFormat);
		if (hasSmkAudio) {
			audioFrame.resize(smkAudioInfo.idealBufferSize);
		}
	}

	bool running = true;
	bool decodeOk = true;
	bool skipped = false;
	bool quitRequested = false;
	for (std::uint32_t frame = 0; frame < frameCount && running; ++frame) {
		const std::uint64_t frameStart = SDL_GetTicks();

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				quitRequested = true;
				running = false;
			} else if (allowSkip && event.type == SDL_EVENT_KEY_DOWN) {
				if (event.key.key == SDLK_SPACE || event.key.key == SDLK_RETURN || 
				    event.key.key == SDLK_KP_ENTER || event.key.key == SDLK_ESCAPE) {
					skipped = true;
					running = false;
				}
			}
		}

		decoder.GetNextFrame();
		if (hasSmkAudio && !audioFrame.empty()) {
			const std::uint32_t bytesRead = decoder.GetAudioData(0, reinterpret_cast<std::int16_t*>(audioFrame.data()));
			if (bytesRead > 0 && bytesRead <= audioFrame.size()) {
				smkAudio.QueueBytes(audioFrame.data(), bytesRead);
			}
		}
		decoder.GetPalette(palette.data());
		decoder.GetFrame(frameIndices.data());

		for (std::size_t i = 0; i < frameIndices.size(); ++i) {
			const std::uint8_t idx = frameIndices[i];
			const std::size_t p = static_cast<std::size_t>(idx) * 3;
			const std::size_t o = i * 4;
			rgba[o + 0] = palette[p + 0];
			rgba[o + 1] = palette[p + 1];
			rgba[o + 2] = palette[p + 2];
			rgba[o + 3] = 255;
		}

		if (!SDL_UpdateTexture(texture, nullptr, rgba.data(), static_cast<int>(width * 4))) {
			std::fprintf(stderr, "libSmackker: SDL_UpdateTexture failed: %s\n", SDL_GetError());
			decodeOk = false;
			running = false;
			break;
		}

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		SDL_RenderTexture(renderer, texture, nullptr, nullptr);
		SDL_RenderPresent(renderer);

		const std::uint64_t frameElapsed = SDL_GetTicks() - frameStart;
		if (frameElapsed < static_cast<std::uint64_t>(frameDelayMs)) {
			SDL_Delay(static_cast<std::uint32_t>(static_cast<std::uint64_t>(frameDelayMs) - frameElapsed));
		}
	}

	SDL_DestroyTexture(texture);

	if (quitRequested) {
		return PlaybackResult::Quit;
	}
    if (!decodeOk) {
		return PlaybackResult::Error;
	}
	if (skipped) {
		return PlaybackResult::Skipped;
	}
	return PlaybackResult::Completed;
}
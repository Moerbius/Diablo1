#pragma once

#include <SDL3/SDL_audio.h>

#include <cstddef>
#include <cstdint>
#include <vector>

class StormLib;

class Audio {
public:
	enum class MusicTrack {
		Dintro,
		Dtowne,
		Dlvla,
		Dlvlb,
		Dlvlc,
		Dlvld
	};

	Audio();
	~Audio();

	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;

	bool Init(int sampleRate, int channels, SDL_AudioFormat format = SDL_AUDIO_S16LE);
	bool QueueBytes(const void* data, std::size_t byteCount);
	bool QueueS16(const std::int16_t* samples, std::size_t sampleCount);
	bool LoadMusicFromMpq(StormLib& mpq, MusicTrack track);
	bool LoadMusicFromMpq(StormLib& mpq, const char* path);
	bool PlayMusic(bool loop);
	void StopMusic();
	void Update();
	void Clear();
	void Shutdown();

	bool IsInitialized() const;
	bool HasMusicLoaded() const;

private:
	bool RecreateStream(int sampleRate, int channels, SDL_AudioFormat format);

	SDL_AudioStream* stream_;
	bool initialized_;

	std::vector<std::uint8_t> musicData_;
	SDL_AudioSpec musicSpec_;
	bool hasMusicLoaded_;
	bool musicLooping_;
};

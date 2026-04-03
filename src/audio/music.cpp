#include "music.hpp"

#include "storm/stormlib.hpp"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_iostream.h>

#include <cstdio>

namespace {
const char* GetMusicTrackPath(Audio::MusicTrack track)
{
	switch (track) {
	case Audio::MusicTrack::Dintro:
		return "music\\dintro.wav";
	case Audio::MusicTrack::Dtowne:
		return "music\\dtowne.wav";
	case Audio::MusicTrack::Dlvla:
		return "music\\dlvla.wav";
	case Audio::MusicTrack::Dlvlb:
		return "music\\dlvlb.wav";
	case Audio::MusicTrack::Dlvlc:
		return "music\\dlvlc.wav";
	case Audio::MusicTrack::Dlvld:
		return "music\\dlvld.wav";
	}

	return nullptr;
}
}

Audio::Audio()
	: stream_(nullptr)
	, initialized_(false)
	, musicSpec_{}
	, hasMusicLoaded_(false)
	, musicLooping_(false)
{
}

Audio::~Audio()
{
	Shutdown();
}

bool Audio::Init(int sampleRate, int channels, SDL_AudioFormat format)
{
	if (initialized_) {
		return RecreateStream(sampleRate, channels, format);
	}

	return RecreateStream(sampleRate, channels, format);
}

bool Audio::RecreateStream(int sampleRate, int channels, SDL_AudioFormat format)
{
	if (stream_ != nullptr) {
		SDL_DestroyAudioStream(stream_);
		stream_ = nullptr;
		initialized_ = false;
	}

	if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		std::fprintf(stderr, "Audio::Init SDL_INIT_AUDIO failed: %s\n", SDL_GetError());
		return false;
	}

	SDL_AudioSpec spec{};
	spec.freq = sampleRate;
	spec.channels = static_cast<std::uint8_t>(channels);
	spec.format = format;

	stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
	if (stream_ == nullptr) {
		std::fprintf(stderr, "Audio::Init SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
		return false;
	}

	if (!SDL_ResumeAudioStreamDevice(stream_)) {
		std::fprintf(stderr, "Audio::Init SDL_ResumeAudioStreamDevice failed: %s\n", SDL_GetError());
		SDL_DestroyAudioStream(stream_);
		stream_ = nullptr;
		return false;
	}

	initialized_ = true;
	return true;
}

bool Audio::QueueBytes(const void* data, std::size_t byteCount)
{
	if (!initialized_ || stream_ == nullptr || data == nullptr || byteCount == 0) {
		return false;
	}

	if (byteCount > static_cast<std::size_t>(0x7FFFFFFF)) {
		return false;
	}

	if (!SDL_PutAudioStreamData(stream_, data, static_cast<int>(byteCount))) {
		std::fprintf(stderr, "Audio::QueueBytes SDL_PutAudioStreamData failed: %s\n", SDL_GetError());
		return false;
	}

	return true;
}

bool Audio::QueueS16(const std::int16_t* samples, std::size_t sampleCount)
{
	if (samples == nullptr || sampleCount == 0) {
		return false;
	}

	return QueueBytes(samples, sampleCount * sizeof(std::int16_t));
}

bool Audio::LoadMusicFromMpq(StormLib& mpq, MusicTrack track)
{
	const char* path = GetMusicTrackPath(track);
	if (path == nullptr) {
		return false;
	}

	return LoadMusicFromMpq(mpq, path);
}

bool Audio::LoadMusicFromMpq(StormLib& mpq, const char* path)
{
	if (path == nullptr) {
		return false;
	}

	std::vector<std::byte> fileData;
	if (!mpq.ReadFile(path, fileData)) {
		std::fprintf(stderr, "Audio::LoadMusicFromMpq failed to read %s\n", path);
		return false;
	}

	SDL_IOStream* io = SDL_IOFromConstMem(fileData.data(), fileData.size());
	if (io == nullptr) {
		std::fprintf(stderr, "Audio::LoadMusicFromMpq SDL_IOFromConstMem failed: %s\n", SDL_GetError());
		return false;
	}

	SDL_AudioSpec decodedSpec{};
	Uint8* decodedBuffer = nullptr;
	Uint32 decodedLength = 0;
	if (!SDL_LoadWAV_IO(io, true, &decodedSpec, &decodedBuffer, &decodedLength)) {
		std::fprintf(stderr, "Audio::LoadMusicFromMpq SDL_LoadWAV_IO failed for %s: %s\n", path, SDL_GetError());
		return false;
	}

	musicData_.assign(decodedBuffer, decodedBuffer + decodedLength);
	SDL_free(decodedBuffer);

	musicSpec_ = decodedSpec;
	hasMusicLoaded_ = !musicData_.empty();
	musicLooping_ = false;
	return hasMusicLoaded_;
}

bool Audio::PlayMusic(bool loop)
{
	if (!hasMusicLoaded_) {
		return false;
	}

	if (!Init(musicSpec_.freq, musicSpec_.channels, musicSpec_.format)) {
		return false;
	}

	if (!QueueBytes(musicData_.data(), musicData_.size())) {
		return false;
	}

	musicLooping_ = loop;
	return true;
}

void Audio::StopMusic()
{
	musicLooping_ = false;
	Clear();
}

void Audio::Update()
{
	if (!musicLooping_ || stream_ == nullptr || musicData_.empty()) {
		return;
	}

	if (SDL_GetAudioStreamAvailable(stream_) <= 0) {
		QueueBytes(musicData_.data(), musicData_.size());
	}
}

void Audio::Clear()
{
	if (stream_ != nullptr) {
		SDL_ClearAudioStream(stream_);
	}
}

void Audio::Shutdown()
{
	StopMusic();
	hasMusicLoaded_ = false;
	musicData_.clear();
	musicSpec_ = {};

	if (stream_ != nullptr) {
		SDL_DestroyAudioStream(stream_);
		stream_ = nullptr;
	}

	initialized_ = false;
}

bool Audio::IsInitialized() const
{
	return initialized_;
}

bool Audio::HasMusicLoaded() const
{
	return hasMusicLoaded_;
}

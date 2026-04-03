#include "sfx.hpp"

#include "storm/stormlib.hpp"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_iostream.h>

#include <cstdio>

Sfx::Sfx() : stream_(nullptr), spec_{}, loaded_(false)
{
}

Sfx::~Sfx()
{
	Shutdown();
}

bool Sfx::InitStream(const SDL_AudioSpec& spec)
{
	if (stream_ != nullptr) {
		SDL_DestroyAudioStream(stream_);
		stream_ = nullptr;
	}

	if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		std::fprintf(stderr, "Sfx::InitStream SDL_INIT_AUDIO failed: %s\n", SDL_GetError());
		return false;
	}

	stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
	if (stream_ == nullptr) {
		std::fprintf(stderr, "Sfx::InitStream SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
		return false;
	}

	if (!SDL_ResumeAudioStreamDevice(stream_)) {
		std::fprintf(stderr, "Sfx::InitStream SDL_ResumeAudioStreamDevice failed: %s\n", SDL_GetError());
		SDL_DestroyAudioStream(stream_);
		stream_ = nullptr;
		return false;
	}

	return true;
}

static const char* GetSfxPath(Sfx::Animals track)
{
	switch (track) {
	case Sfx::Animals::Mage48:    return "sfx\\animals\\mage48.wav";
	case Sfx::Animals::Mage49:    return "sfx\\animals\\mage49.wav";
	case Sfx::Animals::Mage50:    return "sfx\\animals\\mage50.wav";
	case Sfx::Animals::Mage51:    return "sfx\\animals\\mage51.wav";
	case Sfx::Animals::Mage52:    return "sfx\\animals\\mage52.wav";
	case Sfx::Animals::Mage53:    return "sfx\\animals\\mage53.wav";
	case Sfx::Animals::Rogue48:   return "sfx\\animals\\rogue48.wav";
	case Sfx::Animals::Rogue49:   return "sfx\\animals\\rogue49.wav";
	case Sfx::Animals::Rogue50:   return "sfx\\animals\\rogue50.wav";
	case Sfx::Animals::Rogue51:   return "sfx\\animals\\rogue51.wav";
	case Sfx::Animals::Rogue52:   return "sfx\\animals\\rogue52.wav";
	case Sfx::Animals::Rogue53:   return "sfx\\animals\\rogue53.wav";
	case Sfx::Animals::Warrior48: return "sfx\\animals\\warrior48.wav";
	case Sfx::Animals::Warrior49: return "sfx\\animals\\warrior49.wav";
	case Sfx::Animals::Warrior50: return "sfx\\animals\\warrior50.wav";
	case Sfx::Animals::Warrior51: return "sfx\\animals\\warrior51.wav";
	case Sfx::Animals::Warrior52: return "sfx\\animals\\warrior52.wav";
	case Sfx::Animals::Warrior53: return "sfx\\animals\\warrior53.wav";
	}
	return nullptr;
}

static const char* GetSfxPath(Sfx::Items track)
{
	switch (track) {
	case Sfx::Items::TitleMove:   return "sfx\\items\\titlemov.wav";
	case Sfx::Items::TitleSelect: return "sfx\\items\\titlslct.wav";
	}
	return nullptr;
}

bool Sfx::LoadFromMpq(StormLib& mpq, Animals track)
{
	return LoadFromMpq(mpq, GetSfxPath(track));
}

bool Sfx::LoadFromMpq(StormLib& mpq, Items track)
{
	return LoadFromMpq(mpq, GetSfxPath(track));
}

bool Sfx::LoadFromMpq(StormLib& mpq, const char* path)
{
	if (path == nullptr) {
		return false;
	}

	std::vector<std::byte> fileData;
	if (!mpq.ReadFile(path, fileData)) {
		std::fprintf(stderr, "Sfx::LoadFromMpq failed to read %s\n", path);
		return false;
	}

	SDL_IOStream* io = SDL_IOFromConstMem(fileData.data(), fileData.size());
	if (io == nullptr) {
		std::fprintf(stderr, "Sfx::LoadFromMpq SDL_IOFromConstMem failed: %s\n", SDL_GetError());
		return false;
	}

	Uint8* decodedBuffer = nullptr;
	Uint32 decodedLength = 0;
	if (!SDL_LoadWAV_IO(io, true, &spec_, &decodedBuffer, &decodedLength)) {
		std::fprintf(stderr, "Sfx::LoadFromMpq SDL_LoadWAV_IO failed for %s: %s\n", path, SDL_GetError());
		return false;
	}

	data_.assign(decodedBuffer, decodedBuffer + decodedLength);
	SDL_free(decodedBuffer);

	loaded_ = !data_.empty();
	return loaded_;
}

bool Sfx::PlayOneShot()
{
	if (!loaded_) {
		return false;
	}

	if (!InitStream(spec_)) {
		return false;
	}

	if (!SDL_PutAudioStreamData(stream_, data_.data(), static_cast<int>(data_.size()))) {
		std::fprintf(stderr, "Sfx::PlayOneShot SDL_PutAudioStreamData failed: %s\n", SDL_GetError());
		return false;
	}

	return true;
}

void Sfx::Stop()
{
	if (stream_ != nullptr) {
		SDL_ClearAudioStream(stream_);
	}
}

void Sfx::Shutdown()
{
	Stop();
	if (stream_ != nullptr) {
		SDL_DestroyAudioStream(stream_);
		stream_ = nullptr;
	}
	data_.clear();
	loaded_ = false;
	 spec_ = {};
}

bool Sfx::IsLoaded() const
{
	return loaded_;
}

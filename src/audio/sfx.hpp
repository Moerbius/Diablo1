#pragma once

#include <SDL3/SDL_audio.h>

#include <cstdint>
#include <vector>

class StormLib;

class Sfx {
public:

	enum class Animals {
		Mage48,
		Mage49,
		Mage50,
		Mage51,
		Mage52,
		Mage53,
		Rogue48,
		Rogue49,
		Rogue50,
		Rogue51,
		Rogue52,
		Rogue53,
		Warrior48,
		Warrior49,
		Warrior50,
		Warrior51,
		Warrior52,
		Warrior53
	};

	enum class Items {
		TitleMove,
		TitleSelect
	};

	Sfx();
	~Sfx();

	Sfx(const Sfx&) = delete;
	Sfx& operator=(const Sfx&) = delete;

	bool LoadFromMpq(StormLib& mpq, const char* path);
	bool LoadFromMpq(StormLib& mpq, Animals track);
	bool LoadFromMpq(StormLib& mpq, Items track);
	bool PlayOneShot();
	void Stop();
	void Shutdown();

	bool IsLoaded() const;

private:
	bool InitStream(const SDL_AudioSpec& spec);

	SDL_AudioStream* stream_;
	std::vector<std::uint8_t> data_;
	SDL_AudioSpec spec_;
	bool loaded_;
};

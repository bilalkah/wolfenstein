#include "SoundManager/sound_manager.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace wolfenstein {

std::expected<std::unique_ptr<SoundManager>, std::string> SoundManager::Open(
	const std::string& sound_dir, const std::string& music_dir,
	std::span<const std::string> tracks) {
	const auto error = [](std::string_view what, const char* detail) {
		return std::unexpected(std::string(what) + ": " + detail);
	};
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		return error("cannot initialise SDL audio", SDL_GetError());
	}
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return error("cannot open the audio device", Mix_GetError());
	}
	// From here the destructor closes the device, whatever fails next
	auto sound = std::make_unique<SoundManager>();
	sound->open_ = true;
	Mix_AllocateChannels(kChannels);
	// Sounds from places are mixed in after the channels, by the game: in
	// the 16-bit stereo it asked the device for
	int frequency = 0;
	Uint16 format = 0;
	int channels = 0;
	if (Mix_QuerySpec(&frequency, &format, &channels) != 0 &&
		format == AUDIO_S16SYS && channels == 2) {
		Mix_SetPostMix(&SpatialMixer::MixHook, &sound->spatial_);
		sound->spatial_open_ = true;
	}

	for (const auto& [effect, file, volume] :
		 {std::tuple{SoundEffect::NpcAttack, "npc_attack.wav", 32},
		  std::tuple{SoundEffect::NpcPain, "npc_pain.wav", 32},
		  std::tuple{SoundEffect::NpcDeath, "npc_death.wav", 64},
		  std::tuple{SoundEffect::PlayerPain, "player_pain.wav", 64},
		  std::tuple{SoundEffect::Shotgun, "shotgun.wav", 64},
		  std::tuple{SoundEffect::Pickup, "pickup.wav", 64},
		  std::tuple{SoundEffect::PistolShot, "pistol.wav", 64},
		  std::tuple{SoundEffect::SmgShot, "mp5.wav", 64},
		  std::tuple{SoundEffect::DryFire, "dry_fire.wav", 64},
		  std::tuple{SoundEffect::PlayerFall, "player_fall.wav", 64},
		  std::tuple{SoundEffect::DoorMove, "door.wav", 64},
		  std::tuple{SoundEffect::EnemyAlert, "enemy_alert.wav", 64},
		  std::tuple{SoundEffect::StepLeft, "step_left.wav", 64},
		  std::tuple{SoundEffect::StepRight, "step_right.wav", 64},
		  std::tuple{SoundEffect::AmmoPickup, "ammo_pickup.wav", 64},
		  std::tuple{SoundEffect::KeyPickup, "key_pickup.wav", 64},
		  std::tuple{SoundEffect::WeaponPickup, "weapon_pickup.wav", 64},
		  std::tuple{SoundEffect::DemonAttack, "demon_attack.wav", 64},
		  std::tuple{SoundEffect::DemonAlert, "demon_alert.wav", 64},
		  std::tuple{SoundEffect::DemonPain, "demon_pain.wav", 48},
		  std::tuple{SoundEffect::DemonDeath, "demon_death.wav", 64},
		  std::tuple{SoundEffect::CacoAlert, "caco_alert.wav", 64},
		  std::tuple{SoundEffect::CacoDeath, "caco_death.wav", 64},
		  std::tuple{SoundEffect::CyberAlert, "cyber_alert.wav", 80},
		  std::tuple{SoundEffect::CyberDeath, "cyber_death.wav", 80},
		  std::tuple{SoundEffect::ZombieAlert, "zombie_alert.wav", 64},
		  std::tuple{SoundEffect::ZombieDeath, "zombie_death.wav", 64},
		  std::tuple{SoundEffect::SuperShotgun, "super_shotgun.wav", 64},
		  std::tuple{SoundEffect::SuperShotgunReload,
					 "super_shotgun_reload.wav", 56},
		  std::tuple{SoundEffect::SawUp, "saw_up.wav", 56},
		  std::tuple{SoundEffect::Saw, "saw.wav", 56},
		  std::tuple{SoundEffect::SawHit, "saw_hit.wav", 64},
		  std::tuple{SoundEffect::RocketLaunch, "rocket_launch.wav", 64},
		  std::tuple{SoundEffect::RocketBurst, "rocket_burst.wav", 80},
		  std::tuple{SoundEffect::Plasma, "plasma.wav", 56},
		  std::tuple{SoundEffect::PlasmaBurst, "plasma_burst.wav", 48}}) {
		if (auto loaded = sound->LoadSound(effect, sound_dir + file, volume);
			!loaded) {
			return std::unexpected(loaded.error());
		}
	}

	// Every track, loaded now: switching tracks as levels start allocates
	// nothing
	sound->tracks_.reserve(tracks.size());
	for (const std::string& name : tracks) {
		const std::string file = music_dir + name + ".mp3";
		Mix_Music* music = Mix_LoadMUS(file.c_str());
		if (music == nullptr) {
			return error("cannot load " + file, Mix_GetError());
		}
		sound->tracks_.push_back({name, music});
	}
	Mix_VolumeMusic(64);
	return sound;
}

void SoundManager::PlayMusic(std::string_view name) {
	if (name == playing_) {
		return;	 // already playing: it goes on
	}
	playing_ = name;
	if (!open_) {
		return;
	}
	const auto track = std::ranges::find(tracks_, name, &Track::name);
	if (track == tracks_.end()) {
		Mix_HaltMusic();  // no such track: silence
		return;
	}
	constexpr int kFadeInMs = 400;
	if (Mix_FadeInMusic(track->music, -1, kFadeInMs) == -1) {
		std::cerr << "Failed to play " << name << ": " << Mix_GetError()
				  << '\n';
	}
}

SoundManager::~SoundManager() {
	if (!open_) {
		return;
	}
	Mix_SetPostMix(nullptr, nullptr);
	Mix_HaltMusic();
	Mix_HaltChannel(-1);
	for (Mix_Chunk* chunk : chunks_) {
		Mix_FreeChunk(chunk);
	}
	for (const Track& track : tracks_) {
		Mix_FreeMusic(track.music);
	}
	Mix_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

MixerLevels ToMixerLevels(double master, double music, double effects) {
	constexpr int kMusicLevel = 64;	 // the theme's level at full volume
	const double share = std::clamp(master, 0.0, 1.0);
	return {.music = static_cast<int>(kMusicLevel * share *
									  std::clamp(music, 0.0, 1.0)),
			.effects = static_cast<int>(MIX_MAX_VOLUME * share *
										std::clamp(effects, 0.0, 1.0))};
}

// Changes the audio device, not the manager's members, but is not const:
// it changes what the manager plays
// NOLINTNEXTLINE(readability-make-member-function-const)
void SoundManager::SetVolume(double master, double music, double effects) {
	if (!open_) {
		return;
	}
	const MixerLevels levels = ToMixerLevels(master, music, effects);
	Mix_VolumeMusic(levels.music);
	// The master volume scales the effects' channels, not the music
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
	Mix_MasterVolume(levels.effects);
#else
	Mix_Volume(-1, levels.effects);
#endif
	spatial_.SetVolume(static_cast<float>(levels.effects) /
					   static_cast<float>(MIX_MAX_VOLUME));
}

SoundChannel SoundManager::AllocateChannel() {
	const int channel = next_channel_;
	next_channel_ = (next_channel_ + 1) % kChannels;
	return SoundChannel{channel};
}

void SoundManager::PlayEffect(SoundChannel channel, SoundEffect effect) {
	++play_counts_[std::to_underlying(effect)];
	if (!open_) {
		return;
	}
	const int index = std::to_underlying(channel);
	Mix_HaltChannel(index);
	if (Mix_PlayChannel(index, chunks_[std::to_underlying(effect)], 0) == -1) {
		std::cerr << "Failed to play sound: " << Mix_GetError() << '\n';
	}
}

void SoundManager::PlayAt(SoundEffect effect, const vector2d& where,
						  bool muffled, std::uint32_t source) {
	++play_counts_[std::to_underlying(effect)];
	if (!open_) {
		return;
	}
	Mix_Chunk* chunk = chunks_[std::to_underlying(effect)];
	if (spatial_open_) {
		spatial_.Play(chunk, where, muffled, source);
	}
	else if (Mix_PlayChannel(-1, chunk, 0) == -1) {
		std::cerr << "Failed to play sound: " << Mix_GetError() << '\n';
	}
}

void SoundManager::SetListener(const vector2d& ear, double theta) {
	if (spatial_open_) {
		spatial_.SetListener(ear, theta);
	}
}

std::expected<void, std::string> SoundManager::LoadSound(
	SoundEffect effect, const std::string& sound_path, int volume) {
	Mix_Chunk* sound = Mix_LoadWAV(sound_path.c_str());
	if (sound == nullptr) {
		return std::unexpected("cannot load " + sound_path + ": " +
							   Mix_GetError());
	}
	Mix_VolumeChunk(sound, volume);
	chunks_[std::to_underlying(effect)] = sound;
	return {};
}

}  // namespace wolfenstein

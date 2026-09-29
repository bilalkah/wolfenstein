#include "SoundManager/sound_manager.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace wolfenstein {

namespace {

// What the sounds are mixed in: float stereo at CD rate, as the sounds are
// recorded (SDL converts it to what the device plays)
constexpr SDL_AudioSpec kMix{.format = SDL_AUDIO_F32,
							 .channels = 2,
							 .freq = 44100};
constexpr int kFrameBytes = static_cast<int>(SDL_AUDIO_FRAMESIZE(kMix));

}  // namespace

std::expected<std::unique_ptr<SoundManager>, std::string> SoundManager::Open(
	const std::string& sound_dir, const std::string& music_dir,
	std::span<const std::string> tracks) {
	const auto error = [](std::string_view what) {
		return std::unexpected(std::string(what) + ": " + SDL_GetError());
	};
	if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		return error("cannot initialise SDL audio");
	}
	if (!MIX_Init()) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return error("cannot initialise SDL_mixer");
	}
	MIX_Mixer* mixer = MIX_CreateMixer(&kMix);
	if (mixer == nullptr) {
		MIX_Quit();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return error("cannot create the mixer");
	}
	// From here the destructor closes everything, whatever fails next
	auto sound = std::make_unique<SoundManager>();
	sound->open_ = true;
	sound->mixer_ = mixer;
	sound->music_options_ = SDL_CreateProperties();
	SDL_SetNumberProperty(sound->music_options_, MIX_PROP_PLAY_LOOPS_NUMBER,
						  -1);
	constexpr int kFadeInMs = 400;
	SDL_SetNumberProperty(sound->music_options_,
						  MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, kFadeInMs);

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

	// Every track, loaded now (decoded as it plays), each on a track of its
	// own: switching tracks as levels start allocates nothing
	sound->tracks_.reserve(tracks.size());
	const MixerLevels levels = ToMixerLevels(1.0, 1.0, 1.0);
	for (const std::string& name : tracks) {
		const std::string file = music_dir + name + ".mp3";
		MIX_Audio* audio = MIX_LoadAudio(mixer, file.c_str(), false);
		if (audio == nullptr) {
			return error("cannot load " + file);
		}
		Track& track =
			sound->tracks_.emplace_back(name, audio, MIX_CreateTrack(mixer));
		if (track.track == nullptr || !MIX_SetTrackAudio(track.track, audio)) {
			return error("cannot load " + file);
		}
		MIX_SetTrackGain(track.track, levels.music);
	}
	// Each track played a moment, unheard, as it will be: SDL_mixer sets up
	// what a track needs to play (its buffers) the first time it plays,
	// which would otherwise be as a level starts
	constexpr int kPrimeFeeds = 8;
	for (const Track& track : sound->tracks_) {
		MIX_PlayTrack(track.track, sound->music_options_);
		for (int feed = 0; feed < kPrimeFeeds; ++feed) {
			sound->Generate(kFeedFrames);
		}
		MIX_StopTrack(track.track, 0);
		sound->Generate(kFeedFrames);
	}

	// Last, the device: from here SDL's audio thread asks for what to play
	sound->device_ =
		SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &kMix,
								  &SoundManager::Feed, sound.get());
	if (sound->device_ == nullptr) {
		return error("cannot open the audio device");
	}
	SDL_ResumeAudioStreamDevice(sound->device_);
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
	if (current_ != nullptr) {
		MIX_StopTrack(current_->track, 0);
		current_ = nullptr;
	}
	const auto track = std::ranges::find(tracks_, name, &Track::name);
	if (track == tracks_.end()) {
		return;	 // no such track: silence
	}
	if (!MIX_PlayTrack(track->track, music_options_)) {
		std::cerr << "Failed to play " << name << ": " << SDL_GetError()
				  << '\n';
		return;
	}
	current_ = &*track;
}

SoundManager::~SoundManager() {
	if (!open_) {
		return;
	}
	// The device first, so its thread stops asking; then the mixer with its
	// tracks, and the music they played
	SDL_DestroyAudioStream(device_);
	MIX_DestroyMixer(mixer_);
	for (const Track& track : tracks_) {
		MIX_DestroyAudio(track.audio);
	}
	SDL_DestroyProperties(music_options_);
	MIX_Quit();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

MixerLevels ToMixerLevels(double master, double music, double effects) {
	constexpr double kMusicLevel = 0.5;	 // the theme's level at full volume
	const double share = std::clamp(master, 0.0, 1.0);
	return {
		.music = static_cast<float>(kMusicLevel * share *
									std::clamp(music, 0.0, 1.0)),
		.effects = static_cast<float>(share * std::clamp(effects, 0.0, 1.0))};
}

// Changes the audio device, not the manager's members, but is not const:
// it changes what the manager plays
// NOLINTNEXTLINE(readability-make-member-function-const)
void SoundManager::SetVolume(double master, double music, double effects) {
	if (!open_) {
		return;
	}
	const MixerLevels levels = ToMixerLevels(master, music, effects);
	for (const Track& track : tracks_) {
		MIX_SetTrackGain(track.track, levels.music);
	}
	spatial_.SetVolume(levels.effects);
}

SoundChannel SoundManager::AllocateChannel() {
	const std::uint32_t channel = next_channel_;
	next_channel_ = (next_channel_ + 1) % kChannels;
	return SoundChannel{channel};
}

void SoundManager::PlayEffect(SoundChannel channel, SoundEffect effect) {
	++play_counts_[std::to_underlying(effect)];
	if (!open_) {
		return;
	}
	spatial_.PlayCentred(&clips_[std::to_underlying(effect)],
						 std::to_underlying(channel));
}

void SoundManager::PlayAt(SoundEffect effect, const vector2d& where,
						  bool muffled, std::uint32_t source) {
	++play_counts_[std::to_underlying(effect)];
	if (!open_) {
		return;
	}
	spatial_.Play(&clips_[std::to_underlying(effect)], where, muffled, source);
}

void SoundManager::SetListener(const vector2d& ear, double theta) {
	if (open_) {
		spatial_.SetListener(ear, theta);
	}
}

void SoundManager::Generate(int frames) {
	const int bytes = frames * kFrameBytes;
	if (MIX_Generate(mixer_, feed_.data(), bytes) != bytes) {
		std::ranges::fill(feed_, 0.0F);
	}
	spatial_.Mix(feed_.data(), frames, kMix.channels);
}

void SDLCALL SoundManager::Feed(void* manager, SDL_AudioStream* stream,
								int additional_amount, int /*total_amount*/) {
	auto& sound = *static_cast<SoundManager*>(manager);
	for (int left = additional_amount; left > 0;) {
		const int frames =
			std::min((left + kFrameBytes - 1) / kFrameBytes, kFeedFrames);
		sound.Generate(frames);
		SDL_PutAudioStreamData(stream, sound.feed_.data(),
							   frames * kFrameBytes);
		left -= frames * kFrameBytes;
	}
}

std::expected<void, std::string> SoundManager::LoadSound(
	SoundEffect effect, const std::string& sound_path, int volume) {
	const auto failed = [&] {
		return std::unexpected("cannot load " + sound_path + ": " +
							   SDL_GetError());
	};
	SDL_AudioSpec spec{};
	Uint8* data = nullptr;
	Uint32 length = 0;
	if (!SDL_LoadWAV(sound_path.c_str(), &spec, &data, &length)) {
		return failed();
	}
	Uint8* converted = nullptr;
	int converted_length = 0;
	const bool ok =
		SDL_ConvertAudioSamples(&spec, data, static_cast<int>(length), &kMix,
								&converted, &converted_length);
	SDL_free(data);
	if (!ok) {
		return failed();
	}
	const std::size_t index = std::to_underlying(effect);
	std::vector<float>& samples = samples_[index];
	samples.resize(static_cast<std::size_t>(converted_length) / sizeof(float));
	std::memcpy(samples.data(), converted, samples.size() * sizeof(float));
	SDL_free(converted);
	constexpr float kFullVolume = 128.0F;
	clips_[index] = {.samples = samples.data(),
					 .frames = static_cast<std::uint32_t>(samples.size() / 2),
					 .level = static_cast<float>(volume) / kFullVolume};
	return {};
}

}  // namespace wolfenstein

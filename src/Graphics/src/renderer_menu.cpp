#include "Graphics/renderer_menu.h"
#include "Settings/settings.h"
#include "State/weapon_state.h"
#include "TextureManager/texture_manager.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <format>
#include <string>

namespace wolfenstein {

namespace {

constexpr int kButtonWidth = 420;
constexpr int kButtonHeight = 72;
constexpr int kButtonGap = 18;

// A column of equally sized buttons centred horizontally, starting at top
SDL_Rect ButtonRect(int screen_width, int top, int index) {
	return {(screen_width - kButtonWidth) / 2,
			top + index * (kButtonHeight + kButtonGap), kButtonWidth,
			kButtonHeight};
}

}  // namespace

Menu::Menu(RendererContext& context, SoundManager& sound,
		   std::span<const WeaponConfig> weapons)
	: context_(&context),
	  ui_(std::make_unique<ui::Ui>(
		  context_->GetRenderer(),
		  std::string(RESOURCE_DIR) + "font/EternalAncient.ttf",
		  std::string(RESOURCE_DIR) + "font/Roboto-Light.ttf")),
	  weapon_configs_(weapons) {
	background_texture_ = context_->Textures().GetTextureId("menu_background");
	for (const WeaponConfig& config : weapon_configs_) {
		weapons_.push_back(
			std::make_unique<Weapon>(config, context_->Textures(), sound));
	}
}

void Menu::DrawLevelBanner(std::string_view title, std::string_view name,
						   Uint8 alpha) {
	if (alpha == 0) {
		return;
	}
	const auto& config = context_->GetConfig();
	const int centre_x = config.width / 2;
	const int top = config.height / 2 - 110;
	SDL_Color heading = ui::color::kText;
	heading.a = alpha;
	ui_->Text(title, centre_x, top, ui::FontStyle::Title, heading,
			  ui::Align::Center);
	if (!name.empty()) {
		SDL_Color accent = ui::color::kAccentBright;
		accent.a = alpha;
		ui_->Text(name, centre_x, top + 118, ui::FontStyle::Heading, accent,
				  ui::Align::Center);
	}
}

void Menu::Open(MenuScreen screen) {
	if (screen == MenuScreen::Controls || screen == MenuScreen::Settings) {
		return_screen_ = screen_;
	}
	screen_ = screen;
	if (screen == MenuScreen::WeaponSelect) {
		for (const auto& weapon : weapons_) {
			weapon->TransitionTo(WeaponStateType::Loaded);
		}
		previewed_weapon_ = -1;
	}
	ui_->ResetFocus();
}

void Menu::SetPreviewedWeapon(int index) {
	if (index == previewed_weapon_) {
		return;
	}
	// The card losing focus goes back to its first frame instead of freezing
	// mid-animation; the card gaining it plays the reload animation
	if (previewed_weapon_ >= 0) {
		weapons_[static_cast<std::size_t>(previewed_weapon_)]->TransitionTo(
			WeaponStateType::Loaded);
	}
	previewed_weapon_ = index;
	if (index >= 0) {
		weapons_[static_cast<std::size_t>(index)]->Reload();
	}
}

void Menu::GoBack() {
	if (screen_ == MenuScreen::Settings) {
		Settings::Get().Save();
	}
	// Land on the button that opened the screen we are leaving; both the main
	// and pause screens list Controls second and Settings third
	const int opener = screen_ == MenuScreen::Controls ? 1 : 2;
	screen_ = return_screen_;
	ui_->ResetFocus(opener);
}

void Menu::HandleEvent(const SDL_Event& event) {
	input_.Handle(event);
}

MenuAction Menu::Update(double delta_time) {
	ui_->BeginFrame(input_);
	MenuAction action;
	switch (screen_) {
		case MenuScreen::Main:
			action = MainScreen();
			break;
		case MenuScreen::WeaponSelect:
			action = WeaponSelectScreen(delta_time);
			break;
		case MenuScreen::Controls:
			action = ControlsScreen();
			break;
		case MenuScreen::Settings:
			action = SettingsScreen();
			break;
		case MenuScreen::Pause:
			action = PauseScreen();
			break;
		case MenuScreen::Result:
			action = ResultScreen();
			break;
	}
	ui_->EndFrame();
	input_.BeginFrame();
	return action;
}

MenuAction Menu::MainScreen() {
	const int width = context_->GetConfig().width;
	DrawBackground();
	DrawDimmer(150);

	ui_->Text("WOLFENSTEIN", width / 2, 120, ui::FontStyle::Title,
			  ui::color::kText, ui::Align::Center);
	ui_->FillRect({width / 2 - 180, 262, 360, 4}, ui::color::kAccent);
	ui_->Text("Hold the line, comrade.", width / 2, 290, ui::FontStyle::Body,
			  ui::color::kMuted, ui::Align::Center);

	constexpr int kTop = 380;
	MenuAction action;
	if (ui_->Button("PLAY", ButtonRect(width, kTop, 0))) {
		Open(MenuScreen::WeaponSelect);
	}
	if (ui_->Button("CONTROLS", ButtonRect(width, kTop, 1))) {
		Open(MenuScreen::Controls);
	}
	if (ui_->Button("SETTINGS", ButtonRect(width, kTop, 2))) {
		Open(MenuScreen::Settings);
	}
#ifndef __EMSCRIPTEN__
	// A browser tab cannot close itself, so only native builds offer Quit
	if (ui_->Button("QUIT", ButtonRect(width, kTop, 3))) {
		action.type = MenuAction::Type::Quit;
	}
#endif
	DrawHint("Arrow keys or mouse to choose  ·  Enter to select");
	return action;
}

MenuAction Menu::WeaponSelectScreen(double delta_time) {
	const int width = context_->GetConfig().width;
	DrawBackground();
	DrawDimmer(190);

	ui_->Text("CHOOSE YOUR WEAPON", width / 2, 50, ui::FontStyle::Heading,
			  ui::color::kText, ui::Align::Center);

	// Cards sit side by side, so left/right move between them too
	if (input_.left) {
		ui_->MoveFocus(-1);
	}
	if (input_.right) {
		ui_->MoveFocus(1);
	}

	constexpr int kCardWidth = 470;
	constexpr int kCardHeight = 590;
	constexpr int kCardGap = 40;
	const int cards_left =
		(width - static_cast<int>(weapons_.size()) * kCardWidth -
		 (static_cast<int>(weapons_.size()) - 1) * kCardGap) /
		2;

	MenuAction action;
	int focused_card = -1;
	for (std::size_t i = 0; i < weapons_.size(); ++i) {
		const SDL_Rect card{
			cards_left + static_cast<int>(i) * (kCardWidth + kCardGap), 140,
			kCardWidth, kCardHeight};
		bool focused = false;
		if (ui_->Selectable(card, focused)) {
			action.type = MenuAction::Type::StartGame;
			action.weapon = weapon_configs_[i].weapon_name;
		}
		if (focused) {
			focused_card = static_cast<int>(i);
			SetPreviewedWeapon(focused_card);
			weapons_[i]->Update(delta_time);
		}
		DrawWeaponCard(card, *weapons_[i], focused);
	}

	if (focused_card < 0) {
		SetPreviewedWeapon(-1);
	}

	if (ui_->Button("BACK", {(width - 300) / 2, 760, 300, 64}) || input_.back) {
		Open(MenuScreen::Main);
	}
	DrawHint("Click a weapon or press Enter to start  ·  Esc to go back");
	return action;
}

void Menu::DrawWeaponCard(const SDL_Rect& rect, const Weapon& weapon,
						  bool focused) {
	constexpr int kPadding = 28;
	ui_->FillRect(rect, focused ? ui::color::kPanelFocused : ui::color::kPanel);
	ui_->DrawRect(rect, focused ? ui::color::kAccent : ui::color::kBorder,
				  focused ? 3 : 1);

	// Weapon sprite, scaled to fit the preview area and kept in proportion
	const SDL_Rect preview{rect.x + kPadding, rect.y + kPadding,
						   rect.w - 2 * kPadding, 230};
	const auto& texture =
		context_->Textures().GetTexture(weapon.GetTextureId());
	if (texture.texture != nullptr && texture.width > 0 && texture.height > 0) {
		const double scale =
			std::min(static_cast<double>(preview.w) / texture.width,
					 static_cast<double>(preview.h) / texture.height);
		const int w = static_cast<int>(texture.width * scale);
		const int h = static_cast<int>(texture.height * scale);
		const SDL_Rect dest{preview.x + (preview.w - w) / 2,
							preview.y + preview.h - h, w, h};
		SDL_RenderCopy(context_->GetRenderer(), texture.texture, nullptr,
					   &dest);
	}

	const auto info = std::ranges::find(weapon_configs_, weapon.GetWeaponName(),
										&WeaponConfig::weapon_name);
	int y = preview.y + preview.h + 22;
	ui_->Text(info->label, rect.x + kPadding, y, ui::FontStyle::Heading,
			  focused ? ui::color::kText : ui::color::kMuted);
	y += 62;
	ui_->Text(info->description, rect.x + kPadding, y, ui::FontStyle::Small,
			  ui::color::kMuted);
	y += 44;

	// Stats as bars relative to the best weapon for each stat
	double best_damage = 0, best_rate = 0, best_capacity = 0, best_reload = 1e9;
	for (const auto& w : weapons_) {
		const auto [max_damage, min_damage] = w->GetAttackDamage();
		best_damage = std::max(best_damage, (max_damage + min_damage) / 2);
		best_rate = std::max(best_rate, 1.0 / w->GetAttackSpeed());
		best_capacity =
			std::max(best_capacity, static_cast<double>(w->GetAmmoCapacity()));
		best_reload = std::min(best_reload, w->GetReloadSpeed());
	}
	const auto [max_damage, min_damage] = weapon.GetAttackDamage();
	const double rate = 1.0 / weapon.GetAttackSpeed();
	struct Stat
	{
		const char* label = nullptr;
		ui::FixedText<16> value;
		double fill = 0.0;
	};
	const std::array<Stat, 4> stats = {{
		{"Damage", ui::FixedText<16>("{:.0f}-{:.0f}", min_damage, max_damage),
		 (max_damage + min_damage) / 2 / best_damage},
		{"Fire rate", ui::FixedText<16>("{:.1f}/s", rate), rate / best_rate},
		{"Magazine", ui::FixedText<16>("{}", weapon.GetAmmoCapacity()),
		 static_cast<double>(weapon.GetAmmoCapacity()) / best_capacity},
		{"Reload", ui::FixedText<16>("{:.1f}s", weapon.GetReloadSpeed()),
		 best_reload / weapon.GetReloadSpeed()},
	}};
	const int bar_left = rect.x + 170;
	const int bar_width = rect.w - 170 - kPadding - 80;
	for (const auto& stat : stats) {
		ui_->Text(stat.label, rect.x + kPadding, y, ui::FontStyle::Small,
				  ui::color::kMuted);
		ui_->FillRect({bar_left, y + 9, bar_width, 8}, ui::color::kTrack);
		ui_->FillRect(
			{bar_left, y + 9, static_cast<int>(bar_width * stat.fill), 8},
			focused ? ui::color::kAccent : ui::color::kMuted);
		ui_->Text(stat.value, rect.x + rect.w - kPadding, y,
				  ui::FontStyle::Small, ui::color::kText, ui::Align::Right);
		y += 38;
	}
}

MenuAction Menu::ControlsScreen() {
	const int width = context_->GetConfig().width;
	// Opened from the pause screen, the frozen game frame stays behind it
	if (return_screen_ == MenuScreen::Main) {
		DrawBackground();
	}
	DrawDimmer(200);

	ui_->Text("CONTROLS", width / 2, 70, ui::FontStyle::Heading,
			  ui::color::kText, ui::Align::Center);

	struct Binding
	{
		const char* action;
		const char* keys;
	};
	constexpr std::array<Binding, 7> kBindings = {{
		{"Move", "W  A  S  D"},
		{"Turn", "Mouse, or Left / Right arrows"},
		{"Fire", "Left click, or Left Ctrl"},
		{"Reload", "R"},
		{"Map view", "P"},
		{"Pause", "Esc"},
		{"Menus", "Arrow keys, Enter, Esc"},
	}};
	const SDL_Rect panel{(width - 800) / 2, 160, 800, 520};
	ui_->FillRect(panel, ui::color::kPanel);
	ui_->DrawRect(panel, ui::color::kBorder);
	int y = panel.y + 36;
	for (const auto& binding : kBindings) {
		ui_->Text(binding.action, panel.x + 48, y, ui::FontStyle::Body,
				  ui::color::kMuted);
		ui_->Text(binding.keys, panel.x + panel.w - 48, y, ui::FontStyle::Body,
				  ui::color::kText, ui::Align::Right);
		y += 56;
	}
#ifdef __EMSCRIPTEN__
	ui_->Text(
		"Click the game to capture the mouse; Esc releases it and pauses.",
		width / 2, panel.y + panel.h - 62, ui::FontStyle::Small,
		ui::color::kMuted, ui::Align::Center);
#endif

	if (ui_->Button("BACK", {(width - 300) / 2, 720, 300, 64}) || input_.back) {
		GoBack();
	}
	return {};
}

MenuAction Menu::SettingsScreen() {
	const int width = context_->GetConfig().width;
	if (return_screen_ == MenuScreen::Main) {
		DrawBackground();
	}
	DrawDimmer(200);

	ui_->Text("SETTINGS", width / 2, 70, ui::FontStyle::Heading,
			  ui::color::kText, ui::Align::Center);

	auto& settings = Settings::Get();
	MenuAction action;
	constexpr int kRowWidth = 760;
	constexpr int kRowHeight = 76;
	const int left = (width - kRowWidth) / 2;
	int y = 200;

	if (ui_->Slider("Mouse sensitivity",
					ui::FixedText<>("{:.2f}x", settings.mouse_sensitivity),
					{left, y, kRowWidth, kRowHeight},
					settings.mouse_sensitivity, Settings::kMinMouseSensitivity,
					Settings::kMaxMouseSensitivity, 0.05)) {
		action.type = MenuAction::Type::SettingsChanged;
	}
	y += kRowHeight + kButtonGap;
	if (ui_->Slider("Volume", ui::FixedText<>("{:.0f}%", settings.volume * 100),
					{left, y, kRowWidth, kRowHeight}, settings.volume, 0.0, 1.0,
					0.05)) {
		action.type = MenuAction::Type::SettingsChanged;
	}
	y += kRowHeight + kButtonGap;
	if (ui_->Toggle("Show FPS", {left, y, kRowWidth, kRowHeight},
					settings.show_fps)) {
		action.type = MenuAction::Type::SettingsChanged;
	}

	if (ui_->Button("BACK", {(width - 300) / 2, 720, 300, 64}) || input_.back) {
		GoBack();
	}
	DrawHint("Left / Right to adjust  ·  Esc to go back");
	return action;
}

MenuAction Menu::PauseScreen() {
	const int width = context_->GetConfig().width;
	DrawDimmer(170);

	ui_->Text("PAUSED", width / 2, 170, ui::FontStyle::Title, ui::color::kText,
			  ui::Align::Center);

	constexpr int kTop = 360;
	MenuAction action;
	if (ui_->Button("RESUME", ButtonRect(width, kTop, 0)) || input_.back) {
		action.type = MenuAction::Type::Resume;
	}
	if (ui_->Button("CONTROLS", ButtonRect(width, kTop, 1))) {
		Open(MenuScreen::Controls);
	}
	if (ui_->Button("SETTINGS", ButtonRect(width, kTop, 2))) {
		Open(MenuScreen::Settings);
	}
	if (ui_->Button("QUIT TO MENU", ButtonRect(width, kTop, 3))) {
		action.type = MenuAction::Type::QuitToMenu;
	}
	return action;
}

MenuAction Menu::ResultScreen() {
	const int width = context_->GetConfig().width;
	MenuAction action;
	if (ui_->Button("CONTINUE", {(width - 300) / 2, 780, 300, 64}) ||
		input_.back) {
		action.type = MenuAction::Type::QuitToMenu;
	}
	return action;
}

void Menu::DrawBackground() {
	SDL_RenderCopy(context_->GetRenderer(),
				   context_->Textures().GetTexture(background_texture_).texture,
				   nullptr, nullptr);
}

void Menu::DrawDimmer(Uint8 alpha) {
	const auto& config = context_->GetConfig();
	ui_->FillRect({0, 0, config.width, config.height}, {0, 0, 0, alpha});
}

void Menu::DrawHint(std::string_view text) {
	const auto& config = context_->GetConfig();
	// Dark strip so the hint stays readable over the background art
	ui_->FillRect({0, config.height - 64, config.width, 64}, {0, 0, 0, 170});
	ui_->Text(text, config.width / 2, config.height - 46, ui::FontStyle::Small,
			  ui::color::kMuted, ui::Align::Center);
}

}  // namespace wolfenstein

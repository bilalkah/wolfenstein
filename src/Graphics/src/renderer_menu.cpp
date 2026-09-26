#include "Graphics/renderer_menu.h"
#include "Core/scene.h"
#include "Settings/settings.h"
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

Menu::Menu(RendererContext& context,
		   std::span<const DifficultyChoice> difficulties)
	: context_(&context),
	  ui_(std::make_unique<ui::Ui>(
		  context_->GetRenderer(),
		  std::string(RESOURCE_DIR) + "font/EternalAncient.ttf",
		  std::string(RESOURCE_DIR) + "font/Roboto-Light.ttf")),
	  difficulties_(difficulties),
	  chosen_difficulty_(difficulties.size() > 1 ? 1 : 0) {
	background_texture_ = context_->Textures().GetTextureId("menu_background");
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

void Menu::DrawNotice(std::string_view text) {
	const auto& config = context_->GetConfig();
	const auto size = ui_->MeasureText(text, ui::FontStyle::Body);
	const SDL_Rect panel{(config.width - size.x) / 2 - 24,
						 config.height * 2 / 3 - 12, size.x + 48, size.y + 24};
	ui_->FillRect(panel, ui::color::kPanel);
	ui_->Text(text, config.width / 2, panel.y + 12, ui::FontStyle::Body,
			  ui::color::kText, ui::Align::Center);
}

void Menu::DrawObjective(std::string_view text) {
	if (text.empty()) {
		return;
	}
	const auto& config = context_->GetConfig();
	const auto size = ui_->MeasureText(text, ui::FontStyle::Small);
	const SDL_Rect panel{(config.width - size.x) / 2 - 18, 12, size.x + 36,
						 size.y + 16};
	ui_->FillRect(panel, ui::color::kPanel);
	ui_->FillRect({panel.x, panel.y, 4, panel.h}, ui::color::kAccent);
	ui_->Text(text, config.width / 2, panel.y + 8, ui::FontStyle::Small,
			  ui::color::kText, ui::Align::Center);
}

void Menu::DrawWeaponSlots(std::size_t count, std::uint8_t owned,
						   std::size_t held) {
	const auto& config = context_->GetConfig();
	constexpr std::array<std::string_view, 8> kSlots{"1", "2", "3", "4",
													 "5", "6", "7", "8"};
	constexpr int kPitch = 30;
	const int right = config.width - 20;
	const int top = config.height - 118;
	for (std::size_t i = 0; i < count && i < kSlots.size(); ++i) {
		const int x =
			right - static_cast<int>(count - 1 - i) * kPitch - kPitch + 6;
		const bool carried = (owned >> i & 1U) != 0;
		if (i == held) {
			ui_->FillRect({x - 4, top - 2, kPitch - 6, 30}, ui::color::kAccent);
		}
		SDL_Color colour = carried ? ui::color::kText : ui::color::kMuted;
		if (!carried) {
			colour.a = 90;
		}
		ui_->Text(kSlots[i], x + (kPitch - 14) / 2 - 3, top,
				  ui::FontStyle::Small, colour);
	}
}

void Menu::DrawEnemyCounter(std::size_t kills, std::size_t enemies) {
	const auto& config = context_->GetConfig();
	// Under the corner map, which is 30% of the screen's shorter side
	const int top = std::min(config.width, config.height) * 3 / 10 + 22;
	const int right = config.width - 14;
	const ui::FixedText<32> count("{} / {}", kills, enemies);
	ui_->Text(count, right, top, ui::FontStyle::Heading, ui::color::kText,
			  ui::Align::Right);
	ui_->Text("ENEMIES", right, top + 44, ui::FontStyle::Small,
			  ui::color::kMuted, ui::Align::Right);
}

void Menu::DrawBriefing(std::string_view heading, std::string_view story,
						std::span<const std::string_view> objectives,
						bool prompt) {
	const auto& config = context_->GetConfig();
	const int centre_x = config.width / 2;
	constexpr int kTextWidth = 820;
	const int left = centre_x - kTextWidth / 2;
	int y = 110;
	ui_->Text(heading, centre_x, y, ui::FontStyle::Heading,
			  ui::color::kAccentBright, ui::Align::Center);
	y += 110;

	// The story, a word at a time: a line ends before the word that would
	// not fit. Each line is a view into the story, so nothing is copied.
	const int line_height = ui_->MeasureText("A", ui::FontStyle::Body).y + 8;
	std::size_t line_start = 0;
	std::size_t line_end = 0;  // after the last word that fits
	std::size_t at = 0;
	while (at <= story.size()) {
		const std::size_t space = std::min(story.find(' ', at), story.size());
		const std::string_view candidate =
			story.substr(line_start, space - line_start);
		if (line_end > line_start &&
			ui_->MeasureText(candidate, ui::FontStyle::Body).x > kTextWidth) {
			ui_->Text(story.substr(line_start, line_end - line_start), left, y,
					  ui::FontStyle::Body, ui::color::kText);
			y += line_height;
			line_start = at;
		}
		line_end = space;
		at = space + 1;
	}
	if (line_end > line_start) {
		ui_->Text(story.substr(line_start, line_end - line_start), left, y,
				  ui::FontStyle::Body, ui::color::kText);
		y += line_height;
	}

	y += 40;
	ui_->Text("OBJECTIVES", left, y, ui::FontStyle::Small, ui::color::kMuted);
	y += 44;
	for (const std::string_view objective : objectives) {
		ui_->FillRect({left, y + line_height / 2 - 6, 10, 10},
					  ui::color::kAccent);
		ui_->Text(objective, left + 28, y, ui::FontStyle::Body,
				  ui::color::kText);
		y += line_height + 6;
	}
	ui_->Text("Reach the exit", left + 28, y, ui::FontStyle::Body,
			  ui::color::kMuted);

	if (prompt) {
		ui_->Text("Press Enter to begin", centre_x, config.height - 110,
				  ui::FontStyle::Small, ui::color::kMuted, ui::Align::Center);
	}
}

void Menu::DrawLevelStats(std::string_view heading, const LevelStats& stats,
						  bool prompt) {
	const auto& config = context_->GetConfig();
	const int centre_x = config.width / 2;
	int y = config.height / 2 - 260;
	ui_->Text("CLEARED", centre_x, y, ui::FontStyle::Title, ui::color::kText,
			  ui::Align::Center);
	ui_->Text(heading, centre_x, y + 118, ui::FontStyle::Heading,
			  ui::color::kAccentBright, ui::Align::Center);

	const SDL_Rect panel{centre_x - 300, y + 190, 600, 336};
	ui_->FillRect(panel, ui::color::kPanel);
	ui_->DrawRect(panel, ui::color::kBorder);
	const auto minutes = static_cast<int>(stats.seconds) / 60;
	const auto seconds = static_cast<int>(stats.seconds) % 60;
	const ui::FixedText<32> enemies("{} / {}", stats.kills, stats.enemies);
	const ui::FixedText<32> pickups("{} / {}", stats.pickups_taken,
									stats.pickups);
	const ui::FixedText<32> secrets("{} / {}", stats.secrets_found,
									stats.secrets);
	const ui::FixedText<16> explored("{}%", stats.explored_percent);
	const ui::FixedText<16> time("{}:{:02}", minutes, seconds);
	y = panel.y + 34;
	for (const auto& [label, value] :
		 {std::pair<std::string_view, std::string_view>{"Enemies", enemies},
		  {"Supplies", pickups},
		  {"Secrets", secrets},
		  {"Explored", explored},
		  {"Time", time}}) {
		ui_->Text(label, panel.x + 48, y, ui::FontStyle::Body,
				  ui::color::kMuted);
		ui_->Text(value, panel.x + panel.w - 48, y, ui::FontStyle::Body,
				  ui::color::kText, ui::Align::Right);
		y += 56;
	}
	if (prompt) {
		ui_->Text("Press Enter to continue", centre_x, panel.y + panel.h + 40,
				  ui::FontStyle::Small, ui::color::kMuted, ui::Align::Center);
	}
}

void Menu::Open(MenuScreen screen) {
	if (screen == MenuScreen::Controls || screen == MenuScreen::Settings) {
		return_screen_ = screen_;
	}
	screen_ = screen;
	if (screen == MenuScreen::DifficultySelect) {
		// On the one chosen last (at first the middle one)
		ui_->ResetFocus(static_cast<int>(chosen_difficulty_));
		return;
	}
	ui_->ResetFocus();
}

void Menu::GoBack() {
	if (screen_ == MenuScreen::Settings) {
		Settings::Get().Save();
	}
	// Land on the button that opened the screen we are leaving; both the main
	// and pause screens list Controls second and Settings third, the main
	// screen one lower when it offers CONTINUE first
	const int opener =
		(screen_ == MenuScreen::Controls ? 1 : 2) +
		(return_screen_ == MenuScreen::Main && has_saved_game_ ? 1 : 0);
	screen_ = return_screen_;
	ui_->ResetFocus(opener);
}

void Menu::HandleEvent(const SDL_Event& event) {
	input_.Handle(event);
}

MenuAction Menu::Update(double /*delta_time*/) {
	ui_->BeginFrame(input_);
	MenuAction action;
	switch (screen_) {
		case MenuScreen::Main:
			action = MainScreen();
			break;
		case MenuScreen::DifficultySelect:
			action = DifficultySelectScreen();
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
	// With a saved game, going on with it comes first
	int row = 0;
	if (has_saved_game_) {
		ui_->Text(saved_game_, width / 2, kTop - 46, ui::FontStyle::Small,
				  ui::color::kAccentBright, ui::Align::Center);
		if (ui_->Button("CONTINUE", ButtonRect(width, kTop, row++))) {
			action.type = MenuAction::Type::Continue;
		}
	}
	if (ui_->Button(has_saved_game_ ? "NEW GAME" : "PLAY",
					ButtonRect(width, kTop, row++))) {
		if (difficulties_.empty()) {
			action.type = MenuAction::Type::StartGame;
		}
		else {
			Open(MenuScreen::DifficultySelect);
		}
	}
	if (ui_->Button("CONTROLS", ButtonRect(width, kTop, row++))) {
		Open(MenuScreen::Controls);
	}
	if (ui_->Button("SETTINGS", ButtonRect(width, kTop, row++))) {
		Open(MenuScreen::Settings);
	}
#ifndef __EMSCRIPTEN__
	// A browser tab cannot close itself, so only native builds offer Quit
	if (ui_->Button("QUIT", ButtonRect(width, kTop, row))) {
		action.type = MenuAction::Type::Quit;
	}
#endif
	DrawHint("Arrow keys or mouse to choose  ·  Enter to select");
	return action;
}

// A new game's difficulty, chosen once for the whole campaign; choosing one
// starts the game
MenuAction Menu::DifficultySelectScreen() {
	const int width = context_->GetConfig().width;
	MenuAction action;
	DrawBackground();
	DrawDimmer(190);
	ui_->Text("CHOOSE DIFFICULTY", width / 2, 70, ui::FontStyle::Heading,
			  ui::color::kText, ui::Align::Center);
	ui_->Text("It holds for the whole campaign.", width / 2, 150,
			  ui::FontStyle::Body, ui::color::kMuted, ui::Align::Center);

	constexpr int kCardWidth = 760;
	constexpr int kCardHeight = 120;
	constexpr int kCardGap = 24;
	const int left = (width - kCardWidth) / 2;
	int y = 230;
	for (std::size_t i = 0; i < difficulties_.size(); ++i) {
		const SDL_Rect card{left, y, kCardWidth, kCardHeight};
		bool focused = false;
		if (ui_->Selectable(card, focused)) {
			chosen_difficulty_ = i;
			action.type = MenuAction::Type::StartGame;
			action.difficulty = i;
		}
		ui_->FillRect(card,
					  focused ? ui::color::kPanelFocused : ui::color::kPanel);
		ui_->DrawRect(card, focused ? ui::color::kAccent : ui::color::kBorder,
					  focused ? 3 : 1);
		ui_->Text(difficulties_[i].label, card.x + 32, card.y + 18,
				  ui::FontStyle::Button,
				  focused ? ui::color::kText : ui::color::kMuted);
		ui_->Text(difficulties_[i].description, card.x + 32, card.y + 72,
				  ui::FontStyle::Small, ui::color::kMuted);
		y += kCardHeight + kCardGap;
	}

	if (ui_->Button("BACK", {(width - 300) / 2, y + 20, 300, 64}) ||
		input_.back) {
		Open(MenuScreen::Main);
	}
	DrawHint("Choose with the arrow keys or mouse  ·  Enter to start");
	return action;
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
	constexpr std::array<Binding, 9> kBindings = {{
		{"Move", "W  A  S  D"},
		{"Turn", "Mouse, or Left / Right arrows"},
		{"Fire", "Left click, or Left Ctrl"},
		{"Reload", "R"},
		{"Weapons", "1 - 4, or the mouse wheel"},
		{"Open door", "E, or Space"},
		{"Map", "M"},
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
		y += 48;
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

#include "Graphics/renderer_menu.h"
#include "Core/scene.h"
#include "Settings/settings.h"
#include "TextureManager/texture_manager.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string>
#include <utility>

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
		  std::string(RESOURCE_DIR) + "font/BlackOpsOne-Regular.ttf",
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

void Menu::DrawMatchStanding(std::string_view count, std::string_view label,
							 std::string_view clock) {
	const auto& config = context_->GetConfig();
	// Under the corner map, as the enemy counter is
	const int top = std::min(config.width, config.height) * 3 / 10 + 22;
	const int right = config.width - 14;
	ui_->Text(count, right, top, ui::FontStyle::Heading, ui::color::kText,
			  ui::Align::Right);
	ui_->Text(label, right, top + 44, ui::FontStyle::Small, ui::color::kMuted,
			  ui::Align::Right);
	ui_->Text(clock, right, top + 76, ui::FontStyle::Body, ui::color::kText,
			  ui::Align::Right);
}

void Menu::DrawKillFeed(std::span<const KillLine> kills) {
	constexpr int kLeft = 16;
	constexpr int kTop = 56;
	constexpr int kPadding = 10;
	const int line = ui_->MeasureText("A", ui::FontStyle::Small).y + 12;
	int y = kTop;
	for (const KillLine& kill : kills) {
		const auto alpha =
			static_cast<Uint8>(std::clamp(kill.opacity, 0.0, 1.0) * 255.0);
		const auto faded = [alpha](SDL_Color colour) {
			colour.a = static_cast<Uint8>(colour.a * alpha / 255);
			return colour;
		};
		const ui::FixedText<48> with(" {} ", kill.weapon);
		const int killer =
			ui_->MeasureText(kill.killer, ui::FontStyle::Small).x;
		const int weapon = ui_->MeasureText(with, ui::FontStyle::Small).x;
		const int victim =
			ui_->MeasureText(kill.victim, ui::FontStyle::Small).x;
		ui_->FillRect(
			{kLeft, y, killer + weapon + victim + 2 * kPadding, line - 4},
			faded(ui::color::kPanel));
		int x = kLeft + kPadding;
		ui_->Text(kill.killer, x, y + 4, ui::FontStyle::Small,
				  faded(kill.killer_colour));
		x += killer;
		ui_->Text(with, x, y + 4, ui::FontStyle::Small,
				  faded(ui::color::kMuted));
		x += weapon;
		ui_->Text(kill.victim, x, y + 4, ui::FontStyle::Small,
				  faded(kill.victim_colour));
		y += line;
	}
}

void Menu::DrawScoreboard(std::string_view heading,
						  std::span<const ScoreLine> lines, bool race,
						  std::string_view footer) {
	const auto& config = context_->GetConfig();
	constexpr int kWidth = 760;
	const int left = (config.width - kWidth) / 2;
	const int row = ui_->MeasureText("A", ui::FontStyle::Body).y + 18;
	const int height = 170 + row * static_cast<int>(lines.size()) + 70;
	const int top = std::max((config.height - height) / 2, 20);
	ui_->FillRect({left, top, kWidth, height}, ui::color::kPanel);
	ui_->FillRect({left, top, kWidth, 4}, ui::color::kAccent);
	ui_->Text(heading, config.width / 2, top + 26, ui::FontStyle::Heading,
			  ui::color::kAccentBright, ui::Align::Center);
	// The columns: the name, then the frags (a gun race: the weapon), then
	// the deaths, right-aligned
	const int name_x = left + 70;
	const int first_x = left + kWidth - 200;
	const int second_x = left + kWidth - 50;
	int y = top + 110;
	ui_->Text("PLAYER", name_x, y, ui::FontStyle::Small, ui::color::kMuted);
	ui_->Text(race ? "WEAPON" : "FRAGS", first_x, y, ui::FontStyle::Small,
			  ui::color::kMuted, ui::Align::Right);
	ui_->Text(race ? "FRAGS" : "DEATHS", second_x, y, ui::FontStyle::Small,
			  ui::color::kMuted, ui::Align::Right);
	y += 50;
	for (std::size_t i = 0; i < lines.size(); ++i) {
		const ScoreLine& line = lines[i];
		if (line.local) {
			ui_->FillRect({left + 20, y - 8, kWidth - 40, row},
						  ui::color::kPanelFocused);
		}
		const ui::FixedText<8> place("{}", i + 1);
		ui_->Text(place, left + 50, y, ui::FontStyle::Body, ui::color::kMuted,
				  ui::Align::Right);
		ui_->FillRect({name_x, y + 6, 12, 12}, line.colour);
		ui_->Text(line.name, name_x + 24, y, ui::FontStyle::Body,
				  ui::color::kText);
		const ui::FixedText<16> first("{}", race ? line.step : line.frags);
		const ui::FixedText<16> second("{}", race ? line.frags : line.deaths);
		ui_->Text(first, first_x, y, ui::FontStyle::Body, ui::color::kText,
				  ui::Align::Right);
		ui_->Text(second, second_x, y, ui::FontStyle::Body, ui::color::kText,
				  ui::Align::Right);
		y += row;
	}
	ui_->Text(footer, config.width / 2, top + height - 50, ui::FontStyle::Small,
			  ui::color::kMuted, ui::Align::Center);
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

	y = DrawWrapped(story, left, y, kTextWidth, ui::FontStyle::Body,
					ui::color::kText);
	const int line_height = ui_->MeasureText("A", ui::FontStyle::Body).y + 8;

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

int Menu::DrawWrapped(std::string_view text, int left, int y, int width,
					  ui::FontStyle style, SDL_Color colour, bool draw) {
	// A word at a time: a line ends before the word that would not fit.
	// Each line is a view into the text, so nothing is copied.
	const int line_height = ui_->MeasureText("A", style).y + 8;
	std::size_t line_start = 0;
	std::size_t line_end = 0;  // after the last word that fits
	std::size_t at = 0;
	while (at <= text.size()) {
		const std::size_t space = std::min(text.find(' ', at), text.size());
		const std::string_view candidate =
			text.substr(line_start, space - line_start);
		if (line_end > line_start &&
			ui_->MeasureText(candidate, style).x > width) {
			if (draw) {
				ui_->Text(text.substr(line_start, line_end - line_start), left,
						  y, style, colour);
			}
			y += line_height;
			line_start = at;
		}
		line_end = space;
		at = space + 1;
	}
	if (line_end > line_start) {
		if (draw) {
			ui_->Text(text.substr(line_start, line_end - line_start), left, y,
					  style, colour);
		}
		y += line_height;
	}
	return y;
}

void Menu::DrawDocument(std::string_view title, std::string_view text,
						double opacity) {
	const auto& config = context_->GetConfig();
	constexpr int kWidth = 760;
	constexpr int kMargin = 26;
	const auto faded = [&](SDL_Color colour) {
		colour.a = static_cast<Uint8>(
			std::lround(colour.a * std::clamp(opacity, 0.0, 1.0)));
		return colour;
	};
	// As tall as its text, its foot above the HUD
	const int text_height =
		DrawWrapped(text, 0, 0, kWidth - 2 * kMargin, ui::FontStyle::Small,
					ui::color::kText, /*draw=*/false);
	const int height = kMargin + 40 + text_height + kMargin;
	const SDL_Rect panel{(config.width - kWidth) / 2,
						 config.height - 130 - height, kWidth, height};
	ui_->FillRect(panel, faded(ui::color::kPanel));
	ui_->DrawRect(panel, faded(ui::color::kBorder));
	ui_->Text(title, panel.x + kMargin, panel.y + kMargin, ui::FontStyle::Small,
			  faded(ui::color::kAccentBright));
	DrawWrapped(text, panel.x + kMargin, panel.y + kMargin + 40,
				kWidth - 2 * kMargin, ui::FontStyle::Small,
				faded(ui::color::kText));
}

void Menu::DrawStoryPage(std::string_view heading, std::string_view title,
						 std::string_view text, std::size_t page,
						 std::size_t pages, bool prompt) {
	const auto& config = context_->GetConfig();
	const int centre_x = config.width / 2;
	constexpr int kTextWidth = 780;
	int y = 150;
	if (!heading.empty()) {
		ui_->Text(heading, centre_x, y, ui::FontStyle::Small,
				  ui::color::kAccentBright, ui::Align::Center);
		y += 44;
	}
	if (!title.empty()) {
		ui_->Text(title, centre_x, y, ui::FontStyle::Heading, ui::color::kText,
				  ui::Align::Center);
		y += 110;
	}
	DrawWrapped(text, centre_x - kTextWidth / 2, y, kTextWidth,
				ui::FontStyle::Body, ui::color::kText);
	// Where it is in the story: a mark for each page, the one showing lit
	if (pages > 1) {
		constexpr int kMark = 10;
		constexpr int kPitch = 22;
		const int left = centre_x - static_cast<int>(pages) * kPitch / 2;
		for (std::size_t i = 0; i < pages; ++i) {
			ui_->FillRect({left + static_cast<int>(i) * kPitch,
						   config.height - 160, kMark, kMark},
						  i == page ? ui::color::kAccent : ui::color::kBorder);
		}
	}
	if (prompt) {
		ui_->Text("Press Enter to go on", centre_x, config.height - 110,
				  ui::FontStyle::Small, ui::color::kMuted, ui::Align::Center);
	}
}

void Menu::DrawLevelStats(std::string_view heading, const LevelStats& stats,
						  std::string_view debrief, bool prompt) {
	const auto& config = context_->GetConfig();
	const int centre_x = config.width / 2;
	// Higher up when there is a word on what was learnt, below the results
	int y = debrief.empty() ? config.height / 2 - 260 : 60;
	ui_->Text("CLEARED", centre_x, y, ui::FontStyle::Title, ui::color::kText,
			  ui::Align::Center);
	ui_->Text(heading, centre_x, y + 118, ui::FontStyle::Heading,
			  ui::color::kAccentBright, ui::Align::Center);

	// A row for intel where the level has some
	const int rows = stats.documents > 0 ? 6 : 5;
	const SDL_Rect panel{centre_x - 300, y + 190, 600, 56 + rows * 56};
	ui_->FillRect(panel, ui::color::kPanel);
	ui_->DrawRect(panel, ui::color::kBorder);
	const auto minutes = static_cast<int>(stats.seconds) / 60;
	const auto seconds = static_cast<int>(stats.seconds) % 60;
	const ui::FixedText<32> enemies("{} / {}", stats.kills, stats.enemies);
	const ui::FixedText<32> pickups("{} / {}", stats.pickups_taken,
									stats.pickups);
	const ui::FixedText<32> secrets("{} / {}", stats.secrets_found,
									stats.secrets);
	const ui::FixedText<32> intel("{} / {}", stats.documents_found,
								  stats.documents);
	const ui::FixedText<16> explored("{}%", stats.explored_percent);
	const ui::FixedText<16> time("{}:{:02}", minutes, seconds);
	y = panel.y + 34;
	const std::array<std::pair<std::string_view, std::string_view>, 6> lines{{
		{"Enemies", enemies},
		{"Supplies", pickups},
		{"Intel", intel},
		{"Secrets", secrets},
		{"Explored", explored},
		{"Time", time},
	}};
	for (const auto& [label, value] : lines) {
		if (label == "Intel" && stats.documents == 0) {
			continue;
		}
		ui_->Text(label, panel.x + 48, y, ui::FontStyle::Body,
				  ui::color::kMuted);
		ui_->Text(value, panel.x + panel.w - 48, y, ui::FontStyle::Body,
				  ui::color::kText, ui::Align::Right);
		y += 56;
	}
	int below = panel.y + panel.h + 30;
	if (!debrief.empty()) {
		constexpr int kTextWidth = 760;
		below =
			DrawWrapped(debrief, centre_x - kTextWidth / 2, below, kTextWidth,
						ui::FontStyle::Small, ui::color::kText) +
			10;
	}
	if (prompt) {
		ui_->Text("Press Enter to continue", centre_x, below + 10,
				  ui::FontStyle::Small, ui::color::kMuted, ui::Align::Center);
	}
}

void Menu::Open(MenuScreen screen) {
	if (screen == MenuScreen::Controls || screen == MenuScreen::Settings) {
		return_screen_ = screen_;
	}
	screen_ = screen;
	// Only the multiplayer screen is typed into
	const bool typing = screen == MenuScreen::Multiplayer;
	if (typing != input_.text_mode) {
		input_.text_mode = typing;
		if (typing) {
			SDL_StartTextInput(context_->GetWindow());
		}
		else {
			SDL_StopTextInput(context_->GetWindow());
		}
	}
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
		case MenuScreen::Multiplayer:
			action = MultiplayerScreen();
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
	if (ui_->Button("MULTIPLAYER", ButtonRect(width, kTop, row++))) {
		Open(MenuScreen::Multiplayer);
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

namespace {

// What was typed into a text field, applied to the setting it shows
template <std::size_t N>
void Edit(SettingText<N>& text, const ui::TextEdits& edits) {
	for (int i = 0; i < edits.erased; ++i) {
		text.EraseLast();
	}
	for (const char c : edits.typed) {
		text.Append(c);
	}
}

}  // namespace

// Joining a match: the name to play under, the server and the room (none:
// the server's open game), remembered for the next time
MenuAction Menu::MultiplayerScreen() {
	const int width = context_->GetConfig().width;
	MenuAction action;
	DrawBackground();
	DrawDimmer(190);
	ui_->Text("MULTIPLAYER", width / 2, 70, ui::FontStyle::Heading,
			  ui::color::kText, ui::Align::Center);
	ui_->Text("A deathmatch for up to eight, in the browser or not.", width / 2,
			  150, ui::FontStyle::Body, ui::color::kMuted, ui::Align::Center);
	Settings& settings = Settings::Get();
	constexpr int kWidth = 760;
	constexpr int kHeight = 64;
	constexpr int kGap = 16;
	const int left = (width - kWidth) / 2;
	int y = 230;
	Edit(settings.player_name,
		 ui_->TextField("NAME", settings.player_name.View(),
						{left, y, kWidth, kHeight}));
	y += kHeight + kGap;
	Edit(settings.server, ui_->TextField("SERVER", settings.server.View(),
										 {left, y, kWidth, kHeight}));
	y += kHeight + kGap;
	Edit(settings.room, ui_->TextField("ROOM", settings.room.View(),
									   {left, y, kWidth, kHeight}));
	y += kHeight + 12;
	ui_->Text(
		"No room: the server's open game. Friends in the same room "
		"play together.",
		width / 2, y, ui::FontStyle::Small, ui::color::kMuted,
		ui::Align::Center);
	const int buttons = y + 60;
	if (ui_->Button("JOIN", ButtonRect(width, buttons, 0))) {
		settings.Save();
		action.type = MenuAction::Type::Join;
	}
	if (ui_->Button("BACK", ButtonRect(width, buttons, 1)) || input_.back) {
		settings.Save();
		Open(MenuScreen::Main);
	}
	DrawHint("Tab or arrows to move  ·  Enter to choose  ·  Esc to go back");
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
		{"Fire", "Left click"},
		{"Reload", "R"},
		{"Weapons", "Number keys, or the mouse wheel"},
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
	constexpr int kRowWidth = 760;
	constexpr int kRowHeight = 64;
	constexpr int kRowGap = 12;
	const int left = (width - kRowWidth) / 2;
	int y = 150;
	// Each row below the last
	const auto next_row = [&] {
		const SDL_Rect rect{left, y, kRowWidth, kRowHeight};
		y += kRowHeight + kRowGap;
		return rect;
	};
	const auto percent = [](double share) {
		return ui::FixedText<>("{:.0f}%", share * 100);
	};
	// Every row is drawn, and any change applied at once
	bool changed = false;
	changed |= ui_->Slider(
		"Mouse sensitivity",
		ui::FixedText<>("{:.2f}x", settings.mouse_sensitivity), next_row(),
		settings.mouse_sensitivity, Settings::kMinMouseSensitivity,
		Settings::kMaxMouseSensitivity, 0.05);
	changed |=
		ui_->Toggle("Invert mouse Y", next_row(), settings.invert_mouse_y);
	changed |= ui_->Slider(
		"Field of view", ui::FixedText<>("{:.0f}\u00b0", settings.fov),
		next_row(), settings.fov, Settings::kMinFov, Settings::kMaxFov, 5.0);
	changed |= ui_->Slider("Volume", percent(settings.volume), next_row(),
						   settings.volume, 0.0, 1.0, 0.05);
	changed |= ui_->Slider("Music", percent(settings.music_volume), next_row(),
						   settings.music_volume, 0.0, 1.0, 0.05);
	changed |= ui_->Slider("Effects", percent(settings.effects_volume),
						   next_row(), settings.effects_volume, 0.0, 1.0, 0.05);
	changed |= ui_->Toggle("Show FPS", next_row(), settings.show_fps);
	MenuAction action;
	if (changed) {
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
	SDL_RenderTexture(
		context_->GetRenderer(),
		context_->Textures().GetTexture(background_texture_).texture, nullptr,
		nullptr);
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

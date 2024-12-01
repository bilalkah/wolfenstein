/**
 * @file renderer_menu.h
 * @author Bilal Kahraman (kahramannbilal@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2024-12-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef GRAPHICS_INCLUDE_GRAPHICS_RENDERER_MENU_H_
#define GRAPHICS_INCLUDE_GRAPHICS_RENDERER_MENU_H_

#include "Graphics/renderer_interface.h"
#include "Strike/weapon.h"
#include <memory>

namespace wolfenstein {

class Menu
{
  public:
	Menu(std::shared_ptr<RendererContext> context);
	~Menu() = default;

	void Render();
	void ChangeSelection(int direction);
	std::shared_ptr<Weapon> GetSelectedWeapon();

  private:
	void RenderBackground();
	void RenderText(const SDL_Color& color);
	void RenderWeapon();
	void Animate();
	void ClearScreen();
	std::shared_ptr<RendererContext> context_;
	std::vector<std::shared_ptr<Weapon>> weapons_;
	int selected_weapon_index_ = 0;
};

}  // namespace wolfenstein

#endif	// GRAPHICS_INCLUDE_GRAPHICS_RENDERER_MENU_H_

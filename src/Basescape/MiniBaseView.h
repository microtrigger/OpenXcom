#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <vector>
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
#include <array>
#include <cstdint>
#endif
#include "../Engine/InteractiveSurface.h"

namespace OpenXcom
{

class Base;
class SurfaceSet;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
namespace Observer { class AgentProjectileOutput; }
struct AgentMiniBaseSlot { unsigned slot; bool selected; };
#endif

/**
 * Mini view of a base.
 * Takes all the bases and displays their layout
 * and allows players to swap between them.
 */
class MiniBaseView : public InteractiveSurface
{
private:
	static const int MINI_SIZE = 14;

	std::vector<Base*> *_bases;
	SurfaceSet *_texture;
	size_t _base, _hoverBase;
	Uint8 _red, _green;
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    std::uint64_t _agentRevision = 1;
    std::array<std::array<Uint8, 256>, 8> _agentPixels{};
    std::array<bool, 8> _agentDrawn{}, _agentSelectedInk{}, _agentSelectedContrast{};
    std::array<const void *, 8> _agentMapping{};
    std::array<unsigned, 8> _agentSelectedPoint{};
    void agentCaptureSlots(const std::array<const void *, 8> &mapping);
#endif
public:
	static const size_t MAX_BASES = 8;
	/// Creates a new mini base view at the specified position and size.
	MiniBaseView(int width, int height, int x = 0, int y = 0);
	/// Cleans up the mini base view.
	~MiniBaseView();
	/// Sets the base list to display.
	void setBases(std::vector<Base*> *bases);
	/// Sets the texture for the mini base view.
	void setTexture(SurfaceSet *texture);
	/// Gets the base the mouse is over.
	size_t getHoveredBase() const;
	/// Sets the selected base for the mini base view.
	void setSelectedBase(size_t base);
	/// Draws the mini base view.
	void draw();
	/// Special handling for mouse hovers.
	void mouseOver(Action *action, State *state);
    // Shared ordinary geometric hover routing, independent of existence.
    void hoverAt(int localX);
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
    void blit(Surface *surface) override;
    std::vector<AgentMiniBaseSlot> agentSlots(const Observer::AgentProjectileOutput &output) const;
    std::uint64_t agentRevision() const { return _agentRevision; }
#endif
	void setColor(Uint8 color);
	void setSecondaryColor(Uint8 color);
};

}

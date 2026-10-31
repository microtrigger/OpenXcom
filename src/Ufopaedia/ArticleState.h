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
#include "../Engine/State.h"
#include "../Mod/RuleItem.h"
#include <string>

namespace OpenXcom
{
	class Game;
	class Action;
	class Surface;
	class TextButton;
	class Text;

	/**
	 * UfopaediaArticle is the base class for all articles of various types.
	 *
	 * It encapsulates the basic characteristics.
	 */

	class ArticleState : public State
	{
	protected:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        Text *_agentTitle = nullptr, *_agentBody = nullptr;
        Text *_agentCraftStats = nullptr;
        void agentRegisterCraftStats(Text *text);
        std::vector<Surface *> _agentTransparentLayers;
        bool _agentArticlePaintFull = false;
        std::vector<std::uint64_t> _agentStoredPaintRevisions;
        bool agentDestinationFull() const;
        std::vector<std::uint64_t> agentPaintRevisions() const;
        void agentRegisterText(Text *title, Text *body);
        void agentRegisterTransparentLayer(Surface *surface);
#endif
		/// constructor (protected, so it can only be instantiated by derived classes)
		ArticleState(const std::string &article_id);
		/// destructor
		virtual ~ArticleState();

	public:
#ifdef OPENXCOM_EXPERIMENTAL_AGENT_CONTROL
        void blit() override;
        bool agentUiState(Observer::AgentUiState &out) const override;
        bool agentDispatch(const Observer::AgentRequest &request) override;
#endif
		/// return the article id
		std::string getId() const { return _id; }

	protected:

		/// converts damage type to string
		std::string getDamageTypeText(ItemDamageType dt) const;

		/// screen layout helpers
		void initLayout();

		/// callback for OK button
		void btnOkClick(Action *action);

		/// callback for PREV button
		void btnPrevClick(Action *action);

		/// callback for NEXT button
		void btnNextClick(Action *action);
		/// the article id
		std::string _id;

		/// screen elements common to all articles!
		Surface *_bg;
		TextButton *_btnOk;
		TextButton *_btnPrev;
		TextButton *_btnNext;
	};
}

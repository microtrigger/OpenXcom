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
// Fork modification 2026-10-10: default-off bounded observer-effects and natural-FOV test hooks.
// See LICENSE-NOTICES.md; existing upstream license retained.
// Fork modification 2026-10-10: default-off battlefield-audio test seam.
// See LICENSE-NOTICES.md; upstream license retained.
#ifdef OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE
#include "../tests/battle-observer/BattlefieldAudioGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
#include "../tests/battle-observer/ObserverEffectsGate.h"
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
#include "../tests/battle-observer/NaturalFovGate.h"
#endif
// Fork modification 2026-10-10: default-off render-only test gate.
// SPDX-License-Identifier: GPL-3.0-or-later; upstream notices retained.
#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
#include "../tests/battle-observer/RenderPairGate.h"
#endif
#include <sstream>
#include "version.h"
#include "Engine/Logger.h"
#include "Engine/CrossPlatform.h"
#include "Engine/Game.h"
#include "Engine/Options.h"
#include "Menu/StartState.h"
// Fork modification 2026-10-10: qualify effective diagnostics before observation.
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_DIAGNOSTICS
#include "Observer/DiagnosticProfile.h"
#endif

/** @mainpage
 * @author OpenXcom Developers
 *
 * OpenXcom is an open-source clone of the original X-Com
 * written entirely in C++ and SDL. This documentation contains info
 * on every class contained in the source code and its public methods.
 * The code itself also contains in-line comments for more complicated
 * code blocks. Hopefully all of this will make the code a lot more
 * readable for you in case you which to learn or make use of it in
 * your own projects, though note that all the source code is licensed
 * under the GNU General Public License. Enjoy!
 */

using namespace OpenXcom;

// Crash handling routines
#ifdef _MSC_VER
#include <windows.h>
LONG WINAPI crashLogger(PEXCEPTION_POINTERS exception)
{
	CrossPlatform::crashDump(exception, "");
	return EXCEPTION_CONTINUE_SEARCH;
}
#else
#include <signal.h>
void signalLogger(int sig)
{
	CrossPlatform::crashDump(&sig, "");
	exit(EXIT_FAILURE);
}

#include <exception>
void exceptionLogger()
{
	static bool logged = false;
	std::string error;
	try
	{
		if (!logged)
		{
			logged = true;
			throw;
		}
	}
	catch (const std::exception &e)
	{
		error = e.what();
	}
	catch (...)
	{
		error = "Unknown exception";
	}
	CrossPlatform::crashDump(0, error);
	abort();
}
#endif

Game *game = 0;

// If you can't tell what the main() is for you should have your
// programming license revoked...
int main(int argc, char *argv[])
{
#ifdef _MSC_VER
	// Uncomment to check memory leaks in VS
	//_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );

	SetUnhandledExceptionFilter(crashLogger);
	// Uncomment to debug crash handler
	// AddVectoredContinueHandler(1, crashLogger);
#else
	signal(SIGSEGV, signalLogger);
	std::set_terminate(exceptionLogger);
#endif

	CrossPlatform::getErrorDialog();

#ifndef NDEBUG
	Logger::reportingLevel() = LOG_DEBUG;
#else
	Logger::reportingLevel() = LOG_INFO;
#endif
	if (!Options::init(argc, argv))
		return EXIT_SUCCESS;
	std::ostringstream title;
	title << "OpenXcom " << OPENXCOM_VERSION_SHORT << OPENXCOM_VERSION_GIT;
	if (Options::verboseLogging)
		Logger::reportingLevel() = LOG_VERBOSE;
	// Never rewrite operator configuration to qualify observation.
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_DIAGNOSTICS
	Observer::initializeDiagnosticProfile();
#endif
	Options::baseXResolution = Options::displayWidth;
	Options::baseYResolution = Options::displayHeight;

#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
	RenderPairGate::prepare();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
	ObserverEffectsGate::prepare();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
	NaturalFovGate::prepare();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE
	BattlefieldAudioGate::prepare();
#endif
	game = new Game(title.str());
#ifdef OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE
	ObserverEffectsGate::silence();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE
	NaturalFovGate::silence();
#endif
#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
	RenderPairGate::silence();
#endif
	State::setGamePtr(game);
	game->setState(new StartState);
	game->run();

	// Comment this for faster exit.
	delete game;
#ifdef OPENXCOM_EXPERIMENTAL_BATTLE_PAIR_GATE
	return RenderPairGate::exitStatus();
#elif defined(OPENXCOM_EXPERIMENTAL_OBSERVER_EFFECTS_GATE)
	return ObserverEffectsGate::exitStatus();
#elif defined(OPENXCOM_EXPERIMENTAL_NATURAL_FOV_GATE)
	return NaturalFovGate::exitStatus();
#elif defined(OPENXCOM_EXPERIMENTAL_BATTLEFIELD_AUDIO_GATE)
	return BattlefieldAudioGate::exitStatus();
#else
	return EXIT_SUCCESS;
#endif
}


#ifdef __MORPHOS__
const char Version[] = "$VER: OpenXCom " OPENXCOM_VERSION_SHORT " (" __AMIGADATE__  ")";
#endif

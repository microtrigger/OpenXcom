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
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <sstream>
#include <string>
#include <stdio.h>
#include "CrossPlatform.h"

namespace OpenXcom
{

/**
 * Defines the various severity levels of
 * information logged by the game.
 */
enum SeverityLevel
{
	LOG_FATAL,		/**< Something horrible has happened and the game is going to die! */
	LOG_ERROR,		/**< Something bad happened but we can still move on. */
	LOG_WARNING,	/**< Something weird happened, nothing special but it's good to know. */
	LOG_INFO,		/**< Useful information for users/developers to help debug and figure stuff out. */
	LOG_DEBUG,		/**< Purely test stuff to help developers implement, not really relevant to users. */
	LOG_VERBOSE     /**< Extra details that even developers won't really need 90% of the time. */
};

/**
 * A basic logging and debugging class, prints output to stdout/files.
 * @note Wasn't really satisfied with any of the libraries around
 * so I rolled my own. Based on http://www.drdobbs.com/cpp/logging-in-c/201804215
 */
class Logger
{
public:
	Logger();
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
	virtual ~Logger() noexcept;

	/**
	 * Suppresses Log on this thread before any logging expression is evaluated.
	 * The prior-state representation supports nesting without a depth overflow.
	 * Keep this scope on its creating thread; future loading callers must cover
	 * their entire owner tick, including catches, and freeze logging settings
	 * before starting a worker. This helper does not activate those callers.
	 */
	class ScopedSuppression
	{
	public:
		ScopedSuppression() noexcept : _previous(suppressionState())
		{
			suppressionState() = true;
		}
		~ScopedSuppression() noexcept
		{
			suppressionState() = _previous;
		}
		ScopedSuppression(const ScopedSuppression&) = delete;
		ScopedSuppression& operator=(const ScopedSuppression&) = delete;
		ScopedSuppression(ScopedSuppression&&) = delete;
		ScopedSuppression& operator=(ScopedSuppression&&) = delete;
	private:
		const bool _previous;
	};

	static bool suppressed() noexcept { return suppressionState(); }
#else
	virtual ~Logger();
#endif
	std::ostringstream& get(SeverityLevel level = LOG_INFO);

	static SeverityLevel& reportingLevel();
	static std::string& logFile();
	static std::string toString(SeverityLevel level);
protected:
	std::ostringstream os;
private:
	Logger(const Logger&);
#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
	static bool& suppressionState() noexcept
	{
		// One per-thread state shared by all consistent inline definitions.
		static thread_local bool suppressed = false;
		return suppressed;
	}
#endif
};

inline Logger::Logger()
{
}

inline std::ostringstream& Logger::get(SeverityLevel level)
{
	os << "[" << toString(level) << "]" << "\t";
	return os;
}

#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
inline Logger::~Logger() noexcept
{
	FILE *file = NULL;
	try
	{
		os << std::endl;
		if (!os)
			return;
		const std::string message = os.str();
		std::ostringstream ss;
		ss << "[" << CrossPlatform::now() << "]" << "\t" << message;
		if (!ss)
			return;
		// Complete all allocating formatting/extraction before opening a file.
		const std::string record = ss.str();
		file = fopen(logFile().c_str(), "a");
		const bool opened = file != NULL;
		if (file)
		{
			fprintf(file, "%s", record.c_str());
			fflush(file);
			FILE *closing = file;
			file = NULL; // fclose is attempted once, even if that attempt fails.
			fclose(closing);
		}
		if (!opened || reportingLevel() == LOG_DEBUG || reportingLevel() == LOG_VERBOSE)
		{
			fprintf(stderr, "%s", message.c_str());
			fflush(stderr);
		}
	}
	catch (...)
	{
		// Best effort only. Never recurse into Log from this failure path.
		if (file)
		{
			FILE *closing = file;
			file = NULL;
			try { fclose(closing); }
			catch (...) {}
		}
	}
}
#else
inline Logger::~Logger()
{
	os << std::endl;
	std::ostringstream ss;
	ss << "[" << CrossPlatform::now() << "]" << "\t" << os.str();
	FILE *file = fopen(logFile().c_str(), "a");
	if (file)
	{
		fprintf(file, "%s", ss.str().c_str());
		fflush(file);
		fclose(file);
	}
	if (!file || reportingLevel() == LOG_DEBUG || reportingLevel() == LOG_VERBOSE)
	{
		fprintf(stderr, "%s", os.str().c_str());
		fflush(stderr);
	}
}
#endif

inline SeverityLevel& Logger::reportingLevel()
{
	static SeverityLevel reportingLevel = LOG_DEBUG;
	return reportingLevel;
}

inline std::string& Logger::logFile()
{
	static std::string logFile = "openxcom.log";
	return logFile;
}

inline std::string Logger::toString(SeverityLevel level)
{
	static const char* const buffer[] = {"FATAL", "ERROR", "WARN", "INFO", "DEBUG", "VERB"};
	return buffer[level];
}

#ifdef OPENXCOM_LOADING_PRESENTATION_HELPERS
// Deliberately preserve legacy non-suppressed evaluation: a passing level
// expression is evaluated twice, a filtered one once, a suppressed one never.
#define Log(level) \
	if (Logger::suppressed()) ; \
	else if (level > Logger::reportingLevel()) ; \
	else Logger().get(level)
#else
#define Log(level) \
	if (level > Logger::reportingLevel()) ; \
	else Logger().get(level)
#endif

}

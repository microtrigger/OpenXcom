// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// This file is free software: you may redistribute and/or modify it under
// GNU GPL version 3 or, at your option, any later version.
// Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
#include "../../src/Observer/PublicHud.h"
#include "../../src/Observer/HudFileObserver.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

using namespace OpenXcom::Observer;
static std::string readFile(const std::string &path)
{
	std::ifstream in(path.c_str());
	std::ostringstream out; out << in.rdbuf(); return out.str();
}
static std::string temporaryDirectory()
{
	char path[] = "/tmp/openxcom-hud-test-XXXXXX";
	char *dir = mkdtemp(path); assert(dir); return dir;
}
static void cleanup(const std::string &d)
{
	unlink((d + "/snapshot.json").c_str());
	unlink((d + "/snapshot.pending").c_str());
	unlink((d + "/observer.lock").c_str());
	assert(rmdir(d.c_str()) == 0);
}
int main()
{
	const std::string session(32, 'a');
	PublicHudCache cache(session), pair(session);
	assert(cache.revision() == 0 && cache.snapshot() == pair.snapshot());
	assert(cache.snapshot().find("\"freshness\":\"unverified\"") != std::string::npos);
	PublicHud hud; hud.selected = true;
	hud.timeUnits = 60; hud.energy = 55; hud.health = 30; hud.morale = 100;
	assert(cache.observe(hud) && pair.observe(hud));
	const std::string first = cache.snapshot();
	assert(cache.revision() == 1 && first == pair.snapshot());
	for (int unchangedPoll = 0; unchangedPoll < 10000; ++unchangedPoll)
		assert(!cache.observe(hud) && cache.snapshot() == first && cache.revision() == 1);
	--hud.timeUnits;
	assert(cache.observe(hud) && cache.revision() == 2 && cache.snapshot() != first);
	hud.selected = false; hud.health = 987654321;
	assert(cache.observe(hud));
	assert(cache.snapshot().find("987654321") == std::string::npos);
	assert(cache.snapshot().find("selected_hud\":null") != std::string::npos);
	assert(cache.close() && !cache.close());
	assert(cache.snapshot().find("sampled\":false") != std::string::npos);
	PublicHudCache next(std::string(32, 'b'));
	assert(next.revision() == 0 && next.snapshot() != pair.snapshot());
	bool rejected = false;
	try { PublicHudCache bad("\"injection"); } catch (const std::invalid_argument &) { rejected = true; }
	assert(rejected);
	HudFileObserver disabled(0); assert(!disabled.enabled());
	std::string d = temporaryDirectory();
	std::string observed;
	{
		HudFileObserver sink(d.c_str()); assert(sink.enabled());
		HudFileObserver competing(d.c_str()); assert(!competing.enabled());
		hud.selected = true; hud.health = 30;
		sink.observe(hud);
		observed = readFile(d + "/snapshot.json");
		assert(observed.find("\"health\":30") != std::string::npos);
		struct stat before, after; assert(stat((d + "/snapshot.json").c_str(), &before) == 0);
		assert((before.st_mode & 0777) == 0600);
		sink.observe(hud);
		assert(stat((d + "/snapshot.json").c_str(), &after) == 0);
		assert(before.st_ino == after.st_ino && readFile(d + "/snapshot.json") == observed);
	}
	assert(readFile(d + "/snapshot.json").find("sampled\":false") != std::string::npos);
	{
		HudFileObserver restarted(d.c_str()); assert(restarted.enabled());
		restarted.observe(hud);
		assert(readFile(d + "/snapshot.json") != observed); // New session nonce.
	}
	assert(chmod(d.c_str(), 0755) == 0);
	{ HudFileObserver publicDir(d.c_str()); assert(!publicDir.enabled()); }
	assert(chmod(d.c_str(), 0700) == 0);
	std::string link = d + "-link"; assert(symlink(d.c_str(), link.c_str()) == 0);
	{ HudFileObserver symlinkDir(link.c_str()); assert(!symlinkDir.enabled()); }
	unlink(link.c_str());
	// A malicious/stale pending entry must never be followed or overwritten.
	assert(symlink("snapshot.json", (d + "/snapshot.pending").c_str()) == 0);
	std::string protectedFile = readFile(d + "/snapshot.json");
	{ HudFileObserver pending(d.c_str()); assert(!pending.enabled()); }
	assert(readFile(d + "/snapshot.json") == protectedFile);

	cleanup(d);
	// Lock entries must themselves be private, regular and singly linked.
	d = temporaryDirectory();
	std::string victim = d + "/victim";
	{ std::ofstream out(victim.c_str()); out << "do not overwrite"; }
	chmod(victim.c_str(), 0600);
	assert(symlink("victim", (d + "/observer.lock").c_str()) == 0);
	{ HudFileObserver locked(d.c_str()); assert(!locked.enabled()); }
	unlink((d + "/observer.lock").c_str());
	assert(::link(victim.c_str(), (d + "/observer.lock").c_str()) == 0);
	{ HudFileObserver locked(d.c_str()); assert(!locked.enabled()); }
	assert(readFile(victim) == "do not overwrite");
	unlink(victim.c_str()); cleanup(d);
	// Publication failure disables sampling, preserving a last observation only.
	d = temporaryDirectory();
	{
		HudFileObserver sink(d.c_str()); assert(sink.enabled());
		sink.observe(hud);
		std::string old = readFile(d + "/snapshot.json");
		{ std::ofstream pending((d + "/snapshot.pending").c_str()); pending << "stale"; }
		--hud.timeUnits; sink.observe(hud); assert(!sink.enabled());
		assert(readFile(d + "/snapshot.json") == old);
		assert(old.find("\"freshness\":\"unverified\"") != std::string::npos);
	}
	cleanup(d);
	std::cout << "PASS: public HUD cache, revisions, lifecycle, permissions, exclusive lock, symlink fail-closed\n";
}

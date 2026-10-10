// SPDX-License-Identifier: GPL-3.0-or-later
// New contribution maintained in this OpenXcom fork, 2026-10-10.
// This file is free software: you may redistribute and/or modify it under
// GNU GPL version 3 or, at your option, any later version.
// Provided without warranty; see the repository LICENSE.txt and LICENSE-NOTICES.md.
#include "HudFileObserver.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace OpenXcom
{
namespace Observer
{
namespace
{
bool privateFile(int fd)
{
	struct stat s;
	return fd >= 0 && fstat(fd, &s) == 0 && S_ISREG(s.st_mode)
		&& s.st_uid == geteuid() && (s.st_mode & 0777) == 0600 && s.st_nlink == 1;
}
bool nonce(std::string &value)
{
	unsigned char bytes[16];
	int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0) return false;
	std::size_t offset = 0;
	while (offset < sizeof(bytes))
	{
		ssize_t n = read(fd, bytes + offset, sizeof(bytes) - offset);
		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) { ::close(fd); return false; }
		offset += n;
	}
	::close(fd);
	const char *hex = "0123456789abcdef";
	for (std::size_t i = 0; i < sizeof(bytes); ++i)
	{
		value += hex[bytes[i] >> 4];
		value += hex[bytes[i] & 15];
	}
	return true;
}
}

HudFileObserver::HudFileObserver(const char *directory) : _directory(-1), _lock(-1)
{
	if (!directory || !*directory) return;
	_directory = open(directory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	struct stat s;
	if (_directory < 0 || fstat(_directory, &s) != 0 || s.st_uid != geteuid() || (s.st_mode & 0777) != 0700)
	{ disable(); return; }
	_lock = openat(_directory, "observer.lock", O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (!privateFile(_lock) || flock(_lock, LOCK_EX | LOCK_NB) != 0)
	{ disable(); return; }
	std::string session;
	if (!nonce(session)) { disable(); return; }
	_cache.reset(new PublicHudCache(session));
	if (!publish()) disable();
}

HudFileObserver::~HudFileObserver()
{
	if (_cache && _cache->close()) publish();
	disable();
}

void HudFileObserver::disable()
{
	_cache.reset();
	if (_lock >= 0) ::close(_lock);
	if (_directory >= 0) ::close(_directory);
	_lock = _directory = -1;
}

bool HudFileObserver::publish()
{
	// A fresh exclusive file prevents following existing symlinks/hardlinks.
	// A stale .pending file causes fail-closed behavior; it is never overwritten.
	int fd = openat(_directory, "snapshot.pending", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (fd < 0) return false;
	const std::string &bytes = _cache->snapshot();
	std::size_t offset = 0;
	while (offset < bytes.size())
	{
		ssize_t n = write(fd, bytes.data() + offset, bytes.size() - offset);
		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) { ::close(fd); unlinkat(_directory, "snapshot.pending", 0); return false; }
		offset += n;
	}
	bool ok = ::close(fd) == 0;
	if (ok) ok = renameat(_directory, "snapshot.pending", _directory, "snapshot.json") == 0;
	if (!ok) unlinkat(_directory, "snapshot.pending", 0);
	return ok;
}

void HudFileObserver::observe(const PublicHud &hud)
{
	if (_cache && _cache->observe(hud) && !publish()) disable();
}
}
}

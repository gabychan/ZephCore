/*
 * SPDX-License-Identifier: MIT
 * Arduino-style FILESYSTEM / File over the Zephyr fs API, so storage code
 * ported verbatim from upstream MeshCore compiles unchanged. A ZephyrFS is
 * rooted at a directory: upstream's "/regions2" opens <root>/regions2.
 */

#pragma once

#include "Stream.h"
#include <string.h>
#include <zephyr/fs/fs.h>

class File : public Stream {
public:
	/* Longest path here: <root>/<name> (64) plus ".tmp". */
	static constexpr size_t PATH_LEN = 68;

private:
	struct fs_file_t _file;
	bool _open;
	bool _ok = true;   /* no short or failed write since the file was opened */
	/* "w" mode: _tmp is renamed over _target on close. */
	char _tmp[PATH_LEN] = {0};
	char _target[PATH_LEN] = {0};

public:
	File() : _open(false) { fs_file_t_init(&_file); }
	File(File &&other) : _file(other._file), _open(other._open), _ok(other._ok)
	{
		memcpy(_tmp, other._tmp, sizeof(_tmp));
		memcpy(_target, other._target, sizeof(_target));
		other._open = false;
		other._tmp[0] = '\0';
	}
	File(const File &) = delete;
	File &operator=(const File &) = delete;
	~File() { close(); }

	bool openPath(const char *path, fs_mode_t flags);
	/* Open tmp_path for writing; close() renames it over path. */
	bool openCommit(const char *tmp_path, const char *path);

	explicit operator bool() const { return _open; }

	size_t read(uint8_t *buf, size_t size);
	size_t write(const uint8_t *buf, size_t size) override;
	size_t write(uint8_t c) override { return write(&c, 1); }
	using Print::write;

	int read() override;
	int peek() override;
	int available() override;
	void close();
};

class ZephyrFS {
	const char *_root;

	void fullPath(char *dest, size_t dest_len, const char *path) const;

public:
	explicit ZephyrFS(const char *root) : _root(root) {}

	bool exists(const char *path);
	/* "r" (default): read. "w": replace on close, through "<path>.tmp", so an
	 * interrupted or failed save leaves the previous content in place. */
	File open(const char *path, const char *mode = "r", bool create = false);
	bool remove(const char *path);
	bool mkdir(const char *path);
};

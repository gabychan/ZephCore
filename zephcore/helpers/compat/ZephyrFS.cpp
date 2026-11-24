/*
 * SPDX-License-Identifier: MIT
 */

#include "ZephyrFS.h"
#include <stdio.h>
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zephcore_fs_compat, CONFIG_ZEPHCORE_DATASTORE_LOG_LEVEL);

bool File::openPath(const char *path, fs_mode_t flags)
{
	close();
	fs_file_t_init(&_file);
	_ok = true;
	_open = fs_open(&_file, path, flags) == 0;
	return _open;
}

bool File::openCommit(const char *tmp_path, const char *path)
{
	struct fs_dirent entry;

	close();
	strcpy(_tmp, tmp_path);
	strcpy(_target, path);
	_ok = true;
	fs_file_t_init(&_file);
	/* A temp file left by an interrupted write */
	if (fs_stat(tmp_path, &entry) == 0) {
		fs_unlink(tmp_path);
	}
	_open = fs_open(&_file, tmp_path, FS_O_CREATE | FS_O_WRITE) == 0;
	if (!_open) {
		_tmp[0] = '\0';
		_target[0] = '\0';
	}
	return _open;
}

size_t File::read(uint8_t *buf, size_t size)
{
	if (!_open) {
		return 0;
	}
	ssize_t n = fs_read(&_file, buf, size);
	return n > 0 ? (size_t)n : 0;
}

size_t File::write(const uint8_t *buf, size_t size)
{
	if (!_open) {
		return 0;
	}
	ssize_t n = fs_write(&_file, buf, size);

	if (n < 0 || (size_t)n != size) {
		_ok = false;  /* close() then keeps the previous file */
	}
	return n > 0 ? (size_t)n : 0;
}

int File::read()
{
	uint8_t c;
	return read(&c, 1) == 1 ? c : -1;
}

int File::peek()
{
	int c = read();
	if (c >= 0) {
		fs_seek(&_file, -1, FS_SEEK_CUR);
	}
	return c;
}

int File::available()
{
	if (!_open) {
		return 0;
	}
	off_t pos = fs_tell(&_file);
	if (pos < 0 || fs_seek(&_file, 0, FS_SEEK_END) < 0) {
		return 0;
	}
	off_t end = fs_tell(&_file);
	fs_seek(&_file, pos, FS_SEEK_SET);
	return end > pos ? (int)(end - pos) : 0;
}

void File::close()
{
	if (!_open) {
		return;
	}
	int sync_rc = _tmp[0] != '\0' ? fs_sync(&_file) : 0;

	fs_close(&_file);
	_open = false;
	if (_tmp[0] == '\0') {
		return;
	}
	if (!_ok || sync_rc < 0 || fs_rename(_tmp, _target) < 0) {
		LOG_ERR("%s: not replaced, previous content kept", _target);
		fs_unlink(_tmp);
	}
	_tmp[0] = '\0';
}

void ZephyrFS::fullPath(char *dest, size_t dest_len, const char *path) const
{
	snprintf(dest, dest_len, "%s%s%s", _root, path[0] == '/' ? "" : "/", path);
}

bool ZephyrFS::exists(const char *path)
{
	char full[64];
	fullPath(full, sizeof(full), path);
	struct fs_dirent entry;
	return fs_stat(full, &entry) == 0;
}

File ZephyrFS::open(const char *path, const char *mode, bool create)
{
	char full[64];
	char tmp[File::PATH_LEN];
	File file;

	fullPath(full, sizeof(full), path);
	if (mode[0] == 'w') {
		int len = snprintf(tmp, sizeof(tmp), "%s.tmp", full);

		/* Write through a temp file: a power cut or a failed write must not
		 * leave the target truncated or empty. */
		if (len > 0 && len < (int)sizeof(tmp)) {
			file.openCommit(tmp, full);
		}
	} else {
		file.openPath(full, FS_O_READ);
	}
	(void)create;
	return file;
}

bool ZephyrFS::remove(const char *path)
{
	char full[64];
	fullPath(full, sizeof(full), path);
	return fs_unlink(full) == 0;
}

bool ZephyrFS::mkdir(const char *path)
{
	char full[64];
	fullPath(full, sizeof(full), path);
	return fs_mkdir(full) == 0;
}

/*
	StarStruck - a Free Software reimplementation for the Nintendo/BroadOn IOS.
	FS - Shared FS functions for other modules to us

	Copyright (C) 2026	DacoTaco

# This code is licensed to you under the terms of the GNU GPL, version 2;
# see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/

#include "types.h"
#include "string.h"
#include "fs/errors.h"
#include "fs/fs.h"
#include "fs/ioctls.h"
#include "ios/syscalls.h"
#include "ios/errno.h"

static s32 _fd = -1;
static u8 _cmd_buf[0x100] ALIGNED(0x20);
StaticAssert(sizeof(GetAttributesParameters) <= sizeof(_cmd_buf),
             "GetAttributesParameters too large for _cmd_buf");

static s32 GetFileDescriptor(void)
{
	if (_fd < 0)
		_fd = OSOpenFD("/dev/fs", 0);

	return _fd;
}

s32 OpenFile(const char *path, AccessMode mode)
{
	if (path == NULL)
		return FS_EINVAL;

	u32 len = strnlen(path, MAX_FILE_PATH);
	if (len == MAX_FILE_PATH)
		return FS_EINVAL;

	/* Copy through the module-local buffer to guarantee DMA-accessible memory. */
	memcpy(_cmd_buf, path, len + 1);
	return OSOpenFD((const char *)_cmd_buf, (s32)mode);
}

s32 GetFileStats(s32 fd, FileStatistics *out)
{
	return OSIoctlFD(fd, IOCTL_GETFILESTATS, NULL, 0, out, sizeof(FileStatistics));
}

s32 ShutdownFileSystem(void)
{
	s32 fd = GetFileDescriptor();
	if (fd < 0)
		return fd;
	return OSIoctlFD(fd, IOCTL_SHUTDOWN, NULL, 0, NULL, 0);
}

s32 FormatFileSystem(void)
{
	s32 fd = GetFileDescriptor();
	if (fd < 0)
		return fd;
	return OSIoctlFD(fd, IOCTL_FORMAT, NULL, 0, NULL, 0);
}

s32 GetNandStatistics(SFFSStatistics *stats)
{
	s32 fd = GetFileDescriptor();
	if (fd < 0)
		return fd;
	return OSIoctlFD(fd, IOCTL_GETSTATS, NULL, 0, stats, sizeof(SFFSStatistics));
}

s32 CreateDirectory(const char *path, u8 attrib, u8 owner, u8 group, u8 other)
{
	s32 fd = GetFileDescriptor();
	if (fd < 0)
		return fd;

	if (path == NULL)
		return FS_EINVAL;
	u32 len = strnlen(path, MAX_FILE_PATH);
	if (len == MAX_FILE_PATH)
		return FS_EINVAL;

	FileOperationsParameter *param = (FileOperationsParameter *)_cmd_buf;
	memset(param, 0, sizeof(FileOperationsParameter));
	strncpy(param->Path, path, sizeof(param->Path) - 1);
	param->OwnerPermissions = owner;
	param->GroupPermissions = group;
	param->OtherPermissions = other;
	param->Attributes = attrib;

	return OSIoctlFD(fd, IOCTL_CREATEDIR, param, sizeof(FileOperationsParameter), NULL, 0);
}

s32 CreateDirectoryRecursive(const char *path, u8 attrib, u8 owner, u8 group, u8 other)
{
	u32 entries;
	char buffer[MAX_FILE_PATH];
	s32 ret = ReadDirectory(path, NULL, &entries);
	if (ret != FS_ENOENT)
		return ret;

	memset(buffer, 0, sizeof(buffer));

	const char *path_end = path + strnlen(path, MAX_FILE_PATH);
	const char *seperator = strchr(path + 1, '/');

	while (1)
	{
		if (!seperator)
			seperator = path_end;

		u32 prefixLength = (u32)(seperator - path);
		strncpy(buffer, path, prefixLength);
		buffer[prefixLength] = '\0';

		ret = ReadDirectory(buffer, NULL, &entries);
		if (ret == 0)
			ret = SetAttributes(buffer, 0, 0, attrib, owner, group, other);
		else if (ret == FS_ENOENT)
			ret = CreateDirectory(buffer, attrib, owner, group, other);

		if (ret != 0)
			return ret;

		if (seperator >= path_end)
			break;

		seperator = strchr(seperator + 1, '/');
	}

	return 0;
}

s32 ReadDirectory(const char *path, char *out_entries, u32 *count)
{
	s32 fd = GetFileDescriptor();
	if (fd < 0)
		return fd;

	if (path == NULL || count == NULL)
		return FS_EINVAL;
	u32 length = strnlen(path, MAX_FILE_PATH);
	if (length == MAX_FILE_PATH)
		return FS_EINVAL;

	memset(_cmd_buf, 0, MAX_FILE_PATH);
	memcpy(_cmd_buf, path, length + 1);

	STACK_ALIGN(IoctlvMessageData, messageData, 4, 0x20);
	messageData[0].Data = _cmd_buf;
	messageData[0].Length = MAX_FILE_PATH;

	s32 ret;
	u32 count_io = *count;
	if (out_entries == NULL)
	{
		messageData[1].Data = &count_io;
		messageData[1].Length = sizeof(u32);
		ret = OSIoctlvFD(fd, IOCTLV_READDIR, 1, 1, messageData);
	}
	else
	{
		count_io = *count;
		messageData[1].Data = &count_io;
		messageData[1].Length = sizeof(u32);
		messageData[2].Data = out_entries;
		messageData[2].Length = *count * (MAX_FILE_SIZE + 1);
		messageData[3].Data = &count_io;
		messageData[3].Length = sizeof(u32);
		ret = OSIoctlvFD(fd, IOCTLV_READDIR, 2, 2, messageData);
	}

	if (ret == IPC_SUCCESS)
		*count = count_io;

	return ret;
}

s32 SetAttributes(const char *path, u32 uid, u16 gid, u8 attrib, u8 owner, u8 group, u8 other)
{
	s32 fd = GetFileDescriptor();
	if (fd < 0)
		return fd;

	if (path == NULL)
		return FS_EINVAL;
	u32 length = strnlen(path, MAX_FILE_PATH);
	if (length == MAX_FILE_PATH)
		return FS_EINVAL;

	FileOperationsParameter *param = (FileOperationsParameter *)_cmd_buf;
	memset(param, 0, sizeof(FileOperationsParameter));
	param->UserId = uid;
	param->GroupId = gid;
	strncpy(param->Path, path, sizeof(param->Path) - 1);
	param->OwnerPermissions = owner;
	param->GroupPermissions = group;
	param->OtherPermissions = other;
	param->Attributes = attrib;

	return OSIoctlFD(fd, IOCTL_SETATTR, param, sizeof(FileOperationsParameter), NULL, 0);
}

s32 GetAttributes(const char *path, u32 *uid, u16 *gid, u32 *attrib, u32 *owner,
                  u32 *group, u32 *other)
{
	s32 fd = GetFileDescriptor();
	if (fd < 0)
		return fd;

	if (path == NULL || uid == NULL || gid == NULL || attrib == NULL ||
	    owner == NULL || group == NULL || other == NULL)
		return FS_EINVAL;

	u32 len = strnlen(path, MAX_FILE_PATH);
	if (len == MAX_FILE_PATH)
		return FS_EINVAL;

	GetAttributesParameters *buf = (GetAttributesParameters *)_cmd_buf;
	memset(buf, 0, sizeof(GetAttributesParameters));
	strncpy(buf->Path, path, sizeof(buf->Path) - 1);

	s32 ret = OSIoctlFD(fd, IOCTL_GETATTR, buf->Path, sizeof(buf->Path),
	                    &buf->Parameters, sizeof(buf->Parameters));
	if (ret == 0)
	{
		*uid = buf->Parameters.UserId;
		*gid = buf->Parameters.GroupId;
		*owner = buf->Parameters.OwnerPermissions;
		*group = buf->Parameters.GroupPermissions;
		*other = buf->Parameters.OtherPermissions;
		*attrib = buf->Parameters.Attributes;
	}
	return ret;
}

s32 DeletePath(const char *path)
{
	s32 fd = GetFileDescriptor();
	if (fd < 0)
		return fd;

	if (path == NULL)
		return FS_EINVAL;
	u32 length = strnlen(path, MAX_FILE_PATH);
	if (length == MAX_FILE_PATH)
		return FS_EINVAL;

	memset(_cmd_buf, 0, MAX_FILE_PATH);
	memcpy(_cmd_buf, path, length + 1);

	return OSIoctlFD(fd, IOCTL_DELETE, _cmd_buf, MAX_FILE_PATH, NULL, 0);
}

s32 RenamePath(const char *source, const char *destination)
{
	s32 fd = GetFileDescriptor();
	if (fd < 0)
		return fd;

	if (source == NULL || destination == NULL)
		return FS_EINVAL;

	u32 destinationLength = strnlen(destination, MAX_FILE_PATH);
	u32 sourceLength = strnlen(source, MAX_FILE_PATH);
	if (destinationLength == MAX_FILE_PATH || sourceLength == MAX_FILE_PATH)
		return FS_EINVAL;

	FileRenameParameter *param = (FileRenameParameter *)_cmd_buf;
	memset(param, 0, sizeof(FileRenameParameter));
	strncpy(param->Source, source, sizeof(param->Source) - 1);
	strncpy(param->Destination, destination, sizeof(param->Destination) - 1);

	return OSIoctlFD(fd, IOCTL_RENAME, param, sizeof(FileRenameParameter), NULL, 0);
}

s32 CreateFile(const char *path, u8 attrib, u8 owner, u8 group, u8 other)
{
	s32 fd = GetFileDescriptor();
	if (fd < 0)
		return fd;

	if (path == NULL)
		return FS_EINVAL;
	u32 len = strnlen(path, MAX_FILE_PATH);
	if (len == MAX_FILE_PATH)
		return FS_EINVAL;

	FileOperationsParameter *param = (FileOperationsParameter *)_cmd_buf;
	memset(param, 0, sizeof(FileOperationsParameter));
	strncpy(param->Path, path, sizeof(param->Path) - 1);
	param->OwnerPermissions = owner;
	param->GroupPermissions = group;
	param->OtherPermissions = other;
	param->Attributes = attrib;

	return OSIoctlFD(fd, IOCTL_CREATEFILE, param, sizeof(FileOperationsParameter), NULL, 0);
}

s32 SetFileVersionControl(const char *path, u8 version)
{
	s32 fd = GetFileDescriptor();
	if (fd < 0)
		return fd;

	if (path == NULL)
		return FS_EINVAL;
	u32 len = strnlen(path, MAX_FILE_PATH);
	if (len == MAX_FILE_PATH)
		return FS_EINVAL;

	FileOperationsParameter *param = (FileOperationsParameter *)_cmd_buf;
	memset(param, 0, sizeof(FileOperationsParameter));
	strncpy(param->Path, path, sizeof(param->Path) - 1);
	param->Attributes = version;

	return OSIoctlFD(fd, IOCTL_SETFILEVERCTRL, param,
	                 sizeof(FileOperationsParameter), NULL, 0);
}

//Create a file and write the data
//This function uses an intermediate temp file in /tmp to ensure the final write is correct
//after which it is renamed to the final path, and all necessary parent directories are created if they don't exist
s32 CreateAndWriteFile(const char *filepath, u8 attrib, u8 owner, u8 group,
                       u8 other, const u8 *data, u32 len)
{
	//compile the temp path and delete it just in case
	char *tempPath = "/tmp/000000000000";
	const char *filename = strrchr(filepath, '/');
	strncpy(tempPath + 5, filename + 1, 12);
	DeletePath(tempPath);

	//create the file and open it for writing
	s32 ret = CreateFile(tempPath, attrib, owner, group, other);
	if (ret != IPC_SUCCESS)
		return ret;

	s32 fd = OpenFile(tempPath, Write);
	if (fd < 0)
		return fd;

	//write and close the handle again.
	//if any of the 2 fails we will return a error
	s32 written = WriteFile(fd, data, len);
	ret = OSCloseFD(fd);
	if (written != (s32)len)
		return (written < 0) ? written : IOS_EIO;
	else if (ret != IPC_SUCCESS)
		return ret;

	/* Ensure the parent directory tree exists. */
	char parentDirectory[MAX_FILE_PATH];
	memset(parentDirectory, 0, sizeof(parentDirectory));
	strncpy(parentDirectory, filepath, (u32)((int)filename - (int)filepath));
	ret = CreateDirectoryRecursive(parentDirectory, attrib, owner, group, other);
	if (ret != IPC_SUCCESS)
		return ret;

	return RenamePath(tempPath, filepath);
}
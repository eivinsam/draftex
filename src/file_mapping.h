#pragma once

#include <string_view>

struct FileMapping
{
	const std::string_view data;

	FileMapping(const char* filename);
	~FileMapping();

	FileMapping(FileMapping&&) = delete;
	FileMapping(const FileMapping&) = delete;

	FileMapping& operator=(FileMapping&&) = delete;
	FileMapping& operator=(const FileMapping&) = delete;
};
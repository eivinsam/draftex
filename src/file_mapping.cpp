#include "file_mapping.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static std::string_view map_file(const char* filename)
{
	auto file = CreateFileA(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return{};

	DWORD size_hi;
	const DWORD size = GetFileSize(file, &size_hi);
	if (size_hi > 0)
	{
		throw std::runtime_error("very large files not supported");
		CloseHandle(file);
	}

	auto mapping = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, size_hi, NULL);
	if (mapping == NULL)
		return{};
	CloseHandle(file);

	const auto ptr = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, size);

	CloseHandle(mapping);

	return { (const char*)ptr, size };
}

FileMapping::FileMapping(const char* filename) : 
	data(map_file(filename))
{

}

FileMapping::~FileMapping()
{
	UnmapViewOfFile(data.data());
}
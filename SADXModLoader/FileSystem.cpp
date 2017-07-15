#include "stdafx.h"
#include <windows.h>
#include <string>
using std::string;
using std::wstring;

bool Exists(const wstring& path)
{
	return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}
bool Exists(const string& path)
{
	return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool IsDirectory(const wstring& path)
{
	const DWORD attrs = GetFileAttributesW(path.c_str());
	return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY));
}
bool IsDirectory(const string& path)
{
	const DWORD attrs = GetFileAttributesA(path.c_str());
	return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY));
}

bool IsFile(const wstring& path)
{
	const DWORD attrs = GetFileAttributesW(path.c_str());
	return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
}
bool IsFile(const string& path)
{
	const DWORD attrs = GetFileAttributesA(path.c_str());
	return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
}

string GetDirectory(const string& path)
{
	auto slash = path.find_last_of("/\\");
	if (slash == string::npos)
	{
		return string();
	}

	if (slash != path.size() - 1)
	{
		return path.substr(0, slash);
	}

	auto last = slash;
	slash = path.find_last_of("/\\", last);
	if (slash == string::npos)
	{
		return string();
	}

	result = path.substr(last);
	return result;
}

string GetBaseName(const string& path)
{
	auto slash = path.find_last_of("/\\");
	if (slash == string::npos)
	{
		return path;
	}

	if (slash != path.size() - 1)
	{
		return path.substr(slash + 1);
	}

	auto last = slash - 1;
	slash = path.find_last_of("/\\", last);
	return (!slash || slash == npos) ? string() : path.substr(slash + 1, last - slash);
}

void StripExtension(string& path)
{
	auto dot = path.find('.');
	if (dot != string::npos)
	{
		path.resize(dot);
	}
}

string GetExtension(const string& path, bool includeDot)
{
	auto dot = path.find('.');
	if (dot == string::npos)
	{
		return string();
	}

	if (!includeDot)
	{
		++dot;
	}

	return path.substr(dot);
}

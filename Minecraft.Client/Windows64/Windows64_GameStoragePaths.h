#pragma once

// Root folder for Win64-style saves: Windows64\GameHDD\<saveId>\saveData.ms
// - Desktop Win64: relative to process CWD (exe directory after WinMain sets it).
// - UWP: absolute under ApplicationData LocalFolder (package install is read-only).

#ifdef _WINDOWS64

#include <string>

#ifdef _UWP
extern wchar_t g_LocalStatePathW[512];

inline std::wstring Win64_GetGameHddRootW()
{
	std::wstring r(g_LocalStatePathW);
	if (!r.empty())
	{
		wchar_t trail = r.back();
		if (trail != L'\\' && trail != L'/')
			r.push_back(L'\\');
	}
	r.append(L"Windows64\\GameHDD");
	return r;
}
#else
inline std::wstring Win64_GetGameHddRootW()
{
	return std::wstring(L"Windows64\\GameHDD");
}
#endif

#endif // _WINDOWS64

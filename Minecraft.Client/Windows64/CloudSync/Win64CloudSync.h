#pragma once

#ifdef _WINDOWS64

#include <cstdint>
#include <string>

/** Upload or download one GameHDD save folder (UTF8SaveFilename) via linked Dropbox / Google Drive / OneDrive. */
namespace Win64CloudSync
{
	bool IsPlayFabCloudSaveAvailable();

	/** errOut is UTF-8 for logging / MessageBox conversion at call site */
	bool UploadSaveFolder(const char *utf8SaveFolderName, std::string &errOut);
	bool DownloadSaveFolder(const char *utf8SaveFolderName, std::string &errOut);

	/** Call from main menu each frame: background pull/push for all GameHDD worlds when cloud is linked (throttled). */
	void TickTitleScreenCloudSync(uint32_t tickCountMs);
}

#endif

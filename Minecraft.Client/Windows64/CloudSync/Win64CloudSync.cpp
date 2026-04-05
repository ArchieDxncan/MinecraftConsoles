#include "stdafx.h"

#ifdef _WINDOWS64

#include "Win64CloudSync.h"
#include "../Leaderboards/WindowsLeaderboardManager.h"
#include "../Leaderboards/PlayFabConfig.h"
#include "../Windows64_GameStoragePaths.h"
#include "Common/Leaderboards/LeaderboardManager.h"

#include <nlohmann/json.hpp>

#include "../4JLibs/inc/4J_Profile.h"
#include "../4JLibs/inc/4J_Storage.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <set>
#include <thread>
#include <vector>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace
{
	std::wstring Utf8ToWide(const std::string &u8)
	{
		if (u8.empty())
			return std::wstring();
		int n = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), (int)u8.size(), nullptr, 0);
		if (n <= 0)
			return std::wstring();
		std::wstring w((size_t)n, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), (int)u8.size(), &w[0], n);
		return w;
	}

	std::string WideToUtf8(const std::wstring &w)
	{
		if (w.empty())
			return std::string();
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
		if (n <= 0)
			return std::string();
		std::string u8((size_t)n, '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &u8[0], n, nullptr, nullptr);
		return u8;
	}

	std::string SanitizeSegment(const char *utf8)
	{
		std::string o;
		if (!utf8)
			return "world";
		for (; *utf8; ++utf8)
		{
			unsigned char c = (unsigned char)*utf8;
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')
				o += (char)c;
			else
				o += '_';
		}
		if (o.empty())
			o = "world";
		if (o.size() > 80)
			o.resize(80);
		return o;
	}

	WindowsLeaderboardManager *Wlm()
	{
		return dynamic_cast<WindowsLeaderboardManager *>(LeaderboardManager::Instance());
	}

	bool WinHttpReadResponseLocation(HINTERNET hRequest, std::string *outLocationUtf8)
	{
		if (!outLocationUtf8)
			return true;
		outLocationUtf8->clear();
		DWORD dwSize = 0;
		SetLastError(0);
		if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER,
				&dwSize, WINHTTP_NO_HEADER_INDEX))
			return true;
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || dwSize < sizeof(wchar_t))
			return true;
		std::vector<wchar_t> wbuf(dwSize / sizeof(wchar_t) + 2u, L'\0');
		DWORD bufBytes = (DWORD)(wbuf.size() * sizeof(wchar_t));
		if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, wbuf.data(), &bufBytes,
				WINHTTP_NO_HEADER_INDEX))
			return true;
		*outLocationUtf8 = WideToUtf8(std::wstring(wbuf.data()));
		return true;
	}

	bool WinHttpRequest(const wchar_t *verb, const std::wstring &host, const std::wstring &pathAndQuery,
		const std::string &headersUtf8, const void *body, DWORD bodyLen, long &httpStatus, std::string &respBody,
		std::string &err, std::string *outLocationUtf8 = nullptr)
	{
		httpStatus = 0;
		respBody.clear();
		err.clear();
		if (outLocationUtf8)
			outLocationUtf8->clear();

		HINTERNET hSession = WinHttpOpen(L"MinecraftConsoles-CloudSync/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSession)
		{
			err = "WinHttpOpen failed";
			return false;
		}

		HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!hConnect)
		{
			WinHttpCloseHandle(hSession);
			err = "WinHttpConnect failed";
			return false;
		}

		HINTERNET hRequest = WinHttpOpenRequest(hConnect, verb, pathAndQuery.c_str(), nullptr, WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!hRequest)
		{
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			err = "WinHttpOpenRequest failed";
			return false;
		}

		std::wstring hdr = Utf8ToWide(headersUtf8);
		BOOL ok = WinHttpSendRequest(hRequest, hdr.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdr.c_str(),
			hdr.empty() ? 0 : (DWORD)-1, (LPVOID)body, bodyLen, bodyLen, 0);

		if (!ok || !WinHttpReceiveResponse(hRequest, nullptr))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			err = "WinHttpSendRequest/ReceiveResponse failed";
			return false;
		}

		DWORD statusCode = 0;
		DWORD sz = sizeof(statusCode);
		if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &sz, WINHTTP_NO_HEADER_INDEX))
		{
			httpStatus = (long)statusCode;
		}

		(void)WinHttpReadResponseLocation(hRequest, outLocationUtf8);

		std::string acc;
		for (;;)
		{
			char buf[16384];
			DWORD read = 0;
			if (!WinHttpReadData(hRequest, buf, sizeof(buf), &read) || read == 0)
				break;
			acc.append(buf, read);
		}

		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);

		respBody = std::move(acc);
		return true;
	}

	bool ParseHttpsUrl(const std::string &url, std::wstring &outHost, std::wstring &outPathQuery)
	{
		static const char kHttps[] = "https://";
		const size_t n = sizeof(kHttps) - 1u;
		if (url.size() < n || url.compare(0, n, kHttps) != 0)
			return false;
		size_t i = n;
		size_t slash = url.find('/', i);
		if (slash == std::string::npos)
		{
			outHost = Utf8ToWide(url.substr(i));
			outPathQuery = L"/";
			return !outHost.empty();
		}
		outHost = Utf8ToWide(url.substr(i, slash - i));
		outPathQuery = Utf8ToWide(url.substr(slash));
		return !outHost.empty() && !outPathQuery.empty();
	}

	struct LocalFileRec
	{
		std::wstring absPath;
		std::string relPosix;
	};

	void CollectLocalFilesRecursive(const std::wstring &dirAbs, const std::wstring &dirRel, std::vector<LocalFileRec> &out)
	{
		std::wstring spec = dirAbs + L"\\*";
		WIN32_FIND_DATAW fd{};
		HANDLE h = FindFirstFileW(spec.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			return;
		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
					continue;
				std::wstring subAbs = dirAbs + L"\\" + fd.cFileName;
				std::wstring subRel = dirRel.empty() ? std::wstring(fd.cFileName) : (dirRel + L"\\" + fd.cFileName);
				CollectLocalFilesRecursive(subAbs, subRel, out);
			}
			else
			{
				LocalFileRec r;
				r.absPath = dirAbs + L"\\" + fd.cFileName;
				std::wstring relFull = dirRel.empty() ? std::wstring(fd.cFileName) : (dirRel + L"\\" + fd.cFileName);
				r.relPosix = WideToUtf8(relFull);
				std::replace(r.relPosix.begin(), r.relPosix.end(), '\\', '/');
				out.push_back(std::move(r));
			}
		} while (FindNextFileW(h, &fd));
		FindClose(h);
	}

	bool ReadWholeFileW(const std::wstring &path, std::vector<unsigned char> &out, std::string &err)
	{
		out.clear();
		HANDLE hf = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hf == INVALID_HANDLE_VALUE)
		{
			err = "Cannot open local file";
			return false;
		}
		LARGE_INTEGER li{};
		if (!GetFileSizeEx(hf, &li) || li.QuadPart < 0 || li.QuadPart > (200LL * 1024 * 1024))
		{
			CloseHandle(hf);
			err = "Local file too large or invalid";
			return false;
		}
		DWORD n = (DWORD)li.QuadPart;
		out.resize(n);
		DWORD rd = 0;
		if (n > 0 && (!ReadFile(hf, out.data(), n, &rd, nullptr) || rd != n))
		{
			CloseHandle(hf);
			err = "ReadFile failed";
			return false;
		}
		CloseHandle(hf);
		return true;
	}

	bool WriteWholeFileW(const std::wstring &path, const void *data, size_t len, std::string &err)
	{
		std::wstring dir = path;
		size_t slash = dir.find_last_of(L"\\/");
		if (slash != std::wstring::npos)
		{
			dir.resize(slash);
			DWORD a = GetFileAttributesW(dir.c_str());
			if (a == INVALID_FILE_ATTRIBUTES)
			{
				// create nested dirs
				std::wstring partial;
				for (size_t i = 0; i < dir.size(); ++i)
				{
					partial.push_back(dir[i]);
					if (dir[i] == L'\\' || dir[i] == L'/')
					{
						if (GetFileAttributesW(partial.c_str()) == INVALID_FILE_ATTRIBUTES)
							CreateDirectoryW(partial.c_str(), nullptr);
					}
				}
				if (GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES)
					CreateDirectoryW(dir.c_str(), nullptr);
			}
		}

		HANDLE hf = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hf == INVALID_HANDLE_VALUE)
		{
			err = "Cannot create local file";
			return false;
		}
		DWORD wr = 0;
		const BYTE *p = static_cast<const BYTE *>(data);
		size_t left = len;
		while (left > 0)
		{
			DWORD chunk = (DWORD)std::min<size_t>(left, 1u << 20);
			if (!WriteFile(hf, p, chunk, &wr, nullptr) || wr != chunk)
			{
				CloseHandle(hf);
				err = "WriteFile failed";
				return false;
			}
			p += chunk;
			left -= chunk;
		}
		CloseHandle(hf);
		return true;
	}

	// --- Dropbox ---
	bool DropboxUpload(const std::string &accessToken, const std::string &dropboxPath, const std::vector<unsigned char> &bytes,
		std::string &err)
	{
		nlohmann::json arg;
		arg["path"] = dropboxPath;
		arg["mode"] = "overwrite";
		arg["mute"] = true;
		std::string argStr = arg.dump();
		std::string headers = "Authorization: Bearer " + accessToken + "\r\nContent-Type: application/octet-stream\r\n";
		headers += "Dropbox-API-Arg: " + argStr + "\r\n";
		long http = 0;
		std::string body;
		if (!WinHttpRequest(L"POST", L"content.dropboxapi.com", L"/2/files/upload", headers, bytes.data(),
				(DWORD)bytes.size(), http, body, err))
			return false;
		if (http < 200 || http >= 300)
		{
			err = "Dropbox upload HTTP " + std::to_string(http) + " " + body;
			return false;
		}
		return true;
	}

	bool DropboxDownload(const std::string &accessToken, const std::string &dropboxPath, std::vector<unsigned char> &out,
		std::string &err)
	{
		nlohmann::json arg;
		arg["path"] = dropboxPath;
		std::string argStr = arg.dump();
		std::string headers =
			"Authorization: Bearer " + accessToken + "\r\nDropbox-API-Arg: " + argStr + "\r\n";
		long http = 0;
		std::string body;
		if (!WinHttpRequest(L"POST", L"content.dropboxapi.com", L"/2/files/download", headers, nullptr, 0, http, body, err))
			return false;
		if (http < 200 || http >= 300)
		{
			err = "Dropbox download HTTP " + std::to_string(http) + " " + body;
			return false;
		}
		out.assign(body.begin(), body.end());
		return true;
	}

	bool DropboxListAll(const std::string &accessToken, const std::string &pathPrefix, std::vector<nlohmann::json> &entries,
		std::string &err)
	{
		entries.clear();
		nlohmann::json req;
		req["path"] = pathPrefix;
		req["recursive"] = true;
		req["include_media_info"] = false;
		req["include_deleted"] = false;
		req["include_has_explicit_shared_members"] = false;
		std::string reqStr = req.dump();
		std::string headers = "Authorization: Bearer " + accessToken + "\r\nContent-Type: application/json\r\n";

		std::string cursor;
		for (int pass = 0; pass < 200; ++pass)
		{
			long http = 0;
			std::string resp;
			const char *subpath = cursor.empty() ? "/2/files/list_folder" : "/2/files/list_folder/continue";
			std::string bodyJson = cursor.empty() ? reqStr : nlohmann::json{ { "cursor", cursor } }.dump();
			if (!WinHttpRequest(L"POST", L"api.dropboxapi.com", Utf8ToWide(subpath), headers, bodyJson.data(),
					(DWORD)bodyJson.size(), http, resp, err))
				return false;
			if (http < 200 || http >= 300)
			{
				err = "Dropbox list HTTP " + std::to_string(http) + " " + resp;
				return false;
			}
			nlohmann::json j;
			try
			{
				j = nlohmann::json::parse(resp);
			}
			catch (...)
			{
				err = "Dropbox list JSON";
				return false;
			}
			if (j.contains("entries") && j["entries"].is_array())
			{
				for (const auto &e : j["entries"])
					entries.push_back(e);
			}
			bool hasMore = j.value("has_more", false);
			cursor = j.value("cursor", std::string());
			if (!hasMore)
				break;
			if (cursor.empty())
				break;
		}
		return true;
	}

	/** Immediate children folder names under path (non-recursive). Empty if path missing. */
	bool DropboxListImmediateChildFolderNames(const std::string &accessToken, const std::string &path,
		std::vector<std::string> &outNames, std::string &err)
	{
		outNames.clear();
		nlohmann::json req;
		req["path"] = path;
		req["recursive"] = false;
		req["include_media_info"] = false;
		req["include_deleted"] = false;
		req["include_has_explicit_shared_members"] = false;
		std::string reqStr = req.dump();
		std::string headers = "Authorization: Bearer " + accessToken + "\r\nContent-Type: application/json\r\n";

		std::string cursor;
		for (int pass = 0; pass < 200; ++pass)
		{
			long http = 0;
			std::string resp;
			const char *subpath = cursor.empty() ? "/2/files/list_folder" : "/2/files/list_folder/continue";
			std::string bodyJson = cursor.empty() ? reqStr : nlohmann::json{ { "cursor", cursor } }.dump();
			if (!WinHttpRequest(L"POST", L"api.dropboxapi.com", Utf8ToWide(subpath), headers, bodyJson.data(),
					(DWORD)bodyJson.size(), http, resp, err))
				return false;
			if (http < 200 || http >= 300)
			{
				if (pass == 0 && (resp.find("not_found") != std::string::npos ||
									 resp.find("path_not_found") != std::string::npos))
				{
					err.clear();
					return true;
				}
				err = "Dropbox list (shallow) HTTP " + std::to_string(http) + " " + resp;
				return false;
			}
			try
			{
				nlohmann::json j = nlohmann::json::parse(resp);
				if (j.contains("entries") && j["entries"].is_array())
				{
					for (const auto &e : j["entries"])
					{
						if (!e.contains(".tag") || e[".tag"] != "folder")
							continue;
						std::string n = e.value("name", std::string());
						if (!n.empty())
							outNames.push_back(std::move(n));
					}
				}
				bool hasMore = j.value("has_more", false);
				cursor = j.value("cursor", std::string());
				if (!hasMore)
					break;
				if (cursor.empty())
					break;
			}
			catch (...)
			{
				err = "Dropbox list (shallow) JSON";
				return false;
			}
		}
		return true;
	}

	// --- Google Drive ---
	bool GooglePatchParents(const std::string &accessToken, const std::string &fileId, const std::string &parentId,
		const std::string &removeParent, std::string &err)
	{
		std::wstring path =
			Utf8ToWide(std::string("/drive/v3/files/") + fileId + "?addParents=" + parentId + "&removeParents=" + removeParent +
				"&fields=id");
		std::string headers = "Authorization: Bearer " + accessToken + "\r\nContent-Type: application/json\r\n";
		long http = 0;
		std::string resp;
		if (!WinHttpRequest(L"PATCH", L"www.googleapis.com", path, headers, nullptr, 0, http, resp, err))
			return false;
		if (http < 200 || http >= 300)
		{
			err = "Drive PATCH HTTP " + std::to_string(http) + " " + resp;
			return false;
		}
		return true;
	}

	bool GoogleUploadMedia(const std::string &accessToken, const std::vector<unsigned char> &bytes, std::string &outFileId,
		std::string &outParent, std::string &err)
	{
		outFileId.clear();
		outParent = "root";
		std::string headers = "Authorization: Bearer " + accessToken + "\r\nContent-Type: application/octet-stream\r\n";
		long http = 0;
		std::string resp;
		if (!WinHttpRequest(L"POST", L"www.googleapis.com",
				L"/upload/drive/v3/files?uploadType=media", headers, bytes.data(), (DWORD)bytes.size(), http, resp, err))
			return false;
		if (http < 200 || http >= 300)
		{
			err = "Drive upload HTTP " + std::to_string(http) + " " + resp;
			return false;
		}
		try
		{
			nlohmann::json j = nlohmann::json::parse(resp);
			outFileId = j.value("id", std::string());
			if (j.contains("parents") && j["parents"].is_array() && !j["parents"].empty())
				outParent = j["parents"][0].get<std::string>();
		}
		catch (...)
		{
			err = "Drive upload parse";
			return false;
		}
		if (outFileId.empty())
		{
			err = "Drive upload missing id";
			return false;
		}
		return true;
	}

	/** Google Drive resumable upload (single PUT) for files above simple upload limits. */
	bool GoogleDriveUploadResumable(const std::string &accessToken, const std::string &parentFolderId,
		const std::string &displayName, const std::vector<unsigned char> &bytes, std::string &err)
	{
		nlohmann::json meta;
		meta["name"] = displayName;
		meta["parents"] = nlohmann::json::array({parentFolderId});
		std::string jsonBody = meta.dump();
		std::string headers = "Authorization: Bearer " + accessToken +
			"\r\nContent-Type: application/json; charset=UTF-8\r\nX-Upload-Content-Type: application/octet-stream\r\n"
			"X-Upload-Content-Length: " +
			std::to_string(bytes.size()) + "\r\n";
		long http = 0;
		std::string resp;
		std::string location;
		if (!WinHttpRequest(L"POST", L"www.googleapis.com", L"/upload/drive/v3/files?uploadType=resumable&fields=id", headers,
				jsonBody.data(), (DWORD)jsonBody.size(), http, resp, err, &location))
			return false;
		if (http < 200 || http >= 300)
		{
			err = "Drive resumable init HTTP " + std::to_string(http) + " " + resp;
			return false;
		}
		if (location.empty())
		{
			err = "Drive resumable init missing Location header";
			return false;
		}
		std::wstring putHost, putPath;
		if (!ParseHttpsUrl(location, putHost, putPath))
		{
			err = "Drive resumable bad Location URL";
			return false;
		}
		std::string rangeHdr = bytes.empty() ? std::string("bytes */0")
											 : ("bytes 0-" + std::to_string(bytes.size() - 1) + "/" + std::to_string(bytes.size()));
		std::string putHeaders = "Authorization: Bearer " + accessToken +
			"\r\nContent-Type: application/octet-stream\r\nContent-Length: " + std::to_string(bytes.size()) +
			"\r\nContent-Range: " + rangeHdr + "\r\n";
		long httpPut = 0;
		std::string putResp;
		const void *putBody = bytes.empty() ? nullptr : bytes.data();
		const DWORD putLen = (DWORD)bytes.size();
		if (!WinHttpRequest(L"PUT", putHost, putPath, putHeaders, putBody, putLen, httpPut, putResp, err, nullptr))
			return false;
		if (httpPut < 200 || httpPut >= 300)
		{
			err = "Drive resumable upload HTTP " + std::to_string(httpPut) + " " + putResp;
			return false;
		}
		return true;
	}

	bool GooglePatchName(const std::string &accessToken, const std::string &fileId, const std::string &name, std::string &err)
	{
		nlohmann::json meta;
		meta["name"] = name;
		std::string body = meta.dump();
		std::wstring path = Utf8ToWide("/drive/v3/files/" + fileId + "?fields=id");
		std::string headers = "Authorization: Bearer " + accessToken + "\r\nContent-Type: application/json\r\n";
		long http = 0;
		std::string resp;
		if (!WinHttpRequest(L"PATCH", L"www.googleapis.com", path, headers, body.data(), (DWORD)body.size(), http, resp, err))
			return false;
		if (http < 200 || http >= 300)
		{
			err = "Drive rename HTTP " + std::to_string(http) + " " + resp;
			return false;
		}
		return true;
	}

	bool GoogleListChildren(const std::string &accessToken, const std::string &parentId, std::vector<nlohmann::json> &files,
		std::string &err)
	{
		files.clear();
		std::string q = "q=";
		std::string enc;
		{
			std::string raw = "'" + parentId + "' in parents and trashed=false";
			for (unsigned char c : raw)
			{
				if (c == ' ')
					enc += "%20";
				else if (c == '\'')
					enc += "%27";
				else
					enc += (char)c;
			}
		}
		q += enc;
		std::wstring path =
			Utf8ToWide("/drive/v3/files?" + q + "&fields=files(id,name,mimeType,modifiedTime)&pageSize=1000");
		std::string headers = "Authorization: Bearer " + accessToken + "\r\n";
		long http = 0;
		std::string resp;
		if (!WinHttpRequest(L"GET", L"www.googleapis.com", path, headers, nullptr, 0, http, resp, err))
			return false;
		if (http < 200 || http >= 300)
		{
			err = "Drive list HTTP " + std::to_string(http) + " " + resp;
			return false;
		}
		try
		{
			nlohmann::json j = nlohmann::json::parse(resp);
			if (j.contains("files") && j["files"].is_array())
			{
				for (const auto &f : j["files"])
					files.push_back(f);
			}
		}
		catch (...)
		{
			err = "Drive list parse";
			return false;
		}
		return true;
	}

	bool GoogleDownloadMedia(const std::string &accessToken, const std::string &fileId, std::vector<unsigned char> &out,
		std::string &err)
	{
		std::wstring path = Utf8ToWide("/drive/v3/files/" + fileId + "?alt=media");
		std::string headers = "Authorization: Bearer " + accessToken + "\r\n";
		long http = 0;
		std::string body;
		if (!WinHttpRequest(L"GET", L"www.googleapis.com", path, headers, nullptr, 0, http, body, err))
			return false;
		if (http < 200 || http >= 300)
		{
			err = "Drive download HTTP " + std::to_string(http);
			return false;
		}
		out.assign(body.begin(), body.end());
		return true;
	}

	bool CollectGoogleDriveFilesRecursive(const std::string &accessToken, const std::string &folderId,
		const std::string &relBase, std::vector<std::pair<std::string, std::string>> &outIdAndRel, std::string &err)
	{
		std::vector<nlohmann::json> files;
		if (!GoogleListChildren(accessToken, folderId, files, err))
			return false;
		for (const auto &f : files)
		{
			std::string name = f.value("name", std::string());
			std::string id = f.value("id", std::string());
			std::string mime = f.value("mimeType", std::string());
			if (name.empty() || id.empty())
				continue;
			std::string nextRel = relBase.empty() ? name : (relBase + "/" + name);
			if (mime == "application/vnd.google-apps.folder")
			{
				if (!CollectGoogleDriveFilesRecursive(accessToken, id, nextRel, outIdAndRel, err))
					return false;
			}
			else
			{
				outIdAndRel.push_back({ id, nextRel });
			}
		}
		return true;
	}

	std::string GoogleFindChildFolderId(const std::string &accessToken, const std::string &parentId, const std::string &name,
		std::string &err)
	{
		std::vector<nlohmann::json> files;
		if (!GoogleListChildren(accessToken, parentId, files, err))
			return std::string();
		for (const auto &f : files)
		{
			if (f.value("name", std::string()) == name &&
				f.value("mimeType", std::string()) == "application/vnd.google-apps.folder")
			{
				return f.value("id", std::string());
			}
		}
		return std::string();
	}

	std::string GoogleEnsureChildFolder(const std::string &accessToken, const std::string &parentId, const std::string &name,
		std::string &err)
	{
		std::string existing = GoogleFindChildFolderId(accessToken, parentId, name, err);
		if (!existing.empty())
			return existing;
		nlohmann::json meta;
		meta["name"] = name;
		meta["mimeType"] = "application/vnd.google-apps.folder";
		meta["parents"] = nlohmann::json::array({ parentId });
		std::string body = meta.dump();
		std::string headers = "Authorization: Bearer " + accessToken + "\r\nContent-Type: application/json\r\n";
		long http = 0;
		std::string resp;
		if (!WinHttpRequest(L"POST", L"www.googleapis.com", L"/drive/v3/files?fields=id", headers, body.data(),
				(DWORD)body.size(), http, resp, err))
			return std::string();
		if (http < 200 || http >= 300)
		{
			err = "Drive mkdir HTTP " + std::to_string(http) + " " + resp;
			return std::string();
		}
		try
		{
			return nlohmann::json::parse(resp).value("id", std::string());
		}
		catch (...)
		{
			err = "Drive mkdir parse";
			return std::string();
		}
	}

	bool GoogleListCloudWorldFolderNames(const std::string &accessToken, const std::string &rootId,
		std::vector<std::string> &outNames, std::string &err)
	{
		outNames.clear();
		std::vector<nlohmann::json> files;
		if (!GoogleListChildren(accessToken, rootId, files, err))
			return false;
		for (const auto &f : files)
		{
			if (f.value("mimeType", std::string()) != "application/vnd.google-apps.folder")
				continue;
			std::string n = f.value("name", std::string());
			if (!n.empty())
				outNames.push_back(std::move(n));
		}
		return true;
	}

	// --- Microsoft Graph ---
	bool GraphListChildren(const std::string &accessToken, const std::string &itemId, std::vector<nlohmann::json> &items,
		std::string &err)
	{
		items.clear();
		std::wstring path = Utf8ToWide("/v1.0/me/drive/items/" + itemId + "/children?$top=999");
		std::string headers = "Authorization: Bearer " + accessToken + "\r\n";
		long http = 0;
		std::string resp;
		if (!WinHttpRequest(L"GET", L"graph.microsoft.com", path, headers, nullptr, 0, http, resp, err))
			return false;
		if (http < 200 || http >= 300)
		{
			err = "Graph list HTTP " + std::to_string(http) + " " + resp;
			return false;
		}
		try
		{
			nlohmann::json j = nlohmann::json::parse(resp);
			if (j.contains("value") && j["value"].is_array())
			{
				for (const auto &v : j["value"])
					items.push_back(v);
			}
		}
		catch (...)
		{
			err = "Graph list parse";
			return false;
		}
		return true;
	}

	bool GraphListCloudWorldFolderNames(const std::string &accessToken, const std::string &rootItemId,
		std::vector<std::string> &outNames, std::string &err)
	{
		outNames.clear();
		std::vector<nlohmann::json> items;
		if (!GraphListChildren(accessToken, rootItemId, items, err))
			return false;
		for (const auto &it : items)
		{
			if (!it.contains("folder"))
				continue;
			std::string n = it.value("name", std::string());
			if (!n.empty())
				outNames.push_back(std::move(n));
		}
		return true;
	}

	std::string GraphFindChildFolderId(const std::string &accessToken, const std::string &parentId, const std::string &name,
		std::string &err)
	{
		std::vector<nlohmann::json> items;
		if (!GraphListChildren(accessToken, parentId, items, err))
			return std::string();
		for (const auto &it : items)
		{
			if (it.value("name", std::string()) == name && it.contains("folder"))
				return it.value("id", std::string());
		}
		return std::string();
	}

	std::string GraphEnsureChildFolder(const std::string &accessToken, const std::string &parentId, const std::string &name,
		std::string &err)
	{
		std::string ex = GraphFindChildFolderId(accessToken, parentId, name, err);
		if (!ex.empty())
			return ex;
		nlohmann::json body;
		body["name"] = name;
		body["folder"] = nlohmann::json::object();
		body["@microsoft.graph.conflictBehavior"] = "rename";
		std::string payload = body.dump();
		std::wstring path = Utf8ToWide("/v1.0/me/drive/items/" + parentId + "/children");
		std::string headers = "Authorization: Bearer " + accessToken + "\r\nContent-Type: application/json\r\n";
		long http = 0;
		std::string resp;
		if (!WinHttpRequest(L"POST", L"graph.microsoft.com", path, headers, payload.data(), (DWORD)payload.size(), http, resp,
				err))
			return std::string();
		if (http < 200 || http >= 300)
		{
			err = "Graph mkdir HTTP " + std::to_string(http) + " " + resp;
			return std::string();
		}
		try
		{
			return nlohmann::json::parse(resp).value("id", std::string());
		}
		catch (...)
		{
			err = "Graph mkdir parse";
			return std::string();
		}
	}

	bool GraphPutContent(const std::string &accessToken, const std::string &parentId, const std::string &fileName,
		const std::vector<unsigned char> &bytes, std::string &err)
	{
		std::string encName;
		for (unsigned char c : fileName)
		{
			if (std::isalnum(c) || c == '.' || c == '_' || c == '-')
				encName += (char)c;
			else
			{
				char buf[8];
				sprintf_s(buf, "%%%02X", c);
				encName += buf;
			}
		}
		std::wstring path =
			Utf8ToWide("/v1.0/me/drive/items/" + parentId + ":/" + encName + ":/content");
		std::string headers = "Authorization: Bearer " + accessToken + "\r\nContent-Type: application/octet-stream\r\n";
		long http = 0;
		std::string resp;
		if (!WinHttpRequest(L"PUT", L"graph.microsoft.com", path, headers, bytes.data(), (DWORD)bytes.size(), http, resp, err))
			return false;
		if (http < 200 || http >= 300)
		{
			err = "Graph upload HTTP " + std::to_string(http) + " " + resp;
			return false;
		}
		return true;
	}

	bool GraphDownloadItem(const std::string &accessToken, const std::string &itemId, std::vector<unsigned char> &out,
		std::string &err)
	{
		std::wstring path = Utf8ToWide("/v1.0/me/drive/items/" + itemId + "/content");
		std::string headers = "Authorization: Bearer " + accessToken + "\r\n";
		long http = 0;
		std::string body;
		if (!WinHttpRequest(L"GET", L"graph.microsoft.com", path, headers, nullptr, 0, http, body, err))
			return false;
		if (http < 200 || http >= 300)
		{
			err = "Graph download HTTP " + std::to_string(http);
			return false;
		}
		out.assign(body.begin(), body.end());
		return true;
	}

	void GraphCollectFilesRecursive(const std::string &accessToken, const std::string &folderId, const std::string &relPosix,
		std::vector<std::pair<std::string, std::string>> &outFileIdAndRel, std::string &err)
	{
		std::vector<nlohmann::json> items;
		if (!GraphListChildren(accessToken, folderId, items, err))
			return;
		for (const auto &it : items)
		{
			std::string name = it.value("name", std::string());
			std::string id = it.value("id", std::string());
			if (name.empty() || id.empty())
				continue;
			std::string nextRel = relPosix.empty() ? name : (relPosix + "/" + name);
			if (it.contains("folder"))
			{
				GraphCollectFilesRecursive(accessToken, id, nextRel, outFileIdAndRel, err);
			}
			else
			{
				outFileIdAndRel.push_back({ id, nextRel });
			}
		}
	}

	bool LoadPlayFabConfig(WindowsLeaderboardManager *wlm, nlohmann::json &cfg, std::string &err)
	{
		std::string fr;
		if (!wlm->ExecuteCloudScript(MINECRAFT_PLAYFAB_CLOUDSAVE_GET_CONFIG, "{}", fr, err))
			return false;
		try
		{
			cfg = nlohmann::json::parse(fr);
		}
		catch (...)
		{
			err = "GetConfig JSON";
			return false;
		}
		if (!cfg.value("enabled", false))
		{
			err = "Cloud save not linked (use LegacyDxncan Account Manager).";
			return false;
		}
		return true;
	}

	bool GetAccessTokenJson(WindowsLeaderboardManager *wlm, nlohmann::json &tok, std::string &err)
	{
		std::string fr;
		if (!wlm->ExecuteCloudScript(MINECRAFT_PLAYFAB_CLOUDSAVE_GET_TOKEN, "{}", fr, err))
			return false;
		try
		{
			tok = nlohmann::json::parse(fr);
		}
		catch (...)
		{
			err = "GetAccessToken JSON";
			return false;
		}
		if (!tok.contains("accessToken"))
		{
			err = "No accessToken in CloudScript result";
			return false;
		}
		return true;
	}

	void TouchLastSync(WindowsLeaderboardManager *wlm)
	{
		std::string fr, err;
		wlm->ExecuteCloudScript(MINECRAFT_PLAYFAB_CLOUDSAVE_SET_SYNC, "{}", fr, err);
		(void)fr;
		(void)err;
	}

	const size_t kLargeFileWarnBytes = 4u * 1024u * 1024u;

	/** OutputDebugStringA — use Sysinternals DebugView or VS Output (debug) to read. */
	void LogCloudSyncPhase(const char *phase, const std::string &worldUtf8, bool ok, const std::string &err)
	{
		char buf[1024];
		if (ok)
		{
			sprintf_s(buf, sizeof(buf), "[Win64CloudSync] %s OK '%s'\n", phase, worldUtf8.c_str());
			OutputDebugStringA(buf);
			return;
		}
		std::string e = err.empty() ? std::string("(no message)") : err;
		if (e.size() > 420)
			e = e.substr(0, 420) + "...";
		sprintf_s(buf, sizeof(buf), "[Win64CloudSync] %s '%s': %s\n", phase, worldUtf8.c_str(), e.c_str());
		OutputDebugStringA(buf);
	}

	uint64_t FileTimeToUnixMsUtc(const FILETIME &ft)
	{
		ULARGE_INTEGER uli;
		uli.LowPart = ft.dwLowDateTime;
		uli.HighPart = ft.dwHighDateTime;
		const uint64_t epoch1601Ms = uli.QuadPart / 10000ULL;
		const uint64_t unixEpochMs = 11644473600000ULL;
		if (epoch1601Ms < unixEpochMs)
			return 0;
		return epoch1601Ms - unixEpochMs;
	}

	inline void ConsiderMaxParsedTime(uint64_t parsedMs, uint64_t &outMax, bool &any)
	{
		if (parsedMs == 0)
			return;
		if (!any || parsedMs > outMax)
		{
			outMax = parsedMs;
			any = true;
		}
	}

	/** PlayFab / JS ISO string e.g. 2024-04-05T15:04:21.000Z — second precision is enough for sync decisions. */
	uint64_t ParseIso8601UtcToUnixMs(const std::string &iso)
	{
		if (iso.size() < 19)
			return 0;
		int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0;
		if (sscanf_s(iso.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se) != 6)
			return 0;
		struct tm t = {};
		t.tm_year = y - 1900;
		t.tm_mon = mo - 1;
		t.tm_mday = d;
		t.tm_hour = h;
		t.tm_min = mi;
		t.tm_sec = se;
		t.tm_isdst = 0;
		const time_t tt = _mkgmtime(&t);
		if (tt == (time_t)-1)
			return 0;
		return (uint64_t)tt * 1000ULL;
	}

	void ConsiderNewerFileTime(const FILETIME &candidate, FILETIME &best, bool &hasAny)
	{
		if (!hasAny || CompareFileTime(&candidate, &best) > 0)
		{
			best = candidate;
			hasAny = true;
		}
	}

	void GetLatestWriteTimeRecursive(const std::wstring &dirAbs, FILETIME &outBest, bool &hasAny)
	{
		std::wstring spec = dirAbs + L"\\*";
		WIN32_FIND_DATAW fd{};
		HANDLE h = FindFirstFileW(spec.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			return;
		do
		{
			if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
				continue;
			std::wstring full = dirAbs + L"\\" + fd.cFileName;
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				GetLatestWriteTimeRecursive(full, outBest, hasAny);
			}
			else
			{
				ConsiderNewerFileTime(fd.ftLastWriteTime, outBest, hasAny);
			}
		} while (FindNextFileW(h, &fd));
		FindClose(h);
	}

	bool GoogleMaxFileModifiedMsInTree(const std::string &accessToken, const std::string &folderId, uint64_t &outMax,
		bool &anyFile, std::string &err)
	{
		std::vector<nlohmann::json> files;
		if (!GoogleListChildren(accessToken, folderId, files, err))
			return false;
		for (const auto &f : files)
		{
			std::string mime = f.value("mimeType", std::string());
			std::string id = f.value("id", std::string());
			if (mime == "application/vnd.google-apps.folder")
			{
				if (!GoogleMaxFileModifiedMsInTree(accessToken, id, outMax, anyFile, err))
					return false;
			}
			else if (!id.empty())
			{
				ConsiderMaxParsedTime(ParseIso8601UtcToUnixMs(f.value("modifiedTime", std::string())), outMax, anyFile);
			}
		}
		return true;
	}

	bool DropboxMaxModifiedInPrefix(const std::string &accessToken, const std::string &pathPrefix, uint64_t &outMax,
		bool &anyFile, std::string &err)
	{
		outMax = 0;
		anyFile = false;
		std::vector<nlohmann::json> entries;
		if (!DropboxListAll(accessToken, pathPrefix, entries, err))
		{
			if (err.find("not_found") != std::string::npos || err.find("path_not_found") != std::string::npos)
			{
				err.clear();
				return true;
			}
			return false;
		}
		for (const auto &e : entries)
		{
			if (!e.contains(".tag") || e[".tag"] != "file")
				continue;
			ConsiderMaxParsedTime(ParseIso8601UtcToUnixMs(e.value("server_modified", std::string())), outMax, anyFile);
		}
		return true;
	}

	bool GraphMaxModifiedMsInTree(const std::string &accessToken, const std::string &itemId, uint64_t &outMax,
		bool &anyFile, std::string &err)
	{
		std::vector<nlohmann::json> items;
		if (!GraphListChildren(accessToken, itemId, items, err))
			return false;
		for (const auto &it : items)
		{
			std::string id = it.value("id", std::string());
			if (it.contains("folder"))
			{
				if (!GraphMaxModifiedMsInTree(accessToken, id, outMax, anyFile, err))
					return false;
			}
			else if (!id.empty())
			{
				ConsiderMaxParsedTime(ParseIso8601UtcToUnixMs(it.value("lastModifiedDateTime", std::string())), outMax,
					anyFile);
			}
		}
		return true;
	}

	bool RemoteWorldMaxModifiedUtcMs(const nlohmann::json &cfg, const std::string &accessToken, const std::string &seg,
		uint64_t &outMs, bool &hasRemoteFiles, std::string &err)
	{
		outMs = 0;
		hasRemoteFiles = false;
		std::string provider = cfg.value("provider", std::string());
		if (provider == "google")
		{
			std::string rootId = cfg.value("googleRootId", std::string());
			if (rootId.empty())
			{
				err = "Missing googleRootId.";
				return false;
			}
			std::string e2;
			const std::string worldId = GoogleFindChildFolderId(accessToken, rootId, seg, e2);
			if (worldId.empty())
			{
				err.clear();
				return true;
			}
			return GoogleMaxFileModifiedMsInTree(accessToken, worldId, outMs, hasRemoteFiles, err);
		}
		if (provider == "dropbox")
		{
			std::string rootBase = cfg.value("dropboxRootPath", std::string("/Minecraft LCE Cloud Saves"));
			if (rootBase.empty())
				rootBase = "/Minecraft LCE Cloud Saves";
			std::string prefix = rootBase + "/" + seg;
			std::replace(prefix.begin(), prefix.end(), '\\', '/');
			return DropboxMaxModifiedInPrefix(accessToken, prefix, outMs, hasRemoteFiles, err);
		}
		if (provider == "microsoft")
		{
			std::string rootItem = cfg.value("msRootId", std::string());
			if (rootItem.empty())
			{
				err = "Missing msRootId.";
				return false;
			}
			std::string e2;
			const std::string worldId = GraphFindChildFolderId(accessToken, rootItem, seg, e2);
			if (worldId.empty())
			{
				err.clear();
				return true;
			}
			return GraphMaxModifiedMsInTree(accessToken, worldId, outMs, hasRemoteFiles, err);
		}
		err = "Unknown cloud provider.";
		return false;
	}

	bool CollectCloudWorldFolderNames(const nlohmann::json &cfg, const std::string &accessToken,
		std::vector<std::string> &outNames, std::string &err)
	{
		outNames.clear();
		std::string provider = cfg.value("provider", std::string());
		if (provider == "google")
		{
			std::string rootId = cfg.value("googleRootId", std::string());
			if (rootId.empty())
			{
				err = "Missing googleRootId.";
				return false;
			}
			return GoogleListCloudWorldFolderNames(accessToken, rootId, outNames, err);
		}
		if (provider == "dropbox")
		{
			std::string rootBase = cfg.value("dropboxRootPath", std::string("/Minecraft LCE Cloud Saves"));
			if (rootBase.empty())
				rootBase = "/Minecraft LCE Cloud Saves";
			return DropboxListImmediateChildFolderNames(accessToken, rootBase, outNames, err);
		}
		if (provider == "microsoft")
		{
			std::string rootItem = cfg.value("msRootId", std::string());
			if (rootItem.empty())
			{
				err = "Missing msRootId.";
				return false;
			}
			return GraphListCloudWorldFolderNames(accessToken, rootItem, outNames, err);
		}
		err = "Unknown cloud provider.";
		return false;
	}

	std::string LocalFolderNameForSeg(const std::string &seg, const std::vector<std::string> &localFolders)
	{
		for (const auto &L : localFolders)
		{
			if (SanitizeSegment(L.c_str()) == seg)
				return L;
		}
		return seg;
	}

	void MergeWorldSegs(const std::vector<std::string> &localFolders, const std::vector<std::string> &cloudWorldNames,
		std::vector<std::string> &outSegsSorted)
	{
		std::set<std::string> segset;
		for (const auto &L : localFolders)
			segset.insert(SanitizeSegment(L.c_str()));
		for (const auto &c : cloudWorldNames)
			segset.insert(c);
		outSegsSorted.assign(segset.begin(), segset.end());
	}

	void EnumerateGameHddSaveFolderNames(std::vector<std::string> &outUtf8Folders)
	{
		outUtf8Folders.clear();
		std::wstring root = Win64_GetGameHddRootW();
		std::wstring spec = root + L"\\*";
		WIN32_FIND_DATAW fd{};
		HANDLE h = FindFirstFileW(spec.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			return;
		do
		{
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				continue;
			if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
				continue;
			std::wstring marker = root + L"\\" + fd.cFileName + L"\\saveData.ms";
			DWORD a = GetFileAttributesW(marker.c_str());
			if (a == INVALID_FILE_ATTRIBUTES || (a & FILE_ATTRIBUTE_DIRECTORY))
				continue;
			outUtf8Folders.push_back(WideToUtf8(std::wstring(fd.cFileName)));
		} while (FindNextFileW(h, &fd));
		FindClose(h);
	}

	void TitleScreenBackgroundSyncWorker()
	{
		WindowsLeaderboardManager *wlm = Wlm();
		std::string err;
		if (!wlm || !wlm->PlayFabEnabled())
			return;
		if (!wlm->EnsureLoggedIn(err))
			return;

		nlohmann::json cfg;
		if (!LoadPlayFabConfig(wlm, cfg, err))
			return;

		nlohmann::json tok;
		if (!GetAccessTokenJson(wlm, tok, err))
			return;
		const std::string accessToken = tok["accessToken"].get<std::string>();

		/* Compare newest local file mtime vs newest cloud file modified time (per provider). */
		const uint64_t kSlackMs = 3000;

		std::vector<std::string> localFolders;
		EnumerateGameHddSaveFolderNames(localFolders);

		std::vector<std::string> cloudWorlds;
		std::string errCloud;
		if (!CollectCloudWorldFolderNames(cfg, accessToken, cloudWorlds, errCloud))
		{
			std::string em = errCloud.size() > 400 ? errCloud.substr(0, 400) + "..." : errCloud;
			OutputDebugStringA(("[Win64CloudSync] Cloud root listing failed (continuing with local saves only): " + em + "\n").c_str());
			cloudWorlds.clear();
		}

		std::vector<std::string> allSegs;
		MergeWorldSegs(localFolders, cloudWorlds, allSegs);

		char dbg[224];
		sprintf_s(dbg, "[Win64CloudSync] Title-screen sync: %zu local save(s), %zu cloud world folder(s), %zu to reconcile\n",
			localFolders.size(), cloudWorlds.size(), allSegs.size());
		OutputDebugStringA(dbg);

		if (allSegs.empty())
		{
			OutputDebugStringA(
				"[Win64CloudSync] Title-screen sync: no local saves and no world folders under cloud root.\n");
			if (cfg.value("provider", std::string()) == "google")
			{
				OutputDebugStringA(
					"[Win64CloudSync] Google: put each world as a direct subfolder inside \"Minecraft LCE Cloud Saves\" "
					"(the folder linked in PlayFab). If you use drive.google.com, OAuth must use scope "
					"https://www.googleapis.com/auth/drive (not drive.file) or the API cannot see manual folders.\n");
			}
			return;
		}

		for (const std::string &seg : allSegs)
		{
			const std::string localName = LocalFolderNameForSeg(seg, localFolders);

			std::wstring localRoot = Win64_GetGameHddRootW() + L"\\" + Utf8ToWide(localName);
			FILETIME bestLocalWrite{};
			bool hasLocalTime = false;
			GetLatestWriteTimeRecursive(localRoot, bestLocalWrite, hasLocalTime);
			const uint64_t localMs = hasLocalTime ? FileTimeToUnixMsUtc(bestLocalWrite) : 0;

			uint64_t remoteMs = 0;
			bool hasRemoteFiles = false;
			std::string eMeta;
			if (!RemoteWorldMaxModifiedUtcMs(cfg, accessToken, seg, remoteMs, hasRemoteFiles, eMeta))
			{
				LogCloudSyncPhase("Meta", localName, false, eMeta);
				continue;
			}

			if (!hasRemoteFiles)
			{
				if (hasLocalTime && localMs > 0)
				{
					std::string eUp;
					const bool upOk = Win64CloudSync::UploadSaveFolder(localName.c_str(), eUp);
					LogCloudSyncPhase("Upload", localName, upOk, eUp);
				}
				else
				{
					char note[256];
					sprintf_s(note, "[Win64CloudSync] Skip '%s' (no cloud files yet)\n", localName.c_str());
					OutputDebugStringA(note);
				}
				continue;
			}

			if (localMs > remoteMs + kSlackMs)
			{
				std::string eUp;
				const bool upOk = Win64CloudSync::UploadSaveFolder(localName.c_str(), eUp);
				LogCloudSyncPhase("Upload", localName, upOk, eUp);
			}
			else if (remoteMs > localMs + kSlackMs)
			{
				std::string eDown;
				const bool downOk = Win64CloudSync::DownloadSaveFolder(localName.c_str(), eDown);
				LogCloudSyncPhase("Download", localName, downOk, eDown);
			}
			else
			{
				char note[320];
				sprintf_s(note, "[Win64CloudSync] Skip '%s' (local and cloud in sync)\n", localName.c_str());
				OutputDebugStringA(note);
			}
		}
	}

	std::atomic_bool g_titleCloudSyncRunning{false};
	std::atomic<DWORD> g_titleCloudSyncLastCompleteTick{0};

	void PollTitleScreenCloudSync(uint32_t tickNow)
	{
		const DWORD kMinIntervalMs = 60000;

		if (!Win64CloudSync::IsPlayFabCloudSaveAvailable())
			return;

		const bool allow =
			ProfileManager.IsFullVersion() && !StorageManager.GetSaveDisabled();
		if (!allow)
			return;

		bool expected = false;
		if (!g_titleCloudSyncRunning.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
			return;

		const DWORD last = g_titleCloudSyncLastCompleteTick.load(std::memory_order_relaxed);
		if (last != 0)
		{
			const DWORD dt = (tickNow >= last) ? (tickNow - last) : (0xFFFFFFFFu - last + tickNow + 1);
			if (dt < kMinIntervalMs)
			{
				g_titleCloudSyncRunning.store(false, std::memory_order_release);
				return;
			}
		}

		std::thread([]() {
			TitleScreenBackgroundSyncWorker();
			g_titleCloudSyncLastCompleteTick.store(GetTickCount(), std::memory_order_relaxed);
			g_titleCloudSyncRunning.store(false, std::memory_order_release);
		}).detach();
	}
}

bool Win64CloudSync::IsPlayFabCloudSaveAvailable()
{
	WindowsLeaderboardManager *wlm = Wlm();
	return wlm != nullptr && wlm->PlayFabEnabled();
}

bool Win64CloudSync::UploadSaveFolder(const char *utf8SaveFolderName, std::string &errOut)
{
	errOut.clear();
	WindowsLeaderboardManager *wlm = Wlm();
	if (!wlm || !wlm->PlayFabEnabled())
	{
		errOut = "PlayFab not configured.";
		return false;
	}
	std::string e;
	if (!wlm->EnsureLoggedIn(e))
	{
		errOut = e;
		return false;
	}

	nlohmann::json cfg;
	if (!LoadPlayFabConfig(wlm, cfg, errOut))
		return false;

	nlohmann::json tok;
	if (!GetAccessTokenJson(wlm, tok, errOut))
		return false;

	std::string accessToken = tok["accessToken"].get<std::string>();
	std::string provider = cfg.value("provider", std::string());

	std::wstring localRoot = Win64_GetGameHddRootW() + L"\\" + Utf8ToWide(std::string(utf8SaveFolderName));
	DWORD attr = GetFileAttributesW(localRoot.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
	{
		errOut = "Local save folder not found.";
		return false;
	}

	std::vector<LocalFileRec> locals;
	CollectLocalFilesRecursive(localRoot, L"", locals);
	std::string seg = SanitizeSegment(utf8SaveFolderName);

	if (provider == "dropbox")
	{
		std::string rootBase = cfg.value("dropboxRootPath", std::string("/Minecraft LCE Cloud Saves"));
		if (rootBase.empty())
			rootBase = "/Minecraft LCE Cloud Saves";
		for (const auto &lf : locals)
		{
			std::vector<unsigned char> bytes;
			if (!ReadWholeFileW(lf.absPath, bytes, errOut))
				return false;
			if (bytes.size() > 150u * 1024u * 1024u)
			{
				errOut = "File too large for Dropbox upload.";
				return false;
			}
			std::string dbPath = rootBase + "/" + seg + "/" + lf.relPosix;
			std::replace(dbPath.begin(), dbPath.end(), '\\', '/');
			if (!DropboxUpload(accessToken, dbPath, bytes, errOut))
				return false;
		}
		TouchLastSync(wlm);
		return true;
	}

	if (provider == "google")
	{
		std::string rootId = cfg.value("googleRootId", std::string());
		if (rootId.empty())
		{
			errOut = "Missing googleRootId.";
			return false;
		}
		std::string worldFolderId = GoogleEnsureChildFolder(accessToken, rootId, seg, errOut);
		if (worldFolderId.empty())
			return false;

		for (const auto &lf : locals)
		{
			std::vector<unsigned char> bytes;
			if (!ReadWholeFileW(lf.absPath, bytes, errOut))
				return false;
			size_t slash = lf.relPosix.find_last_of('/');
			std::string leaf = slash == std::string::npos ? lf.relPosix : lf.relPosix.substr(slash + 1);
			if (leaf.empty())
			{
				errOut = "Empty file name in save folder.";
				return false;
			}
			/* <= 4 MiB: multipart media upload + move into world folder + rename (legacy path). */
			if (bytes.size() > kLargeFileWarnBytes)
			{
				if (!GoogleDriveUploadResumable(accessToken, worldFolderId, leaf, bytes, errOut))
					return false;
			}
			else
			{
				std::string fileId, parent0;
				if (!GoogleUploadMedia(accessToken, bytes, fileId, parent0, errOut))
					return false;
				if (!GooglePatchParents(accessToken, fileId, worldFolderId, parent0, errOut))
					return false;
				if (!GooglePatchName(accessToken, fileId, leaf, errOut))
					return false;
			}
		}
		TouchLastSync(wlm);
		return true;
	}

	if (provider == "microsoft")
	{
		std::string rootItem = cfg.value("msRootId", std::string());
		if (rootItem.empty())
		{
			errOut = "Missing msRootId.";
			return false;
		}
		std::string worldFolderId = GraphEnsureChildFolder(accessToken, rootItem, seg, errOut);
		if (worldFolderId.empty())
			return false;

		for (const auto &lf : locals)
		{
			std::vector<unsigned char> bytes;
			if (!ReadWholeFileW(lf.absPath, bytes, errOut))
				return false;
			if (bytes.size() > kLargeFileWarnBytes)
			{
				errOut = "A file exceeds 4 MiB; OneDrive simple upload path not used for large files. Use Dropbox.";
				return false;
			}
			// mirror subdirs: ensure nested Graph folders
			std::string rel = lf.relPosix;
			size_t pos = 0;
			std::string parentId = worldFolderId;
			for (;;)
			{
				size_t sl = rel.find('/', pos);
				if (sl == std::string::npos)
					break;
				std::string part = rel.substr(pos, sl - pos);
				if (!part.empty())
				{
					parentId = GraphEnsureChildFolder(accessToken, parentId, part, errOut);
					if (parentId.empty())
						return false;
				}
				pos = sl + 1;
			}
			std::string leaf = rel.substr(pos);
			if (!GraphPutContent(accessToken, parentId, leaf, bytes, errOut))
				return false;
		}
		TouchLastSync(wlm);
		return true;
	}

	errOut = "Unknown cloud provider.";
	return false;
}

bool Win64CloudSync::DownloadSaveFolder(const char *utf8SaveFolderName, std::string &errOut)
{
	errOut.clear();
	WindowsLeaderboardManager *wlm = Wlm();
	if (!wlm || !wlm->PlayFabEnabled())
	{
		errOut = "PlayFab not configured.";
		return false;
	}
	std::string e;
	if (!wlm->EnsureLoggedIn(e))
	{
		errOut = e;
		return false;
	}

	nlohmann::json cfg;
	if (!LoadPlayFabConfig(wlm, cfg, errOut))
		return false;

	nlohmann::json tok;
	if (!GetAccessTokenJson(wlm, tok, errOut))
		return false;

	std::string accessToken = tok["accessToken"].get<std::string>();
	std::string provider = cfg.value("provider", std::string());
	std::string seg = SanitizeSegment(utf8SaveFolderName);

	std::wstring localRoot = Win64_GetGameHddRootW() + L"\\" + Utf8ToWide(std::string(utf8SaveFolderName));

	if (provider == "dropbox")
	{
		std::string rootBase = cfg.value("dropboxRootPath", std::string("/Minecraft LCE Cloud Saves"));
		if (rootBase.empty())
			rootBase = "/Minecraft LCE Cloud Saves";
		std::string remotePrefix = rootBase + "/" + seg;
		std::replace(remotePrefix.begin(), remotePrefix.end(), '\\', '/');

		std::vector<nlohmann::json> all;
		if (!DropboxListAll(accessToken, remotePrefix, all, errOut))
			return false;

		for (const auto &ent : all)
		{
			if (!ent.contains(".tag") || ent[".tag"] != "file")
				continue;
			std::string rp = ent.value("path_display", ent.value("path_lower", std::string()));
			if (rp.size() <= remotePrefix.size())
				continue;
			std::string tail = rp.substr(remotePrefix.size());
			while (!tail.empty() && (tail[0] == '/' || tail[0] == '\\'))
				tail.erase(0, 1);
			if (tail.empty())
				continue;
			for (auto &c : tail)
			{
				if (c == '\\')
					c = '/';
			}
			std::wstring outPath = localRoot + L"\\" + Utf8ToWide(tail);
			std::replace(outPath.begin(), outPath.end(), L'/', L'\\');

			std::vector<unsigned char> bytes;
			if (!DropboxDownload(accessToken, rp, bytes, errOut))
				return false;
			if (!WriteWholeFileW(outPath, bytes.data(), bytes.size(), errOut))
				return false;
		}
		TouchLastSync(wlm);
		return true;
	}

	if (provider == "google")
	{
		std::string rootId = cfg.value("googleRootId", std::string());
		if (rootId.empty())
		{
			errOut = "Missing googleRootId.";
			return false;
		}
		std::string worldFolderId = GoogleFindChildFolderId(accessToken, rootId, seg, errOut);
		if (worldFolderId.empty())
		{
			errOut = "No matching cloud folder for this world.";
			return false;
		}

		std::vector<std::pair<std::string, std::string>> stack;
		if (!CollectGoogleDriveFilesRecursive(accessToken, worldFolderId, "", stack, errOut))
			return false;

		for (const auto &g : stack)
		{
			std::vector<unsigned char> bytes;
			if (!GoogleDownloadMedia(accessToken, g.first, bytes, errOut))
				return false;
			std::wstring outPath = localRoot + L"\\" + Utf8ToWide(g.second);
			std::replace(outPath.begin(), outPath.end(), L'/', L'\\');
			if (!WriteWholeFileW(outPath, bytes.data(), bytes.size(), errOut))
				return false;
		}
		TouchLastSync(wlm);
		return true;
	}

	if (provider == "microsoft")
	{
		std::string rootItem = cfg.value("msRootId", std::string());
		if (rootItem.empty())
		{
			errOut = "Missing msRootId.";
			return false;
		}
		std::string worldFolderId = GraphFindChildFolderId(accessToken, rootItem, seg, errOut);
		if (worldFolderId.empty())
		{
			errOut = "No matching cloud folder for this world.";
			return false;
		}
		std::vector<std::pair<std::string, std::string>> files;
		GraphCollectFilesRecursive(accessToken, worldFolderId, "", files, errOut);
		if (!errOut.empty())
			return false;

		for (const auto &pr : files)
		{
			std::vector<unsigned char> bytes;
			if (!GraphDownloadItem(accessToken, pr.first, bytes, errOut))
				return false;
			std::wstring outPath = localRoot + L"\\" + Utf8ToWide(pr.second);
			std::replace(outPath.begin(), outPath.end(), L'/', L'\\');
			if (!WriteWholeFileW(outPath, bytes.data(), bytes.size(), errOut))
				return false;
		}
		TouchLastSync(wlm);
		return true;
	}

	errOut = "Unknown cloud provider.";
	return false;
}

void Win64CloudSync::TickTitleScreenCloudSync(uint32_t tickNow)
{
	PollTitleScreenCloudSync(tickNow);
}

#endif // _WINDOWS64

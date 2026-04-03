#include "stdafx.h"
#include "../Leaderboards/PlayFabConfig.h"

#if defined(_UWP) && MINECRAFT_PLAYFAB_LOBBY_UPNP

#include "UpnpIgdUwp.h"

#include <winhttp.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <ipifcons.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

void LogTrace(const char *fmt, ...);

namespace
{
	static void UpnpUwpDiag(const char *fmt, ...)
	{
		char buf[900];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		buf[sizeof(buf) - 1] = '\0';
#if defined(_FINAL_BUILD)
		LogTrace("%s", buf);
#else
		app.DebugPrintf("%s", buf);
#endif
	}

	static void CollectDefaultGateways(std::vector<std::string> &out)
	{
		out.clear();
		std::unordered_set<std::string> seen;
		ULONG bufLen = sizeof(IP_ADAPTER_INFO);
		std::vector<BYTE> buf(bufLen);
		auto *info = reinterpret_cast<PIP_ADAPTER_INFO>(buf.data());
		if (GetAdaptersInfo(info, &bufLen) == ERROR_BUFFER_OVERFLOW)
		{
			buf.resize(bufLen);
			info = reinterpret_cast<PIP_ADAPTER_INFO>(buf.data());
		}
		if (GetAdaptersInfo(info, &bufLen) != NO_ERROR)
			return;

		for (PIP_ADAPTER_INFO a = info; a != nullptr; a = a->Next)
		{
			if (a->Type == MIB_IF_TYPE_LOOPBACK)
				continue;
			for (PIP_ADDR_STRING gw = &a->GatewayList; gw != nullptr; gw = gw->Next)
			{
				const char *g = gw->IpAddress.String;
				if (g == nullptr || g[0] == 0 || strcmp(g, "0.0.0.0") == 0)
					continue;
				if (seen.insert(g).second)
					out.emplace_back(g);
			}
		}
	}
	std::mutex g_stateMu;
	std::atomic<int> g_mappedPort{0};
	std::string g_controlUrl;
	std::string g_serviceUrn;

	static void TrimInPlace(std::string &s)
	{
		while (!s.empty() && (unsigned char)s.front() <= ' ')
			s.erase(s.begin());
		while (!s.empty() && (unsigned char)s.back() <= ' ')
			s.pop_back();
	}

	static std::wstring Utf8ToWide(const char *utf8)
	{
		if (utf8 == nullptr || utf8[0] == 0)
			return std::wstring();
		const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		if (n <= 0)
			return std::wstring();
		std::wstring w((size_t)n - 1, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], n);
		return w;
	}

	static bool LooksLikeIpv4(const char *s)
	{
		unsigned a = 0, b = 0, c = 0, d = 0;
		return s != nullptr && sscanf_s(s, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 && a <= 255 && b <= 255 && c <= 255 && d <= 255;
	}

	static bool SplitHttpUrl(const std::string &url, bool &https, std::string &host, INTERNET_PORT &port, std::string &path)
	{
		https = false;
		host.clear();
		path = "/";
		size_t i = 0;
		if (url.size() >= 7 && _strnicmp(url.c_str(), "http://", 7) == 0)
		{
			i = 7;
			https = false;
		}
		else if (url.size() >= 8 && _strnicmp(url.c_str(), "https://", 8) == 0)
		{
			i = 8;
			https = true;
		}
		else
			return false;

		const size_t slash = url.find('/', i);
		std::string hostport = slash == std::string::npos ? url.substr(i) : url.substr(i, slash - i);
		path = slash == std::string::npos ? "/" : url.substr(slash);
		if (hostport.empty())
			return false;

		const size_t colon = hostport.rfind(':');
		if (colon != std::string::npos && colon + 1 < hostport.size())
		{
			// IPv6 in brackets [::1] — skip colon inside brackets
			const bool v6 = !hostport.empty() && hostport.front() == '[';
			if (!v6)
			{
				host = hostport.substr(0, colon);
				const int p = atoi(hostport.substr(colon + 1).c_str());
				if (p <= 0 || p > 65535)
					return false;
				port = (INTERNET_PORT)p;
				return !host.empty();
			}
		}
		host = hostport;
		port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
		return true;
	}

	static std::string ResolveControlUrl(const std::string &location, const std::string &controlRel)
	{
		if (controlRel.empty())
			return std::string();
		if (controlRel.size() >= 7 && _strnicmp(controlRel.c_str(), "http://", 7) == 0)
			return controlRel;
		if (controlRel.size() >= 8 && _strnicmp(controlRel.c_str(), "https://", 8) == 0)
			return controlRel;

		bool locHttps = false;
		std::string locHost;
		INTERNET_PORT locPort = 80;
		std::string locPath;
		if (!SplitHttpUrl(location, locHttps, locHost, locPort, locPath))
			return std::string();

		std::string baseDir = "/";
		if (!locPath.empty() && locPath != "/")
		{
			const size_t lastSlash = locPath.rfind('/');
			if (lastSlash != std::string::npos)
				baseDir = locPath.substr(0, lastSlash + 1);
		}

		std::string rel = controlRel;
		TrimInPlace(rel);
		std::string combined;
		if (!rel.empty() && rel[0] == '/')
			combined = rel;
		else
			combined = baseDir + rel;

		char portBuf[16];
		const bool defPort = (!locHttps && locPort == INTERNET_DEFAULT_HTTP_PORT) || (locHttps && locPort == INTERNET_DEFAULT_HTTPS_PORT);
		if (defPort)
			portBuf[0] = '\0';
		else
			sprintf_s(portBuf, ":%u", (unsigned)locPort);

		std::string out;
		out.reserve(64 + locHost.size() + combined.size());
		out += locHttps ? "https://" : "http://";
		out += locHost;
		out += portBuf;
		out += combined;
		return out;
	}

	static bool WinHttpDownload(const std::wstring &whost, INTERNET_PORT port, bool https, const std::wstring &wpath,
		const wchar_t *verb, const std::string *optionalUtf8Body, const wchar_t *extraHeaders,
		std::string &outBody, std::string &err)
	{
		outBody.clear();
		err.clear();

		HINTERNET hSession = WinHttpOpen(L"MinecraftConsoles-UPnP/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSession)
		{
			err = "WinHttpOpen";
			return false;
		}

		DWORD prot = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
		WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &prot, sizeof(prot));

		HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), port, 0);
		if (!hConnect)
		{
			WinHttpCloseHandle(hSession);
			err = "WinHttpConnect";
			return false;
		}

		DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
		HINTERNET hRequest = WinHttpOpenRequest(hConnect, verb, wpath.c_str(), nullptr, WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!hRequest)
		{
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			err = "WinHttpOpenRequest";
			return false;
		}

		const wchar_t *hdr = extraHeaders ? extraHeaders : WINHTTP_NO_ADDITIONAL_HEADERS;
		const void *bodyPtr = WINHTTP_NO_REQUEST_DATA;
		DWORD bodyLen = 0;
		if (optionalUtf8Body != nullptr && !optionalUtf8Body->empty())
		{
			bodyPtr = optionalUtf8Body->data();
			bodyLen = (DWORD)optionalUtf8Body->size();
		}

		if (!WinHttpSendRequest(hRequest, hdr, hdr == WINHTTP_NO_ADDITIONAL_HEADERS ? 0 : (DWORD)-1, (LPVOID)bodyPtr, bodyLen,
				bodyLen, 0)
			|| !WinHttpReceiveResponse(hRequest, nullptr))
		{
			err = "WinHttpSendRequest/ReceiveResponse";
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		DWORD status = 0;
		DWORD sz = sizeof(status);
		WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz,
			WINHTTP_NO_HEADER_INDEX);
		if (status < 200 || status >= 300)
		{
			char e[64];
			sprintf_s(e, "HTTP %u", (unsigned)status);
			err = e;
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		for (;;)
		{
			char buf[4096];
			DWORD read = 0;
			if (!WinHttpReadData(hRequest, buf, sizeof(buf), &read) || read == 0)
				break;
			outBody.append(buf, read);
		}

		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return true;
	}

	static bool HttpGetUrl(const std::string &url, std::string &body, std::string &err)
	{
		bool https = false;
		std::string host;
		INTERNET_PORT port = 80;
		std::string path;
		if (!SplitHttpUrl(url, https, host, port, path))
		{
			err = "bad URL";
			return false;
		}
		const std::wstring whost = Utf8ToWide(host.c_str());
		const std::wstring wpath = Utf8ToWide(path.c_str());
		return WinHttpDownload(whost, port, https, wpath, L"GET", nullptr, nullptr, body, err);
	}

	static bool SoapPostUrl(const std::string &url, const std::string &soapAction, const std::string &soapBody, std::string &resp,
		std::string &err)
	{
		bool https = false;
		std::string host;
		INTERNET_PORT port = 80;
		std::string path;
		if (!SplitHttpUrl(url, https, host, port, path))
		{
			err = "bad SOAP URL";
			return false;
		}
		const std::wstring whost = Utf8ToWide(host.c_str());
		const std::wstring wpath = Utf8ToWide(path.c_str());

		std::string headersUtf8;
		headersUtf8 = "Content-Type: text/xml; charset=utf-8\r\nSOAPAction: \"";
		headersUtf8 += soapAction;
		headersUtf8 += "\"\r\n";
		const std::wstring wheaders = Utf8ToWide(headersUtf8.c_str());

		return WinHttpDownload(whost, port, https, wpath, L"POST", &soapBody, wheaders.c_str(), resp, err);
	}

	static bool ExtractTagContent(const std::string &chunk, const char *tag, std::string &out)
	{
		out.clear();
		const std::string open = std::string("<") + tag + ">";
		const std::string openSp = std::string("<") + tag + " ";
		size_t pos = chunk.find(open);
		if (pos == std::string::npos)
		{
			pos = chunk.find(openSp);
			if (pos == std::string::npos)
				return false;
			const size_t gt = chunk.find('>', pos);
			if (gt == std::string::npos)
				return false;
			pos = gt + 1;
		}
		else
			pos += open.size();

		const std::string close = std::string("</") + tag + ">";
		const size_t end = chunk.find(close, pos);
		if (end == std::string::npos)
			return false;
		out = chunk.substr(pos, end - pos);
		TrimInPlace(out);
		return !out.empty();
	}

	static bool ExtractWanService(const std::string &xml, std::string &serviceUrn, std::string &controlRel)
	{
		serviceUrn.clear();
		controlRel.clear();
		size_t scan = 0;
		while (scan < xml.size())
		{
			const size_t svcStart = xml.find("<service", scan);
			if (svcStart == std::string::npos)
				break;
			const size_t svcEnd = xml.find("</service>", svcStart);
			if (svcEnd == std::string::npos)
				break;
			const std::string chunk = xml.substr(svcStart, svcEnd - svcStart);

			std::string stype;
			if (!ExtractTagContent(chunk, "serviceType", stype))
			{
				scan = svcEnd + 10;
				continue;
			}

			const bool wanIp = stype.find("WANIPConnection") != std::string::npos;
			const bool wanPpp = stype.find("WANPPPConnection") != std::string::npos;
			if (!wanIp && !wanPpp)
			{
				scan = svcEnd + 10;
				continue;
			}

			std::string curl;
			if (!ExtractTagContent(chunk, "controlURL", curl))
			{
				scan = svcEnd + 10;
				continue;
			}

			serviceUrn = stype;
			controlRel = curl;
			return true;
		}
		return false;
	}

	static bool ExtractSoapString(const std::string &resp, const char *tag, std::string &out)
	{
		// Namespaced responses: find tag name after optional prefix
		const std::string needle = tag;
		size_t p = 0;
		while ((p = resp.find(needle, p)) != std::string::npos)
		{
			size_t gt = resp.find('>', p);
			if (gt == std::string::npos)
				break;
			// skip if this is closing tag
			if (gt > 0 && resp[gt - 1] == '/')
			{
				p = gt + 1;
				continue;
			}
			const size_t valStart = gt + 1;
			const size_t lt = resp.find('<', valStart);
			if (lt == std::string::npos)
				break;
			out = resp.substr(valStart, lt - valStart);
			TrimInPlace(out);
			if (!out.empty())
				return true;
			p = lt + 1;
		}
		return false;
	}

	static bool SoapGetExternalIp(const std::string &controlUrl, const std::string &urn, std::string &ipv4, std::string &err)
	{
		std::string soapAction = urn + "#GetExternalIPAddress";
		char body[2048];
		sprintf_s(body,
			"<?xml version=\"1.0\"?>\r\n"
			"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
			"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
			"<s:Body>"
			"<u:GetExternalIPAddress xmlns:u=\"%s\"/>"
			"</s:Body></s:Envelope>",
			urn.c_str());

		std::string resp;
		if (!SoapPostUrl(controlUrl, soapAction, body, resp, err))
			return false;

		if (resp.find("Fault") != std::string::npos || resp.find("fault") != std::string::npos)
		{
			err = "GetExternalIPAddress SOAP fault";
			return false;
		}

		if (!ExtractSoapString(resp, "NewExternalIPAddress", ipv4))
		{
			err = "missing NewExternalIPAddress";
			return false;
		}
		return true;
	}

	static bool SoapAddPortMapping(const std::string &controlUrl, const std::string &urn, int extPort, const char *lanIp,
		std::string &err)
	{
		std::string soapAction = urn + "#AddPortMapping";
		char body[4096];
		sprintf_s(body,
			"<?xml version=\"1.0\"?>\r\n"
			"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
			"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
			"<s:Body>"
			"<u:AddPortMapping xmlns:u=\"%s\">"
			"<NewRemoteHost></NewRemoteHost>"
			"<NewExternalPort>%d</NewExternalPort>"
			"<NewProtocol>TCP</NewProtocol>"
			"<NewInternalPort>%d</NewInternalPort>"
			"<NewInternalClient>%s</NewInternalClient>"
			"<NewEnabled>1</NewEnabled>"
			"<NewPortMappingDescription>MinecraftConsoles</NewPortMappingDescription>"
			"<NewLeaseDuration>0</NewLeaseDuration>"
			"</u:AddPortMapping>"
			"</s:Body></s:Envelope>",
			urn.c_str(), extPort, extPort, lanIp);

		std::string resp;
		if (!SoapPostUrl(controlUrl, soapAction, body, resp, err))
			return false;

		if (resp.find("Fault") != std::string::npos || resp.find("fault") != std::string::npos)
		{
			err = "AddPortMapping SOAP fault";
			return false;
		}
		return true;
	}

	static void SoapDeletePortMappingIgnore(const std::string &controlUrl, const std::string &urn, int extPort)
	{
		std::string soapAction = urn + "#DeletePortMapping";
		char body[2048];
		sprintf_s(body,
			"<?xml version=\"1.0\"?>\r\n"
			"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
			"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
			"<s:Body>"
			"<u:DeletePortMapping xmlns:u=\"%s\">"
			"<NewRemoteHost></NewRemoteHost>"
			"<NewExternalPort>%d</NewExternalPort>"
			"<NewProtocol>TCP</NewProtocol>"
			"</u:DeletePortMapping>"
			"</s:Body></s:Envelope>",
			urn.c_str(), extPort);

		std::string resp, err;
		SoapPostUrl(controlUrl, soapAction, body, resp, err);
	}

	static void SsdpSendSearch(SOCKET s, const sockaddr_in &to, const char *hostHdr, const char *st)
	{
		char msg[640];
		sprintf_s(msg,
			"M-SEARCH * HTTP/1.1\r\n"
			"HOST: %s\r\n"
			"MAN: \"ssdp:discover\"\r\n"
			"MX: 2\r\n"
			"ST: %s\r\n"
			"\r\n",
			hostHdr, st);
		sendto(s, msg, (int)strlen(msg), 0, (const sockaddr *)&to, sizeof(to));
	}

	static void SsdpSendMulticast(SOCKET s, const char *st)
	{
		sockaddr_in mcast{};
		mcast.sin_family = AF_INET;
		mcast.sin_port = htons(1900);
		inet_pton(AF_INET, "239.255.255.250", &mcast.sin_addr);
		SsdpSendSearch(s, mcast, "239.255.255.250:1900", st);
	}

	// Xbox / UWP often block SSDP multicast; many routers still answer unicast M-SEARCH to the default gateway.
	static void SsdpSendToGateway(SOCKET s, const char *gatewayIpv4, const char *st)
	{
		sockaddr_in to{};
		to.sin_family = AF_INET;
		to.sin_port = htons(1900);
		if (inet_pton(AF_INET, gatewayIpv4, &to.sin_addr) != 1)
			return;
		char hostHdr[96];
		sprintf_s(hostHdr, "%s:1900", gatewayIpv4);
		SsdpSendSearch(s, to, hostHdr, st);
	}

	static bool SsdpDiscover(std::vector<std::string> &locations, std::string &err)
	{
		locations.clear();
		err.clear();

		std::vector<std::string> gateways;
		CollectDefaultGateways(gateways);

		SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (s == INVALID_SOCKET)
		{
			err = "socket";
			return false;
		}

		BOOL b = TRUE;
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&b, sizeof(b));

		sockaddr_in bindAddr{};
		bindAddr.sin_family = AF_INET;
		bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		bindAddr.sin_port = 0;
		if (::bind(s, (sockaddr *)&bindAddr, sizeof(bindAddr)) != 0)
		{
			err = "bind";
			closesocket(s);
			return false;
		}

		DWORD sliceMs = 400;
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&sliceMs, sizeof(sliceMs));

		static const char *const sts[] = {
			"urn:schemas-upnp-org:device:InternetGatewayDevice:1",
			"urn:schemas-upnp-org:device:InternetGatewayDevice:2",
			"urn:schemas-upnp-org:service:WANIPConnection:1",
			"urn:schemas-upnp-org:service:WANIPConnection:2",
		};

		for (const std::string &gw : gateways)
		{
			for (const char *st : sts)
				SsdpSendToGateway(s, gw.c_str(), st);
			SsdpSendToGateway(s, gw.c_str(), "ssdp:all");
		}

		for (const char *st : sts)
			SsdpSendMulticast(s, st);
		SsdpSendMulticast(s, "ssdp:all");

		char buf[4096];
		sockaddr_in from{};
		int fromlen = sizeof(from);
		const DWORD deadline = GetTickCount() + 4000;
		while (GetTickCount() < deadline)
		{
			fromlen = sizeof(from);
			const int n = recvfrom(s, buf, sizeof(buf) - 1, 0, (sockaddr *)&from, &fromlen);
			if (n <= 0)
				continue;
			buf[n] = 0;

			std::string response(buf);
			std::string loc;
			size_t lineStart = 0;
			while (lineStart < response.size())
			{
				size_t lineEnd = response.find("\r\n", lineStart);
				if (lineEnd == std::string::npos)
					lineEnd = response.size();
				std::string line = response.substr(lineStart, lineEnd - lineStart);
				TrimInPlace(line);
				if (line.size() > 9 && _strnicmp(line.c_str(), "LOCATION:", 9) == 0)
				{
					loc = line.substr(9);
					TrimInPlace(loc);
					break;
				}
				lineStart = lineEnd + 2;
			}
			if (!loc.empty())
			{
				const bool dup = std::find(locations.begin(), locations.end(), loc) != locations.end();
				if (!dup)
					locations.push_back(loc);
			}
		}

		closesocket(s);
		return !locations.empty();
	}
} // namespace

void UpnpIgdUwp_RemoveMappingIfAny()
{
	std::string url, urn;
	int port = 0;
	{
		std::lock_guard<std::mutex> lock(g_stateMu);
		port = g_mappedPort.exchange(0);
		url = std::move(g_controlUrl);
		urn = std::move(g_serviceUrn);
	}
	if (port <= 0 || url.empty() || urn.empty())
		return;

	SoapDeletePortMappingIgnore(url, urn, port);
	UpnpUwpDiag("[PlayFabLobby] UPnP (UWP IGD): removed TCP mapping for port %d\n", port);
}

bool UpnpIgdUwp_AddTcpMappingAndGetExternalIPv4(int gamePort, const char *lanIpUtf8, char *outAnnounceHost, size_t announceHostSize,
	int *outAnnouncePort)
{
	if (outAnnouncePort != nullptr)
		*outAnnouncePort = gamePort;
	if (outAnnounceHost == nullptr || announceHostSize < 8)
		return false;
	outAnnounceHost[0] = 0;
	if (gamePort <= 0 || gamePort > 65535 || lanIpUtf8 == nullptr || lanIpUtf8[0] == 0)
		return false;

	UpnpIgdUwp_RemoveMappingIfAny();

	UpnpUwpDiag("[PlayFabLobby] UPnP (UWP IGD): starting (LAN=%s TCP port %d)\n", lanIpUtf8, gamePort);

	std::vector<std::string> locations;
	std::string err;
	if (!SsdpDiscover(locations, err))
	{
		UpnpUwpDiag("[PlayFabLobby] UPnP (UWP IGD): SSDP discover failed (%s)\n", err.c_str());
		return false;
	}

	UpnpUwpDiag("[PlayFabLobby] UPnP (UWP IGD): SSDP got %zu LOCATION(s)\n", locations.size());

	for (const std::string &loc : locations)
	{
		std::string xml, e2;
		if (!HttpGetUrl(loc, xml, e2))
		{
			UpnpUwpDiag("[PlayFabLobby] UPnP (UWP IGD): GET %s failed (%s)\n", loc.c_str(), e2.c_str());
			continue;
		}

		std::string urn, rel;
		if (!ExtractWanService(xml, urn, rel))
			continue;

		const std::string controlUrl = ResolveControlUrl(loc, rel);
		if (controlUrl.empty())
			continue;

		SoapDeletePortMappingIgnore(controlUrl, urn, gamePort);

		if (!SoapAddPortMapping(controlUrl, urn, gamePort, lanIpUtf8, e2))
		{
			UpnpUwpDiag("[PlayFabLobby] UPnP (UWP IGD): AddPortMapping failed (%s)\n", e2.c_str());
			continue;
		}

		std::string extIp;
		if (!SoapGetExternalIp(controlUrl, urn, extIp, e2) || !LooksLikeIpv4(extIp.c_str()))
		{
			UpnpUwpDiag("[PlayFabLobby] UPnP (UWP IGD): GetExternalIPAddress failed or not IPv4 (%s)\n", e2.c_str());
			SoapDeletePortMappingIgnore(controlUrl, urn, gamePort);
			continue;
		}

		strncpy_s(outAnnounceHost, announceHostSize, extIp.c_str(), _TRUNCATE);
		{
			std::lock_guard<std::mutex> lock(g_stateMu);
			g_mappedPort = gamePort;
			g_controlUrl = controlUrl;
			g_serviceUrn = urn;
		}
		UpnpUwpDiag("[PlayFabLobby] UPnP (UWP IGD): mapped public %s:%d (TCP %d -> %s)\n", outAnnounceHost, gamePort, gamePort,
			lanIpUtf8);
		return true;
	}

	UpnpUwpDiag("[PlayFabLobby] UPnP (UWP IGD): no usable IGD / mapping failed\n");
	return false;
}

#endif // defined(_UWP) && MINECRAFT_PLAYFAB_LOBBY_UPNP

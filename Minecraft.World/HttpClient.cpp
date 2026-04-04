#include "stdafx.h"
#include "HttpClient.h"
#include <curl/curl.h>

static size_t writeCallback(char *data, size_t size, size_t nmemb, void *userdata)
{
	auto *buf = static_cast<std::string *>(userdata);
	buf->append(data, size * nmemb);
	return size * nmemb;
}

static HttpResponse performRequest(CURL *curl, struct curl_slist *extraHeaders = nullptr)
{
	std::string responseBody;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	if (extraHeaders)
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, extraHeaders);

	CURLcode res = curl_easy_perform(curl);

	int statusCode = 0;
	if (res == CURLE_OK)
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

	curl_easy_cleanup(curl);
	if (extraHeaders)
		curl_slist_free_all(extraHeaders);
	return {statusCode, std::move(responseBody)};
}

static struct curl_slist *buildHeaders(const std::vector<std::string> &headers)
{
	struct curl_slist *list = nullptr;
	for (const auto &h : headers)
		list = curl_slist_append(list, h.c_str());
	return list;
}

HttpResponse HttpClient::get(const std::string &url, const std::vector<std::string> &headers)
{
	CURL *curl = curl_easy_init();
	if (!curl)
		return {0, ""};

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	return performRequest(curl, headers.empty() ? nullptr : buildHeaders(headers));
}

HttpResponse HttpClient::post(const std::string &url, const std::string &body, const std::string &contentType, const std::vector<std::string> &headers)
{
	CURL *curl = curl_easy_init();
	if (!curl)
		return {0, ""};

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));

	auto *headerList = buildHeaders(headers);
	headerList = curl_slist_append(headerList, ("Content-Type: " + contentType).c_str());

	return performRequest(curl, headerList);
}

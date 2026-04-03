#include "stdafx.h"
#include "HttpClient.h"
#include <curl/curl.h>

static size_t writeCallback(char *data, size_t size, size_t nmemb, void *userdata)
{
	auto *buf = static_cast<std::string *>(userdata);
	buf->append(data, size * nmemb);
	return size * nmemb;
}

static HttpResponse performRequest(CURL *curl)
{
	std::string responseBody;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	CURLcode res = curl_easy_perform(curl);

	int statusCode = 0;
	if (res == CURLE_OK)
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

	curl_easy_cleanup(curl);
	return {statusCode, std::move(responseBody)};
}

HttpResponse HttpClient::get(const std::string &url)
{
	CURL *curl = curl_easy_init();
	if (!curl)
		return {0, ""};

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	return performRequest(curl);
}

HttpResponse HttpClient::post(const std::string &url, const std::string &body, const std::string &contentType)
{
	CURL *curl = curl_easy_init();
	if (!curl)
		return {0, ""};

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));

	struct curl_slist *headers = nullptr;
	headers = curl_slist_append(headers, ("Content-Type: " + contentType).c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	HttpResponse resp = performRequest(curl);
	curl_slist_free_all(headers);
	return resp;
}

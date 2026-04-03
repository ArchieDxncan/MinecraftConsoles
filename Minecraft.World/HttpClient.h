#pragma once

#include <string>

struct HttpResponse
{
	int statusCode;
	std::string body;
};

class HttpClient
{
public:
	static HttpResponse get(const std::string &url);
	static HttpResponse post(const std::string &url, const std::string &body, const std::string &contentType = "application/json");
};

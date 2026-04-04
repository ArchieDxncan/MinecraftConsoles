#pragma once

#include <string>
#include <vector>

struct HttpResponse
{
	int statusCode;
	std::string body;
};

class HttpClient
{
public:
	static HttpResponse get(const std::string &url, const std::vector<std::string> &headers = {});
	static HttpResponse post(const std::string &url, const std::string &body, const std::string &contentType = "application/json", const std::vector<std::string> &headers = {});
};

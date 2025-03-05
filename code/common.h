#pragma once
#define _WIN32_WINNT 0x0601
#include <iostream>
#include <fstream>
#include <filesystem>
#include <asio.hpp>

struct Configuration {
	Configuration() = default;

	Configuration(const std::string& server_ip, const int server_port,
		const std::string directory = "client_files/", const size_t buffer_size = 4096)
		: server_ip_(server_ip), server_port_(server_port), directory_(directory), buffer_size_(buffer_size) {}

	const std::string server_ip_;
	const int server_port_;
	const std::string directory_;
	const size_t buffer_size_;
};
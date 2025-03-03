#define _WIN32_WINNT 0x0601
#include <iostream>
#include <fstream>
#include <filesystem>
#include <asio.hpp>

using asio::ip::tcp;
namespace fs = std::filesystem;

// Configuration
const std::string SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 9000;
const std::string CLIENT_FOLDER = "client_files/";
const std::string SERVER_FOLDER = "server_files/";
const size_t BUFFER_SIZE = 4096;

class FileTransferClient {
public:
    FileTransferClient(asio::io_context& io_context, const std::string& server_ip, short server_port)
        : socket_(io_context), endpoint_(asio::ip::make_address(server_ip), server_port) {
        for (const auto& entry : std::filesystem::directory_iterator(CLIENT_FOLDER)) {
            files_.push_back(entry.path().string());
        }
        file_iter_ = files_.begin();
    }
    void start_transfer() {
        socket_.async_connect(endpoint_, [this](std::error_code ec) {
            if (!ec) {
                std::cout << "Connected to server." << std::endl;
                send_next_file();
            }
            });
    }

private:
    void send_next_file() {
        if (file_iter_ == files_.end()) {
            std::cout << "All files sent." << std::endl;
            return;
        }

        current_file_.open(*file_iter_, std::ios::binary);
        if (!current_file_) {
            std::cerr << "Failed to open file: " << *file_iter_ << std::endl;
            ++file_iter_;
            send_next_file();
            return;
        }
        std::string path(fs::path(*file_iter_).filename().string());
        std::string size(std::to_string(fs::file_size(*file_iter_)));
        auto filename = std::make_shared<std::string>(path + ' ' + size + '\n');

        std::cout << "Sending: " << path << std::endl << "Size: " << size << std::endl;

        asio::async_write(socket_, asio::buffer(*filename),
            [this, filename](std::error_code ec, std::size_t) {
            if (!ec) {
                send_file_data();
            }
            });
    }

    void send_file_data() {
        if (current_file_.eof()) {
            current_file_.close();
            ++file_iter_;
            send_next_file();
            return;
        }

        auto buffer = std::make_shared<std::vector<char>>(BUFFER_SIZE);
        current_file_.read(buffer->data(), buffer->size());
        std::streamsize bytes_read = current_file_.gcount();

        asio::async_write(socket_, asio::buffer(buffer->data(), bytes_read),
            [this](std::error_code ec, std::size_t) {
                if (!ec) {
                    send_file_data();
                }
            });

    }
    tcp::socket socket_;
    tcp::endpoint endpoint_;
    std::vector<std::string> files_;
    std::vector<std::string>::iterator file_iter_ = files_.begin();
    std::ifstream current_file_;
};

int main() {
    try {
        asio::io_context io_context;

        fs::create_directories(CLIENT_FOLDER);
        FileTransferClient client(io_context, SERVER_IP, SERVER_PORT);
        client.start_transfer();
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
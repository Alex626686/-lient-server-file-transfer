#include "common.h"

using asio::ip::tcp;
namespace fs = std::filesystem;

class FileTransferClient {
public:
    FileTransferClient(asio::io_context& io_context, Configuration config)
        : config_(config), socket_(io_context) {
        for (const auto& entry : std::filesystem::directory_iterator(config.directory_)) {
            files_.push_back(entry.path().string());
        }
        file_iter_ = files_.begin();
    }
    void StartTransfer() {
        tcp::endpoint endpoint(asio::ip::make_address(config_.server_ip_), config_.server_port_);
        socket_.async_connect(endpoint, [this](std::error_code ec) {
            if (!ec) {
                std::cout << "Connected to server." << std::endl;
                SendNextFile();
            }
            else {
                std::cerr << "Failed to connect to server\n";
            }
            });
    }

private:
    void SendNextFile() {
        if (file_iter_ == files_.end()) {
            std::cout << "All files sent." << std::endl;
            return;
        }

        current_file_.open(*file_iter_, std::ios::binary);
        if (!current_file_) {
            std::cerr << "Failed to open file: " << *file_iter_ << std::endl;
            ++file_iter_;
            SendNextFile();
            return;
        }
        std::string path(fs::path(*file_iter_).filename().string());
        std::string size(std::to_string(fs::file_size(*file_iter_)));
        auto filename = std::make_shared<std::string>(path + ' ' + size + '\n');

        std::cout << "Sending: " << path << std::endl << "Size: " << size << std::endl;

        asio::async_write(socket_, asio::buffer(*filename),
            [this, filename](std::error_code ec, std::size_t) {
            if (!ec) {
                SendFileData();
            }
            else {
                std::cerr << "Failed to send filename: " << ec.message() << std::endl;
                current_file_.close();
                ++file_iter_;
                SendNextFile();
            }
            });
    }

    void SendFileData() {
        if (current_file_.eof()) {
            current_file_.close();
            ++file_iter_;
            SendNextFile();
            return;
        }

        auto buffer = std::make_shared<std::vector<char>>(config_.buffer_size_);
        current_file_.read(buffer->data(), buffer->size());
        std::streamsize bytes_read = current_file_.gcount();

        asio::async_write(socket_, asio::buffer(buffer->data(), bytes_read),
            [this](std::error_code ec, std::size_t) {
                if (!ec) {
                    SendFileData();
                }
                else if (ec == asio::error::eof) {
                    std::cout << "Lost connection with server\n";
                    StartTransfer();
                }
                else {
                    std::cerr << "Failed to send file data: " << ec.message() << std::endl;
                    current_file_.close();
                    ++file_iter_;
                    SendNextFile();
                }
            });

    }
    Configuration config_;
    tcp::socket socket_;
    std::vector<std::string> files_;
    std::vector<std::string>::iterator file_iter_;
    std::ifstream current_file_;
};

int main() {
    try {
        Configuration config("127.0.0.1", 9000);
        asio::io_context io_context;

        fs::create_directories(config.directory_);
        FileTransferClient client(io_context, config);
        client.StartTransfer();
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}


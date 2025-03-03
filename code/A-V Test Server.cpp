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
const std::string SERVER_FOLDER = "server_files/";
const size_t BUFFER_SIZE = 4096;

class FileTransferServer {
public:
    FileTransferServer(asio::io_context& io_context, short port)
        : io_context_(io_context),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        start_accept();
    }

private:
    void start_accept() {
        auto socket = std::make_shared<tcp::socket>(io_context_);
        acceptor_.async_accept(*socket, [this, socket](std::error_code ec) {
            if (!ec) {
                std::cout << "Client connected." << std::endl;
                receive_file(socket);
            }
            start_accept();
            });
    }

    void receive_file(std::shared_ptr<tcp::socket> socket) {
        auto buffer = std::make_shared<std::vector<char>>(BUFFER_SIZE);
        auto filename = std::make_shared<std::string>();
        auto file = std::make_shared<std::ofstream>();

        asio::async_read_until(*socket, asio::dynamic_buffer(*filename), '\n',
            [this, socket, buffer, filename, file](std::error_code ec, std::size_t length) {
                if (!ec) {
                    filename->erase(filename->find('\n'));
                    size_t size_pos = filename->find_last_of(' ');
                    size_t filesize = std::stol(filename->substr(size_pos));
                    filename->erase(size_pos);

                    std::cout << "Receiving: " << *filename << std::endl;
                    file->open(SERVER_FOLDER + *filename, std::ios::binary);
                    read_file_data(socket, buffer, file, filesize);
                }
                else if (asio::error::eof) {
                    std::cout << "Client disconnected.\n";
                }
                else {
                    std::cerr << "Title read error: " << ec.value() << std::endl;
                }
            });
    }

    void read_file_data(std::shared_ptr<tcp::socket> socket, std::shared_ptr<std::vector<char>> buffer, std::shared_ptr<std::ofstream> file, size_t filesize) {
        socket->async_read_some(asio::buffer(*buffer, std::min(BUFFER_SIZE, filesize)),
            [this, socket, buffer, file, filesize](std::error_code ec, std::size_t length) {
                if (!ec && length > 0) {
                    file->write(buffer->data(), length);

                    read_file_data(socket, buffer, file, filesize - length);
                }
                else if (ec) {
                    throw ec;
                }
                else {
                    file->close();
                    std::cout << "File received." << std::endl;
                    receive_file(socket);
                }
            });
    }
    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
};

int main() {
    try {
        asio::io_context io_context;

        fs::create_directories(SERVER_FOLDER);
        FileTransferServer server(io_context, SERVER_PORT);
        io_context.run();       
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
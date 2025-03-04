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
        auto strbuff = std::make_shared<std::string>();
        auto file = std::make_shared<std::ofstream>();

        asio::async_read_until(*socket, asio::dynamic_buffer(*strbuff), '\n',
            [this, socket, strbuff, file](std::error_code ec, std::size_t length) {
                if (!ec) {
                    size_t n_pos = strbuff->find('\n');
                    size_t space_pos = strbuff->find_last_of(' ',n_pos );

                    std::string filename(strbuff->data(), space_pos);
                    size_t filesize = std::stol(strbuff->substr(space_pos + 1, n_pos - 1));
                    std::cout << "Receiving: " << filename << std::endl << "Size: " << filesize << std::endl;
                    
                    file->open(SERVER_FOLDER + filename, std::ios::binary);
                    std::string tail(strbuff->substr(n_pos + 1));
                    *file << tail;
                    read_file_data(socket, file, filesize - tail.size());
                }
                else if (ec == asio::error::eof) {
                    std::cout << "Client disconnected.\n";
                }
                else {
                    std::cerr << "Title read error: " << ec.value() << std::endl;
                }
            });
    }

    void read_file_data(std::shared_ptr<tcp::socket> socket, std::shared_ptr<std::ofstream> file, size_t filesize) {

        auto buffer = std::make_shared<std::vector<char>>(BUFFER_SIZE);
        socket->async_read_some(asio::buffer(*buffer, std::min(BUFFER_SIZE, filesize)),
            [this, socket, buffer, file, filesize](std::error_code ec, std::size_t length) {
                if (!ec && length > 0) {
                    file->write(buffer->data(), length);
                    read_file_data(socket, file, filesize - length);
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
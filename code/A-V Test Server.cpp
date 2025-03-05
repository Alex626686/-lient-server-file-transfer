#include "common.h"

using asio::ip::tcp;
namespace fs = std::filesystem;

class FileTransferServer {
public:
    FileTransferServer(asio::io_context& io_context, Configuration&& config)
        : config_(config), io_context_(io_context),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), config.server_port_)) {
        StartAccept();
    }

private:  
    void StartAccept() {
        auto socket = std::make_shared<tcp::socket>(io_context_);
        acceptor_.async_accept(*socket, [this, socket](std::error_code ec) {
            if (!ec) {
                std::cout << "Client connected." << std::endl;
                ReceiveFile(socket);
            }
            StartAccept();
            });
    }

    

    void ReceiveFile(std::shared_ptr<tcp::socket> socket) {
        auto strbuff = std::make_shared<std::string>();

        asio::async_read_until(*socket, asio::dynamic_buffer(*strbuff), '\n',
            [this, strbuff, socket](std::error_code ec, std::size_t length) {
                if (!ec) {
                    size_t n_pos = strbuff->find('\n');
                    size_t space_pos = strbuff->find_last_of(' ',n_pos );

                    std::string filename(strbuff->data(), space_pos);
                    size_t filesize = std::stol(strbuff->substr(space_pos + 1, n_pos - 1));
                    std::cout << "Receiving: " << filename << std::endl << "Size: " << filesize << std::endl;

                    auto file = std::make_shared<std::ofstream>();
                    file->open(config_.directory_ + filename, std::ios::binary);
                    std::string tail(strbuff->substr(n_pos + 1));
                    *file << tail;

                    ReadFileData(socket, file, filesize - tail.size());
                }
                else if (ec == asio::error::eof) {
                    std::cout << "Client disconnected.\n";
                }
                else {
                    std::cerr << "Title read error: " << ec.value() << std::endl;
                }
            });
    }

    void ReadFileData(std::shared_ptr<tcp::socket> socket, std::shared_ptr<std::ofstream> file, size_t filesize) {
        auto buffer = std::make_shared<std::vector<char>>(config_.buffer_size_);

        socket->async_read_some(asio::buffer(*buffer, std::min(config_.buffer_size_, filesize)),
            [this, socket, buffer, file, filesize](std::error_code ec, std::size_t length) {
                if (!ec) {
                    if (length > 0) {
                        file->write(buffer->data(), length);
                        ReadFileData(socket, file, filesize - length);
                    }
                    else{

                        file->close();
                        std::cout << "File received." << std::endl;
                        ReceiveFile(socket);
                    }
                }
                else if (ec == asio::error::eof) {
                    std::cout << "Client disconnected during transmission\n";
                    file->close();
                }
                else {
                    throw ec;
                }
            });
    }
    asio::io_context& io_context_;
    Configuration config_;
    tcp::acceptor acceptor_;
};

int main() {
    try {
        Configuration config("127.0.0.1", 9000, "server_files/");
        asio::io_context io_context;

        fs::create_directories(config.directory_);
        FileTransferServer server(io_context, std::move(config));
        io_context.run();       
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
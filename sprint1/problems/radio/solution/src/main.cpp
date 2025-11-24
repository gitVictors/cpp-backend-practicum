#include "audio.h"
#include <boost/asio.hpp>
#include <iostream>
#include <string>
#include <thread>

using namespace std::literals;
namespace asio = boost::asio;
using boost::asio::ip::udp;

void StartServer(uint16_t port) {
    try {
        asio::io_context io_context;
        udp::socket socket(io_context, udp::endpoint(udp::v4(), port));
        
        Player player(ma_format_u8, 1);
        std::vector<char> buffer(65000);
        
        std::cout << "Server listening on port " << port << std::endl;
        
        while (true) {
            udp::endpoint remote_endpoint;
            boost::system::error_code error;
            
            // Получаем данные
            size_t length = socket.receive_from(
                asio::buffer(buffer), remote_endpoint, 0, error);
            
            if (error && error != asio::error::message_size) {
                throw boost::system::system_error(error);
            }
            
            // Вычисляем количество фреймов
            size_t frames = length / player.GetFrameSize();
            
            std::cout << "Received " << length << " bytes (" << frames << " frames) from " 
                      << remote_endpoint.address().to_string() << std::endl;
            
            // Воспроизводим звук
            player.PlayBuffer(buffer.data(), frames, 1.5s);
            std::cout << "Playing done" << std::endl;
        }
    }
    catch (std::exception& e) {
        std::cerr << "Server exception: " << e.what() << std::endl;
    }
}

void StartClient(uint16_t port) {
    try {
        asio::io_context io_context;
        udp::socket socket(io_context, udp::v4());
        udp::endpoint server_endpoint;
        
        Recorder recorder(ma_format_u8, 1);
        
        while (true) {
            std::string server_ip;
            std::cout << "Enter server IP address: ";
            std::getline(std::cin, server_ip);
            
            // Устанавливаем endpoint сервера
            server_endpoint = udp::endpoint(
                asio::ip::make_address(server_ip), port);
            
            std::cout << "Press Enter to record message..." << std::endl;
            std::string dummy;
            std::getline(std::cin, dummy);
            
            // Записываем аудио
            auto rec_result = recorder.Record(65000, 1.5s);
            std::cout << "Recording done, " << rec_result.frames << " frames recorded" << std::endl;
            
            // Вычисляем количество байт для отправки
            size_t bytes_to_send = rec_result.frames * recorder.GetFrameSize();
            
            // Отправляем данные
            socket.send_to(
                asio::buffer(rec_result.data.data(), bytes_to_send), 
                server_endpoint);
            
            std::cout << "Sent " << bytes_to_send << " bytes to " << server_ip << std::endl;
        }
    }
    catch (std::exception& e) {
        std::cerr << "Client exception: " << e.what() << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <client|server> <port>" << std::endl;
        return 1;
    }
    
    std::string mode = argv[1];
    uint16_t port = std::stoi(argv[2]);
    
    if (mode == "server") {
        StartServer(port);
    } else if (mode == "client") {
        StartClient(port);
    } else {
        std::cerr << "Invalid mode. Use 'client' or 'server'" << std::endl;
        return 1;
    }
    
    return 0;
}

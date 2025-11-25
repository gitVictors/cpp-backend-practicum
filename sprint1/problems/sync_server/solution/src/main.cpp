#ifdef WIN32
#include <sdkddkver.h>
#endif
// boost.beast будет использовать std::string_view вместо boost::string_view
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <thread>
#include <optional>
#include <string>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

void DumpRequest(const StringRequest& req) {
    std::cout << req.method_string() << ' ' << req.target() << std::endl;
    // Выводим заголовки запроса
    for (const auto& header : req) {
        std::cout << "  "sv << header.name_string() << ": "sv << header.value() << std::endl;
    }
}

std::optional<StringRequest> ReadRequest(tcp::socket& socket, beast::flat_buffer& buffer) {
    beast::error_code ec;
    StringRequest req;
    // Считываем из socket запрос req, используя buffer для хранения данных.
    // В ec функция запишет код ошибки.
    http::read(socket, buffer, req, ec);

    if (ec == http::error::end_of_stream) {
        return std::nullopt;
    }

    if (ec) {
        throw std::runtime_error("Failed to read request: "s.append(ec.message()));
    }

    return req;
}

// Функция для обработки запроса
StringResponse HandleRequest(StringRequest&& req) {
    StringResponse response;
    
    // Проверяем метод запроса
    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        response.result(http::status::method_not_allowed);
        response.set(http::field::content_type, "text/html");
        response.set(http::field::allow, "GET, HEAD");
        response.body() = "Invalid method.";
        response.prepare_payload();
        return response;
    }
    
    // Извлекаем target из URL (убираем ведущий символ '/')
  //  std::string target = req.target().substr(1); // убираем первый символ '/'
    std::string target = std::string(req.target().substr(1)); 
    
    // Формируем тело ответа
    std::string body = "Hello, " + target;
    
    // Устанавливаем статус и заголовки
    response.result(http::status::ok);
    response.set(http::field::content_type, "text/html");
    response.body() = body;
    
    // Для HEAD-запроса тело должно быть пустым
    if (req.method() == http::verb::head) {
        response.body() = "";
    }
    
    response.prepare_payload();
    return response;
}

template <typename RequestHandler>
void HandleConnection(tcp::socket& socket, RequestHandler&& handle_request) {
    try {
        beast::flat_buffer buffer;
        
        while (auto request = ReadRequest(socket, buffer)) {
            DumpRequest(*request);
            // Делегируем обработку запроса функции handle_request
            StringResponse response = handle_request(*std::move(request));
            
            // Отправляем ответ
            http::write(socket, response);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling connection: " << e.what() << std::endl;
    }
}

int main() {
    const auto address = net::ip::make_address("0.0.0.0");
    const unsigned short port = 8080;
    
    net::io_context ioc;

    tcp::acceptor acceptor(ioc, {address, port});
    std::cout << "Server has started..."sv << std::endl;

    while (true) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
    
        std::thread t(
            [](tcp::socket socket) {
                // Вызываем HandleConnection, передавая ей функцию-обработчик запроса
                HandleConnection(socket, HandleRequest);
            },
            std::move(socket));
        t.detach();
    }
}
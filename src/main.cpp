#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>

#include "http_server.h"
#include "json_loader.h"
#include "request_handler.h"

using boost::asio::ip::tcp;
using namespace std::literals;
namespace net = boost::asio;

namespace
{

    // Запускает функцию fn на n потоках, включая текущий
    template <typename Fn>
    void RunWorkers(unsigned n, const Fn &fn)
    {
        n = std::max(1u, n);
        std::vector<std::jthread> workers;
        workers.reserve(n - 1);
        // Запускаем n-1 рабочих потоков, выполняющих функцию fn
        while (--n)
        {
            workers.emplace_back(fn);
        }
        fn();
    }

} // namespace

int main(int argc, const char *argv[])
{
    const int PORT = 8080;
    if (argc != 2)
    {
        std::cerr << "Usage: game_server <game-config-json>"sv << std::endl;
        return EXIT_FAILURE;
    }
    try
    {
        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(argv[1]);

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code &ec, int signal_number)
                           {
            if (!ec) {
                ioc.stop();
            } });

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        http_handler::RequestHandler handler{game};

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        std::shared_ptr<void> server_keeper;
        http_server::ServeHttp(ioc, {tcp::v4(), PORT}, [&handler](auto &&req, auto &&send)
                               { handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send)); }, server_keeper);

        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        std::cout << "Server has started at port "sv << PORT << std::endl;

        // 6. Запускаем обработку асинхронных операций
        ioc.run();
    }
    catch (const std::exception &ex)
    {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}

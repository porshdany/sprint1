#pragma once
#include "sdk.h"
//
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <memory>

namespace http_server
{

    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace net = boost::asio;
    using tcp = net::ip::tcp;

    template <typename RequestHandler>
    class Session : public std::enable_shared_from_this<Session<RequestHandler>>
    {
    public:
        Session(tcp::socket socket, std::shared_ptr<RequestHandler> handler)
            : socket_{std::move(socket)}, handler_{handler}
        {
        }

        void Start()
        {
            Read();
        }

    private:
        tcp::socket socket_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> request_;
        std::shared_ptr<RequestHandler> handler_;

        void Read()
        {
            auto self = this->shared_from_this();
            http::async_read(
                socket_,
                buffer_,
                request_,
                [self](beast::error_code ec, std::size_t)
                {
                    self->OnRead(ec);
                });
        }

        void OnRead(beast::error_code ec)
        {
            if (ec == http::error::end_of_stream)
            {
                return DoClose();
            }

            if (ec)
            {
                return;
            }

            // Обрабатываем запрос и отправляем ответ
            auto self = this->shared_from_this();

            // Создаём send лямбду для отправки ответа
            auto send = [self](http::response<http::string_body> &&response)
            {
                self->Write(std::move(response));
            };

            // Вызываем обработчик
            (*handler_)(std::move(request_), send);
        }

        void Write(http::response<http::string_body> &&response)
        {
            auto self = this->shared_from_this();

            auto resp = std::make_shared<http::response<http::string_body>>(std::move(response));

            http::async_write(
                socket_,
                *resp,
                [self, resp](beast::error_code ec, std::size_t)
                {
                    self->OnWrite(ec);
                });
        }

        void OnWrite(beast::error_code ec)
        {
            if (ec)
            {
                return;
            }
            // Читаем следующий запрос
            buffer_.clear();
            request_ = {};
            Read();
        }

        void DoClose()
        {
            beast::error_code ec;
            socket_.shutdown(tcp::socket::shutdown_both, ec);
        }
    };

    // Вспомогательная функция для рекурсивной обработки соединений
    namespace detail
    {
        template <typename RequestHandler>
        class Acceptor : public std::enable_shared_from_this<Acceptor<RequestHandler>>
        {
        public:
            Acceptor(std::shared_ptr<tcp::acceptor> acceptor,
                     std::shared_ptr<RequestHandler> handler)
                : acceptor_{acceptor}, handler_{handler} {}

            void DoAccept()
            {
                auto self = this->shared_from_this();
                acceptor_->async_accept(
                    [self](beast::error_code ec, tcp::socket socket)
                    {
                        if (!ec)
                        {
                            std::make_shared<Session<RequestHandler>>(
                                std::move(socket), self->handler_)
                                ->Start();
                        }
                        self->DoAccept();
                    });
            }

        private:
            std::shared_ptr<tcp::acceptor> acceptor_;
            std::shared_ptr<RequestHandler> handler_;
        };
    } // namespace detail

    template <typename RequestHandler>
    void ServeHttp(net::io_context &ioc, const tcp::endpoint &endpoint, RequestHandler &&handler,
                   std::shared_ptr<void> &keeper)
    {
        auto acceptor = std::make_shared<tcp::acceptor>(ioc, endpoint);

        // Сохраняем обработчик в shared_ptr
        auto handler_ptr = std::make_shared<std::remove_reference_t<RequestHandler>>(
            std::forward<RequestHandler>(handler));

        // Создаём объект для асинхронного принятия соединений
        auto acceptor_impl = std::make_shared<detail::Acceptor<std::remove_reference_t<RequestHandler>>>(
            acceptor, handler_ptr);

        // Сохраняем acceptor_impl в keeper чтобы он не был уничтожен
        keeper = acceptor_impl;

        acceptor_impl->DoAccept();
    }

} // namespace http_server

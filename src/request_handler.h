#pragma once
#include "http_server.h"
#include "model.h"

#include <sstream>
#include <cstring>
#include <boost/json.hpp>

namespace http_handler
{
    namespace json = boost::json;
    namespace beast = boost::beast;
    namespace http = beast::http;
    using namespace std::literals;

    // Класс-хранилка для эндпоинтов API
    class ApiEndpoints
    {
    public:
        static constexpr const char *API_PREFIX = "/api/";
        static constexpr const char *MAPS_ENDPOINT = "/api/v1/maps";
        static constexpr const char *MAP_BY_ID_PREFIX = "/api/v1/maps/";
    };

    // Вспомогательные функции для создания JSON ответов
    namespace
    {

        json::object RoadToJson(const model::Road &road)
        {
            json::object obj;
            obj["x0"] = road.GetStart().x;
            obj["y0"] = road.GetStart().y;

            if (road.IsHorizontal())
            {
                obj["x1"] = road.GetEnd().x;
            }
            else
            {
                obj["y1"] = road.GetEnd().y;
            }

            return obj;
        }

        json::object BuildingToJson(const model::Building &building)
        {
            const auto &bounds = building.GetBounds();
            json::object obj;
            obj["x"] = bounds.position.x;
            obj["y"] = bounds.position.y;
            obj["w"] = bounds.size.width;
            obj["h"] = bounds.size.height;
            return obj;
        }

        json::object OfficeToJson(const model::Office &office)
        {
            json::object obj;
            obj["id"] = *office.GetId();
            obj["x"] = office.GetPosition().x;
            obj["y"] = office.GetPosition().y;
            obj["offsetX"] = office.GetOffset().dx;
            obj["offsetY"] = office.GetOffset().dy;
            return obj;
        }

        json::object MapToJson(const model::Map &map)
        {
            json::object obj;
            obj["id"] = *map.GetId();
            obj["name"] = map.GetName();

            // Добавляем дороги
            json::array roads_json;
            for (const auto &road : map.GetRoads())
            {
                roads_json.push_back(RoadToJson(road));
            }
            obj["roads"] = roads_json;

            // Добавляем здания
            json::array buildings_json;
            for (const auto &building : map.GetBuildings())
            {
                buildings_json.push_back(BuildingToJson(building));
            }
            obj["buildings"] = buildings_json;

            // Добавляем офисы
            json::array offices_json;
            for (const auto &office : map.GetOffices())
            {
                offices_json.push_back(OfficeToJson(office));
            }
            obj["offices"] = offices_json;

            return obj;
        }

        // Парсит путь из запроса
        // Возвращает пустую строку, если путь не содержит параметр
        std::string ExtractMapIdFromPath(const std::string &path)
        {
            // Ожидаем формат: /api/v1/maps/{map_id}
            const std::string prefix = ApiEndpoints::MAP_BY_ID_PREFIX;
            if (path.size() > prefix.size() && path.substr(0, prefix.size()) == prefix)
            {
                return path.substr(prefix.size());
            }
            return "";
        }

        // Создаёт JSON ответ об ошибке
        json::object ErrorResponse(const std::string &code, const std::string &message)
        {
            json::object obj;
            obj["code"] = code;
            obj["message"] = message;
            return obj;
        }

    } // anonymous namespace

    class RequestHandler
    {
    public:
        explicit RequestHandler(model::Game &game)
            : game_{game}
        {
        }

        RequestHandler(const RequestHandler &) = delete;
        RequestHandler &operator=(const RequestHandler &) = delete;

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>> &&req, Send &&send)
        {
            // Объявляем строковый ответ для упрощения работы с JSON
            auto make_response = [&req](http::status status, std::string body)
            {
                http::response<http::string_body> res{status, req.version()};
                res.set(http::field::content_type, "application/json");
                res.body() = body;
                res.prepare_payload();
                return res;
            };

            const auto &path = req.target();
            std::string path_str(path.begin(), path.end());

            // Проверяем, что это запрос к API
            if (path_str.substr(0, std::strlen(ApiEndpoints::API_PREFIX)) == ApiEndpoints::API_PREFIX)
            {
                // Проверяем /api/v1/maps
                if (path_str == ApiEndpoints::MAPS_ENDPOINT && req.method() == http::verb::get)
                {
                    json::array maps_json;
                    for (const auto &map : game_.GetMaps())
                    {
                        json::object map_obj;
                        map_obj["id"] = *map.GetId();
                        map_obj["name"] = map.GetName();
                        maps_json.push_back(map_obj);
                    }

                    auto res = make_response(http::status::ok, json::serialize(maps_json));
                    send(std::move(res));
                    return;
                }

                // Проверяем /api/v1/maps/{id}
                if (path_str.substr(0, std::strlen(ApiEndpoints::MAP_BY_ID_PREFIX)) == ApiEndpoints::MAP_BY_ID_PREFIX && req.method() == http::verb::get && path_str.size() > std::strlen(ApiEndpoints::MAP_BY_ID_PREFIX))
                {
                    std::string map_id = ExtractMapIdFromPath(path_str);

                    if (const model::Map *map = game_.FindMap(model::Map::Id{map_id}))
                    {
                        auto map_json = MapToJson(*map);
                        auto res = make_response(http::status::ok, json::serialize(map_json));
                        send(std::move(res));
                        return;
                    }
                    else
                    {
                        auto error_json = ErrorResponse("mapNotFound", "Map not found");
                        auto res = make_response(http::status::not_found, json::serialize(error_json));
                        send(std::move(res));
                        return;
                    }
                }

                // Неизвестный API endpoint
                auto error_json = ErrorResponse("badRequest", "Bad request");
                auto res = make_response(http::status::bad_request, json::serialize(error_json));
                send(std::move(res));
                return;
            }

            // Для всех остальных запросов возвращаем 404
            auto error_json = ErrorResponse("badRequest", "Bad request");
            auto res = make_response(http::status::bad_request, json::serialize(error_json));
            send(std::move(res));
        }

    private:
        model::Game &game_;
    };

} // namespace http_handler

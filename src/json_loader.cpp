#include "json_loader.h"

#include <fstream>
#include <sstream>

#include <boost/json.hpp>

namespace json_loader
{
    namespace json = boost::json;
    using namespace std::literals;

    void LoadRoads(const json::object &map_obj, model::Map &map)
    {
        const auto &roads_array = map_obj.at("roads").as_array();
        for (const auto &road_val : roads_array)
        {
            const auto &road_obj = road_val.as_object();

            int x0 = json::value_to<int>(road_obj.at("x0"));
            int y0 = json::value_to<int>(road_obj.at("y0"));

            // Проверяем, горизонтальная ли дорога (есть x1) или вертикальная (есть y1)
            if (road_obj.contains("x1"))
            {
                int x1 = json::value_to<int>(road_obj.at("x1"));
                map.AddRoad(model::Road(model::Road::HORIZONTAL, model::Point{x0, y0}, x1));
            }
            else
            {
                int y1 = json::value_to<int>(road_obj.at("y1"));
                map.AddRoad(model::Road(model::Road::VERTICAL, model::Point{x0, y0}, y1));
            }
        }
    }

    void LoadBuildings(const json::object &map_obj, model::Map &map)
    {
        if (map_obj.contains("buildings"))
        {
            const auto &buildings_array = map_obj.at("buildings").as_array();
            for (const auto &building_val : buildings_array)
            {
                const auto &building_obj = building_val.as_object();

                int x = json::value_to<int>(building_obj.at("x"));
                int y = json::value_to<int>(building_obj.at("y"));
                int w = json::value_to<int>(building_obj.at("w"));
                int h = json::value_to<int>(building_obj.at("h"));

                model::Building building{model::Rectangle{
                    model::Point{x, y},
                    model::Size{w, h}}};
                map.AddBuilding(building);
            }
        }
    }

    void LoadOffices(const json::object &map_obj, model::Map &map)
    {
        if (map_obj.contains("offices"))
        {
            const auto &offices_array = map_obj.at("offices").as_array();
            for (const auto &office_val : offices_array)
            {
                const auto &office_obj = office_val.as_object();

                std::string office_id = json::value_to<std::string>(office_obj.at("id"));
                int x = json::value_to<int>(office_obj.at("x"));
                int y = json::value_to<int>(office_obj.at("y"));
                int offset_x = json::value_to<int>(office_obj.at("offsetX"));
                int offset_y = json::value_to<int>(office_obj.at("offsetY"));

                model::Office office{
                    model::Office::Id{office_id},
                    model::Point{x, y},
                    model::Offset{offset_x, offset_y}};
                map.AddOffice(std::move(office));
            }
        }
    }

    model::Game LoadGame(const std::filesystem::path &json_path)
    {
        // Загружаем содержимое файла
        std::ifstream file(json_path);
        if (!file.is_open())
        {
            throw std::runtime_error("Cannot open file: " + json_path.string());
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_str = buffer.str();

        // Парсим JSON
        try
        {
            auto value = json::parse(json_str);
            auto maps_array = value.as_object().at("maps").as_array();

            model::Game game;

            // Загружаем каждую карту
            for (const auto &map_val : maps_array)
            {
                const auto &map_obj = map_val.as_object();

                // Получаем id и name карты
                std::string map_id = json::value_to<std::string>(map_obj.at("id"));
                std::string map_name = json::value_to<std::string>(map_obj.at("name"));

                model::Map map{model::Map::Id{map_id}, map_name};

                LoadRoads(map_obj, map);
                LoadBuildings(map_obj, map);
                LoadOffices(map_obj, map);

                game.AddMap(std::move(map));
            }

            return game;
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Failed to parse JSON: " + std::string(e.what()));
        }
    }

} // namespace json_loader

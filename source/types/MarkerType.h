#pragma once

#include <optional>
#include <string>

struct MarkerType
{
    enum Enum
    {
        markers,
        regions
    };

    static std::optional<Enum> fromString (const std::string& str)
    {
        if (str == "markers")
            return markers;
        else if (str == "regions")
            return regions;
        else
            return std::nullopt;
    }

    static std::string toString (Enum type)
    {
        switch (type)
        {
            case markers: return "markers";
            case regions: return "regions";
        }
        return "";
    }
};

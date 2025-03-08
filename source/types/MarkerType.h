#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

struct MarkerType
{
    enum Enum
    {
        markers,
        regions,
        notes,
        count // Used for array size
    };

    static constexpr std::array<std::string_view, count> strings = {
        "markers",
        "regions",
        "notes"
    };

    static std::optional<Enum> fromString (const std::string& str)
    {
        for (size_t i = 0; i < strings.size(); ++i)
            if (str == strings[i])
                return static_cast<Enum> (i);
        return std::nullopt;
    }

    static std::string toString (Enum type)
    {
        if (type >= 0 && type < count)
            return std::string (strings[type]);
        return "";
    }
};

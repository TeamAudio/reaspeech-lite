#pragma once

#include <string>
#include <juce_core/juce_core.h>

struct SafeUTF8
{
    static juce::String encode (const char* const buffer)
    {
        if (buffer == nullptr)
            return {};

        std::string input(buffer);
        std::string output;
        output.reserve(input.length());

        for (size_t i = 0; i < input.length();)
        {
            char c = input[i];
            size_t len = 1;

            if ((c & 0x80) == 0)
            {
                // ASCII character
                output += c;
                i++;
                continue;
            }

            // Get expected UTF-8 sequence length
            if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) len = 4;
            else
            {
                // Invalid UTF-8 sequence
                output += "\xEF\xBF\xBD"; // UTF-8 replacement character
                i++;
                continue;
            }

            // Check if sequence is complete
            if (i + len > input.length())
            {
                output += "\xEF\xBF\xBD";
                break;
            }

            // Validate continuation bytes
            bool valid = true;
            for (size_t j = 1; j < len; j++)
            {
                if ((input[i + j] & 0xC0) != 0x80)
                {
                    valid = false;
                    break;
                }
            }

            if (valid)
                output.append(input.substr(i, len));
            else
                output += "\xEF\xBF\xBD";

            i += valid ? len : 1;
        }

        return juce::String::fromUTF8(output.c_str());
    }
};

#pragma once

#include <JuceHeader.h>

struct Resources
{
    static std::optional<juce::WebBrowserComponent::Resource> get (const juce::String& url)
    {
        static const std::unordered_map<juce::String, ResourceData> resourceMap =
        {
            { "index.html", { BinaryData::index_html, BinaryData::index_htmlSize, "text/html" } },
            { "css/bootstrap.min.css", { BinaryData::bootstrap_min_css, BinaryData::bootstrap_min_cssSize, "text/css" } },
            { "css/bootstrap.min.css.map", { BinaryData::bootstrap_min_css_map, BinaryData::bootstrap_min_css_mapSize, "application/json" } },
            { "img/tech-audio-logo.png", { BinaryData::techaudiologo_png, BinaryData::techaudiologo_pngSize, "image/png" } },
            { "js/bootstrap.bundle.min.js", { BinaryData::bootstrap_bundle_min_js, BinaryData::bootstrap_bundle_min_jsSize, "application/javascript" } },
            { "js/bootstrap.bundle.min.js.map", { BinaryData::bootstrap_bundle_min_js_map, BinaryData::bootstrap_bundle_min_js_mapSize, "application/json" } },
            { "js/main.js", { BinaryData::main_js, BinaryData::main_jsSize, "application/javascript" } }
        };

        const auto urlToRetrieve = url == "/" ? juce::String { "index.html" }
                                              : url.fromFirstOccurrenceOf ("/", false, false);

        const auto it = resourceMap.find (urlToRetrieve);
        if (it != resourceMap.end())
            return it->second.getResource();

        return {};
    }

private:
    struct ResourceData
    {
        const char* data;
        size_t size;
        const char* mimeType;

        std::vector<std::byte> getBinaryData() const
        {
            return std::vector<std::byte> (
                reinterpret_cast<const std::byte*> (data),
                reinterpret_cast<const std::byte*> (data + size)
            );
        }

        juce::WebBrowserComponent::Resource getResource() const
        {
            return { getBinaryData(), mimeType };
        }
    };
};

#pragma once

#include <JuceHeader.h>

struct Resources
{
    static std::optional<juce::WebBrowserComponent::Resource> get (const juce::String& url)
    {
        const auto urlToRetrieve = url == "/" ? juce::String { "index.html" }
                                              : url.fromFirstOccurrenceOf ("/", false, false);

        if (urlToRetrieve == "index.html")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::index_html, BinaryData::index_htmlSize),
                "text/html"
            };
        }

        if (urlToRetrieve == "css/bootstrap.min.css")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::bootstrap_min_css, BinaryData::bootstrap_min_cssSize),
                "text/css"
            };
        }

        if (urlToRetrieve == "css/bootstrap.min.css.map")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::bootstrap_min_css_map, BinaryData::bootstrap_min_css_mapSize),
                "application/json"
            };
        }

        if (urlToRetrieve == "img/tech-audio-logo.png")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::techaudiologo_png, BinaryData::techaudiologo_pngSize),
                "image/png"
            };
        }

        if (urlToRetrieve == "js/bootstrap.bundle.min.js")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::bootstrap_bundle_min_js, BinaryData::bootstrap_bundle_min_jsSize),
                "application/javascript"
            };
        }

        if (urlToRetrieve == "js/bootstrap.bundle.min.js.map")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::bootstrap_bundle_min_js_map, BinaryData::bootstrap_bundle_min_js_mapSize),
                "application/json"
            };
        }

        if (urlToRetrieve == "js/main.js")
        {
            return juce::WebBrowserComponent::Resource {
                getBinaryData (BinaryData::main_js, BinaryData::main_jsSize),
                "application/javascript"
            };
        }

        return {};
    }

private:
    static std::vector<std::byte> getBinaryData (const char* data, size_t size)
    {
        return std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(data),
            reinterpret_cast<const std::byte*>(data + size)
        );
    }
};

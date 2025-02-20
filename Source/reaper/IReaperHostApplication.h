#pragma once

#include <JuceHeader.h>

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wshadow-field-in-constructor",
                                     "-Wnon-virtual-dtor")

#include <pluginterfaces/base/ftypes.h>
#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/vst/ivsthostapplication.h>

JUCE_END_IGNORE_WARNINGS_GCC_LIKE

namespace reaper
{
    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wzero-as-null-pointer-constant",
                                         "-Wunused-parameter",
                                         "-Wnon-virtual-dtor")
    JUCE_BEGIN_IGNORE_WARNINGS_MSVC (4100)

    using namespace Steinberg;
    using INT_PTR = juce::pointer_sized_int;
    using uint32 = Steinberg::uint32;

    #include "../extern/reaper_vst3_interfaces.h"

    JUCE_END_IGNORE_WARNINGS_MSVC
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE
}

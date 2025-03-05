#pragma once

#include <exception>
#include <juce_core/juce_core.h>

#include "IReaperHostApplication.h"

struct ReaperProxy
{
    class Missing : public std::exception
    {
    public:
        Missing (const char* fn) : functionName (fn) {}
        const char* what() const noexcept { return functionName; }
    private:
        const char* functionName;
    };

    ReaperProxy() {}

    void load(reaper::IReaperHostApplication* reaperHost)
    {
        if (reaperHost == nullptr)
            return;

        hasAddProjectMarker2 = reaperHost->getReaperApi("AddProjectMarker2");
        hasPreventUIRefresh = reaperHost->getReaperApi("PreventUIRefresh");
        hasUndo_BeginBlock2 = reaperHost->getReaperApi("Undo_BeginBlock2");
        hasUndo_EndBlock2 = reaperHost->getReaperApi("Undo_EndBlock2");
    }

    class ReaProject;

    static constexpr ReaProject* activeProject = nullptr;

    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wgnu-zero-variadic-macro-arguments")

#define REAPER_CALL(functionName, typ, ...) \
    if (has##functionName) \
        return reinterpret_cast<typ> (has##functionName) (__VA_ARGS__); \
    else \
        throw Missing (#functionName);

    void* hasAddProjectMarker2 = nullptr;
    int AddProjectMarker2 (ReaProject* proj, bool isrgn, double pos, double rgnend, const char* name, int wantidx, int color)
    {
        REAPER_CALL(AddProjectMarker2, int (*) (ReaProject*, bool, double, double, const char*, int, int), proj, isrgn, pos, rgnend, name, wantidx, color)
    }

    void* hasPreventUIRefresh = nullptr;
    void PreventUIRefresh (int state)
    {
        REAPER_CALL(PreventUIRefresh, void (*) (int), state)
    }

    void* hasUndo_BeginBlock2 = nullptr;
    void Undo_BeginBlock2(ReaProject* proj)
    {
        REAPER_CALL(Undo_BeginBlock2, void (*) (ReaProject*), proj)
    }

    void* hasUndo_EndBlock2 = nullptr;
    void Undo_EndBlock2 (ReaProject* proj, const char* descchange, int extraflags)
    {
        REAPER_CALL(Undo_EndBlock2, void (*) (ReaProject*, const char*, int), proj, descchange, extraflags)
    }

#undef REAPER_CALL

    JUCE_END_IGNORE_WARNINGS_GCC_LIKE

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReaperProxy)
};

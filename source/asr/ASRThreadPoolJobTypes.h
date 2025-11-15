#pragma once

#include <string>
#include <vector>
#include "ASRSegment.h"

enum class ASRThreadPoolJobStatus
{
    ready,
    exporting,
    downloadingModel,
    loadingModel,
    transcribing,
    aborted,
    finished,
    failed
};

struct ASRThreadPoolJobResult
{
    bool isError;
    std::string errorMessage;
    std::vector<ASRSegment> segments;
};

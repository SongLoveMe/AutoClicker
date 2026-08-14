#ifndef SEQUENCEPOINTGEOMETRY_H
#define SEQUENCEPOINTGEOMETRY_H

#include "PlatformAdapter.h"

struct ResolvedPointGeometry {
    bool hasBinding = false;
    bool windowFound = false;
    uintptr_t windowId = 0;
    int referenceRelX = 0;
    int referenceRelY = 0;
    int currentRelX = 0;
    int currentRelY = 0;
    int globalX = 0;
    int globalY = 0;
};

ResolvedPointGeometry resolveSequencePoint(const SequencePoint& point, const WindowInfo* currentWindow);
bool pointHasWindowBinding(const SequencePoint& point);

#endif

#include "SequencePointGeometry.h"

#include <QtGlobal>

bool pointHasWindowBinding(const SequencePoint& point)
{
    return point.hasWindowBinding || point.windowId != 0 || !point.windowTitle.empty()
        || !point.processName.empty() || !point.windowClass.empty();
}

ResolvedPointGeometry resolveSequencePoint(const SequencePoint& point, const WindowInfo* currentWindow)
{
    ResolvedPointGeometry result;
    result.hasBinding = pointHasWindowBinding(point);
    result.referenceRelX = point.relX;
    result.referenceRelY = point.relY;
    result.currentRelX = point.relX;
    result.currentRelY = point.relY;
    result.globalX = point.screenX;
    result.globalY = point.screenY;
    if (!result.hasBinding || !currentWindow || !currentWindow->id) return result;

    result.windowFound = true;
    result.windowId = currentWindow->id;
    const double scaleX = point.boundWindowWidth > 0 && currentWindow->width > 0
        ? double(currentWindow->width) / double(point.boundWindowWidth) : 1.0;
    const double scaleY = point.boundWindowHeight > 0 && currentWindow->height > 0
        ? double(currentWindow->height) / double(point.boundWindowHeight) : 1.0;
    result.currentRelX = qRound(point.relX * scaleX);
    result.currentRelY = qRound(point.relY * scaleY);
    result.globalX = currentWindow->x + result.currentRelX;
    result.globalY = currentWindow->y + result.currentRelY;
    return result;
}

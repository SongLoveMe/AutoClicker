#ifndef VISUALDETECTOR_H
#define VISUALDETECTOR_H

#include <QImage>
#include "Workflow.h"

class VisualDetector
{
public:
    static bool matches(const QImage& image, const VisualTransition& transition, QString* detail = nullptr);
private:
    static bool pointInRegion(const QPointF& point, const PolygonRegion& region);
    static bool templatePresent(const QImage& image, const VisualTransition& transition, double* score);
};

#endif

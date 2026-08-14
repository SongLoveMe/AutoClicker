#include "VisualDetector.h"
#include <QBuffer>
#include <QPainterPath>
#include <QtMath>

bool VisualDetector::pointInRegion(const QPointF& point, const PolygonRegion& region) {
    if (region.points.isEmpty()) return true;
    QPainterPath path; path.addPolygon(QPolygonF(region.points));
    return path.contains(point);
}

bool VisualDetector::matches(const QImage& image, const VisualTransition& transition, QString* detail) {
    if (image.isNull()) { if (detail) *detail = QStringLiteral("目标窗口画面无效"); return false; }
    if (transition.type == VisualConditionType::Template) {
        double score = 0.0; const bool present = templatePresent(image, transition, &score);
        if (detail) *detail = QStringLiteral("模板相似度 %1% (%2)").arg(score * 100.0, 0, 'f', 1).arg(present ? QStringLiteral("出现") : QStringLiteral("未出现"));
        return present;
    }
    if (transition.region.points.size() < 3) { if (detail) *detail = QStringLiteral("颜色检测区域无效"); return false; }
    qint64 total = 0, matching = 0; const QColor target = transition.colorCondition.color; const int tolerance = transition.colorCondition.tolerance;
    const QRect bounds = QPolygonF(transition.region.points).boundingRect().toAlignedRect().intersected(image.rect());
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) for (int x = bounds.left(); x <= bounds.right(); ++x) {
        if (!pointInRegion(QPointF(x + 0.5, y + 0.5), transition.region)) continue;
        ++total; const QColor pixel = image.pixelColor(x, y);
        if (qAbs(pixel.red() - target.red()) <= tolerance && qAbs(pixel.green() - target.green()) <= tolerance && qAbs(pixel.blue() - target.blue()) <= tolerance) ++matching;
    }
    const double ratio = total ? double(matching) / double(total) : 0.0;
    const bool result = transition.colorCondition.atLeast ? ratio >= transition.colorCondition.ratio : ratio <= transition.colorCondition.ratio;
    if (detail) *detail = QStringLiteral("颜色占比 %1% / 阈值 %2%").arg(ratio * 100.0, 0, 'f', 1).arg(transition.colorCondition.ratio * 100.0, 0, 'f', 1);
    return result;
}

bool VisualDetector::templatePresent(const QImage& image, const VisualTransition& transition, double* score) {
    QImage templ; templ.loadFromData(transition.templateCondition.pngData, "PNG");
    if (templ.isNull()) { *score = 0.0; return false; }
    const QImage source = image.convertToFormat(QImage::Format_RGB32);
    templ = templ.convertToFormat(QImage::Format_RGB32);
    const QRect search = transition.region.points.size() >= 3
        ? QPolygonF(transition.region.points).boundingRect().toAlignedRect().intersected(source.rect())
        : source.rect();
    if (templ.width() > search.width() || templ.height() > search.height()) { *score = 0.0; return false; }
    // Sample a compact grid. This is deliberately dependency-free and sufficient for stable UI templates.
    double best = 0.0; const int stride = qMax(1, qMin(templ.width(), templ.height()) / 24);
    for (int y = search.top(); y <= search.bottom() - templ.height() + 1; y += 2) for (int x = search.left(); x <= search.right() - templ.width() + 1; x += 2) {
        const QPointF center(x + templ.width() / 2.0, y + templ.height() / 2.0);
        if (transition.region.points.size() >= 3 && !pointInRegion(center, transition.region)) continue;
        qint64 error = 0, samples = 0;
        for (int ty = 0; ty < templ.height(); ty += stride) for (int tx = 0; tx < templ.width(); tx += stride) {
            const QRgb a = source.pixel(x + tx, y + ty), b = templ.pixel(tx, ty);
            error += qAbs(qRed(a)-qRed(b)) + qAbs(qGreen(a)-qGreen(b)) + qAbs(qBlue(a)-qBlue(b)); ++samples;
        }
        const double current = samples ? 1.0 - double(error) / (samples * 765.0) : 0.0;
        best = qMax(best, current); if (best >= transition.templateCondition.similarity) { *score = best; return true; }
    }
    *score = best; return false;
}

#include <QtGui>
#include <QtCore>

#include <iostream>
#include <list>
#include <vector>
#include <assert.h>

#include "depixelator.h"

using namespace depixelator;

static void printHelp(char *app)
{
    qDebug("Usage:");
    qDebug(">%s [options]", app);
    qDebug("Options:");
    qDebug("     --file [file]          Source file to process.");
    qDebug();
    qDebug("     --render-grid          Render the grid");
    qDebug("     --render-source        Render the source image");
    qDebug("     --render-raw           Render the raw contour lines, unprocessed");
    qDebug("     --render-fill          Render the final result as a filled vector path");
    qDebug("     --render-stroke        Render the final result as a stroked vector path");
    qDebug();
    qDebug("     --reduce [threshold]?  Reduce line segments making up the final path");
    qDebug("     --render-points        Render the points, after reduction...");
    qDebug();
    qDebug("     --smooth-before-reduce Run smoothing before reduction..");
    qDebug("     --smooth [fac]? [it]?  Run smoothing of points, [fac] is the factor, 0->1, [it] is number of iterations");
    qDebug();
    qDebug("     --trace-slopes         Run a slope finding algorithm over the line to recreate sloped lines");
    qDebug();
    qDebug("     --cubic-beziers        Turn into a continuous series of cubic beziers, perfect for path rendering..");
    qDebug();
    qDebug(" -h  --help         Print this help...");
}

static void render_grid(QPainter *p, const Bitmap &map)
{
    // Draw a light gray grid in the background..
    p->setRenderHint(QPainter::Antialiasing, false);
    p->setPen(QPen(QColor(200, 200, 200), 0, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin));
    for (unsigned int x=0; x<map.width; ++x) {
        p->drawLine(x, 0, x, map.height);
    }
    for (unsigned int y=0; y<map.height; ++y) {
        p->drawLine(0, y, map.width, y);
    }
}

static void render_bitmap(QPainter *p, const Bitmap &map)
{
    // The draw a red dot for each filled pixel..
    p->setPen(Qt::NoPen);
    p->setBrush(Qt::red);
    p->setRenderHint(QPainter::Antialiasing);
    for (unsigned int y=0; y<map.height; ++y) {
        for (unsigned int x=0; x<map.width; ++x) {
            if (map.checkBit(x, y)) {
                const float rad = 0.2;
                float cx = x + 0.5;
                float cy = y + 0.5;
                p->drawEllipse(QPointF(cx, cy), rad, rad);
            }
        }
    }
}

static void render_polylines(QPainter *p, const std::vector<Polyline> &polylines, QColor color)
{
    QPen pen(color, 1, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);
    pen.setCosmetic(true);
    p->setPen(pen);
    for (const Polyline &poly : polylines) {
        QPolygonF polyline;
        for (Point pt : poly) {
            polyline.append(QPointF(pt.x, pt.y));
        }
        p->drawPolyline(polyline);
    }
}

static void render_polylinePoints(QPainter *p, const std::vector<Polyline> &lines, QColor color)
{
    QPen pen(color, 4, Qt::SolidLine, Qt::RoundCap, Qt::BevelJoin);
    pen.setCosmetic(true);
    p->setPen(pen);
    for (const Polyline &poly : lines) {
        for (Point pt : poly) {
            p->drawPoint(QPointF(pt.x, pt.y));
        }
    }
}

static void render_fillPolylines(QPainter *p, const std::vector<Polyline> &polylines, QColor color)
{
    p->setPen(Qt::NoPen);
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    for (const Polyline &poly : polylines) {
        bool first = true;
        for (Point pt : poly) {
            if (first) {
                path.moveTo(pt.x, pt.y);
                first = false;
            } else {
                path.lineTo(pt.x, pt.y);
            }
        }
    }
    p->fillPath(path, color);
}

static void render_fillCubicPath(QPainter *p, const std::vector<Polyline> &polylines, QColor color)
{
    p->setPen(Qt::NoPen);
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    for (const Polyline &poly : polylines) {
        if (poly.size() < 4)
            continue;
        path.moveTo(poly[0].x, poly[0].y);
        for (unsigned int i=1; i<poly.size(); i+=3) {
            path.cubicTo(poly[i+0].x, poly[i+0].y,
                         poly[i+1].x, poly[i+1].y,
                         poly[i+2].x, poly[i+2].y);
        }

    }
    p->fillPath(path, color);
}

int main(int argc, char **argv)
{
    QString file;
    int scaleFactor = 20;

    bool renderGrid = false;
    bool renderSource = false;
    bool renderRaw = false;
    bool renderFill = false;
    bool renderStroke = false;
    bool renderPoints = false;

    bool processReduce = false;
    float reduceThreshold = 0.0f;

    bool processSmoothBeforeReduce = false;
    bool processSmooth = false;
    int smoothIterations = 0;
    float smoothFactor = 0.0f;

    bool processTraceSlopes = false;

    bool processBezier = false;

    for (int i=1; i<argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--file") && i <= argc - 1) {
            file = QString::fromLocal8Bit(argv[i+1]);
            ++i;
        } else if (arg == QStringLiteral("--scale") && i <= argc - 1) {
            scaleFactor = atoi(argv[i+1]);
            ++i;
        } else if (arg == QStringLiteral("--render-grid")) {
            renderGrid = true;
        } else if (arg == QStringLiteral("--render-source")) {
            renderSource = true;
        } else if (arg == QStringLiteral("--render-raw")) {
            renderRaw = true;
        } else if (arg == QStringLiteral("--render-points")) {
            renderPoints = true;
        } else if (arg == QStringLiteral("--render-fill")) {
            renderFill = true;
        } else if (arg == QStringLiteral("--render-stroke")) {
            renderStroke = true;
        } else if (arg == QStringLiteral("--cubic-beziers")) {
            processBezier = true;
        } else if (arg == QStringLiteral("--trace-slopes")) {
            processTraceSlopes = true;
        } else if (arg == QStringLiteral("--smooth-before-reduce")) {
            processSmoothBeforeReduce = true;
        } else if (arg == QStringLiteral("--reduce")) {
            processReduce = true;
            bool ok = false;
            if (i <= argc - 1) {
                float v = QString::fromLocal8Bit(argv[i+1]).toFloat(&ok);
                if (ok) {
                    reduceThreshold = v;
                    ++i;
                }
            }
            if (!ok) {
                reduceThreshold = 0.01f;
            }
        } else if (arg == QStringLiteral("--smooth")) {
            processSmooth = true;
            bool ok = false;
            if (i <= argc - 1) {
                float v = QString::fromLocal8Bit(argv[i+1]).toFloat(&ok);
                if (ok) {
                    smoothFactor = v;
                    ++i;
                }
            }
            if (!ok) {
                smoothFactor = 0.1f;
            }
            ok = false;
            if (i <= argc - 1) {
                int v = QString::fromLocal8Bit(argv[i+1]).toInt(&ok);
                if (ok) {
                    smoothIterations = v;
                    ++i;
                }
            }
            if (!ok) {
                smoothIterations = 1;
            }
        } else if (arg == QStringLiteral("-h") || arg == QStringLiteral("--help")) {
            printHelp(argv[0]);
            return 0;
        }
    }

    if (file.isEmpty()) {
        printHelp(argv[0]);
        return 0;
    }

    if (!renderGrid
        && !renderSource
        && !renderRaw
        && !renderFill
        && !renderStroke
        && !renderPoints) {
        qDebug("No rendering options specified, choosing defaults...");
        renderGrid = true;
        renderSource = true;
        renderStroke = false;

        renderFill = true;

        processReduce = true;
        reduceThreshold = 0.01f;

        processSmooth = true;
        smoothFactor = 0.1;
        smoothIterations = 10;

        processSmoothBeforeReduce = true;

        processBezier = true;
    }

    qDebug("Using file ...............: %s", qPrintable(file));
    qDebug();
    qDebug("Rendering options:");
    qDebug(" --scale .................: %d", scaleFactor);
    qDebug(" --render-grid ...........: %s", renderGrid ? "yes" : "no");
    qDebug(" --render-source .........: %s", renderSource ? "yes" : "no");
    qDebug(" --render-raw ............: %s", renderRaw ? "yes" : "no");
    qDebug(" --render-fill ...........: %s", renderFill ? "yes" : "no");
    qDebug(" --render-stroke .........: %s", renderStroke ? "yes" : "no");
    qDebug(" --render-points .........: %s", renderPoints ? "yes" : "no");
    qDebug();
    qDebug("Processing Options:");
    qDebug(" --reduce ................: %s, threshold=%f", processReduce ? "yes" : "no", reduceThreshold);
    qDebug(" --smooth ................: %s, factor=%f, iterations=%d", processSmooth ? "yes" : "no", smoothFactor, smoothIterations);
    qDebug(" --smooth-before-reduce ..: %s", processSmoothBeforeReduce ? "yes" : "no");
    qDebug(" --trace-slopes ..........: %s", processTraceSlopes ? "yes" : "no");
    qDebug(" --cubic-beziers .........: %s", processBezier ? "yes" : "no");

    QImage image(file);

    QElapsedTimer timer; timer.start();
    image = image.convertToFormat(QImage::Format_MonoLSB);
    qDebug(" - converted to mono in: %.3fms", timer.nsecsElapsed() / 1000000.0);

    Bitmap map;
    map.data = (unsigned char *) image.bits();
    map.width = image.width();
    map.height= image.height();
    map.stride = image.bytesPerLine();

    QImage result(image.width() * scaleFactor, image.height() * scaleFactor, QImage::Format_RGB32);
    result.fill(Qt::white);

    QPainter p(&result);
    p.scale(scaleFactor, scaleFactor);

    if (renderGrid) {
        render_grid(&p, map);
    }

    p.setRenderHint(QPainter::Antialiasing);

    if (renderSource) {
        render_bitmap(&p, map);
    }

    timer.restart();
    auto polylines = findContours(map);
    qDebug(" - found %d polylines in %.3fms", (int) polylines.size(), timer.nsecsElapsed() / 1000000.0);

    if (renderRaw) {
        render_polylines(&p, polylines, QColor(255, 0, 0, 100));
    }

    if (processTraceSlopes) {
        timer.restart();
        std::vector<Polyline> tmp;
        int originalCount = 0;
        int simplifiedCount = 0;
        for (auto poly : polylines) {
            originalCount += poly.size();
            tmp.push_back(traceSlopes(poly));
            simplifiedCount += tmp.back().size();
        }
        polylines = tmp;
        qDebug(" - traced slopes in %.3fms, %d/%d points remaining",
               timer.nsecsElapsed() / 1000000.0,
               simplifiedCount,
               originalCount);
    }

    if (processSmooth && processSmoothBeforeReduce) {
        timer.restart();
        std::vector<Polyline> smoothedPolylines;
        for (auto poly : polylines) {
            smoothedPolylines.push_back(smoothen(poly, smoothFactor, smoothIterations));
        }
        qDebug(" - smoothed %d polylines in %.3fms", (int) smoothedPolylines.size(), timer.nsecsElapsed() / 1000000.0);
        polylines = smoothedPolylines;
    }

    if (processReduce) {
        timer.restart();
        std::vector<Polyline> simplifiedPolylines;
        int originalCount = 0;
        int simplifiedCount = 0;
        for (auto poly : polylines) {
            originalCount += poly.size();
            simplifiedPolylines.push_back(simplifyRDP(poly, reduceThreshold));
            simplifiedCount += simplifiedPolylines.back().size();
        }
        qDebug(" - simplified, %d->%d pts, (%f%%) in %.3fms",
               originalCount,
               simplifiedCount,
               (simplifiedCount * 100.0 / originalCount),
               timer.nsecsElapsed() / 1000000.0);
        polylines = simplifiedPolylines;
    }

    if (processSmooth && !processSmoothBeforeReduce) {
        timer.restart();
        std::vector<Polyline> smoothedPolylines;
        for (auto poly : polylines) {
            smoothedPolylines.push_back(smoothen(poly, smoothFactor, smoothIterations));
        }
        qDebug(" - smoothed %d polylines in %.3fms", (int) smoothedPolylines.size(), timer.nsecsElapsed() / 1000000.0);
        polylines = smoothedPolylines;
    }

    if (processBezier) {
        timer.restart();
        std::vector<Polyline> paths;
        for (auto poly : polylines) {
            paths.push_back(convertToCubicPath(poly));
        }
        qDebug(" - converted to cubic bezier paths in %.3fms", timer.nsecsElapsed() / 1000000.0);
        polylines = paths;
    }

    if (renderFill) {
        if (processBezier) {
            render_fillCubicPath(&p, polylines, QColor(0, 0, 0, 155));
        } else {
            render_fillPolylines(&p, polylines, QColor(0, 0, 0, 150));
        }
    }

    if (renderStroke) {
        render_polylines(&p, polylines, Qt::black);
    }

    if (renderPoints) {
        render_polylinePoints(&p, polylines, QColor(255, 0, 0, 100));
    }


    p.end();

    result.save("result.png");

    return 0;
}


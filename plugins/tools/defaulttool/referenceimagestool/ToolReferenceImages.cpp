/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "ToolReferenceImages.h"

#include <QDesktopServices>
#include <QFile>
#include <QLayout>
#include <QMenu>
#include <QMessageBox>
#include <QAction>
#include <QApplication>
#include <QPainter>
#include <QtMath>
#include <cmath>

#include <KoSelection.h>
#include <KoPointerEvent.h>
#include <KoShapeRegistry.h>
#include <KoShapeManager.h>
#include <KoShapeController.h>
#include <KoFileDialog.h>
#include <KoInteractionStrategy.h>
#include <commands/KoShapeTransformCommand.h>
#include "KisMimeDatabase.h"

#include <kis_action_registry.h>
#include <kis_canvas2.h>
#include <kis_coordinates_converter.h>
#include <kis_canvas_resource_provider.h>
#include <kis_node_manager.h>
#include <KisViewManager.h>
#include <KisDocument.h>
#include <KisReferenceImagesLayer.h>
#include <kis_image.h>
#include "QClipboard"
#include "kis_action.h"
#include <KisCursorOverrideLock.h>

#include "ToolReferenceImagesWidget.h"
#include "KisReferenceImageCollection.h"

namespace {

QTransform documentToWidgetTransform(const KoViewConverter *converter)
{
    if (const auto *coordinatesConverter = dynamic_cast<const KisCoordinatesConverter*>(converter)) {
        return coordinatesConverter->documentToWidgetTransform();
    }
    return converter->viewToWidget() * converter->documentToView();
}

QPointF documentToWidget(const KoViewConverter *converter, const QPointF &point)
{
    return documentToWidgetTransform(converter).map(point);
}

enum class PinnedHandle {
    Move,
    Rotate,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left
};

class PinnedReferenceTransformStrategy : public KoInteractionStrategy
{
public:
    PinnedReferenceTransformStrategy(KoToolBase *tool, KisReferenceImage *reference,
                                     const QPointF &startDocument,
                                     PinnedHandle handle)
        : KoInteractionStrategy(tool)
        , m_reference(reference)
        , m_startWidget(documentToWidget(tool->canvas()->viewConverter(), startDocument))
        , m_initialTransform(reference->transformation())
        , m_initialInverse(m_initialTransform.inverted())
        , m_outline(reference->outline().toFillPolygon())
        , m_handle(handle)
    {
        const QRectF bounds = m_outline.boundingRect();
        m_initialCenter = m_initialTransform.map(bounds.center());
        m_startLocal = m_initialInverse.map(m_startWidget);

        switch (m_handle) {
        case PinnedHandle::TopLeft: m_anchorLocal = bounds.bottomRight(); break;
        case PinnedHandle::Top: m_anchorLocal = QPointF(bounds.center().x(), bounds.bottom()); break;
        case PinnedHandle::TopRight: m_anchorLocal = bounds.bottomLeft(); break;
        case PinnedHandle::Right: m_anchorLocal = QPointF(bounds.left(), bounds.center().y()); break;
        case PinnedHandle::BottomRight: m_anchorLocal = bounds.topLeft(); break;
        case PinnedHandle::Bottom: m_anchorLocal = QPointF(bounds.center().x(), bounds.top()); break;
        case PinnedHandle::BottomLeft: m_anchorLocal = bounds.topRight(); break;
        case PinnedHandle::Left: m_anchorLocal = QPointF(bounds.right(), bounds.center().y()); break;
        default: break;
        }
        m_anchorWidget = m_initialTransform.map(m_anchorLocal);
        m_startAngle = std::atan2(m_startWidget.y() - m_initialCenter.y(),
                                  m_startWidget.x() - m_initialCenter.x());
    }

    void handleMouseMove(const QPointF &documentPoint, Qt::KeyboardModifiers modifiers) override
    {
        const QPointF widgetPoint = documentToWidget(tool()->canvas()->viewConverter(), documentPoint);
        QTransform transform = m_initialTransform;

        if (m_handle == PinnedHandle::Move) {
            QTransform delta;
            delta.translate(widgetPoint.x() - m_startWidget.x(),
                            widgetPoint.y() - m_startWidget.y());
            transform = delta * m_initialTransform;
        } else if (m_handle == PinnedHandle::Rotate) {
            const qreal currentAngle = std::atan2(widgetPoint.y() - m_initialCenter.y(),
                                                  widgetPoint.x() - m_initialCenter.x());
            qreal angle = qRadiansToDegrees(currentAngle - m_startAngle);
            if (modifiers & (Qt::AltModifier | Qt::ControlModifier)) {
                angle = qRound(angle / 45.0) * 45.0;
            }
            QTransform delta;
            delta.translate(m_initialCenter.x(), m_initialCenter.y());
            delta.rotate(angle);
            delta.translate(-m_initialCenter.x(), -m_initialCenter.y());
            transform = delta * m_initialTransform;
        } else {
            const QPointF currentLocal = m_initialInverse.map(widgetPoint);
            QPointF scale(1.0, 1.0);
            const qreal dx = m_startLocal.x() - m_anchorLocal.x();
            const qreal dy = m_startLocal.y() - m_anchorLocal.y();

            if (m_handle == PinnedHandle::TopLeft || m_handle == PinnedHandle::Top ||
                m_handle == PinnedHandle::TopRight || m_handle == PinnedHandle::BottomLeft ||
                m_handle == PinnedHandle::Bottom || m_handle == PinnedHandle::BottomRight) {
                scale.ry() = safeScale((currentLocal.y() - m_anchorLocal.y()) / dy);
            }
            if (m_handle == PinnedHandle::TopLeft || m_handle == PinnedHandle::TopRight ||
                m_handle == PinnedHandle::Right || m_handle == PinnedHandle::BottomRight ||
                m_handle == PinnedHandle::BottomLeft || m_handle == PinnedHandle::Left) {
                scale.rx() = safeScale((currentLocal.x() - m_anchorLocal.x()) / dx);
            }

            if (m_reference->keepAspectRatio() || (modifiers & Qt::ShiftModifier)) {
                const qreal aspectScale = qAbs(scale.x()) > qAbs(scale.y()) ? scale.x() : scale.y();
                scale = QPointF(aspectScale, aspectScale);
            }

            QTransform delta;
            delta.translate(m_anchorLocal.x(), m_anchorLocal.y());
            delta.scale(scale.x(), scale.y());
            delta.translate(-m_anchorLocal.x(), -m_anchorLocal.y());
            transform = m_initialTransform * delta;
        }

        m_reference->setTransformation(transform);
        m_finalTransform = transform;
        m_changed = true;
    }

    KUndo2Command *createCommand() override
    {
        if (!m_changed || m_finalTransform == m_initialTransform) {
            return nullptr;
        }
        return new KoShapeTransformCommand({m_reference}, {m_initialTransform}, {m_finalTransform});
    }

    void finishInteraction(Qt::KeyboardModifiers) override {}
    void paint(QPainter &, const KoViewConverter &) override {}

private:
    static qreal safeScale(qreal value)
    {
        if (qAbs(value) < 0.001) {
            return value < 0.0 ? -0.001 : 0.001;
        }
        return value;
    }

    KisReferenceImage *m_reference;
    QPointF m_startWidget;
    QTransform m_initialTransform;
    QTransform m_initialInverse;
    QPolygonF m_outline;
    QPointF m_initialCenter;
    QPointF m_startLocal;
    QPointF m_anchorLocal;
    QPointF m_anchorWidget;
    qreal m_startAngle = 0.0;
    PinnedHandle m_handle;
    QTransform m_finalTransform;
    bool m_changed = false;
};

}

ToolReferenceImages::ToolReferenceImages(KoCanvasBase * canvas)
    : DefaultTool(canvas, false)
{
    setObjectName("ToolReferenceImages");

    m_pinAction = new QAction(i18n("Pin to Viewport"), this);
    m_pinAction->setCheckable(true);
    connect(m_pinAction, &QAction::triggered,
            this, &ToolReferenceImages::toggleSelectedReferencePinned);
}

ToolReferenceImages::~ToolReferenceImages()
{
}

void ToolReferenceImages::activate(const QSet<KoShape*> &shapes)
{
    DefaultTool::activate(shapes);

    auto kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT(kisCanvas);
    connect(kisCanvas->image(), SIGNAL(sigNodeAddedAsync(KisNodeSP, KisNodeAdditionFlags)), this, SLOT(slotNodeAdded(KisNodeSP, KisNodeAdditionFlags)));
    connect(kisCanvas->imageView()->document(), &KisDocument::sigReferenceImagesLayerChanged, this, qOverload<KisNodeSP>(&ToolReferenceImages::slotNodeAdded));

    auto referenceImageLayer = document()->referenceImagesLayer();
    if (referenceImageLayer) {
        setReferenceImageLayer(referenceImageLayer);
    }
}

void ToolReferenceImages::deactivate()
{
    cancelRoiCreation();
    DefaultTool::deactivate();
}

void ToolReferenceImages::mousePressEvent(KoPointerEvent *event)
{
    if (event->button() & Qt::RightButton) {
        KoShape *shape = shapeAt(event->point, KoFlake::ShapeOnTop);
        if (shape && koSelection()) {
            if (!(event->modifiers() & Qt::ShiftModifier)) {
                koSelection()->deselectAll();
            }
            koSelection()->select(shape);
            event->accept();
            repaintDecorations();
            return;
        }
    }

    if (m_roiCreationMode && (event->button() & Qt::LeftButton)) {
        m_roiImage = selectedEmbeddedReferenceImage();
        if (!m_roiImage) {
            cancelRoiCreation();
            event->ignore();
            return;
        }

        m_roiDragStart = m_roiImage->documentToShape(event->point);
        m_roiDragEnd = m_roiDragStart;
        m_roiDragging = true;
        event->accept();
        repaintDecorations();
        return;
    }

    DefaultTool::mousePressEvent(event);
}

void ToolReferenceImages::mouseMoveEvent(KoPointerEvent *event)
{
    if (m_roiDragging) {
        m_roiDragEnd = m_roiImage->documentToShape(event->point);
        event->accept();
        repaintDecorations();
        return;
    }

    DefaultTool::mouseMoveEvent(event);
}

void ToolReferenceImages::mouseReleaseEvent(KoPointerEvent *event)
{
    if (m_roiDragging && (event->button() & Qt::LeftButton)) {
        m_roiDragEnd = m_roiImage->documentToShape(event->point);
        KisReferenceImage *reference = selectedEmbeddedReferenceImage();
        const bool appliesToOriginalSelection = reference && reference == m_roiImage;
        m_roiDragging = false;
        m_roiCreationMode = false;
        m_roiImage = nullptr;

        if (appliesToOriginalSelection) {
            if (reference->applyRoi(QRectF(m_roiDragStart, m_roiDragEnd))) {
                document()->setModified(true);
            }
        }

        if (m_optionsWidget && koSelection()) {
            m_optionsWidget->selectionChanged(koSelection());
        }

        event->accept();
        repaintDecorations();
        return;
    }

    DefaultTool::mouseReleaseEvent(event);
}

void ToolReferenceImages::paint(QPainter &painter, const KoViewConverter &converter)
{
    DefaultTool::paint(painter, converter);

    if (!m_roiDragging) {
        return;
    }

    const QRectF roiRect(m_roiDragStart, m_roiDragEnd);

    painter.save();
    painter.setTransform(m_roiImage->absoluteTransformation() *
                         converter.documentToView() *
                         painter.transform());

    QPen pen(QColor(8, 60, 167, 204), 1.0, Qt::DashLine);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(roiRect.normalized());
    painter.restore();
}

QRectF ToolReferenceImages::decorationsRect() const
{
    QRectF result = DefaultTool::decorationsRect();
    if (m_roiDragging) {
        const QPointF margin = canvas()->viewConverter()->viewToDocument(QPointF(2.0, 2.0));
        QRectF roiRect = m_roiImage->shapeToDocument(QRectF(m_roiDragStart, m_roiDragEnd).normalized());
        roiRect.adjust(-margin.x(), -margin.y(), margin.x(), margin.y());
        result |= roiRect;
    }
    return result;
}

void ToolReferenceImages::slotNodeAdded(KisNodeSP node)
{
    slotNodeAdded(node, KisNodeAdditionFlag::None);
}

void ToolReferenceImages::slotNodeAdded(KisNodeSP node, KisNodeAdditionFlags flags)
{
    Q_UNUSED(flags)

    auto *referenceImagesLayer = dynamic_cast<KisReferenceImagesLayer*>(node.data());

    if (referenceImagesLayer) {
        setReferenceImageLayer(referenceImagesLayer);
    }
}

void ToolReferenceImages::setReferenceImageLayer(KisSharedPtr<KisReferenceImagesLayer> layer)
{
    m_layer = layer;
    connect(layer.data(), SIGNAL(selectionChanged()), this, SLOT(slotSelectionChanged()));
    connect(layer->shapeManager(), SIGNAL(selectionChanged()), this, SLOT(repaintDecorations()));
    connect(layer->shapeManager(), SIGNAL(selectionContentChanged()), this, SLOT(repaintDecorations()));
}

bool ToolReferenceImages::hasSelection()
{
    const KoShapeManager *manager = shapeManager();
    return manager && manager->selection()->count() != 0;
}

void ToolReferenceImages::addReferenceImage()
{
    auto kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);

            KoFileDialog dialog(kisCanvas->viewManager()->mainWindowAsQWidget(), KoFileDialog::OpenFile, "OpenReferenceImage");
    dialog.setCaption(i18n("Select a Reference Image"));

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    if (!locations.isEmpty()) {
        dialog.setDefaultDir(locations.first());
    }

    QString filename = dialog.filename();
    if (filename.isEmpty()) return;
    if (!QFileInfo(filename).exists()) return;

    auto *reference = KisReferenceImage::fromFile(filename, *kisCanvas->coordinatesConverter(), canvas()->canvasWidget());
    if (reference) {
        if (document()->referenceImagesLayer()) {
            reference->setZIndex(document()->referenceImagesLayer()->shapes().size());
        }
        canvas()->addCommand(KisReferenceImagesLayer::addReferenceImages(document(), {reference}));
    }
}

void ToolReferenceImages::addReferenceImageFromLayer()
{
    KisCanvas2* kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);
    kisCanvas->viewManager()->nodeManager()->createReferenceImageFromLayer();
}

void ToolReferenceImages::addReferenceImageFromVisible()
{
    KisCanvas2* kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);
    kisCanvas->viewManager()->nodeManager()->createReferenceImageFromVisible();
}

void ToolReferenceImages::pasteReferenceImage()
{
    KisCanvas2* kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);

    KisReferenceImage* reference = KisReferenceImage::fromClipboard(*kisCanvas->coordinatesConverter());
    if (reference) {
        if (document()->referenceImagesLayer()) {
            reference->setZIndex(document()->referenceImagesLayer()->shapes().size());
        }
        canvas()->addCommand(KisReferenceImagesLayer::addReferenceImages(document(), {reference}));
    } else {
        if (canvas()->canvasWidget()) {
            QMessageBox::critical(canvas()->canvasWidget(), i18nc("@title:window", "Krita"), i18n("Could not load reference image from clipboard"));
        }
    }
}

void ToolReferenceImages::removeSelectedReferenceImages()
{
    auto layer = m_layer.toStrongRef();
    if (!layer) return;
    if (!koSelection()) return;
    if (koSelection()->selectedEditableShapes().isEmpty()) return;

    canvas()->addCommand(layer->removeReferenceImages(document(), koSelection()->selectedEditableShapes()));
}

void ToolReferenceImages::removeAllReferenceImages()
{
    auto layer = m_layer.toStrongRef();
    if (!layer) return;

    canvas()->addCommand(layer->removeReferenceImages(document(), layer->shapes()));
}

void ToolReferenceImages::loadReferenceImages()
{
    auto kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);

            KoFileDialog dialog(kisCanvas->viewManager()->mainWindowAsQWidget(), KoFileDialog::OpenFile, "OpenReferenceImageCollection");
    dialog.setMimeTypeFilters(QStringList() << "application/x-krita-reference-images");
    dialog.setCaption(i18n("Load Reference Images"));

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    if (!locations.isEmpty()) {
        dialog.setDefaultDir(locations.first());
    }

    QString filename = dialog.filename();
    if (filename.isEmpty()) return;
    if (!QFileInfo(filename).exists()) return;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("Could not open '%1'.", filename));
        return;
    }

    KisReferenceImageCollection collection;

    int currentZIndex = 0;
    if (document()->referenceImagesLayer()) {
        currentZIndex = document()->referenceImagesLayer()->shapes().size();
    }

    if (collection.load(&file)) {
        QList<KoShape*> shapes;
        Q_FOREACH(auto *reference, collection.referenceImages()) {
            reference->setZIndex(currentZIndex);
            shapes.append(reference);
            currentZIndex += 1;
        }

        canvas()->addCommand(KisReferenceImagesLayer::addReferenceImages(document(), shapes));
    } else {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("Could not load reference images from '%1'.", filename));
    }
    file.close();
}

void ToolReferenceImages::saveReferenceImages()
{
    KisCursorOverrideLock cursorLock(Qt::BusyCursor);

    auto layer = m_layer.toStrongRef();
    if (!layer || layer->shapeCount() == 0) return;

    auto kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);

            KoFileDialog dialog(kisCanvas->viewManager()->mainWindowAsQWidget(), KoFileDialog::SaveFile, "SaveReferenceImageCollection");
    QString mimetype = "application/x-krita-reference-images";
    dialog.setMimeTypeFilters(QStringList() << mimetype, mimetype);
    dialog.setCaption(i18n("Save Reference Images"));

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    if (!locations.isEmpty()) {
        dialog.setDefaultDir(locations.first());
    }

    QString filename = dialog.filename();
    if (filename.isEmpty()) return;

    QString fileMime = KisMimeDatabase::mimeTypeForFile(filename, false);
    if (fileMime != "application/x-krita-reference-images") {
        filename.append(filename.endsWith(".") ? "krf" : ".krf");
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("Could not open '%1' for saving.", filename));
        return;
    }

    KisReferenceImageCollection collection(layer->referenceImages());
    bool ok = collection.save(&file);
    file.close();

    if (!ok) {
        QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Krita"), i18n("Failed to save reference images."));
    }
}

void ToolReferenceImages::slotSelectionChanged()
{
    auto layer = m_layer.toStrongRef();
    if (!layer) return;

    cancelRoiCreation();
    m_optionsWidget->selectionChanged(layer->shapeManager()->selection());
    if (m_pinAction) {
        KisReferenceImage *reference = selectedReferenceImage();
        m_pinAction->setEnabled(reference != nullptr);
        m_pinAction->setChecked(reference && reference->pinned());
    }
    updateActions();
}

void ToolReferenceImages::beginRoiCreation()
{
    if (!selectedEmbeddedReferenceImage()) {
        return;
    }

    m_roiCreationMode = true;
    m_roiDragging = false;
    m_roiImage = nullptr;
}

void ToolReferenceImages::clearRoi()
{
    KisReferenceImage *reference = selectedEmbeddedReferenceImage();
    if (!reference || !reference->hasRoi()) {
        return;
    }

    if (reference->clearRoi()) {
        document()->setModified(true);
    }
    if (m_optionsWidget && koSelection()) {
        m_optionsWidget->selectionChanged(koSelection());
    }
}

QList<QPointer<QWidget>> ToolReferenceImages::createOptionWidgets()
{
    // Instead of inheriting DefaultTool's multi-tab implementation, inherit straight from KoToolBase
    return KoToolBase::createOptionWidgets();
}

QWidget *ToolReferenceImages::createOptionWidget()
{
    if (!m_optionsWidget) {
        m_optionsWidget = new ToolReferenceImagesWidget(this);
        // See https://bugs.kde.org/show_bug.cgi?id=316896
        QWidget *specialSpacer = new QWidget(m_optionsWidget);
        specialSpacer->setObjectName("SpecialSpacer");
        specialSpacer->setFixedSize(0, 0);
        m_optionsWidget->layout()->addWidget(specialSpacer);
    }
    return m_optionsWidget;
}

bool ToolReferenceImages::isValidForCurrentLayer() const
{
    return true;
}

KoShapeManager *ToolReferenceImages::shapeManager() const
{
    auto layer = m_layer.toStrongRef();
    return layer ? layer->shapeManager() : nullptr;
}

KoShape *ToolReferenceImages::shapeAt(const QPointF &documentPoint,
                                      KoFlake::ShapeSelection selection) const
{
    auto layer = m_layer.toStrongRef();
    if (!layer || !canvas()->viewConverter()) {
        return nullptr;
    }

    return layer->shapeAt(documentPoint,
                          documentToWidget(canvas()->viewConverter(), documentPoint),
                          selection);
}

void ToolReferenceImages::paintSelectionDecorations(QPainter &painter,
                                                     const KoViewConverter &converter)
{
    Q_UNUSED(converter);

    KisReferenceImage *reference = selectedReferenceImage();
    if (!reference || !reference->pinned()) {
        DefaultTool::paintSelectionDecorations(painter, converter);
        return;
    }

    const QPolygonF outline = reference->absoluteTransformation().map(reference->outline().toFillPolygon());
    if (outline.size() < 4) {
        return;
    }

    painter.save();
    painter.setTransform(QTransform());
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(QColor(8, 60, 167, 204), 1.0, Qt::DashLine);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(outline);

    const qreal radius = handleRadius();
    painter.setPen(QPen(Qt::white, 1.0));
    painter.setBrush(QColor(8, 60, 167, 220));
    for (int i = 0; i < 4; ++i) {
        painter.drawRect(QRectF(outline.at(i) - QPointF(radius, radius),
                                QSizeF(2.0 * radius, 2.0 * radius)));
    }
    for (int i = 0; i < 4; ++i) {
        const QPointF center = 0.5 * (outline.at(i) + outline.at((i + 1) % 4));
        painter.drawRect(QRectF(center - QPointF(radius, radius),
                                QSizeF(2.0 * radius, 2.0 * radius)));
    }

    const QPointF topMiddle = 0.5 * (outline.at(0) + outline.at(1));
    const QPointF center = reference->absoluteTransformation().map(reference->outline().boundingRect().center());
    QPointF outward = topMiddle - center;
    const qreal outwardLength = std::hypot(outward.x(), outward.y());
    outward = outwardLength > 0.0 ? outward / outwardLength * 30.0 : QPointF(0.0, -30.0);
    const QPointF rotationHandle = topMiddle + outward;
    painter.drawLine(topMiddle, rotationHandle);
    painter.drawEllipse(rotationHandle, radius, radius);

    painter.restore();
}

KoInteractionStrategy *ToolReferenceImages::createStrategy(KoPointerEvent *event)
{
    KisReferenceImage *reference = selectedReferenceImage();
    if (!reference && !(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier))) {
        reference = dynamic_cast<KisReferenceImage*>(shapeAt(event->point, KoFlake::ShapeOnTop));
        if (reference && reference->pinned() && koSelection()) {
            koSelection()->deselectAll();
            koSelection()->select(reference);
        }
    }
    if (!reference || !reference->pinned() || (event->button() & Qt::RightButton)) {
        return DefaultTool::createStrategy(event);
    }

    const QPointF widgetPoint = documentToWidget(canvas()->viewConverter(), event->point);
    const QPolygonF outline = reference->absoluteTransformation().map(reference->outline().toFillPolygon());
    if (outline.size() < 4) {
        return DefaultTool::createStrategy(event);
    }

    const qreal threshold = qMax<qreal>(handleRadius() * 1.75, 10.0);
    auto closeTo = [threshold, widgetPoint](const QPointF &point) {
        return kisSquareDistance(point, widgetPoint) <= threshold * threshold;
    };

    PinnedHandle handle = PinnedHandle::Move;
    const QPointF topMiddle = 0.5 * (outline.at(0) + outline.at(1));
    const QPointF center = reference->absoluteTransformation().map(reference->outline().boundingRect().center());
    QPointF outward = topMiddle - center;
    const qreal outwardLength = std::hypot(outward.x(), outward.y());
    outward = outwardLength > 0.0 ? outward / outwardLength * 30.0 : QPointF(0.0, -30.0);
    if (closeTo(topMiddle + outward)) {
        handle = PinnedHandle::Rotate;
    } else if (closeTo(outline.at(0))) {
        handle = PinnedHandle::TopLeft;
    } else if (closeTo(outline.at(1))) {
        handle = PinnedHandle::TopRight;
    } else if (closeTo(outline.at(2))) {
        handle = PinnedHandle::BottomRight;
    } else if (closeTo(outline.at(3))) {
        handle = PinnedHandle::BottomLeft;
    } else {
        const QPointF edgeCenters[] = {
            0.5 * (outline.at(0) + outline.at(1)),
            0.5 * (outline.at(1) + outline.at(2)),
            0.5 * (outline.at(2) + outline.at(3)),
            0.5 * (outline.at(3) + outline.at(0))
        };
        const PinnedHandle edgeHandles[] = {
            PinnedHandle::Top, PinnedHandle::Right,
            PinnedHandle::Bottom, PinnedHandle::Left
        };
        for (int i = 0; i < 4; ++i) {
            if (closeTo(edgeCenters[i])) {
                handle = edgeHandles[i];
                break;
            }
        }
    }

    if (handle == PinnedHandle::Move &&
        !outline.containsPoint(widgetPoint, Qt::OddEvenFill)) {
        return DefaultTool::createStrategy(event);
    }

    return new PinnedReferenceTransformStrategy(this, reference, event->point, handle);
}

KoSelection *ToolReferenceImages::koSelection() const
{
    auto manager = shapeManager();
    return manager ? manager->selection() : nullptr;
}

KisReferenceImage *ToolReferenceImages::selectedEmbeddedReferenceImage() const
{
    KoSelection *selection = koSelection();
    if (!selection) {
        return nullptr;
    }

    const QList<KoShape*> shapes = selection->selectedEditableShapes();
    if (shapes.size() != 1) {
        return nullptr;
    }

    auto *reference = dynamic_cast<KisReferenceImage*>(shapes.first());
    return reference && reference->embed() ? reference : nullptr;
}

KisReferenceImage *ToolReferenceImages::selectedReferenceImage() const
{
    KoSelection *selection = koSelection();
    if (!selection) {
        return nullptr;
    }

    const QList<KoShape*> shapes = selection->selectedEditableShapes();
    if (shapes.size() != 1) {
        return nullptr;
    }

    return dynamic_cast<KisReferenceImage*>(shapes.first());
}

void ToolReferenceImages::toggleSelectedReferencePinned()
{
    KisReferenceImage *reference = selectedReferenceImage();
    if (!reference || !canvas()->viewConverter()) {
        return;
    }

    const bool pin = m_pinAction && m_pinAction->isChecked();
    canvas()->addCommand(new KisReferenceImage::SetPinnedCommand(
            reference, pin, documentToWidgetTransform(canvas()->viewConverter())));
    document()->setModified(true);
    repaintDecorations();
}

void ToolReferenceImages::cancelRoiCreation()
{
    const bool hadDrag = m_roiDragging;
    m_roiCreationMode = false;
    m_roiDragging = false;
    m_roiImage = nullptr;
    if (hadDrag) {
        repaintDecorations();
    }
}

void ToolReferenceImages::updateDistinctiveActions(const QList<KoShape*> &)
{
    action("object_group")->setEnabled(false);
    action("object_unite")->setEnabled(false);
    action("object_intersect")->setEnabled(false);
    action("object_subtract")->setEnabled(false);
    action("object_split")->setEnabled(false);
    action("object_ungroup")->setEnabled(false);
}

void ToolReferenceImages::deleteSelection()
{
    auto layer = m_layer.toStrongRef();
    if (!layer) return;

    QList<KoShape *> shapes = koSelection()->selectedShapes();

    if (!shapes.empty()) {
        canvas()->addCommand(layer->removeReferenceImages(document(), shapes));
    }
}

QMenu* ToolReferenceImages::popupActionsMenu()
{
    if (m_contextMenu) {
        m_contextMenu->clear();
        m_contextMenu->addSection(i18n("Reference Image Actions"));
        m_contextMenu->addSeparator();

        KisReferenceImage *reference = selectedReferenceImage();
        m_pinAction->setEnabled(reference != nullptr);
        m_pinAction->setChecked(reference && reference->pinned());
        m_contextMenu->addAction(m_pinAction);

        QMenu *transform = m_contextMenu->addMenu(i18n("Transform"));

        transform->addAction(action("object_transform_rotate_90_cw"));
        transform->addAction(action("object_transform_rotate_90_ccw"));
        transform->addAction(action("object_transform_rotate_180"));
        transform->addSeparator();
        transform->addAction(action("object_transform_mirror_horizontally"));
        transform->addAction(action("object_transform_mirror_vertically"));
        transform->addSeparator();
        transform->addAction(action("object_transform_reset"));

        m_contextMenu->addSeparator();

        m_contextMenu->addAction(action("edit_cut"));
        m_contextMenu->addAction(action("edit_copy"));
        m_contextMenu->addAction(action("edit_paste"));

        m_contextMenu->addSeparator();

        m_contextMenu->addAction(action("object_order_front"));
        m_contextMenu->addAction(action("object_order_raise"));
        m_contextMenu->addAction(action("object_order_lower"));
        m_contextMenu->addAction(action("object_order_back"));
    }

    return m_contextMenu.data();
}

void ToolReferenceImages::cut()
{
    copy();
    deleteSelection();
}

void ToolReferenceImages::copy() const
{
    QList<KoShape *> shapes = koSelection()->selectedShapes();
    if (!shapes.isEmpty()) {
        KoShape* shape = shapes.at(0);
        KisReferenceImage *reference = dynamic_cast<KisReferenceImage*>(shape);
        KIS_SAFE_ASSERT_RECOVER_RETURN(reference);
        QClipboard *cb = QApplication::clipboard();
        cb->setImage(reference->getImage());
    }
}

bool ToolReferenceImages::paste()
{
    pasteReferenceImage();
    return true;
}

bool ToolReferenceImages::selectAll()
{
    Q_FOREACH(KoShape *shape, shapeManager()->shapes()) {
        if (!shape->isSelectable()) continue;
        koSelection()->select(shape);
    }
    repaintDecorations();

    return true;
}

void ToolReferenceImages::deselect()
{
    koSelection()->deselectAll();
    repaintDecorations();
}

KisDocument *ToolReferenceImages::document() const
{
    auto kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    KIS_ASSERT(kisCanvas);
    return kisCanvas->imageView()->document();
}

QList<QAction *> ToolReferenceImagesFactory::createActionsImpl()
{
    QList<QAction *> defaultActions = DefaultToolFactory::createActionsImpl();
    QList<QAction *> actions;

    QStringList actionNames;
    actionNames << "object_order_front"
                << "object_order_raise"
                << "object_order_lower"
                << "object_order_back"
                << "object_transform_rotate_90_cw"
                << "object_transform_rotate_90_ccw"
                << "object_transform_rotate_180"
                << "object_transform_mirror_horizontally"
                << "object_transform_mirror_vertically"
                << "object_transform_reset";

    Q_FOREACH(QAction *action, defaultActions) {
        if (actionNames.contains(action->objectName())) {
            actions << action;
        } else {
            delete action;
        }
    }
    return actions;
}

/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef TOOL_REFERENCE_IMAGES_H
#define TOOL_REFERENCE_IMAGES_H

#include <QPointer>
#include <QPointF>
#include <QRectF>

#include <KoToolFactoryBase.h>
#include <KoIcon.h>

#include <kis_tool.h>
#include "kis_painting_assistant.h"
#include <kis_icon.h>
#include <kis_canvas2.h>

#include <defaulttool/DefaultTool.h>
#include <defaulttool/DefaultToolFactory.h>

class ToolReferenceImagesWidget;
class KisReferenceImage;
class KisReferenceImagesLayer;
class QPainter;

class ToolReferenceImages : public DefaultTool
{
    Q_OBJECT

public:
    ToolReferenceImages(KoCanvasBase * canvas);
    ~ToolReferenceImages() override;

    virtual quint32 priority() {
        return 3;
    }

    void mouseDoubleClickEvent(KoPointerEvent */*event*/) override {}
    void mousePressEvent(KoPointerEvent *event) override;
    void mouseMoveEvent(KoPointerEvent *event) override;
    void mouseReleaseEvent(KoPointerEvent *event) override;
    void paint(QPainter &painter, const KoViewConverter &converter) override;
    QRectF decorationsRect() const override;

    bool hasSelection() override;

    void deleteSelection() override;

    QMenu* popupActionsMenu() override;

protected:
    QList<QPointer<QWidget>> createOptionWidgets() override;
    QWidget *createOptionWidget() override;

    bool isValidForCurrentLayer() const override;
    KoShapeManager *shapeManager() const override;
    KoSelection *koSelection() const override;

    void updateDistinctiveActions(const QList<KoShape*> &editableShapes) override;

public Q_SLOTS:
    void activate(const QSet<KoShape*> &shapes) override;
    void deactivate() override;

    void addReferenceImage();
    void pasteReferenceImage();
    void addReferenceImageFromLayer();
    void addReferenceImageFromVisible();
    void removeSelectedReferenceImages();
    void removeAllReferenceImages();
    void saveReferenceImages();
    void loadReferenceImages();
    void beginRoiCreation();
    void clearRoi();

    void slotNodeAdded(KisNodeSP node);
    void slotNodeAdded(KisNodeSP node, KisNodeAdditionFlags flags);
    void slotSelectionChanged();

    void cut() override;
    void copy() const override;
    bool paste() override;

    bool selectAll() override;
    void deselect() override;


private:
    friend class ToolReferenceImagesWidget;
    ToolReferenceImagesWidget *m_optionsWidget = nullptr;
    KisWeakSharedPtr<KisReferenceImagesLayer> m_layer;
    KisReferenceImage *m_roiImage = nullptr;
    QPointF m_roiDragStart;
    QPointF m_roiDragEnd;
    bool m_roiCreationMode = false;
    bool m_roiDragging = false;

    KisDocument *document() const;
    void setReferenceImageLayer(KisSharedPtr<KisReferenceImagesLayer> layer);
    KisReferenceImage *selectedEmbeddedReferenceImage() const;
    void cancelRoiCreation();
};


class ToolReferenceImagesFactory : public DefaultToolFactory
{
public:
    ToolReferenceImagesFactory()
    : DefaultToolFactory("ToolReferenceImages") {
        setToolTip(i18n("Reference Images Tool"));
        setSection(ToolBoxSection::View);
        setIconName(koIconNameCStr("krita_tool_reference_images"));
        setPriority(2);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    };


    ~ToolReferenceImagesFactory() override {}

    KoToolBase * createTool(KoCanvasBase * canvas) override {
        return new ToolReferenceImages(canvas);
    }

    QList<QAction *> createActionsImpl() override;

};


#endif

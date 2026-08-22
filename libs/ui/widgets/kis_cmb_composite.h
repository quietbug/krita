/*
 *  widgets/kis_cmb_composite.h - part of KImageShop/Krayon/Krita
 *
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt (boud@valdyas.org)
 *  SPDX-FileCopyrightText: 2011 Silvio Heinrich <plassy@web.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_COMPOSITEOP_WIDGETS_H_
#define KIS_COMPOSITEOP_WIDGETS_H_

#include <KisSqueezedComboBox.h>
#include <kritaui_export.h>
#include "kis_categorized_list_view.h"

class KoID;
class KoColorSpace;
class KisSortedCompositeOpListModel;
class KisAction;
class KisActionManager;

class KRITAUI_EXPORT KisCompositeOpListWidget: public KisCategorizedListView
{
    Q_OBJECT
public:
     KisCompositeOpListWidget(QWidget* parent = 0);
     ~KisCompositeOpListWidget() override;

    KoID selectedCompositeOp() const;

public Q_SLOTS:
    void setCompositeOp(const KoID &id);

private:
    KisSortedCompositeOpListModel *m_model;
};


class KRITAUI_EXPORT KisCompositeOpComboBox: public KisSqueezedComboBox
{
    Q_OBJECT
public:
     KisCompositeOpComboBox(QWidget* parent = 0);
     KisCompositeOpComboBox(bool limitToLayerStyles, QWidget* parent = 0);
    ~KisCompositeOpComboBox() override;

    void showPopup() override;
    void hidePopup() override;

    void validate(const KoColorSpace *cs);
    void selectCompositeOp(const KoID &op);
    KoID selectedCompositeOp() const;

    /**
     * Enables the preview/confirmation protocol used by the Layers docker.
     * Other users of this widget keep the standard QComboBox behaviour.
     */
    void setPreviewEnabled(bool value);
    bool previewEnabled() const;
    bool previewActive() const;
    bool previewCommitInProgress() const;

    void connectBlendmodeActions(KisActionManager *manager);

    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

Q_SIGNALS:
    void sigPreviewPopupOpened();
    void sigPreviewRequested(const QString &compositeOpId);
    void sigPreviewConfirmed(const QString &compositeOpId);
    void sigPreviewCancelled();

private Q_SLOTS:
    void slotCategoryToggled(const QModelIndex& index, bool toggled);
    void slotEntryChecked(const QModelIndex& index);
    void slotPreviewIndex(const QModelIndex &index);
    void slotPreviewIndexActivated(const QModelIndex &index);

    void slotNextBlendingMode();
    void slotPreviousBlendingMode();
    void slotNormal();
    void slotDissolve();
    void slotBehind();
    void slotClear();
    void slotDarken();
    void slotMultiply();
    void slotColorBurn();
    void slotLinearBurn();
    void slotLighten();
    void slotScreen();
    void slotColorDodge();
    void slotLinearDodge();
    void slotOverlay();
    void slotHardOverlay();
    void slotSoftLight();
    void slotHardLight();
    void slotVividLight();
    void slotLinearLight();
    void slotPinLight();
    void slotHardMix();
    void slotDifference();
    void slotExclusion();
    void slotHue();
    void slotSaturation();
    void slotColor();
    void slotLuminosity();

private:
    void selectNeighbouringBlendMode(bool down);
    bool isPreviewableIndex(const QModelIndex &index) const;
    void requestPreview(const QModelIndex &index);
    void confirmPreview(const QModelIndex &index);
    void cancelPreview();

private:
    KisSortedCompositeOpListModel *m_model;
    KisCategorizedListView *m_view;
    bool m_allowToHidePopup;
    bool m_previewEnabled {false};
    bool m_previewActive {false};
    bool m_previewCommitInProgress {false};
    QModelIndex m_previewIndex;
};

class KRITAUI_EXPORT KisLayerStyleCompositeOpComboBox: public KisCompositeOpComboBox
{
public:
    KisLayerStyleCompositeOpComboBox(QWidget* parent = 0);
};

#endif // KIS_COMPOSITEOP_WIDGETS_H_

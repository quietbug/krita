/*
 *  kis_cmb_composite.cc - part of KImageShop/Krayon/Krita
 *
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt (boud@valdyas.org)
 *  SPDX-FileCopyrightText: 2011 Silvio Heinrich <plassy@web.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_cmb_composite.h"

#include <KoCompositeOp.h>
#include <KoCompositeOpRegistry.h>

#include "kis_composite_ops_model.h"
#include "kis_categorized_item_delegate.h"
#include <kis_action.h>
#include <QKeyEvent>
#include <QTimer>
#include <QWheelEvent>
#include "kis_action_manager.h"

//////////////////////////////////////////////////////////////////////////////////////////
// ---- KisCompositeOpListWidget ------------------------------------------------------ //

KisCompositeOpListWidget::KisCompositeOpListWidget(QWidget* parent):
    KisCategorizedListView(parent),
    m_model(new KisSortedCompositeOpListModel(false, this))
{
    setModel(m_model);
    setItemDelegate(new KisCategorizedItemDelegate(this));
}

KisCompositeOpListWidget::~KisCompositeOpListWidget()
{
}

KoID KisCompositeOpListWidget::selectedCompositeOp() const {
    KoID op;

    if (m_model->entryAt(op, currentIndex())) {
        return op;
    }

    return KoCompositeOpRegistry::instance().getDefaultCompositeOp();
}

void KisCompositeOpListWidget::setCompositeOp(const KoID &id)
{
    const QModelIndex index = m_model->indexOf(id);
    if (index.isValid()) {
        setCurrentIndex(index);
    } else {
        qWarning() << "KisCompositeOpListWidget::setCompositeOp: ailed to find index for blendmode" << ppVar(id);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
// ---- KisCompositeOpComboBox -------------------------------------------------------- //

KisCompositeOpComboBox::KisCompositeOpComboBox(QWidget* parent)
    : KisCompositeOpComboBox(false, parent)
{
}

KisCompositeOpComboBox::KisCompositeOpComboBox(bool limitToLayerStyles, QWidget* parent)
    : KisSqueezedComboBox(parent),
      m_model(new KisSortedCompositeOpListModel(limitToLayerStyles, this)),
      m_allowToHidePopup(true)
{
    m_view = new KisCategorizedListView();
    m_view->setCompositeBoxControl(true);

    setMaxVisibleItems(100);
    setSizeAdjustPolicy(AdjustToContents);
    m_view->setResizeMode(QListView::Adjust);

    setToolTip(i18n("Blending Mode"));

    setModel(m_model);
    setView(m_view);
    setItemDelegate(new KisCategorizedItemDelegate(this));

    connect(m_view, SIGNAL(sigCategoryToggled(QModelIndex,bool)), SLOT(slotCategoryToggled(QModelIndex,bool)));
    connect(m_view, SIGNAL(sigEntryChecked(QModelIndex)), SLOT(slotEntryChecked(QModelIndex)));
    connect(m_view, &QListView::entered, this, &KisCompositeOpComboBox::slotPreviewIndex);
    connect(m_view, &KisCategorizedListView::sigCompositeItemPressed,
            this, [this] (const QModelIndex &index, bool checkboxPressed) {
                const bool categoryPressed = index.data(__CategorizedListModelBase::IsHeaderRole).toBool();

                if (m_previewActive && !categoryPressed) {
                    m_view->setCompositeItemPressConsumed(true);

                    if (checkboxPressed) {
                        cancelPreview();
                        QComboBox::hidePopup();
                    } else {
                        slotPreviewIndexActivated(index);
                    }
                } else if (m_previewActive && categoryPressed) {
                    // Category clicks cancel the current preview, but must
                    // continue through QListView so the category can toggle.
                    cancelPreview();
                }
            });

    selectCompositeOp(KoCompositeOpRegistry::instance().getDefaultCompositeOp());
}

KisCompositeOpComboBox::~KisCompositeOpComboBox()
{
    delete m_view;
}

void KisCompositeOpComboBox::setPreviewEnabled(bool value)
{
    if (m_previewEnabled == value) return;

    if (m_previewActive) {
        cancelPreview();
    }

    m_previewEnabled = value;
    m_view->setMouseTracking(value);
}

bool KisCompositeOpComboBox::previewEnabled() const
{
    return m_previewEnabled;
}

bool KisCompositeOpComboBox::previewActive() const
{
    return m_previewActive;
}

bool KisCompositeOpComboBox::previewCommitInProgress() const
{
    return m_previewCommitInProgress;
}

void KisCompositeOpComboBox::showPopup()
{
    QComboBox::showPopup();

    if (!m_previewEnabled || m_previewActive) return;

    m_previewActive = true;
    m_previewIndex = QModelIndex();
    Q_EMIT sigPreviewPopupOpened();
}

void KisCompositeOpComboBox::connectBlendmodeActions(KisActionManager *manager)
{
    KisAction *action = 0;

    action = manager->createAction("Next Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotNextBlendingMode()));

    action = manager->createAction("Previous Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotPreviousBlendingMode()));

    action = manager->createAction("Select Normal Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotNormal()));

    action = manager->createAction("Select Dissolve Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotDissolve()));

    action = manager->createAction("Select Behind Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotBehind()));

    action = manager->createAction("Select Clear Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotClear()));

    action = manager->createAction("Select Darken Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotDarken()));

    action = manager->createAction("Select Multiply Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotMultiply()));

    action = manager->createAction("Select Color Burn Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotColorBurn()));

    action = manager->createAction("Select Linear Burn Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotLinearBurn()));

    action = manager->createAction("Select Lighten Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotLighten()));

    action = manager->createAction("Select Screen Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotScreen()));

    action = manager->createAction("Select Color Dodge Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotColorDodge()));

    action = manager->createAction("Select Linear Dodge Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotLinearDodge()));

    action = manager->createAction("Select Overlay Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotOverlay()));

    action = manager->createAction("Select Hard Overlay Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotHardOverlay()));

    action = manager->createAction("Select Soft Light Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotSoftLight()));

    action = manager->createAction("Select Hard Light Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotHardLight()));

    action = manager->createAction("Select Vivid Light Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotVividLight()));

    action = manager->createAction("Select Linear Light Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotLinearLight()));

    action = manager->createAction("Select Pin Light Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotPinLight()));

    action = manager->createAction("Select Hard Mix Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotHardMix()));

    action = manager->createAction("Select Difference Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotDifference()));

    action = manager->createAction("Select Exclusion Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotExclusion()));

    action = manager->createAction("Select Hue Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotHue()));

    action = manager->createAction("Select Saturation Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotSaturation()));

    action = manager->createAction("Select Color Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotColor()));

    action = manager->createAction("Select Luminosity Blending Mode");
    connect(action, SIGNAL(triggered()), SLOT(slotLuminosity()));
}

void KisCompositeOpComboBox::validate(const KoColorSpace *cs) {
    m_model->validate(cs);
}

void KisCompositeOpComboBox::selectCompositeOp(const KoID &op) {
    KoID currentOp;
    if (m_model->entryAt(currentOp, m_model->index(currentIndex(), 0)) &&
            currentOp == op) {

        return;
    }

    QModelIndex index = m_model->indexOf(op);

    setCurrentIndex(index.row());
    Q_EMIT activated(index.row());
    Q_EMIT textActivated(op.name());
}

KoID KisCompositeOpComboBox::selectedCompositeOp() const {
    KoID op;

    if (m_model->entryAt(op, m_model->index(currentIndex(), 0))) {
        return op;
    }
    return KoCompositeOpRegistry::instance().getDefaultCompositeOp();
}

void KisCompositeOpComboBox::slotCategoryToggled(const QModelIndex& index, bool toggled)
{
    Q_UNUSED(index);
    Q_UNUSED(toggled);

    if (m_previewEnabled) {
        if (m_previewActive) {
            cancelPreview();
        }

        // Keep the popup open while the category expands or collapses. This
        // also lets QComboBox recalculate the popup geometry.
        showPopup();
        return;
    }

    if (m_previewActive) {
        cancelPreview();
        QComboBox::hidePopup();
        return;
    }

    //NOTE: this will (should) fit the size of the
    //      popup widget to the view
    //      don't know if this is expected behaviour
    //      on all supported platforms.
    //      There is nothing written about this in the docs.
    showPopup();
}

void KisCompositeOpComboBox::slotEntryChecked(const QModelIndex& index)
{
    Q_UNUSED(index);

    if (m_previewEnabled) {
        if (m_previewActive) {
            cancelPreview();
        }
        return;
    }

    if (m_previewActive) {
        cancelPreview();
        QComboBox::hidePopup();
        return;
    }

    m_allowToHidePopup = false;
}

bool KisCompositeOpComboBox::isPreviewableIndex(const QModelIndex &index) const
{
    KoID op;
    return index.isValid()
        && (m_model->flags(index) & Qt::ItemIsEnabled)
        && m_model->entryAt(op, index);
}

void KisCompositeOpComboBox::requestPreview(const QModelIndex &index)
{
    if (!m_previewActive || !isPreviewableIndex(index) || m_previewIndex == index) return;

    KoID op;
    const bool found = m_model->entryAt(op, index);
    Q_ASSERT(found);

    m_previewIndex = index;
    setCurrentIndex(index.row());
    Q_EMIT sigPreviewRequested(op.id());
}

void KisCompositeOpComboBox::confirmPreview(const QModelIndex &index)
{
    if (!m_previewActive) return;

    if (!isPreviewableIndex(index)) {
        cancelPreview();
        QComboBox::hidePopup();
        return;
    }

    requestPreview(index);

    KoID op;
    const bool found = m_model->entryAt(op, index);
    Q_ASSERT(found);

    m_previewActive = false;
    m_previewCommitInProgress = true;
    Q_EMIT sigPreviewConfirmed(op.id());
    QComboBox::hidePopup();
    QTimer::singleShot(0, this, [this] { m_previewCommitInProgress = false; });
}

void KisCompositeOpComboBox::cancelPreview()
{
    if (!m_previewActive) return;

    m_previewActive = false;
    m_previewIndex = QModelIndex();
    Q_EMIT sigPreviewCancelled();
}

void KisCompositeOpComboBox::slotPreviewIndex(const QModelIndex &index)
{
    requestPreview(index);
}

void KisCompositeOpComboBox::slotPreviewIndexActivated(const QModelIndex &index)
{
    if (m_previewActive) {
        confirmPreview(index);
    }
}

void KisCompositeOpComboBox::hidePopup()
{
    if (m_previewActive) {
        cancelPreview();
        QComboBox::hidePopup();
        return;
    }

    if (m_allowToHidePopup) {
        QComboBox::hidePopup();
    }
    else  {
        QComboBox::showPopup();
    }

    m_allowToHidePopup = true;
}

void KisCompositeOpComboBox::slotNextBlendingMode()
{
    selectNeighbouringBlendMode(true);
}

void KisCompositeOpComboBox::slotPreviousBlendingMode()
{
    selectNeighbouringBlendMode(false);
}

void KisCompositeOpComboBox::slotNormal()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_OVER));
}

void KisCompositeOpComboBox::slotDissolve()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_DISSOLVE));
}

void KisCompositeOpComboBox::slotBehind()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_BEHIND));
}

void KisCompositeOpComboBox::slotClear()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_CLEAR));
}

void KisCompositeOpComboBox::slotDarken()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_DARKEN));
}

void KisCompositeOpComboBox::slotMultiply()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_MULT));
}

void KisCompositeOpComboBox::slotColorBurn()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_BURN));
}

void KisCompositeOpComboBox::slotLinearBurn()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_LINEAR_BURN));
}

void KisCompositeOpComboBox::slotLighten()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_LIGHTEN));
}

void KisCompositeOpComboBox::slotScreen()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_SCREEN));
}

void KisCompositeOpComboBox::slotColorDodge()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_DODGE));
}

void KisCompositeOpComboBox::slotLinearDodge()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_LINEAR_DODGE));
}

void KisCompositeOpComboBox::slotOverlay()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_OVERLAY));
}

void KisCompositeOpComboBox::slotHardOverlay()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_HARD_OVERLAY));
}

void KisCompositeOpComboBox::slotSoftLight()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_SOFT_LIGHT_PHOTOSHOP));
}

void KisCompositeOpComboBox::slotHardLight()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_HARD_LIGHT));
}

void KisCompositeOpComboBox::slotVividLight()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_VIVID_LIGHT));
}

void KisCompositeOpComboBox::slotLinearLight()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_LINEAR_LIGHT));
}

void KisCompositeOpComboBox::slotPinLight()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_PIN_LIGHT));
}

void KisCompositeOpComboBox::slotHardMix()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_HARD_MIX_PHOTOSHOP));
}

void KisCompositeOpComboBox::slotDifference()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_DIFF));
}

void KisCompositeOpComboBox::slotExclusion()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_EXCLUSION));
}

void KisCompositeOpComboBox::slotHue()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_HUE));
}

void KisCompositeOpComboBox::slotSaturation()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_SATURATION));
}

void KisCompositeOpComboBox::slotColor()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_COLOR));
}

void KisCompositeOpComboBox::slotLuminosity()
{
    selectCompositeOp(KoCompositeOpRegistry::instance().getKoID(COMPOSITE_LUMINIZE));
}

void KisCompositeOpComboBox::selectNeighbouringBlendMode(bool down)
{
    const int rowCount = count();
    int newIndex = currentIndex();

    QAbstractItemModel *model = this->model();
    KoID op;

    if (!down) {
        newIndex--;
        while ((newIndex >= 0) &&
               (!(model->flags(model->index(newIndex, modelColumn(), rootModelIndex())) & Qt::ItemIsEnabled) ||
                !m_model->entryAt(op, m_model->index(newIndex, modelColumn()))))

            newIndex--;
    } else {
        newIndex++;
        while (newIndex < rowCount &&
               (!(model->index(newIndex, modelColumn(), rootModelIndex()).flags() & Qt::ItemIsEnabled) ||
                !m_model->entryAt(op, m_model->index(newIndex, modelColumn()))))

            newIndex++;
    }

    if (newIndex >= 0 && newIndex < rowCount && newIndex != currentIndex()) {
        setCurrentIndex(newIndex);

        Q_EMIT activated(newIndex);
        if (m_model->entryAt(op, m_model->index(newIndex, 0))) {
            Q_EMIT textActivated(op.name());
        }
    }
}

void KisCompositeOpComboBox::wheelEvent(QWheelEvent *e)
{
    /**
     * This code is a copy of QComboBox::wheelEvent. It does the same thing,
     * except that it skips "Category" items, by checking m_model->entryAt()
     * on each step.
     */

    QStyleOptionComboBox opt;
    initStyleOption(&opt);

    if (style()->styleHint(QStyle::SH_ComboBox_AllowWheelScrolling, &opt, this)) {
        if (e->angleDelta().y() != 0) {
            selectNeighbouringBlendMode(e->angleDelta().y() < 0);
            if (m_previewActive) {
                requestPreview(m_model->index(currentIndex(), 0));
            }
        }

        e->accept();
    } else {
        KisSqueezedComboBox::wheelEvent(e);
    }
}

void KisCompositeOpComboBox::keyPressEvent(QKeyEvent *e)
{
    if (m_previewActive) {
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            confirmPreview(m_view->currentIndex());
            e->accept();
            return;
        }

        if (e->key() == Qt::Key_Escape) {
            cancelPreview();
            QComboBox::hidePopup();
            e->accept();
            return;
        }

        QComboBox::keyPressEvent(e);
        QTimer::singleShot(0, this, [this] {
            if (m_previewActive) {
                requestPreview(m_view->currentIndex());
            }
        });
        return;
    }

    /**
     * This code is a copy of QComboBox::keyPressEvent. It does the same thing,
     * except that it skips "Category" items, by checking m_model->entryAt()
     * on each step.
     */

    enum Move { NoMove=0 , MoveUp , MoveDown , MoveFirst , MoveLast};

    Move move = NoMove;
    int newIndex = currentIndex();
    switch (e->key()) {
    case Qt::Key_Up:
        if (e->modifiers() & Qt::ControlModifier)
            break; // pass to line edit for auto completion
        Q_FALLTHROUGH();
    case Qt::Key_PageUp:
        move = MoveUp;
        break;
    case Qt::Key_Down:
        if (e->modifiers() & Qt::AltModifier) {
            showPopup();
            return;
        } else if (e->modifiers() & Qt::ControlModifier)
            break; // pass to line edit for auto completion
        Q_FALLTHROUGH();
    case Qt::Key_PageDown:
        move = MoveDown;
        break;
    case Qt::Key_Home:
        move = MoveFirst;
        break;
    case Qt::Key_End:
        move = MoveLast;
        break;
    case Qt::Key_F4:
        if (!e->modifiers()) {
            showPopup();
            return;
        }
        break;
    case Qt::Key_Space:
        showPopup();
        return;
    default:
        break;
    }

    const int rowCount = count();

    if (move != NoMove) {
        KoID op;

        e->accept();
        switch (move) {
        case MoveFirst:
            newIndex = -1;
            Q_FALLTHROUGH();
        case MoveDown:
            newIndex++;
            while (newIndex < rowCount &&
                   (!(model()->index(newIndex, modelColumn(), rootModelIndex()).flags() & Qt::ItemIsEnabled) ||
                    !m_model->entryAt(op, m_model->index(newIndex, modelColumn()))))
                newIndex++;
            break;
        case MoveLast:
            newIndex = rowCount;
            Q_FALLTHROUGH();
        case MoveUp:
            newIndex--;
            while ((newIndex >= 0) &&
                   (!(model()->flags(model()->index(newIndex, modelColumn(), rootModelIndex())) & Qt::ItemIsEnabled) ||
                    !m_model->entryAt(op, m_model->index(newIndex, modelColumn()))))
                newIndex--;
            break;
        default:
            e->ignore();
            break;
        }

        if (newIndex >= 0 && newIndex < rowCount && newIndex != currentIndex()) {
            setCurrentIndex(newIndex);
            Q_EMIT activated(newIndex);

            if (m_model->entryAt(op, m_model->index(newIndex, 0))) {
                Q_EMIT textActivated(op.name());
            }
        }
    } else {
        KisSqueezedComboBox::keyPressEvent(e);
    }
}

KisLayerStyleCompositeOpComboBox::KisLayerStyleCompositeOpComboBox(QWidget* parent)
    : KisCompositeOpComboBox(true, parent)
{
}

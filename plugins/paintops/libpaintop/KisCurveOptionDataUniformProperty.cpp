/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisCurveOptionDataUniformProperty.h"


#include "KisCurveOptionData.h"
#include "kis_paintop_settings.h"
#include "kis_paintop_preset.h"
#include "KisPaintOpPresetUpdateProxy.h"

KisCurveOptionDataUniformProperty::KisCurveOptionDataUniformProperty(const KisCurveOptionData &data, KisPaintOpSettingsRestrictedSP settings, QObject *parent)
    : KisDoubleSliderBasedPaintOpProperty(Double, data.id, settings, parent)
    , m_data(new KisCurveOptionData(data))
{
    setRange(m_data->strengthMinValue, m_data->strengthMaxValue);
    setSingleStep(0.01);
    requestReadValue();
}

KisCurveOptionDataUniformProperty::~KisCurveOptionDataUniformProperty()
{
}

void KisCurveOptionDataUniformProperty::readValueImpl()
{
    m_data->read(settings().data());
    setValue(m_data->strengthValue);
}

void KisCurveOptionDataUniformProperty::writeValueImpl()
{
    m_data->read(settings().data());
    m_data->strengthValue = value().toReal();
    m_data->write(settings().data());
}

bool KisCurveOptionDataUniformProperty::isVisible() const
{
    return !m_data->isCheckable || m_data->isChecked;
}

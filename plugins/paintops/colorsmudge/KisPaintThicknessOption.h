/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISPAINTTHICKNESSOPTION_H
#define KISPAINTTHICKNESSOPTION_H

#include <KisCurveOption2.h>
#include <KisPaintThicknessOptionData.h>


class KisPaintThicknessOption : public KisCurveOption2
{
public:
    KisPaintThicknessOption(const KisPropertiesConfiguration *setting);

    qreal apply(const KisPaintInformation & info) const;

    KisPaintThicknessOptionData::ThicknessMode mode() const;

private:
    KisPaintThicknessOptionData initializeFromData(const KisPropertiesConfiguration *setting);

private:
    KisPaintThicknessOptionData::ThicknessMode m_mode;
};

#endif // KISPAINTTHICKNESSOPTION_H

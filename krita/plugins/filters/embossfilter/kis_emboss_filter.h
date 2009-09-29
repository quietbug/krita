/*
 * This file is part of the KDE project
 *
 *  Copyright (c) Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#ifndef _KIS_EMBOSS_FILTER_H_
#define _KIS_EMBOSS_FILTER_H_

#include "filter/kis_filter.h"
#include "kis_config_widget.h"

class KisEmbossFilter : public KisFilter
{
public:
    KisEmbossFilter();
public:
    using KisFilter::process;
    void process(KisConstProcessingInformation src,
                 KisProcessingInformation dst,
                 const QSize& size,
                 const KisFilterConfiguration* config,
                 KoUpdater* progressUpdater
                ) const;
    static inline KoID id() {
        return KoID("emboss", i18n("Emboss with Variable Depth"));
    }

public:
    virtual KisConfigWidget * createConfigurationWidget(QWidget* parent, const KisPaintDeviceSP dev, const KisImageWSP image = 0) const;
protected:
    virtual KisFilterConfiguration* factoryConfiguration(const KisPaintDeviceSP) const;

private:
    inline int Lim_Max(int Now, int Up, int Max) const;
};

#endif

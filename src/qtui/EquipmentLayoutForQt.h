#pragma once
#include "ui/EquipmentLayout.h"
#include <QFont>
#include <QString>
#include <QTextLayout>
#include <QTextOption>
#include <cmath>

namespace equipmentLayout {
inline int NameHeightForQt(const QString& name) {
    QFont font(QStringLiteral("Segoe UI"));
    font.setPixelSize(11);
    QTextLayout text(name,font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    text.setTextOption(option);
    text.beginLayout();
    qreal height=0;
    for(;;){
        auto line=text.createLine();
        if(!line.isValid())break;
        line.setLineWidth(nameWidth);
        height+=line.height();
    }
    text.endLayout();
    return static_cast<int>(std::ceil(height));
}
}

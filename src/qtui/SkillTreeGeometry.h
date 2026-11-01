#pragma once
#include "../ui/SkillTreeModel.h"
#include <QVariantMap>

inline QVariantMap SkillTreeGeometryForQt()
{
    using G = skilltree::Geometry;
    return {{"width",G::width},{"height",G::height},{"iconX",G::iconX},
        {"iconY",G::iconY},{"iconSize",G::iconSize},{"arrowSize",G::arrowSize},
        {"arrowY",G::arrowY},{"leftX",G::leftX},{"rightX",G::rightX}};
}

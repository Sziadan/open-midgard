#pragma once
#include <QImage>
#include "core/GuildEmblemKey.h"

namespace ro::guild {
// Emblems saved by image editors often contain almost-magenta background
// pixels. Keep the tolerance narrow so the emblem's pink artwork stays opaque.
inline bool IsEmblemTransparencyKey(QRgb color)
{
    return IsEmblemKey(color);
}

inline QImage TransparentEmblemImage(const QImage& source)
{
    QImage image=source.convertToFormat(QImage::Format_ARGB32);
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)
        if(IsEmblemTransparencyKey(image.pixel(x,y)))image.setPixel(x,y,0);
    return image;
}
}

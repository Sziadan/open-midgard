#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>
#include "GuildEmblemImage.h"

namespace ro::guild {
// CZ_REGISTER_GUILD_EMBLEM_IMG contains a zlib stream, without qCompress's
// four-byte uncompressed-size prefix. Emit a canonical bottom-up 24-bit BMP.
inline QByteArray EmblemUploadPacket(const QImage& image, QString& error)
{
    error.clear();
    if (image.isNull() || image.size() != QSize(24, 24)) {
        error = QStringLiteral("Choose a 24 x 24 pixel BMP or PNG guild emblem.");
        return {};
    }
    QByteArray bmp(54 + 24 * 24 * 3, '\0');
    auto put = [&](int at, quint32 value, int bytes = 4) {
        for (int i = 0; i < bytes; ++i) bmp[at + i] = char(value >> (8 * i));
    };
    bmp[0] = 'B'; bmp[1] = 'M';
    put(2, bmp.size()); put(10, 54); put(14, 40);
    put(18, 24); put(22, 24); put(26, 1, 2); put(28, 24, 2);
    put(34, 24 * 24 * 3);
    for (int y = 0; y < 24; ++y) for (int x = 0; x < 24; ++x) {
        const QColor c = image.pixelColor(x, y);
        // Legacy emblems have binary transparency, keyed by RGB FF00FF.
        const QColor pixel = c.alpha() < 128 || IsEmblemTransparencyKey(c.rgb()) ? QColor(255, 0, 255) : c;
        const int at = 54 + ((23 - y) * 24 + x) * 3;
        bmp[at] = char(pixel.blue()); bmp[at + 1] = char(pixel.green()); bmp[at + 2] = char(pixel.red());
    }
    const QByteArray compressed = qCompress(bmp, 9).mid(4);
    if (compressed.isEmpty() || compressed.size() > 2048) {
        error = QStringLiteral("Unable to compress the guild emblem.");
        return {};
    }
    QByteArray packet(4, '\0');
    packet[0] = char(0x53); packet[1] = char(0x01);
    const int length = 4 + compressed.size();
    packet[2] = char(length); packet[3] = char(length >> 8);
    packet += compressed;
    return packet;
}
}

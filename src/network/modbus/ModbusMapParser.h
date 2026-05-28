#pragma once

#include "ModbusTypes.h"

#include <QVector>
#include <cstdint>
#include <cstring>

namespace CentralLogger::Network::ModbusMapParser {

/// Parse the 10 header registers per contract §1. Reads into ModbusHeader;
/// the caller decides what to do when `isValid()` is false (drop snapshot).
inline ModbusHeader parseHeader(const quint16 *regs, int size)
{
    ModbusHeader h;
    if (!regs || size < 10) {
        return h; // mapVersion stays 0 → isValid() false
    }
    h.mapVersion    = regs[0];
    h.statusFlags   = regs[1];
    h.unixTimestamp = (static_cast<quint32>(regs[2]) << 16) | regs[3];
    h.na            = regs[4];
    h.ndi           = regs[5];
    h.ndo           = regs[6];
    return h;
}

inline ModbusHeader parseHeader(const QVector<quint16> &regs)
{
    return parseHeader(regs.constData(), regs.size());
}

/// Decode one 8-register analog block (HR10 + i*8). Contract §2:
///   +0 sensor_id, +1 flags, +2..+3 float32 ABCD (big-endian IEEE754).
inline AnalogSample parseAnalogBlock(const quint16 *regs, int size)
{
    AnalogSample a;
    if (!regs || size < 8) {
        return a;
    }
    a.edgeSensorId = regs[0];
    a.flags        = regs[1];

    // ABCD: register +2 holds the high word (AB), +3 the low word (CD).
    // Float bytes [A, B, C, D] → uint32 big-endian → bit_cast to float.
    const quint32 raw = (static_cast<quint32>(regs[2]) << 16) | regs[3];
    std::memcpy(&a.value, &raw, sizeof(float));
    return a;
}

/// Take the first `size` analog blocks starting at `regs`, where size is
/// the raw register count (must be `na * 8`). Returns `na` parsed samples.
inline QVector<AnalogSample> parseAnalogChunk(const quint16 *regs, int size)
{
    QVector<AnalogSample> out;
    if (!regs || size <= 0) {
        return out;
    }
    const int count = size / 8;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        out.append(parseAnalogBlock(regs + i * 8, 8));
    }
    return out;
}

/// QModbusDataUnit values for FC02/FC01 already arrive one-bit-per-uint16
/// (each "register" is 0 or 1). This helper just normalises that into a
/// QVector<bool> of the requested length.
inline QVector<bool> unpackDiscrete(const quint16 *bits, int count)
{
    QVector<bool> out;
    if (!bits || count <= 0) {
        return out;
    }
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        out.append(bits[i] != 0);
    }
    return out;
}

inline QVector<bool> unpackDiscrete(const QVector<quint16> &bits)
{
    return unpackDiscrete(bits.constData(), bits.size());
}

} // namespace CentralLogger::Network::ModbusMapParser

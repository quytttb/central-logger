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

/// Decode FC02/FC01 payload into `bitCount` booleans (indices 0 .. bitCount-1).
///
/// Qt's QModbusDataUnit normalises DiscreteInputs/Coils values to 0 or 1 per bit,
/// and byte-rounds the response (e.g. 12-bit request → 2 bytes → 16 values).
/// Therefore when regCount >= bitCount each regs[i] is one bit (expanded form).
///
/// Only when regCount < bitCount (pymodbus path / direct TCP) are registers packed
/// as 16-bit words (bit 0 = LSB).
inline QVector<bool> unpackDiscrete(const quint16 *regs, int regCount, int bitCount)
{
    QVector<bool> out;
    if (!regs || bitCount <= 0) {
        return out;
    }
    out.reserve(bitCount);

    if (regCount >= bitCount) {
        // Expanded: one uint16 per bit (Qt QModbusDataUnit path).
        for (int i = 0; i < bitCount; ++i) {
            out.append(regs[i] != 0);
        }
        return out;
    }

    // Packed: 16 bits per uint16 word, LSB-first.
    for (int i = 0; i < bitCount; ++i) {
        const int reg = i / 16;
        const int bit = i % 16;
        out.append(reg < regCount ? (bool)((regs[reg] >> bit) & 1u) : false);
    }
    return out;
}

inline QVector<bool> unpackDiscrete(const QVector<quint16> &regs, int bitCount)
{
    return unpackDiscrete(regs.constData(), regs.size(), bitCount);
}

} // namespace CentralLogger::Network::ModbusMapParser

#pragma once

#include <QVector>
#include <cstdint>

namespace CentralLogger::Network {

/// One Modbus PDU planned for the current poll cycle.
struct PollPdu
{
    enum class Function { Fc03, Fc02, Fc01 };

    Function fc       = Function::Fc03;
    int      start    = 0;
    int      quantity = 0;
};

inline bool operator==(const PollPdu &a, const PollPdu &b)
{
    return a.fc == b.fc && a.start == b.start && a.quantity == b.quantity;
}

/// Plan one full poll cycle per contract §5:
///   1. FC03 header  — start 0, qty 10
///   2. FC03 analog chunks — start 10 + 8*k, qty min(8*(na-k), 120)
///   3. FC02 DI (if ndi > 0) — start 0, qty ndi
///   4. FC01 DO (if ndo > 0) — start 0, qty ndo
///
/// Header is always read first (caller may inspect HR4-6 then re-plan if
/// firmware drifts, but in the steady state we plan with values from the
/// previous header for predictability).
inline QVector<PollPdu> planPollReads(uint16_t na, uint16_t ndi, uint16_t ndo)
{
    constexpr int kHeaderQty       = 10;
    constexpr int kAnalogStart     = 10;
    constexpr int kRegistersPerBlk = 8;
    constexpr int kMaxAnalogChunk  = 15; // 15*8 = 120 ≤ 125-reg FC03 limit

    QVector<PollPdu> out;
    out.append(PollPdu{ PollPdu::Function::Fc03, 0, kHeaderQty });

    int remaining = na;
    int blockIdx  = 0;
    while (remaining > 0) {
        const int blocks = remaining > kMaxAnalogChunk ? kMaxAnalogChunk : remaining;
        out.append(PollPdu{
            PollPdu::Function::Fc03,
            kAnalogStart + blockIdx * kRegistersPerBlk,
            blocks * kRegistersPerBlk,
        });
        blockIdx  += blocks;
        remaining -= blocks;
    }

    if (ndi > 0) {
        out.append(PollPdu{ PollPdu::Function::Fc02, 0, ndi });
    }
    if (ndo > 0) {
        out.append(PollPdu{ PollPdu::Function::Fc01, 0, ndo });
    }
    return out;
}

} // namespace CentralLogger::Network

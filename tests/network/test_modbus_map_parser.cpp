#include "network/modbus/ModbusMapParser.h"
#include "network/modbus/ModbusPollPlan.h"
#include "network/modbus/ModbusTypes.h"

#include <QTest>
#include <QVector>
#include <cstring>

using namespace CentralLogger::Network;

namespace {

quint16 floatHigh(float v)
{
    quint32 raw;
    std::memcpy(&raw, &v, sizeof(raw));
    return static_cast<quint16>((raw >> 16) & 0xFFFFu);
}

quint16 floatLow(float v)
{
    quint32 raw;
    std::memcpy(&raw, &v, sizeof(raw));
    return static_cast<quint16>(raw & 0xFFFFu);
}

} // namespace

class TestModbusMapParser : public QObject
{
    Q_OBJECT

private slots:
    void parseHeaderValid();
    void parseHeaderRejectsWrongMapVersion();
    void parseHeaderRejectsShortBuffer();
    void parseAnalogAbcdDecode();
    void unpackDiscreteBitArray();
    void unpackDiscretePackedRegister();
    void unpackDiscreteQtBytePaddedValues();
    void planPollHeaderOnlyWhenZero();
    void planPoll14Analog();
    void planPoll15Analog();
    void planPoll16AnalogChunks();
    void planPoll30AnalogChunks();
    void planPollIncludesDiAndDo();
    void planPollSkipsDiDoWhenZero();
};

void TestModbusMapParser::parseHeaderValid()
{
    const QVector<quint16> regs{ 1, 0x0007, 0x65F3, 0xAABB, 14, 8, 4, 0, 0, 0 };
    const auto h = ModbusMapParser::parseHeader(regs);

    QVERIFY(h.isValid());
    QVERIFY(h.isPolling());
    QVERIFY(h.isRtuConnected());
    QVERIFY(h.isAnyAlarm());
    QCOMPARE(h.na,  quint16(14));
    QCOMPARE(h.ndi, quint16(8));
    QCOMPARE(h.ndo, quint16(4));
    QCOMPARE(h.unixTimestamp, (quint32(0x65F3) << 16) | quint32(0xAABB));
}

void TestModbusMapParser::parseHeaderRejectsWrongMapVersion()
{
    const QVector<quint16> regs{ 2, 0, 0, 0, 1, 0, 0, 0, 0, 0 };
    QVERIFY(!ModbusMapParser::parseHeader(regs).isValid());
}

void TestModbusMapParser::parseHeaderRejectsShortBuffer()
{
    const QVector<quint16> regs{ 1, 0, 0 };
    QVERIFY(!ModbusMapParser::parseHeader(regs).isValid());
}

void TestModbusMapParser::parseAnalogAbcdDecode()
{
    const float    value = 25.5f;
    const quint16  high  = floatHigh(value);
    const quint16  low   = floatLow(value);

    const QVector<quint16> block{
        /*sensor*/ 7,
        /*flags */ 0x05, // valid + stale
        high, low,
        0, 0, 0, 0,
    };
    const auto sample = ModbusMapParser::parseAnalogBlock(block.constData(), block.size());

    QCOMPARE(sample.edgeSensorId, quint16(7));
    QVERIFY(sample.isValid());
    QVERIFY(!sample.isAlarm());
    QVERIFY(sample.isStale());
    QCOMPARE(sample.value, 25.5f);
}

void TestModbusMapParser::unpackDiscreteBitArray()
{
    // Expanded: one uint16 per discrete (Qt path).
    const QVector<quint16> bits{ 0, 1, 0, 1, 1, 0, 0, 1 };
    const auto unpacked = ModbusMapParser::unpackDiscrete(bits, 8);

    QCOMPARE(unpacked.size(), 8);
    QCOMPARE(unpacked[1], true);
    QCOMPARE(unpacked[3], true);
    QCOMPARE(unpacked[4], true);
    QCOMPARE(unpacked[7], true);
    QCOMPARE(unpacked[0], false);
    QCOMPARE(unpacked[2], false);
}

void TestModbusMapParser::unpackDiscretePackedRegister()
{
    // Packed: DI at bit index 10 ON (data-logger pymodbus / Modbus word LSB-first).
    const QVector<quint16> packed{ 1024, 0 };
    const auto unpacked = ModbusMapParser::unpackDiscrete(packed, 12);

    QCOMPARE(unpacked.size(), 12);
    QCOMPARE(unpacked[10], true);
    QCOMPARE(unpacked[0], false);
    QCOMPARE(unpacked[9], false);
}

void TestModbusMapParser::unpackDiscreteQtBytePaddedValues()
{
    // Qt byte-rounds a 12-bit FC02 request to 2 bytes → 16 values (regCount > bitCount).
    QVector<quint16> regs(16, 0);
    regs[10] = 1;  // bit 10 ON (index 10 of the expanded vector)
    const auto unpacked = ModbusMapParser::unpackDiscrete(regs, 12);

    QCOMPARE(unpacked.size(), 12);
    QCOMPARE(unpacked[10], true);
    QCOMPARE(unpacked[0], false);
    QCOMPARE(unpacked[1], false);
}

void TestModbusMapParser::planPollHeaderOnlyWhenZero()
{
    const auto plan = planPollReads(0, 0, 0);
    QCOMPARE(plan.size(), 1);
    QCOMPARE(plan[0], (PollPdu{ PollPdu::Function::Fc03, 0, 10 }));
}

void TestModbusMapParser::planPoll14Analog()
{
    const auto plan = planPollReads(14, 0, 0);
    QCOMPARE(plan.size(), 2);
    QCOMPARE(plan[0], (PollPdu{ PollPdu::Function::Fc03,  0,  10 }));
    QCOMPARE(plan[1], (PollPdu{ PollPdu::Function::Fc03, 10, 112 }));
}

void TestModbusMapParser::planPoll15Analog()
{
    const auto plan = planPollReads(15, 0, 0);
    QCOMPARE(plan.size(), 2);
    QCOMPARE(plan[1], (PollPdu{ PollPdu::Function::Fc03, 10, 120 }));
}

void TestModbusMapParser::planPoll16AnalogChunks()
{
    const auto plan = planPollReads(16, 0, 0);
    QCOMPARE(plan.size(), 3);
    QCOMPARE(plan[0], (PollPdu{ PollPdu::Function::Fc03,   0,  10 }));
    QCOMPARE(plan[1], (PollPdu{ PollPdu::Function::Fc03,  10, 120 }));
    QCOMPARE(plan[2], (PollPdu{ PollPdu::Function::Fc03, 130,   8 }));
}

void TestModbusMapParser::planPoll30AnalogChunks()
{
    const auto plan = planPollReads(30, 0, 0);
    QCOMPARE(plan.size(), 3);
    QCOMPARE(plan[1], (PollPdu{ PollPdu::Function::Fc03,  10, 120 }));
    QCOMPARE(plan[2], (PollPdu{ PollPdu::Function::Fc03, 130, 120 }));
}

void TestModbusMapParser::planPollIncludesDiAndDo()
{
    const auto plan = planPollReads(2, 8, 4);
    QCOMPARE(plan.size(), 4);
    QCOMPARE(plan[0].fc, PollPdu::Function::Fc03);
    QCOMPARE(plan[1].fc, PollPdu::Function::Fc03);
    QCOMPARE(plan[1].quantity, 16);
    QCOMPARE(plan[2], (PollPdu{ PollPdu::Function::Fc02, 0, 8 }));
    QCOMPARE(plan[3], (PollPdu{ PollPdu::Function::Fc01, 0, 4 }));
}

void TestModbusMapParser::planPollSkipsDiDoWhenZero()
{
    const auto plan = planPollReads(0, 0, 4);
    QCOMPARE(plan.size(), 2);
    QCOMPARE(plan[0], (PollPdu{ PollPdu::Function::Fc03, 0, 10 }));
    QCOMPARE(plan[1], (PollPdu{ PollPdu::Function::Fc01, 0, 4 }));
}

QTEST_MAIN(TestModbusMapParser)
#include "test_modbus_map_parser.moc"

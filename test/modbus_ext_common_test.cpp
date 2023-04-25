#include "modbus_ext_common.h"
#include "serial_exc.h"
#include "gtest/gtest.h"

namespace
{
    class TPortMock: public TPort
    {
    public:
        std::vector<uint8_t> Request;
        std::vector<uint8_t> Response;

        TPortMock()
        {}

        void Open() override
        {}
        void Close() override
        {}
        bool IsOpen() const override
        {
            return false;
        }
        void CheckPortOpen() const override
        {}

        void WriteBytes(const uint8_t* buf, int count) override
        {
            Request.clear();
            Request.insert(Request.end(), buf, buf + count);
        }

        uint8_t ReadByte(const std::chrono::microseconds& timeout) override
        {
            return 0;
        }

        size_t ReadFrame(uint8_t* buf,
                         size_t count,
                         const std::chrono::microseconds& responseTimeout,
                         const std::chrono::microseconds& frameTimeout,
                         TFrameCompletePred frame_complete = 0) override
        {
            memcpy(buf, Response.data(), Response.size());
            return Response.size();
        }

        void SkipNoise() override
        {}

        void SleepSinceLastInteraction(const std::chrono::microseconds& us) override
        {}

        std::string GetDescription(bool verbose) const override
        {
            return std::string();
        }

        std::chrono::milliseconds GetSendTime(double bytesNumber) const override
        {
            return std::chrono::milliseconds((int)bytesNumber);
        }
    };

    class TTestEventsVisitor: public ModbusExt::IEventsVisitor
    {
    public:
        std::unordered_map<uint16_t, uint16_t> Events;
        bool Reboot = false;

        virtual void Event(uint8_t slaveId,
                           uint8_t eventType,
                           uint16_t eventId,
                           const uint8_t* data,
                           size_t dataSize) override
        {
            if (eventType == ModbusExt::TEventType::REBOOT) {
                Reboot = true;
                return;
            }
            Events[eventId] = data[0] | (data[1] << 8);
        }
    };
} // namespace

TEST(TModbusExtTest, EventsEnablerOneReg)
{
    TPortMock port;
    port.Response = {0x0A, 0x46, 0x18, 0x01, 0x80, 0x29, 0x7E};

    std::map<uint16_t, bool> response;
    ModbusExt::TEventsEnabler ev(10,
                                 port,
                                 std::chrono::milliseconds(100),
                                 std::chrono::milliseconds(100),
                                 [&response](uint8_t type, uint16_t reg, bool enabled) { response[reg] = enabled; });
    ev.AddRegister(101, ModbusExt::TEventType::COIL, ModbusExt::TEventPriority::HIGH);

    EXPECT_NO_THROW(ev.SendRequest());

    EXPECT_EQ(port.Request.size(), 11);
    EXPECT_EQ(port.Request[0], 0x0A); // slave id
    EXPECT_EQ(port.Request[1], 0x46); // command
    EXPECT_EQ(port.Request[2], 0x18); // subcommand
    EXPECT_EQ(port.Request[3], 0x05); // settings size

    EXPECT_EQ(port.Request[4], 0x01); // event type
    EXPECT_EQ(port.Request[5], 0x00); // address MSB
    EXPECT_EQ(port.Request[6], 0x65); // address LSB
    EXPECT_EQ(port.Request[7], 0x01); // count
    EXPECT_EQ(port.Request[8], 0x02); // priority

    EXPECT_EQ(port.Request[9], 0xC5);  // CRC16 LSB
    EXPECT_EQ(port.Request[10], 0x90); // CRC16 MSB

    EXPECT_EQ(response.size(), 1);
    EXPECT_TRUE(response[101]);
}

TEST(TModbusExtTest, EventsEnablerIllegalFunction)
{
    TPortMock port;
    port.Response = {0x0A, 0xC6, 0x01, 0xC3, 0xA2};

    ModbusExt::TEventsEnabler ev(10,
                                 port,
                                 std::chrono::milliseconds(100),
                                 std::chrono::milliseconds(100),
                                 [](uint8_t, uint16_t, bool) {});
    ev.AddRegister(101, ModbusExt::TEventType::COIL, ModbusExt::TEventPriority::HIGH);

    EXPECT_THROW(ev.SendRequest(), TSerialDevicePermanentRegisterException);
}

TEST(TModbusExtTest, EventsEnablerTwoRanges)
{
    TPortMock port;
    // 0xC0 = 0b11000000
    // 0xDF = 0b11011111
    // 0x40 = 0b01000000
    port.Response = {0x0A, 0x46, 0x18, 0x03, 0xC0, 0xDF, 0x40, 0xC6, 0x1C};

    std::map<uint16_t, bool> response;
    ModbusExt::TEventsEnabler ev(10,
                                 port,
                                 std::chrono::milliseconds(100),
                                 std::chrono::milliseconds(100),
                                 [&response](uint8_t type, uint16_t reg, bool enabled) { response[reg] = enabled; });

    ev.AddRegister(101, ModbusExt::TEventType::COIL, ModbusExt::TEventPriority::HIGH);
    ev.AddRegister(102, ModbusExt::TEventType::COIL, ModbusExt::TEventPriority::HIGH);

    ev.AddRegister(103, ModbusExt::TEventType::INPUT, ModbusExt::TEventPriority::LOW);
    ev.AddRegister(104, ModbusExt::TEventType::INPUT, ModbusExt::TEventPriority::LOW);
    ev.AddRegister(105, ModbusExt::TEventType::INPUT, ModbusExt::TEventPriority::LOW);
    ev.AddRegister(106, ModbusExt::TEventType::INPUT, ModbusExt::TEventPriority::LOW);
    ev.AddRegister(107, ModbusExt::TEventType::INPUT, ModbusExt::TEventPriority::LOW);
    ev.AddRegister(108, ModbusExt::TEventType::INPUT, ModbusExt::TEventPriority::LOW);
    ev.AddRegister(109, ModbusExt::TEventType::INPUT, ModbusExt::TEventPriority::LOW);
    ev.AddRegister(110, ModbusExt::TEventType::INPUT, ModbusExt::TEventPriority::HIGH);
    ev.AddRegister(111, ModbusExt::TEventType::INPUT, ModbusExt::TEventPriority::HIGH);
    ev.AddRegister(112, ModbusExt::TEventType::INPUT, ModbusExt::TEventPriority::HIGH);

    EXPECT_NO_THROW(ev.SendRequest());

    EXPECT_EQ(port.Request.size(), 26);
    EXPECT_EQ(port.Request[0], 0x0A); // slave id
    EXPECT_EQ(port.Request[1], 0x46); // command
    EXPECT_EQ(port.Request[2], 0x18); // subcommand
    EXPECT_EQ(port.Request[3], 0x14); // settings size

    EXPECT_EQ(port.Request[4], 0x01); // event type
    EXPECT_EQ(port.Request[5], 0x00); // address MSB
    EXPECT_EQ(port.Request[6], 0x65); // address LSB
    EXPECT_EQ(port.Request[7], 0x02); // count
    EXPECT_EQ(port.Request[8], 0x02); // priority 101
    EXPECT_EQ(port.Request[9], 0x02); // priority 102

    EXPECT_EQ(port.Request[10], 0x04); // event type
    EXPECT_EQ(port.Request[11], 0x00); // address MSB
    EXPECT_EQ(port.Request[12], 0x67); // address LSB
    EXPECT_EQ(port.Request[13], 0x0A); // count
    EXPECT_EQ(port.Request[14], 0x01); // priority 103
    EXPECT_EQ(port.Request[15], 0x01); // priority 104
    EXPECT_EQ(port.Request[16], 0x01); // priority 105
    EXPECT_EQ(port.Request[17], 0x01); // priority 106
    EXPECT_EQ(port.Request[18], 0x01); // priority 107
    EXPECT_EQ(port.Request[19], 0x01); // priority 108
    EXPECT_EQ(port.Request[20], 0x01); // priority 109
    EXPECT_EQ(port.Request[21], 0x02); // priority 110
    EXPECT_EQ(port.Request[22], 0x02); // priority 111
    EXPECT_EQ(port.Request[23], 0x02); // priority 112

    EXPECT_EQ(port.Request[24], 0x25); // CRC16 LSB
    EXPECT_EQ(port.Request[25], 0xF0); // CRC16 MSB

    EXPECT_EQ(response.size(), 12);
    EXPECT_TRUE(response[101]);
    EXPECT_TRUE(response[102]);
    EXPECT_TRUE(response[103]);
    EXPECT_TRUE(response[104]);
    EXPECT_FALSE(response[105]);
    EXPECT_TRUE(response[106]);
    EXPECT_TRUE(response[107]);
    EXPECT_TRUE(response[108]);
    EXPECT_TRUE(response[109]);
    EXPECT_TRUE(response[110]);
    EXPECT_FALSE(response[111]);
    EXPECT_TRUE(response[112]);
}

TEST(TModbusExtTest, ReadEventsNoEventsNoConfirmation)
{
    TPortMock port;
    TTestEventsVisitor visitor;
    ModbusExt::TEventConfirmationState state;

    port.Response = {0xFF, 0xFF, 0xFF, 0xFD, 0x46, 0x14, 0xD2, 0x5F}; // No events
    bool ret = true;
    EXPECT_NO_THROW(ret = ModbusExt::ReadEvents(port,
                                                std::chrono::milliseconds(100),
                                                std::chrono::milliseconds(100),
                                                visitor,
                                                state));
    EXPECT_FALSE(ret);

    EXPECT_EQ(port.Request.size(), 9);
    EXPECT_EQ(port.Request[0], 0xFD); // broadcast
    EXPECT_EQ(port.Request[1], 0x46); // command
    EXPECT_EQ(port.Request[2], 0x10); // subcommand
    EXPECT_EQ(port.Request[3], 0x00); // min slave id
    EXPECT_EQ(port.Request[4], 0xF8); // max length
    EXPECT_EQ(port.Request[5], 0x00); // slave id (confirmation)
    EXPECT_EQ(port.Request[6], 0x00); // flag (confirmation)

    EXPECT_EQ(port.Request[7], 0x79); // CRC16 LSB
    EXPECT_EQ(port.Request[8], 0x5B); // CRC16 MSB

    EXPECT_EQ(visitor.Events.size(), 0);
}

TEST(TModbusExtTest, ReadEventsWithConfirmation)
{
    TPortMock port;
    TTestEventsVisitor visitor;
    ModbusExt::TEventConfirmationState state;

    port.Response =
        {0xFF, 0xFF, 0xFF, 0x05, 0x46, 0x11, 0x01, 0x01, 0x06, 0x02, 0x04, 0x01, 0xD0, 0x04, 0x00, 0x2B, 0xAC};
    bool ret = false;
    EXPECT_NO_THROW(ret = ModbusExt::ReadEvents(port,
                                                std::chrono::milliseconds(100),
                                                std::chrono::milliseconds(100),
                                                visitor,
                                                state));
    EXPECT_TRUE(ret);

    EXPECT_EQ(port.Request.size(), 9);
    EXPECT_EQ(port.Request[0], 0xFD); // slave id
    EXPECT_EQ(port.Request[1], 0x46); // command
    EXPECT_EQ(port.Request[2], 0x10); // subcommand
    EXPECT_EQ(port.Request[3], 0x00); // min slave id
    EXPECT_EQ(port.Request[4], 0xF8); // max length
    EXPECT_EQ(port.Request[5], 0x00); // slave id (confirmation)
    EXPECT_EQ(port.Request[6], 0x00); // flag (confirmation)

    EXPECT_EQ(port.Request[7], 0x79); // CRC16 LSB
    EXPECT_EQ(port.Request[8], 0x5B); // CRC16 MSB

    EXPECT_EQ(visitor.Events.size(), 1);
    EXPECT_EQ(visitor.Events[464], 4);

    port.Response = {0xFF, 0xFF, 0xFF, 0xFD, 0x46, 0x14, 0xD2, 0x5F}; // No events
    visitor.Events.clear();
    EXPECT_NO_THROW(ret = ModbusExt::ReadEvents(port,
                                                std::chrono::milliseconds(100),
                                                std::chrono::milliseconds(100),
                                                visitor,
                                                state,
                                                5,
                                                std::chrono::milliseconds(50)));
    EXPECT_FALSE(ret);

    EXPECT_EQ(port.Request.size(), 9);
    EXPECT_EQ(port.Request[0], 0xFD); // slave id
    EXPECT_EQ(port.Request[1], 0x46); // command
    EXPECT_EQ(port.Request[2], 0x10); // subcommand
    EXPECT_EQ(port.Request[3], 0x05); // min slave id
    EXPECT_EQ(port.Request[4], 0x2A); // max length
    EXPECT_EQ(port.Request[5], 0x05); // slave id (confirmation)
    EXPECT_EQ(port.Request[6], 0x01); // flag (confirmation)

    EXPECT_EQ(port.Request[7], 0x1B); // CRC16 LSB
    EXPECT_EQ(port.Request[8], 0x3E); // CRC16 MSB

    EXPECT_EQ(visitor.Events.size(), 0);
}

TEST(TModbusExtTest, ReadEventsReboot)
{
    TPortMock port;
    TTestEventsVisitor visitor;
    ModbusExt::TEventConfirmationState state;

    port.Response =
        {0xFF, 0xFF, 0xFF, 0xFD, 0x46, 0x11, 0x00, 0x00, 0x04, 0x00, 0x0F, 0x00, 0x00, 0xFF, 0x5E}; // Reboot event
    bool ret = false;
    EXPECT_NO_THROW(ret = ModbusExt::ReadEvents(port,
                                                std::chrono::milliseconds(100),
                                                std::chrono::milliseconds(100),
                                                visitor,
                                                state));
    EXPECT_TRUE(ret);

    EXPECT_TRUE(visitor.Reboot);
}

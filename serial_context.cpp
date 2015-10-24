#include <unistd.h>

#include "serial_context.h"
#include "uniel_protocol.h"
#include "milur_protocol.h"

TSerialModbusContext::TSerialModbusContext(PSerialProtocol proto):
    Proto(proto), SlaveAddr(1) {}

void TSerialModbusContext::Connect()
{
    try {
        if (!Proto->IsOpen())
            Proto->Open();
    } catch (const TSerialProtocolException& e) {
        throw TModbusException(e.what());
    }
}

void TSerialModbusContext::Disconnect()
{
    if (Proto->IsOpen())
        Proto->Close();
}

void TSerialModbusContext::SetDebug(bool)
{
    // TBD: set debug
}

void TSerialModbusContext::SetSlave(int slave_addr)
{
    SlaveAddr = slave_addr;
}

void TSerialModbusContext::ReadCoils(int addr, int nb, uint8_t *dest)
{
    try {
        Connect();
        for (int i = 0; i < nb; ++i)
            *dest++ = Proto->ReadRegister(SlaveAddr, addr + i, U8) == 0 ? 0 : 1;
    } catch (const TSerialProtocolTransientErrorException& e) {
        throw TModbusException(e.what());
    } catch (const TSerialProtocolException& e) {
        Disconnect();
        throw TModbusException(e.what());
    }
}

void TSerialModbusContext::WriteCoil(int addr, int value)
{
    try {
        Connect();
        Proto->WriteRegister(SlaveAddr, addr, value ? 0xff : 0, U8);
    } catch (const TSerialProtocolTransientErrorException& e) {
        throw TModbusException(e.what());
    } catch (const TSerialProtocolException& e) {
        Disconnect();
        throw TModbusException(e.what());
    }
}

void TSerialModbusContext::ReadDisceteInputs(int addr, int nb, uint8_t *dest)
{
    ReadCoils(addr, nb, dest);
}

void TSerialModbusContext::ReadHoldingRegisters(int addr, int nb, uint16_t *dest)
{
    try {
        Connect();
        for (int i = 0; i < nb; ++i) {
            // so far, all Uniel address types store register to read
            // in the low byte
            *dest++ = Proto->ReadRegister(SlaveAddr, (addr + i) & 0xFF, U16);
        }
    } catch (const TSerialProtocolTransientErrorException& e) {
        throw TModbusException(e.what());
    } catch (const TSerialProtocolException& e) {
        Disconnect();
        throw TModbusException(e.what());
    }
}

void TSerialModbusContext::WriteHoldingRegister(int addr, uint16_t value)
{
    try {
        Connect();
		if ( (addr >= 0x00) && (addr <= 0xFF) ) {
			// address is between 0x00 and 0xFF, so treat it as
			// normal Uniel register (read via 0x05, write via 0x06)
			Proto->WriteRegister(SlaveAddr, addr, value, U16);
		} else {
			int addr_type = addr >> 24;
			if (addr_type == ADDR_TYPE_BRIGHTNESS ) {
				// address is 0x01XXWWRR, where RR is register to read
				// via 0x05 cmd, WW - register to write via 0x0A cmd
				uint8_t addr_write = (addr & 0xFF00) >> 8;
				Proto->SetBrightness(SlaveAddr, addr_write, value);
			} else {
				throw TModbusException("unsupported register address: " + std::to_string(addr));
			}
		}
    } catch (const TSerialProtocolTransientErrorException& e) {
        throw TModbusException(e.what());
    } catch (const TSerialProtocolException& e) {
        Disconnect();
        throw TModbusException(e.what());
    }
}

void TSerialModbusContext::WriteHoldingRegisters(int addr, int nb, const uint16_t *data)
{
	for (int i = 0; i < nb; ++i) {
		WriteHoldingRegister(addr + i, data[i]);
	}
}

void TSerialModbusContext::ReadInputRegisters(int addr, int nb, uint16_t *dest)
{
    ReadHoldingRegisters(addr, nb, dest);
}

void TSerialModbusContext::ReadDirectRegister(int addr, uint64_t* dest, RegisterFormat format) {
    try {
        Connect();
        *dest++ = Proto->ReadRegister(SlaveAddr, addr, format);
    } catch (const TSerialProtocolTransientErrorException& e) {
        throw TModbusException(e.what());
    } catch (const TSerialProtocolException& e) {
        Disconnect();
        throw TModbusException(e.what());
    }    
}

void TSerialModbusContext::WriteDirectRegister(int addr, uint64_t value, RegisterFormat format) {
    try {
        Connect();
        Proto->WriteRegister(SlaveAddr, addr, value, format);
    } catch (const TSerialProtocolTransientErrorException& e) {
        throw TModbusException(e.what());
    } catch (const TSerialProtocolException& e) {
        Disconnect();
        throw TModbusException(e.what());
    }
}

void TSerialModbusContext::USleep(int usec)
{
    usleep(usec);
}

PModbusContext TSerialConnector::CreateContext(const TSerialPortSettings& settings)
{
    auto proto = CreateProtocol(PAbstractSerialPort(new TSerialPort(settings)));
    return PModbusContext(new TSerialModbusContext(proto));
}

PSerialProtocol TUnielConnector::CreateProtocol(PAbstractSerialPort port) {
    return PSerialProtocol(new TUnielProtocol(port));
}

PSerialProtocol TMilurConnector::CreateProtocol(PAbstractSerialPort port) {
    return PSerialProtocol(new TMilurProtocol(port));
}

// TBD: support debug mode
// TBD: brightness values via direct regs

#include "../include/pci.h"
#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xCF8u
#define PCI_CONFIG_DATA    0xCFCu

static inline void pci_outl(uint16_t port, uint32_t value)
{
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t pci_inl(uint16_t port)
{
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void pci_outw(uint16_t port, uint16_t value)
{
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static uint32_t pci_read_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address = 0x80000000u |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)device << 11) |
                       ((uint32_t)function << 8) |
                       ((uint32_t)offset & 0xFCu);

    pci_outl(PCI_CONFIG_ADDRESS, address);
    return pci_inl(PCI_CONFIG_DATA);
}

static void pci_write_dword(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value)
{
    uint32_t address = 0x80000000u |
                       ((uint32_t)bus << 16) |
                       ((uint32_t)device << 11) |
                       ((uint32_t)function << 8) |
                       ((uint32_t)offset & 0xFCu);

    pci_outl(PCI_CONFIG_ADDRESS, address);
    pci_outl(PCI_CONFIG_DATA, value);
}

uint32_t pci_read_config32(const PciDevice *device, uint8_t offset)
{
    if (!device)
        return 0xFFFFFFFFu;
    return pci_read_dword(device->bus, device->device, device->function, offset);
}

uint16_t pci_read_config16(const PciDevice *device, uint8_t offset)
{
    uint32_t value = pci_read_config32(device, offset);
    return (uint16_t)(value >> ((offset & 2u) * 8u));
}

uint8_t pci_read_config8(const PciDevice *device, uint8_t offset)
{
    uint32_t value = pci_read_config32(device, offset);
    return (uint8_t)(value >> ((offset & 3u) * 8u));
}

void pci_write_config16(const PciDevice *device, uint8_t offset, uint16_t value)
{
    uint32_t address;

    if (!device)
        return;

    address = 0x80000000u |
              ((uint32_t)device->bus << 16) |
              ((uint32_t)device->device << 11) |
              ((uint32_t)device->function << 8) |
              ((uint32_t)offset & 0xFCu);
    pci_outl(PCI_CONFIG_ADDRESS, address);
    pci_outw((uint16_t)(PCI_CONFIG_DATA + (offset & 2u)), value);
}

void pci_write_config32(const PciDevice *device, uint8_t offset, uint32_t value)
{
    if (!device)
        return;
    pci_write_dword(device->bus, device->device, device->function, offset, value);
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id, PciDevice *result)
{
    for (uint32_t bus = 0; bus < 256u; ++bus) {
        for (uint32_t dev = 0; dev < 32u; ++dev) {
            uint32_t id0 = pci_read_dword((uint8_t)bus, (uint8_t)dev, 0, 0x00u);
            uint16_t vendor0 = (uint16_t)(id0 & 0xFFFFu);
            uint8_t header = (uint8_t)(pci_read_dword((uint8_t)bus, (uint8_t)dev, 0, 0x0Cu) >> 16);
            uint32_t functions = (header & 0x80u) ? 8u : 1u;

            if (vendor0 == 0xFFFFu)
                continue;

            for (uint32_t fn = 0; fn < functions; ++fn) {
                uint32_t id;
                uint16_t vendor;
                uint16_t device;

                if (fn == 0u) {
                    id = id0;
                } else {
                    id = pci_read_dword((uint8_t)bus, (uint8_t)dev, (uint8_t)fn, 0x00u);
                }

                vendor = (uint16_t)(id & 0xFFFFu);
                device = (uint16_t)(id >> 16);
                if (vendor != vendor_id || device != device_id)
                    continue;

                if (result) {
                    result->bus = (uint8_t)bus;
                    result->device = (uint8_t)dev;
                    result->function = (uint8_t)fn;
                    result->vendor_id = vendor;
                    result->device_id = device;
                }
                return 0;
            }
        }
    }

    return -1;
}

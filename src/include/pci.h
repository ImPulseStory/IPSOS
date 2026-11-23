#ifndef IPSOS_PCI_H
#define IPSOS_PCI_H

#include <stdint.h>

typedef struct PciDevice {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
} PciDevice;

int pci_find_device(uint16_t vendor_id, uint16_t device_id, PciDevice *result);
uint8_t pci_read_config8(const PciDevice *device, uint8_t offset);
uint16_t pci_read_config16(const PciDevice *device, uint8_t offset);
uint32_t pci_read_config32(const PciDevice *device, uint8_t offset);
void pci_write_config16(const PciDevice *device, uint8_t offset, uint16_t value);
void pci_write_config32(const PciDevice *device, uint8_t offset, uint32_t value);

#endif

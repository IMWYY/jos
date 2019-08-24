#include <inc/error.h>
#include <inc/string.h>
#include <kern/e1000.h>
#include <kern/pmap.h>

volatile void *bar0;

#define E1000REG(offset) (void *)(bar0 + offset)
uint32_t E1000_MAC[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};

struct e1000_tx_desc tx_desc_array[NTXDESC];
char tx_buffer_array[NTXDESC][TX_PKT_SIZE];

struct e1000_rx_desc rx_desc_array[NRXDESC];
char rx_buffer_array[NRXDESC][RX_PKT_SIZE];

int e1000_attach(struct pci_func *pcif) {
  pci_func_enable(pcif);
  // cprintf("reg_base:%08x, reg_size:%08x\n", pcif->reg_base[0],
          // pcif->reg_size[0]);

  bar0 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

  uint32_t *status_reg = E1000REG(E1000_STATUS);
  assert(*status_reg == 0x80080783);

  e1000_transmit_init();
  e1000_receive_init();

  // char buf[10];
  // for (size_t i = 0; i < 10; i++) {
  //   buf[i] = i;
  // }
  // cprintf("e1000 prepare to send data. \n");
  // e1000_transmit(buf, 10);

  return 0;
}

static void e1000_transmit_init() {
  for (int i = 0; i < NTXDESC; ++i) {
    tx_desc_array[i].addr = PADDR(tx_buffer_array[i]);
    tx_desc_array[i].cmd = 0;
    tx_desc_array[i].status |= E1000_TXD_STAT_DD;
  }

  // Transmit Descriptor Base Address
  //(TDBAL/TDBAH) register(s) with the address of the region
  uint32_t *tdbal = (uint32_t *)E1000REG(E1000_TDBAL);
  *tdbal = PADDR(tx_desc_array);

  // Set the Transmit Descriptor Length (TDLEN) register to the size (in bytes)
  // of the descriptor ring.
  // This register must be 128-byte aligned.
  struct e1000_tdlen *tdlen = (struct e1000_tdlen *)E1000REG(E1000_TDLEN);
  tdlen->len = NTXDESC;

  uint32_t *tdbah = (uint32_t *)E1000REG(E1000_TDBAH);
  *tdbah = 0;

  // The Transmit Descriptor Head and Tail (TDH/TDT) registers are initialized
  // (by hardware) to 0b
  struct e1000_tdh *tdh = (struct e1000_tdh *)E1000REG(E1000_TDH);
  tdh->tdh = 0;

  struct e1000_tdt *tdt = (struct e1000_tdt *)E1000REG(E1000_TDT);
  tdt->tdt = 0;

  // Initialize the Transmit Control Register (TCTL) for desired operation
  struct e1000_tctl *tctl = (struct e1000_tctl *)E1000REG(E1000_TCTL);
  tctl->en = 1;
  tctl->psp = 1;
  tctl->ct = 0x10;
  tctl->cold = 0x40;

  // Program the Transmit IPG (TIPG) register with the following decimal values
  // to get the minimum legal Inter Packet Gap
  struct e1000_tipg *tipg = (struct e1000_tipg *)E1000REG(E1000_TIPG);
  tipg->ipgt = 10;
  tipg->ipgr1 = 4;
  tipg->ipgr2 = 6;
}

static void get_ra_address(uint32_t mac[], uint32_t *ral, uint32_t *rah) {
  uint32_t low = 0, high = 0;
  for (int i = 0; i < 4; i++) low |= mac[i] << (8 * i);
  for (int i = 4; i < 6; i++) high |= mac[i] << (8 * i);

  *ral = low;
  *rah = high | E1000_RAH_AV;
}

static void e1000_receive_init() {
  // Program the Receive Address Register(s) (RAL/RAH) with the desired Ethernet
  // addresses. RAL[0]/RAH[0] should always be used to store the Individual
  // Ethernet MAC address of the
  // Ethernet controller.
  uint32_t *ra = (uint32_t *)E1000REG(E1000_RA);
  uint32_t ral, rah;
  get_ra_address(E1000_MAC, &ral, &rah);
  ra[0] = ral;
  ra[1] = rah;

  // Allocate a region of memory for the receive descriptor list.
  for (int i = 0; i < NRXDESC; i++) {
    rx_desc_array[i].addr = PADDR(rx_buffer_array[i]);
  }

  uint32_t *rdbal = (uint32_t *)E1000REG(E1000_RDBAL);
  uint32_t *rdbah = (uint32_t *)E1000REG(E1000_RDBAH);
  *rdbal = PADDR(rx_desc_array);
  *rdbah = 0;

  // Set the Receive Descriptor Length (RDLEN) register to the size (in bytes)
  // of the descriptor ring.This register must be 128-byte aligned
  struct e1000_rdlen *rdlen = (struct e1000_rdlen *)E1000REG(E1000_RDLEN);
  rdlen->len = NRXDESC;

  //  Head should point to the first valid receive descriptor in the
  // descriptor ring and tail should point to one descriptor beyond the last
  // valid descriptor in the descriptor ring.
  struct e1000_rdh *rdh = (struct e1000_rdh *)E1000REG(E1000_RDH);
  struct e1000_rdt *rdt = (struct e1000_rdt *)E1000REG(E1000_RDT);
  rdh->rdh = 0;
  rdt->rdt = NRXDESC - 1;

  // Program the Receive Control (RCTL) register with appropriate values for
  // desired operation
  uint32_t *rctl = (uint32_t *)E1000REG(E1000_RCTL);
  *rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC;
}

int e1000_transmit(void *buf, size_t len) {
  assert(len <= TX_PKT_SIZE);

  struct e1000_tdt *tdt = (struct e1000_tdt *)E1000REG(E1000_TDT);
  uint32_t cur = tdt->tdt % NTXDESC;
  if (!(tx_desc_array[cur].status & E1000_TXD_STAT_DD)) {
    return -E_TRANSMIT_RETRY;
  }

  tx_desc_array[cur].length = len;
  tx_desc_array[cur].status &= ~E1000_TXD_STAT_DD;
  tx_desc_array[cur].cmd |= (E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS);

  memcpy(tx_buffer_array[cur], buf, len);
  tdt->tdt = (cur + 1) % NTXDESC;
  return 0;
}

int e1000_receive(void *addr, size_t *len) {
  // static int32_t next = 0;
  struct e1000_rdt *rdt = (struct e1000_rdt *)E1000REG(E1000_RDT);
  int32_t cur = (rdt->rdt + 1) % NRXDESC;

  if (!(rx_desc_array[cur].status & E1000_RXD_STAT_DD)) {
    return -E_RECEIVE_RETRY;
  }

  if (rx_desc_array[cur].errors) {
    cprintf("e1000_receive got error \n");
    return -E_RECEIVE_RETRY;
  }

  *len = rx_desc_array[cur].length;

  memcpy(addr, rx_buffer_array[cur], *len);

  rdt->rdt = (cur) % NRXDESC;
  // next = (next + 1) % NRXDESC;
  return 0;
}

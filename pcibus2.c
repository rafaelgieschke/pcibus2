#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/pci.h>

#define SECTION(name) __attribute__((used, no_reorder, section(#name)))

#define ARGS(regs) regs->di, regs->si, regs->dx, regs->cx, regs->r8, regs->r9

static void call6(void *func, void *args[6]) {
  ((void (*)(void *, void *, void *, void *, void *, void *))func)(
      args[0], args[1], args[2], args[3], args[4], args[5]);
};

#define probe(func)                                                            \
  struct func##__data_ {                                                       \
    void *regs[6];                                                             \
  };                                                                           \
  static int func##__entry_(struct kretprobe_instance *ri,                     \
                            struct pt_regs *regs) {                            \
    struct func##__data_ *data = (struct func##__data_ *)ri->data;             \
    *data = (struct func##__data_){{ARGS(regs)}};                              \
    call6(func##__entry, data->regs);                                          \
    return 0;                                                                  \
  }                                                                            \
  static int func##__exit_(struct kretprobe_instance *ri,                      \
                           struct pt_regs *regs) {                             \
    struct func##__data_ *data = (struct func##__data_ *)ri->data;             \
    call6(func##__exit, data->regs);                                           \
    return 0;                                                                  \
  }                                                                            \
  static struct kretprobe func##_probe = {                                     \
      .kp = {.symbol_name = #func},                                            \
      .entry_handler = func##__entry_,                                         \
      .handler = func##__exit_,                                                \
      .maxactive = 20,                                                         \
      .data_size = sizeof(struct func##__data_),                               \
  };                                                                           \
  static struct kretprobe *func##_probe__entry SECTION(probes) = &func##_probe;

// https://github.com/torvalds/linux/blob/b5f217084ab3ddd4bdd03cd437f8e3b7e2d1f5b6/drivers/pci/setup-res.c#L326
static void pci_assign_resource__entry(struct pci_dev *pci_dev, int resno) {
  if (!pci_dev)
    return;
  struct pci_bus *pci_bus = pci_dev->bus;
  int bus = pci_bus ? pci_bus->number : -1;
  int devfn = pci_dev->devfn;
  pr_info("pci_assign_resource: %p - %02x:%02x BAR %d %pR\n", pci_dev, bus,
          devfn, resno, &pci_dev->resource[resno]);
}
static void pci_assign_resource__exit(struct pci_dev *pci_dev, int resno) {}
probe(pci_assign_resource);

// https://github.com/torvalds/linux/blob/7503345ac5f5e82fd9a36d6e6b447c016376403a/drivers/pci/quirks.c#L211
static void pci_fixup_device__entry(enum pci_fixup_pass pass,
                                   struct pci_dev *dev) {
  if (pass != pci_fixup_header)
    return;
  if (!dev)
    return;
  struct pci_bus *pci_bus = dev->bus;
  int bus = pci_bus ? pci_bus->number : -1;

  for (int i = 0; i < PCI_NUM_RESOURCES; i++) {
    pr_info("pci_fixup_header: %p - %02x:%02x BAR %d %pR\n", dev, bus,
            dev->devfn, i, &dev->resource[i]);
  }
};
static void pci_fixup_device__exit(void) { };
probe(pci_fixup_device);

// https://github.com/torvalds/linux/blob/7503345ac5f5e82fd9a36d6e6b447c016376403a/drivers/pci/access.c#L570
static void pci_read__entry(struct pci_bus *bus, unsigned int devfn, int where,
                           int size, u32 *value) {
  if (bus->number == 3) {
    return;
  }
}
static void pci_read__exit(struct pci_bus *bus, unsigned int devfn, int where,
                          int size, u32 *value) {
  u32 oldval = *value;
#define _(where, size) ((where << 8) | size)
  switch (_(where, size)) {
  case _(0x420, 4):
    *value = 0x22010015;
    break;
  case _(0x32c, 4):
    *value = 0x00020002;
    break;
  case _(0x32c, 2):
  case _(0x32e, 2):
    *value = 0x0002;
    break;
  }
  if (true || *value != oldval) {
    pr_info("pci_read: %02x:%02x@%03x.%d: %x -> %x\n", bus->number, devfn,
            where, size, oldval, *value);
  }
}
probe(pci_read);

static void sriov_init__entry(void) {
  pr_info("sriov_init");
}
static void sriov_init__exit(void) {}
probe(sriov_init);

extern struct kretprobe *probes[];
static struct kretprobe *_stop_probes[0] SECTION(probes);

static int __init init(void) {
  for (struct kretprobe **probe = probes; probe < _stop_probes; probe++) {
    pr_info("Registering kretprobe for: %s\n", (*probe)->kp.symbol_name);
  }
  register_kretprobes(probes, _stop_probes - probes);
  return 0;
}
module_init(init);

static void __exit cleanup(void) {
  unregister_kretprobes(probes, _stop_probes - probes);
}
module_exit(cleanup);

MODULE_LICENSE("Dual MIT/GPL");

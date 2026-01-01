#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/pci.h>

#define SECTION(name) __attribute__((used, no_reorder, section(#name)))

#define ARGS(regs) regs->di, regs->si, regs->dx, regs->cx, regs->r8, regs->r9

static int call6(void *func, void *args[6]) {
  return ((int (*)(void *, void *, void *, void *, void *, void *))func)(
      args[0], args[1], args[2], args[3], args[4], args[5]);
};

#define probe(func)                                                            \
  struct func##_data_ {                                                        \
    void *regs[6];                                                             \
  };                                                                           \
  static int func##_before_(struct kretprobe_instance *ri,                     \
                            struct pt_regs *regs) {                            \
    struct func##_data_ *data = (struct func##_data_ *)ri->data;               \
    *data = (struct func##_data_){{ARGS(regs)}};                               \
    return call6(func##_before, data->regs);                                   \
  }                                                                            \
  static int func##_after_(struct kretprobe_instance *ri,                      \
                           struct pt_regs *regs) {                             \
    struct func##_data_ *data = (struct func##_data_ *)ri->data;               \
    return call6(func##_after, data->regs);                                    \
  }                                                                            \
  static struct kretprobe func##_probe = {                                     \
      .kp = {.symbol_name = #func},                                            \
      .entry_handler = func##_before_,                                         \
      .handler = func##_after_,                                                \
      .maxactive = 20,                                                         \
      .data_size = sizeof(struct func##_data_),                                \
  };                                                                           \
  static struct kretprobe *func##_probe_entry SECTION(probes) = &func##_probe;

// https://github.com/torvalds/linux/blob/b5f217084ab3ddd4bdd03cd437f8e3b7e2d1f5b6/drivers/pci/setup-res.c#L326
static int pci_assign_resource_before(struct pci_dev *pci_dev, int resno) {
  if (!pci_dev)
    return 1;
  struct pci_bus *pci_bus = pci_dev->bus;
  int bus = pci_bus ? pci_bus->number : -1;
  int devfn = pci_dev->devfn;
  pr_info("pci_assign_resource: %p - %02x:%02x BAR %d %pR\n", pci_dev, bus,
          devfn, resno, &pci_dev->resource[resno]);
  return 1;
}
static int pci_assign_resource_after(struct pci_dev *pci_dev, int resno) {
  return 0;
}
probe(pci_assign_resource);

// https://github.com/torvalds/linux/blob/7503345ac5f5e82fd9a36d6e6b447c016376403a/drivers/pci/quirks.c#L211
static int pci_fixup_device_before(enum pci_fixup_pass pass,
                                   struct pci_dev *dev) {
  if (pass != pci_fixup_header)
    return 1;
  if (!dev)
    return 1;
  struct pci_bus *pci_bus = dev->bus;
  int bus = pci_bus ? pci_bus->number : -1;

  for (int i = 0; i < PCI_NUM_RESOURCES; i++) {
    pr_info("pci_fixup_header: %p - %02x:%02x BAR %d %pR\n", dev, bus,
            dev->devfn, i, &dev->resource[i]);
  }
  return 1;
};
static int pci_fixup_device_after(void) { return 0; };
probe(pci_fixup_device);

// https://github.com/torvalds/linux/blob/7503345ac5f5e82fd9a36d6e6b447c016376403a/drivers/pci/access.c#L570
static int pci_read_before(struct pci_bus *bus, unsigned int devfn, int where,
                           int size, u32 *value) {
  if (bus->number == 3) {
    return 0;
  }
  return 1;
}
static int pci_read_after(struct pci_bus *bus, unsigned int devfn, int where,
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
  return 0;
}
probe(pci_read);

static int sriov_init_before(void) {
  pr_info("sriov_init");
  return 1;
}
static int sriov_init_after(void) { return 0; }
probe(sriov_init);

extern struct kretprobe *probes[];
static struct kretprobe *_stop_probes[0] SECTION(probes);

int init_module(void) {
  for (struct kretprobe **probe = probes; probe < _stop_probes; probe++) {
    pr_info("Registering kretprobe for: %s\n", (*probe)->kp.symbol_name);
  }
  register_kretprobes(probes, _stop_probes - probes);
  return 0;
}

void cleanup_module(void) {
  unregister_kretprobes(probes, _stop_probes - probes);
}

MODULE_LICENSE("Dual MIT/GPL");

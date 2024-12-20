#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/pci.h>

#define _SIZE(_6, _5, _4, _3, _2, _1, n, ...) n
#define SIZE(...) _SIZE(__VA_ARGS__, 6, 5, 4, 3, 2, 1)
#define _CONCAT(a, b) a##b
#define CONCAT(a, b) _CONCAT(a, b)
#define SELECT(f, ...) CONCAT(f, SIZE(__VA_ARGS__))(__VA_ARGS__)

#define args(...) SELECT(args, __VA_ARGS__)
#define reg(var, reg) var = _data->regs.reg = regs->reg
#define args1(_0) \
  ;               \
  reg(_0, di)
#define args2(_0, _1) \
  args1(_0);          \
  reg(_1, si)
#define args3(_0, _1, _2) \
  args2(_0, _1);          \
  reg(_2, dx)
#define args4(_0, _1, _2, _3) \
  args3(_0, _1, _2);          \
  reg(_3, cx)
#define args5(_0, _1, _2, _3, _4) \
  args4(_0, _1, _2, _3);          \
  reg(_4, r8)
#define args6(_0, _1, _2, _3, _4, _5) \
  args5(_0, _1, _2, _3, _4);          \
  reg(_5, r9)

struct regs
{
  unsigned long di, si, dx, cx, r8, r9;
};

#define probe(name, params, fields, in, out)                                     \
  struct name##_data fields;                                                     \
  struct name##_data_data                                                        \
  {                                                                              \
    struct name##_data data;                                                     \
    struct regs regs;                                                            \
  };                                                                             \
  static int name##_in(struct kretprobe_instance *ri, struct pt_regs *regs)      \
  {                                                                              \
    struct name##_data_data *_data = (struct name##_data_data *)ri->data;        \
    struct name##_data *data = &_data->data;                                     \
    args params;                                                                 \
    in                                                                           \
  };                                                                             \
  static int name##_out(struct kretprobe_instance *ri, struct pt_regs *regs_new) \
  {                                                                              \
    struct name##_data_data *_data = (struct name##_data_data *)ri->data;        \
    struct name##_data *data = &_data->data;                                     \
    struct regs *regs = &_data->regs;                                            \
    args params;                                                                 \
    out                                                                          \
  };                                                                             \
  static struct kretprobe name##_probe = {                                       \
      .kp = {.symbol_name = #name},                                              \
      .entry_handler = name##_in,                                                \
      .handler = name##_out,                                                     \
      .maxactive = 20,                                                           \
      .data_size = sizeof(struct name##_data_data),                              \
  }

probe(
    // https://github.com/torvalds/linux/blob/b5f217084ab3ddd4bdd03cd437f8e3b7e2d1f5b6/drivers/pci/setup-res.c#L326
    pci_assign_resource, (struct pci_dev * pci_dev, int resno), {},
    {
      if (!pci_dev)
        return 1;
      struct pci_bus *pci_bus = pci_dev->bus;
      int bus = pci_bus ? pci_bus->number : -1;
      int devfn = pci_dev->devfn;
      pr_info("pci_assign_resource: %p - %02x:%02x BAR %d %pR\n", pci_dev, bus,
              devfn, resno, &pci_dev->resource[resno]);
      return 1;
    },
    { return 0; });

probe(
    // https://github.com/torvalds/linux/blob/7503345ac5f5e82fd9a36d6e6b447c016376403a/drivers/pci/quirks.c#L211
    pci_fixup_device, (enum pci_fixup_pass pass, struct pci_dev *dev), {},
    {
      if (pass != pci_fixup_header)
        return 1;
      if (!dev)
        return 1;
      struct pci_bus *pci_bus = dev->bus;
      int bus = pci_bus ? pci_bus->number : -1;

      for (int i = 0; i < PCI_NUM_RESOURCES; i++)
      {
        pr_info("pci_fixup_header: %p - %02x:%02x BAR %d %pR\n", dev, bus,
                dev->devfn, i, &dev->resource[i]);
      }
      return 1;
    },
    { return 0; });

probe(
    // https://github.com/torvalds/linux/blob/7503345ac5f5e82fd9a36d6e6b447c016376403a/drivers/pci/access.c#L570
    pci_read, (struct pci_bus * bus, unsigned int devfn, int where, int size, u32 *value), {},
    {
      if (bus->number == 3)
      {
        return 0;
      }
      return 1;
    },
    {
      u32 oldval = *value;
#define _(where, size) ((where << 8) | size)
      switch (_(where, size))
      {
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
      if (true || *value != oldval)
      {
        pr_info("pci_read: %02x:%02x@%03x.%d: %x -> %x\n",
                bus->number, devfn, where, size, oldval, *value);
      }
      return 0;
    });

probe(sriov_init, (void *_), {}, {
  pr_info("sriov_init");
  return 1; }, { return 0; });

static struct kretprobe *probes[] = {
    &pci_read_probe,
    &pci_fixup_device_probe,
    &pci_assign_resource_probe,
    &sriov_init_probe,
};

int init_module(void)
{
  register_kretprobes(probes, ARRAY_SIZE(probes));
  return 0;
}

void cleanup_module(void) { unregister_kretprobes(probes, ARRAY_SIZE(probes)); }

MODULE_LICENSE("GPL");

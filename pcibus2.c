#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs4.h>
#include <linux/nfs_fs.h>
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

// https://github.com/gregkh/linux/commit/a3dbaa09db4f5980d0357b9e0da02795c4b218db
static void nfs_server_copy_userdata__entry(struct nfs_server *target, struct nfs_server *source) {
  target->rsize = source->rsize;
  target->wsize = source->wsize;
}
static void nfs_server_copy_userdata__exit(struct nfs_server *target, struct nfs_server *source) {}
probe(nfs_server_copy_userdata);

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

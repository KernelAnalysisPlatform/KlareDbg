#include "config.h"
#include "qemu-common.h"
#include "exec/exec-all.h"           /* MAX_OPC_PARAM_IARGS */
#include "exec/cpu_ldst.h"
#include "tcg-op.h"
#include "kldbg/tcg_track.h"

int GLOBAL_QIRA_did_init = 0;
CPUArchState *GLOBAL_CPUArchState;
struct change *GLOBAL_change_buffer;

uint32_t GLOBAL_qira_log_fd;
size_t GLOBAL_change_size;

struct logstate *GLOBAL_logstate;

// input args
uint32_t GLOBAL_start_clnum = 1;
int GLOBAL_parent_id = -1, GLOBAL_id = -1;

int GLOBAL_tracelibraries = 0;
uint64_t GLOBAL_gatetrace = 0;
FILE *GLOBAL_asm_file = NULL;
FILE *GLOBAL_strace_file = NULL;

struct change GLOBAL_pending_changes[PENDING_CHANGES_MAX_ADDR/4];

void resize_change_buffer(size_t size) {
  if(ftruncate(GLOBAL_qira_log_fd, size)) {
    perror("ftruncate");
  }
  GLOBAL_change_buffer = mmap(NULL, size,
         PROT_READ | PROT_WRITE, MAP_SHARED, GLOBAL_qira_log_fd, 0);
  GLOBAL_logstate = (struct logstate *)GLOBAL_change_buffer;
  if (GLOBAL_change_buffer == NULL) QIRA_DEBUG("MMAP FAILED!\n");
}

void init_QIRA(CPUArchState *env, int id) {
  QIRA_DEBUG("init QIRA called\n");
  GLOBAL_QIRA_did_init = 1;
  GLOBAL_CPUArchState = env;   // unused

  OPEN_GLOBAL_ASM_FILE

  char fn[PATH_MAX];
  sprintf(fn, "/tmp/qira_logs/%d_strace", id);
  GLOBAL_strace_file = fopen(fn, "w");

  sprintf(fn, "/tmp/qira_logs/%d", id);

  // unlink it first
  unlink(fn);
  GLOBAL_qira_log_fd = open(fn, O_RDWR | O_CREAT, 0644);
  GLOBAL_change_size = 1;
  memset(GLOBAL_pending_changes, 0, (PENDING_CHANGES_MAX_ADDR/4) * sizeof(struct change));

  resize_change_buffer(GLOBAL_change_size * sizeof(struct change));
  memset(GLOBAL_change_buffer, 0, sizeof(struct change));

  // skip the first change
  GLOBAL_logstate->change_count = 1;
  GLOBAL_logstate->is_filtered = 0;
  GLOBAL_logstate->in_mod = 0;
  GLOBAL_logstate->this_pid = getpid();

  // do this after init_QIRA
  GLOBAL_logstate->changelist_number = GLOBAL_start_clnum-1;
  GLOBAL_logstate->first_changelist_number = GLOBAL_start_clnum;
  GLOBAL_logstate->parent_id = GLOBAL_parent_id;

  // use all fds up to 30
  int i;
  int dupme = open("/dev/null", O_RDONLY);
  struct stat useless;
  for (i = 0; i < 30; i++) {
    sprintf(fn, "/proc/self/fd/%d", i);
    if (stat(fn, &useless) == -1) {
      //printf("dup2 %d %d\n", dupme, i);
      dup2(dupme, i);
    }
  }

  // if (GLOBAL_librarymap == NULL){
  //     init_librarymap();
  // }

  // no more opens can happen here in QEMU, only the target process
}

struct change *add_change(target_ulong addr, uint64_t data, uint32_t flags) {
  size_t cc = __sync_fetch_and_add(&GLOBAL_logstate->change_count, 1);

  if (cc == GLOBAL_change_size) {
    // double the buffer size
    QIRA_DEBUG("doubling buffer with size %d\n", GLOBAL_change_size);
    resize_change_buffer(GLOBAL_change_size * sizeof(struct change) * 2);
    GLOBAL_change_size *= 2;
  }
  struct change *this_change = GLOBAL_change_buffer + cc;
  this_change->address = (uint64_t)addr;
  this_change->data = data;
  this_change->changelist_number = GLOBAL_logstate->changelist_number;
  this_change->flags = IS_VALID | flags;
  return this_change;
}

void add_pending_change(target_ulong addr, uint64_t data, uint32_t flags) {
  if (addr < PENDING_CHANGES_MAX_ADDR) {
    GLOBAL_pending_changes[addr/4].address = (uint64_t)addr;
    GLOBAL_pending_changes[addr/4].data = data;
    GLOBAL_pending_changes[addr/4].flags = IS_VALID | flags;
  }
}

void commit_pending_changes(void) {
  int i;
  for (i = 0; i < PENDING_CHANGES_MAX_ADDR/4; i++) {
    struct change *c = &GLOBAL_pending_changes[i];
    if (c->flags & IS_VALID) add_change(c->address, c->data, c->flags);
  }
  memset(GLOBAL_pending_changes, 0, (PENDING_CHANGES_MAX_ADDR/4) * sizeof(struct change));
}

struct change *track_syscall_begin(void *env, target_ulong sysnr);
struct change *track_syscall_begin(void *env, target_ulong sysnr) {
  int i;
  QIRA_DEBUG("syscall: %d\n", sysnr);
  if (GLOBAL_logstate->is_filtered == 1) {
    for (i = 0; i < 0x20; i+=4) {
      add_change(i, *(target_ulong*)(env+i), IS_WRITE | (sizeof(target_ulong)*8));
    }
  }
  return add_change(sysnr, 0, IS_SYSCALL);
}


// all loads and store happen in libraries...
void track_load(target_ulong addr, uint64_t data, int size) {
  QIRA_DEBUG("load:  0x%x:%d\n", addr, size);
  add_change(addr, data, IS_MEM | size);
}

void track_store(target_ulong addr, uint64_t data, int size) {
  QIRA_DEBUG("store: 0x%x:%d = 0x%lX\n", addr, size, data);
  add_change(addr, data, IS_MEM | IS_WRITE | size);
}

void track_read(target_ulong base, target_ulong offset, target_ulong data, int size) {
  QIRA_DEBUG("read:  %x+%x:%d = %x\n", base, offset, size, data);
  if ((int)offset < 0) return;
  if (GLOBAL_logstate->is_filtered == 0) add_change(offset, data, size);
}

void track_write(target_ulong base, target_ulong offset, target_ulong data, int size) {
  QIRA_DEBUG("write: %x+%x:%d = %x\n", base, offset, size, data);
  if ((int)offset < 0) return;
  if (GLOBAL_logstate->is_filtered == 0) add_change(offset, data, IS_WRITE | size);
  else add_pending_change(offset, data, IS_WRITE | size);
  //else add_change(offset, data, IS_WRITE | size);
}

#ifndef __TCG_TRACK_H__
#define __TCG_TRACK_H__

//#define QIRA_DEBUG(...) {}
//#define QIRA_DEBUG qemu_debug
#define QIRA_DEBUG printf

/* address and size of module to be traced */
extern target_ulong mod_addr, mod_size;

#define IN_MOD(x) (mod_addr <= x && x < mod_addr + mod_size)
// struct storing change data
struct change {
  uint64_t address;
  uint64_t data;
  uint32_t changelist_number;
  uint32_t flags;
};

// prototypes
void init_QIRA(int id);
struct change *add_change(target_ulong addr, uint64_t data, uint32_t flags);
void track_load(target_ulong a, uint64_t data, int size);
void track_store(target_ulong a, uint64_t data, int size);
void track_read(target_ulong base, target_ulong offset, target_ulong data, int size);
void track_write(target_ulong base, target_ulong offset, target_ulong data, int size);
void add_pending_change(target_ulong addr, uint64_t data, uint32_t flags);
void commit_pending_changes(void);
void resize_change_buffer(size_t size);

#define IS_VALID      0x80000000
#define IS_WRITE      0x40000000
#define IS_MEM        0x20000000
#define IS_START      0x10000000
#define IS_SYSCALL    0x08000000
#define SIZE_MASK 0xFF

#define FAKE_SYSCALL_LOADSEG 0x10001

extern int GLOBAL_QIRA_did_init;
extern CPUState *GLOBAL_CPUState;
extern struct change *GLOBAL_change_buffer;

extern uint32_t GLOBAL_qira_log_fd;
extern size_t GLOBAL_change_size;

// current state that must survive forks
struct logstate {
  uint32_t change_count;
  uint32_t changelist_number;
  uint32_t is_filtered;
  uint32_t first_changelist_number;
  uint32_t in_mod;
  int parent_id;
  int this_pid;
};
extern struct logstate *GLOBAL_logstate;

// input args
extern uint32_t GLOBAL_start_clnum;
extern int GLOBAL_parent_id, GLOBAL_id;

extern int GLOBAL_tracelibraries;
extern uint64_t GLOBAL_gatetracek;

#define OPEN_GLOBAL_ASM_FILE { if (unlikely(GLOBAL_asm_file == NULL)) { GLOBAL_asm_file = fopen("/tmp/qira_asm", "a"); } }
extern FILE *GLOBAL_asm_file;
extern FILE *GLOBAL_strace_file;

// should be 0ed on startup
#define PENDING_CHANGES_MAX_ADDR 0x100

#endif

/*
 Copyright (C) <2016> <Ren Kimura>

 KlTracer is register,memory tracer for KlDbg backend.

 You can redistribute and modify it under the terms of the GNU GPL, version 3 or later,
 but it is made available WITHOUT ANY WARRANTY. See the top-level
 README file for more details.

 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "config.h"

#include "DECAF_main.h"
#include "DECAF_callback.h"
#include "DECAF_target.h"
#include "shared/vmi_callback.h"
#include "shared/hookapi.h"

#include "tcg_track.h"

CPUState *global_state = NULL;

static plugin_interface_t kltracer_interface;
DECAF_Handle mem_write_cb_handle=0;
DECAF_Handle mem_read_cb_handle=0;
DECAF_Handle block_begin_cb_handle=0;

static inline int is_register(target_ulong addr) {
  /* XXX: number of registers is depend on architecture.
    This time, using [CPUState->regs, CPUState->regs + 0x20] region. */
  return (target_ulong)(global_state->regs) <= addr && addr <= (target_ulong)(global_state->regs) + 0x20;
}

static void kltracer_mem_access(DECAF_Callback_Params* dcp, int w)
{
	target_ulong virt_addr, value;
	int size;
  if (GLOBAL_logstate->in_mod) {
  	virt_addr = dcp->mw.vaddr;
    value = dcp->mw.value;
  	size = dcp->mw.dt;
    if (w) {
      if (is_register(virt_addr)) track_write((target_ulong)(global_state->regs),
                                              virt_addr - (target_ulong)(global_state->regs),
                                              value, size);
      else track_store(virt_addr, value, size);
    } else {
      if (is_register(virt_addr)) track_read((target_ulong)(global_state->regs),
                                              virt_addr - (target_ulong)(global_state->regs),
                                              value, size);
      else track_load(virt_addr, value, size);
    }
  }
}
static void kltracer_mem_write(DECAF_Callback_Params* dcp)
{
  kltracer_mem_access(dcp, 1);
}

static void kltracer_mem_read(DECAF_Callback_Params* dcp)
{
  kltracer_mem_access(dcp, 0);
}

static void kltracer_block_begin(DECAF_Callback_Params* dcp)
{
  CPUState *env = dcp->bb.env;
  if (global_state == NULL) { global_state = env; QIRA_DEBUG("regs:0x%lx\n", global_state->regs);  }
  target_ulong pc;
  pc = DECAF_getPC(env);
  if (IN_MOD(pc)) GLOBAL_logstate->in_mod = 1;
  else GLOBAL_logstate->in_mod = 0;
}

void unregister_callbacks()
{
	DECAF_printf(default_mon,"Unregister_callbacks\n");
	if(mem_write_cb_handle){
		DECAF_printf(default_mon,"DECAF_unregister_callback(DECAF_MEM_WRITE_CB,mem_write_cb_handle);\n");
		DECAF_unregister_callback(DECAF_MEM_WRITE_CB, mem_write_cb_handle);
	}
	if(mem_read_cb_handle){
		DECAF_printf(default_mon,"DECAF_unregister_callback(DECAF_MEM_READ_CB,mem_read_cb_handle);\n");
		DECAF_unregister_callback(DECAF_MEM_READ_CB, mem_read_cb_handle);
	}
}

static void kltracer_cleanup()
{
	unregister_callbacks();
}

plugin_interface_t *init_plugin()
{
  kltracer_interface.plugin_cleanup=kltracer_cleanup;
  init_QIRA(0);
  mem_write_cb_handle = DECAF_register_callback(DECAF_MEM_WRITE_CB,
                                                kltracer_mem_write,NULL);
  mem_read_cb_handle = DECAF_register_callback(DECAF_MEM_READ_CB,
                                              kltracer_mem_read,NULL);
  block_begin_cb_handle=DECAF_register_callback(DECAF_BLOCK_BEGIN_CB,
                                                kltracer_block_begin,NULL);

  return &kltracer_interface;
}

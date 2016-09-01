#ifndef __KLTRACE_H__
#define __KLTRACE_H__

#include <inttypes.h>
#include "tcg-op.h"

extern TCGv tempidx, tempidx2;
extern uint16_t *gen_old_opc_ptr;
extern TCGArg *gen_old_opparam_ptr;

#endif /* __DECAF_TCG_TAINT_H__ */

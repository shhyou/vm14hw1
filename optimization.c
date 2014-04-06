/*
 *  (C) 2010 by Computer System Laboratory, IIS, Academia Sinica, Taiwan.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include "exec-all.h"
#include "tcg-op.h"
#include "helper.h"
#define GEN_HELPER 1
#include "helper.h"
#include "optimization.h"

extern uint8_t *optimization_ret_addr;

/*
 * Shadow Stack
 */
list_t *shadow_hash_list;

static inline void shack_init(CPUState *env)
{
}

/*
 * shack_set_shadow()
 *  Insert a guest eip to host eip pair if it is not yet created.
 */
void shack_set_shadow(CPUState *env, target_ulong guest_eip, unsigned long *host_eip)
{
}

/*
 * helper_shack_flush()
 *  Reset shadow stack.
 */
void helper_shack_flush(CPUState *env)
{
}

/*
 * push_shack()
 *  Push next guest eip into shadow stack.
 */
void push_shack(CPUState *env, TCGv_ptr cpu_env, target_ulong next_eip)
{
}

/*
 * pop_shack()
 *  Pop next host eip from shadow stack.
 */
void pop_shack(TCGv_ptr cpu_env, TCGv next_eip)
{
}

/*
 * Indirect Branch Target Cache
 */
__thread int update_ibtc;

static struct ibtc_table *itbc_tbl;
static target_ulong itbc_query_eip;

/*
 * helper_lookup_ibtc()
 *  Look up IBTC. Return next host eip if cache hit or
 *  back-to-dispatcher stub address if cache miss.
 */
void *helper_lookup_ibtc(target_ulong guest_eip)
{
    const target_ulong idx = guest_eip & IBTC_CACHE_MASK;
    if (guest_eip == itbc_tbl->htable[idx].guest_eip) {
        return itbc_tbl->htable[idx].tb->tc_ptr;
    } else {
        update_ibtc = 1;
        itbc_query_eip = guest_eip;
        return optimization_ret_addr;
    }
}

/*
 * update_ibtc_entry()
 *  Populate eip and tb pair in IBTC entry.
 */
void update_ibtc_entry(TranslationBlock *tb)
{
    const target_ulong idx = itbc_query_eip & IBTC_CACHE_MASK;
    itbc_tbl->htable[idx].guest_eip = itbc_query_eip;
    itbc_tbl->htable[idx].tb = tb;
    update_ibtc = 0;
}

/*
 * ibtc_init()
 *  Create and initialize indirect branch target cache.
 */
static inline void ibtc_init(CPUState *env)
{
    itbc_tbl = (struct ibtc_table*)malloc(sizeof (struct ibtc_table));
    memset(itbc_tbl, 0, sizeof(struct ibtc_table));
}

/*
 * init_optimizations()
 *  Initialize optimization subsystem.
 */
int init_optimizations(CPUState *env)
{
    shack_init(env);
    ibtc_init(env);
    return 0;
}

/*
 * vim: ts=8 sts=4 sw=4 expandtab
 */

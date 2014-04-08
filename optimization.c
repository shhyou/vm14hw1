/*
 *  (C) 2010 by Computer System Laboratory, IIS, Academia Sinica, Taiwan.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "exec-all.h"
#include "tcg-op.h"
#include "helper.h"
#define GEN_HELPER 1
#include "helper.h"
#include "optimization.h"

extern uint8_t *optimization_ret_addr;

/*
 * Our stack grows downward, just like x86's stack
 * shack_top ----> +---------------+    |
 *                 +----  ...  ----+    | push
 * shack     ----> +---------------+    v
 */
static unsigned long stk_diff;
static struct shadow_pair **hash_tbl;
static unsigned long slot_idx, **slot_tbl;

static inline void new_slot_block(void) {
    slot_tbl = (unsigned long**)malloc(sizeof(unsigned long*)*MAX_CALL_SLOT);
    slot_idx = -1;
}

static inline unsigned long** alloc_slot(void) {
  ++slot_idx;
  if (slot_idx == MAX_CALL_SLOT)
    new_slot_block();
  return slot_tbl + slot_idx;
}

static inline void shack_init(CPUState *env)
{
    env->shack = (target_ulong*)malloc(sizeof(target_ulong)*SHACK_SIZE);
    env->shadow_ret_addr = (unsigned long*)malloc(sizeof(unsigned long)*SHACK_SIZE);
    env->shack_top = env->shack + SHACK_SIZE - 1;
    *env->shack_top = 0; /* leave an item for margin: guest_eip=0, never reached */
    stk_diff = ((unsigned long)env->shadow_ret_addr) - ((unsigned long)env->shack);

    hash_tbl = (struct shadow_pair**)malloc(sizeof(struct shadow_pair*)*MAX_CALL_SLOT);
    memset(hash_tbl, 0, sizeof(unsigned long)*MAX_CALL_SLOT);

    new_slot_block();
}

static inline unsigned long** shack_hash_query(target_ulong guest_eip) {
    int idx = guest_eip & (MAX_CALL_SLOT - 1);
    struct shadow_pair *pr = hash_tbl[idx];
    while (pr!=NULL && guest_eip!=pr->guest_eip)
      pr = pr->next;
    return pr? pr->shadow_slot : NULL;
}

static inline unsigned long** shack_hash_insert(target_ulong guest_eip) {
    int idx = guest_eip & (MAX_CALL_SLOT - 1);
    struct shadow_pair *pr = (struct shadow_pair*)malloc(sizeof(struct shadow_pair));
    pr->guest_eip = guest_eip;
    pr->shadow_slot = alloc_slot();
    pr->next = hash_tbl[idx];
    hash_tbl[idx] = pr;
    return pr->shadow_slot;
}

/*
 * shack_set_shadow()
 *  Insert a guest eip to host eip pair if it is not yet created.
 */
void shack_set_shadow(CPUState *env, target_ulong guest_eip, unsigned long *host_eip)
{
    unsigned long **slot_ptr = shack_hash_query(guest_eip);
    if (slot_ptr != NULL)
      *slot_ptr = host_eip;
}

void helper_print_shack(CPUState *env)
{
    printf("shack_top=%p,shack=%p\n",env->shack_top,env->shack);
}

/*
 * push_shack()
 *  Push next guest eip into shadow stack.
 */
void push_shack(CPUState *env, TCGv_ptr cpu_env, target_ulong next_eip)
{
    int lbl_push = gen_new_label();
    TCGv_ptr shack_top_ptr = tcg_temp_new(), shack_end_ptr = tcg_temp_new();
    unsigned long **slot_ptr = shack_hash_query(next_eip);
    if (slot_ptr == NULL)
      slot_ptr = shack_hash_insert(next_eip);
    /*
      target_ulong* shack_top_ptr = cpu_env->shack_top
      target_ulong* shack_end_ptr = cpu_env->shack
      if (shack_top_ptr == shack_end_ptr) {
        // flush shadow stack
        shack_top_ptr = cpu_env->shack + SHACK_SIZE - 1
      } else {
      }
    */
    tcg_gen_ld_ptr(
      shack_top_ptr, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(
      shack_end_ptr, cpu_env, offsetof(CPUState, shack));
    tcg_gen_brcond_ptr(
      TCG_COND_NE, shack_top_ptr, shack_end_ptr, lbl_push);
    //flush shadow stack
    tcg_gen_ld_ptr(
      shack_top_ptr, cpu_env, offsetof(CPUState, shack));
    tcg_gen_add_ptr(
      shack_top_ptr, shack_top_ptr,
      tcg_const_ptr(sizeof(target_ulong)*(SHACK_SIZE-1)));
    tcg_gen_st_ptr(
      shack_top_ptr, cpu_env, offsetof(CPUState, shack_top));
    /*
      target_ulong* shack_top_ptr = cpu_env->shack_top
      shack_top_ptr -= sizeof(target_ulong*)
      cpu_env->shack_top = shack_top_ptr
      *shack_top_ptr = next_eip;
      *(shack_top_ptr + stk_diff) = slot
    */
    gen_set_label(lbl_push);
    tcg_gen_ld_ptr(
      shack_top_ptr, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_add_ptr(
      shack_top_ptr, shack_top_ptr, tcg_const_tl(-sizeof(target_ulong)));
    tcg_gen_st_ptr(
      shack_top_ptr, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_st_ptr(
      tcg_const_ptr(next_eip), shack_top_ptr, 0);
    tcg_gen_st_ptr(
      tcg_const_ptr((unsigned long)slot_ptr), shack_top_ptr, stk_diff);
    tcg_temp_free(shack_top_ptr);
    tcg_temp_free(shack_end_ptr);
}

/*
 * pop_shack()
 *  Pop next host eip from shadow stack.
 */
void pop_shack(TCGv_ptr cpu_env, TCGv next_eip)
{
    TCGv_ptr shack_top_ptr = tcg_temp_new(), host_eip = tcg_temp_local_new();
    TCGv_ptr guest_eip = tcg_temp_new_ptr(), slot_ptr = tcg_temp_new_ptr();
    int lbl_else = gen_new_label();
    /*
      target_ulong* shack_top_ptr = cpu_env->shack_top
      guest_eip = *shack_top_ptr;
      if (guest_eip == next_eip) {
        unsigned long **slot_ptr = *(shack_top_ptr + stk_diff)
        shack_top_ptr += sizeof(target_ulong*)
        cpu_env->shack_top = shack_top_ptr
        unsigned long* host_eip = *slot_ptr;
        if (host_eip != 0) {
          gen jump host_eip
        } else {
        }
      } else {
      }
    */
    tcg_gen_ld_ptr(
      shack_top_ptr, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_tl(
      guest_eip, shack_top_ptr, 0);
    tcg_gen_brcond_ptr(
      TCG_COND_NE, guest_eip, next_eip, lbl_else);
    /* reload `shack_top_ptr`: It's a temporary (not a "local" temporary),
     * hence is dead after the branch `brcond`
     */
    tcg_gen_ld_ptr(
      shack_top_ptr, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(
      slot_ptr, shack_top_ptr, stk_diff);
    tcg_gen_add_ptr(
      shack_top_ptr, shack_top_ptr, tcg_const_tl(sizeof(target_ulong)));
    tcg_gen_st_ptr(
      shack_top_ptr, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(
      host_eip, slot_ptr, 0);
    tcg_gen_brcond_ptr(
      TCG_COND_EQ, host_eip, tcg_const_tl(0), lbl_else);
    *gen_opc_ptr++ = INDEX_op_jmp;
    *gen_opparam_ptr++ = GET_TCGV_I32(host_eip);

    gen_set_label(lbl_else);
    tcg_temp_free_ptr(shack_top_ptr);
    tcg_temp_free_ptr(slot_ptr);
    tcg_temp_free_ptr(host_eip);
    tcg_temp_free_ptr(guest_eip);
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

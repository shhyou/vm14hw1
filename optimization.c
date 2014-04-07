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
 * Shadow Stack
 */
list_t *shadow_hash_list;

/*
 * Our stack grows downward, just like x86's stack
 * shack_top ----> +---------------+    |
 *                 +----  ...  ----+    | push
 * shack_end ----> +---------------+    v
 */
static unsigned long stk_diff;

static inline void shack_init(CPUState *env)
{
    env->shack = (target_ulong*)malloc(sizeof(target_ulong)*SHACK_SIZE);
    env->shadow_ret_addr = (unsigned long*)malloc(sizeof(unsigned long)*SHACK_SIZE);
    env->shack_top = env->shack + SHACK_SIZE - 1;
    *env->shack_top = 0; /* leave an item for margin: guest_eip=0, never reached */
    env->shack_end = env->shack;
    stk_diff = ((unsigned long)env->shadow_ret_addr) - ((unsigned long)env->shack);
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
    env->shack_top = env->shack_end + SHACK_SIZE - 1;
}

void helper_print_shack(CPUState *env)
{
    printf("shack_top=%p,shack_end=%p\n",env->shack_top,env->shack_end);
}


/*
 * tb_find_host
 *  find the *tb given a guest_eip
 *  returns NULL if not found
 */
static TranslationBlock *tb_find_host(CPUState* env, target_ulong pc) {
    TranslationBlock *tb, **ptb1;
    unsigned int h;
    tb_page_addr_t phys_pc, phys_page1, phys_page2;
    target_ulong virt_page2, cs_base, pc_;
    int flags;
    cpu_get_tb_cpu_state(env, &pc_, &cs_base, &flags);

    tb_invalidated_flag = 0;

    /* find translated block using physical mappings */
    phys_pc = get_page_addr_code(env, pc);
    phys_page1 = phys_pc & TARGET_PAGE_MASK;
    phys_page2 = -1;
    h = tb_phys_hash_func(phys_pc);
    ptb1 = &tb_phys_hash[h];
    for(tb = *ptb1; tb != NULL; tb = *ptb1) {
        if (tb->pc == pc &&
            tb->page_addr[0] == phys_page1 &&
            tb->cs_base == cs_base &&
            tb->flags == flags) {
            /* check next page if needed */
            if (tb->page_addr[1] != -1) {
                virt_page2 = (pc & TARGET_PAGE_MASK) +
                    TARGET_PAGE_SIZE;
                phys_page2 = get_page_addr_code(env, virt_page2);
                if (tb->page_addr[1] == phys_page2)
                    break;
            } else {
                break;
            }
        }
        ptb1 = &tb->phys_hash_next;
    }
    return tb;
}

/*
 * push_shack()
 *  Push next guest eip into shadow stack.
 */
void push_shack(CPUState *env, TCGv_ptr cpu_env, target_ulong next_eip)
{
    int lbl_push = gen_new_label();
    TranslationBlock *tb = tb_find_host(env, next_eip);
    TCGv_ptr shack_top_ptr = tcg_temp_new(), shack_end_ptr = tcg_temp_new();
    uint8_t* host_pc = NULL;
    if (tb != NULL) {
      host_pc = tb->tc_ptr;
    } else {
      // XXX TODO: insert to hash table
    }
    /*
      target_ulong* shack_top_ptr = cpu_env->shack_top
      target_ulong* shack_end_ptr = cpu_env->shack_end
      if (shack_top_ptr == shack_end_ptr) {
        shack_top_ptr = cpu_env->shack + SHACK_SIZE - 1
      } else {
      }
    */
    tcg_gen_ld_ptr(
      shack_top_ptr, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(
      shack_end_ptr, cpu_env, offsetof(CPUState, shack_end));
    tcg_gen_brcond_ptr(
      TCG_COND_NE, shack_top_ptr, shack_end_ptr, lbl_push);
    tcg_gen_ld_ptr(
      shack_top_ptr, cpu_env, offsetof(CPUState, shack));
    tcg_gen_add_ptr(
      shack_top_ptr, shack_top_ptr, tcg_const_ptr(sizeof(target_ulong)*(SHACK_SIZE-1)));
    tcg_gen_st_ptr(
      shack_top_ptr, cpu_env, offsetof(CPUState, shack_top));
    /*
      target_ulong* shack_top_ptr = cpu_env->shack_top
      shack_top_ptr -= sizeof(target_ulong*)
      *shack_top_ptr = next_eip;
      *(shack_top_ptr + stk_diff) = host_eip
      cpu_env->shack_top = shack_top_ptr
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
      tcg_const_ptr((unsigned long)host_pc),
      shack_top_ptr, stk_diff);
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
    TCGv_ptr guest_eip = tcg_temp_new_ptr();
    int lbl_else = gen_new_label();
    /*
      target_ulong* shack_top_ptr = cpu_env->shack_top
      guest_eip = *shack_top_ptr;
      if (guest_eip == next_eip) {
        target_ulong* host_eip = *shack_top_ptr;
        shack_top_ptr += sizeof(target_ulong*)
        cpu_env->shack_top = shack_top_ptr
        if (host_eip != 0) { //TODO: remove this
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
      host_eip, shack_top_ptr, stk_diff);
    tcg_gen_add_ptr(
      shack_top_ptr, shack_top_ptr, tcg_const_tl(sizeof(target_ulong)));
    tcg_gen_st_ptr(
      shack_top_ptr, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_brcond_ptr(
      TCG_COND_EQ, host_eip, tcg_const_tl(0), lbl_else);
    *gen_opc_ptr++ = INDEX_op_jmp;
    *gen_opparam_ptr++ = GET_TCGV_I32(host_eip);

    gen_set_label(lbl_else);
    tcg_temp_free_ptr(shack_top_ptr);
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

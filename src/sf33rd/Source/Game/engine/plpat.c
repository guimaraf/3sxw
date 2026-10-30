/**
 * @file plpat.c
 * Player Attack Controller
 */

#include "sf33rd/Source/Game/engine/plpat.h"
#include "common.h"
#if defined(SF3_UWP)
#include "port/sdl/sdl_app.h"
#endif
#include "sf33rd/Source/Game/effect/effg6.h"
#include "sf33rd/Source/Game/engine/charset.h"
#include "sf33rd/Source/Game/engine/grade.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/plpat00.h"
#include "sf33rd/Source/Game/engine/plpat01.h"
#include "sf33rd/Source/Game/engine/plpat02.h"
#include "sf33rd/Source/Game/engine/plpat03.h"
#include "sf33rd/Source/Game/engine/plpat04.h"
#include "sf33rd/Source/Game/engine/plpat05.h"
#include "sf33rd/Source/Game/engine/plpat06.h"
#include "sf33rd/Source/Game/engine/plpat07.h"
#include "sf33rd/Source/Game/engine/plpat08.h"
#include "sf33rd/Source/Game/engine/plpat09.h"
#include "sf33rd/Source/Game/engine/plpat10.h"
#include "sf33rd/Source/Game/engine/plpat11.h"
#include "sf33rd/Source/Game/engine/plpat12.h"
#include "sf33rd/Source/Game/engine/plpat13.h"
#include "sf33rd/Source/Game/engine/plpat14.h"
#include "sf33rd/Source/Game/engine/plpat16.h"
#include "sf33rd/Source/Game/engine/plpat17.h"
#include "sf33rd/Source/Game/engine/plpat18.h"
#include "sf33rd/Source/Game/engine/plpat19.h"
#include "sf33rd/Source/Game/engine/plpat20.h"
#include "sf33rd/Source/Game/engine/plpnm.h"
#include "sf33rd/Source/Game/engine/pls00.h"
#include "sf33rd/Source/Game/engine/pls01.h"
#include "sf33rd/Source/Game/engine/pls02.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/io/pulpul.h"

s16 ja_nmj_rno_change(WORK* wk);
void Attack_07000(PLW* wk);
void get_cancel_timer(PLW* wk);
void check_ja_nmj_dummy_RTNM(PLW* wk);
u8 get_cjdR(PLW*);

void (*const plpat_lv_00[16])(PLW* wk);
void (*const plxx_extra_attack_table[])();

const u8* cjdr_karaburi_table[20];
const u8* cjdr_hits_table[20];
const u8* cjdr_blocking_table[20];
const u8* cjdr_defense_table[20];

#if defined(SF3_UWP)
#define UWP_ATTACK_CALL(kind, call) do { \
    const int attack_state = (wk->wu.routine_no[2] << 8) | wk->wu.routine_no[3]; \
    const Uint64 attack_call_ns = SDL_GetTicksNS(); \
    call; \
    SDLApp_RecordUWPCombatDetail(kind, wk->wu.id, attack_state, wk->player_number, SDL_GetTicksNS() - attack_call_ns); \
} while (0)
#else
#define UWP_ATTACK_CALL(kind, call) call
#endif

void Player_attack(PLW* wk) {
#if defined(SF3_UWP)
    const Uint64 attack_begin_ns = SDL_GetTicksNS();
    const int attack_entry_state = (wk->wu.routine_no[2] << 8) | wk->wu.routine_no[3];
#endif
    wk->wu.next_z = wk->wu.my_priority;
    wk->running_f = 0;
    wk->py->flag = 0;
    wk->guard_flag = 3;
    wk->guard_chuu = 0;
    wk->tsukami_f = false;
    wk->tsukamare_f = false;
    wk->scr_pos_set_flag = 1;
    wk->dm_hos_flag = 0;
    wk->ukemi_success = 0;
    wk->zuru_timer = 0;
    wk->zuru_ix_counter = 0;
    wk->sa_stop_flag = 0;
    wk->ukemi_success = 0;
    wk->ukemi_ok_timer = 0;
    wk->uot_cd_ok_flag = 0;
    wk->hazusenai_flag = 0;
    wk->cat_break_reserve = 0;
    wk->wu.swallow_no_effect = 0;
#if defined(SF3_UWP)
    const int attack_power_off_state = (wk->wu.routine_no[2] << 8) | wk->wu.routine_no[3];
    const Uint64 attack_power_off_ns = SDL_GetTicksNS();
#endif
    check_em_tk_power_off(wk, (PLW*)wk->wu.target_adrs);
#if defined(SF3_UWP)
    SDLApp_RecordUWPCombatDetail("attack_power_off", wk->wu.id, attack_power_off_state, wk->player_number, SDL_GetTicksNS() - attack_power_off_ns);
#endif

    if (wk->wu.routine_no[3] == 0) {
        wk->caution_flag = 1;
        wk->dm_vital_backup = 0;
        wk->dm_vital_use = 0;
        wk->total_att_hit_ok = 0;
        wk->hsjp_ok = 0;

        if (wk->wu.routine_no[2] < 16) {
#if defined(SF3_UWP)
            const int attack_chain_reset_state = (wk->wu.routine_no[2] << 8) | wk->wu.routine_no[3];
            const Uint64 attack_chain_reset_ns = SDL_GetTicksNS();
#endif
            clear_chainex_check(wk->wu.id);
#if defined(SF3_UWP)
            SDLApp_RecordUWPCombatDetail("attack_chain_reset", wk->wu.id, attack_chain_reset_state, wk->player_number, SDL_GetTicksNS() - attack_chain_reset_ns);
#endif
        }
    } else {
#if defined(SF3_UWP)
        const int attack_feedback_state = (wk->wu.routine_no[2] << 8) | wk->wu.routine_no[3];
        const Uint64 attack_feedback_ns = SDL_GetTicksNS();
#endif
        pp_pulpara_remake_at(wk);
#if defined(SF3_UWP)
        SDLApp_RecordUWPCombatDetail("attack_feedback", wk->wu.id, attack_feedback_state, wk->player_number, SDL_GetTicksNS() - attack_feedback_ns);
#endif
    }

#if defined(SF3_UWP)
    const int attack_guard_state = (wk->wu.routine_no[2] << 8) | wk->wu.routine_no[3];
    const Uint64 attack_guard_ns = SDL_GetTicksNS();
#endif
    jumping_guard_type_check(wk);
#if defined(SF3_UWP)
    SDLApp_RecordUWPCombatDetail("attack_guard", wk->wu.id, attack_guard_state, wk->player_number, SDL_GetTicksNS() - attack_guard_ns);
#endif

    if (wk->wu.routine_no[2] > 15) {
#if defined(SF3_UWP)
        const int attack_extra_routine_state = (wk->wu.routine_no[2] << 8) | wk->wu.routine_no[3];
        const bool trace_entry = (wk->player_number == 6 || wk->player_number == 11) &&
                                 attack_extra_routine_state == 0x1000;
        if (trace_entry) SDLApp_UWPAttackTraceBegin(wk->player_number, wk->wu.id);
        const Uint64 attack_extra_routine_ns = SDL_GetTicksNS();
#endif
        plxx_extra_attack_table[wk->player_number](wk);
#if defined(SF3_UWP)
        const Uint64 attack_extra_elapsed_ns = SDL_GetTicksNS() - attack_extra_routine_ns;
        if (trace_entry) SDLApp_UWPAttackTraceEnd();
        SDLApp_RecordUWPCombatDetail("attack_extra_routine", wk->wu.id, attack_extra_routine_state, wk->player_number, attack_extra_elapsed_ns);
#endif
    } else {
        wk->sa->saeff_ok = 0;
        wk->sa->saeff_mp = 0;
#if defined(SF3_UWP)
        const int attack_routine_state = (wk->wu.routine_no[2] << 8) | wk->wu.routine_no[3];
        const Uint64 attack_routine_ns = SDL_GetTicksNS();
#endif
        plpat_lv_00[wk->wu.routine_no[2]](wk);
#if defined(SF3_UWP)
        SDLApp_RecordUWPCombatDetail("attack_routine", wk->wu.id, attack_routine_state, wk->player_number, SDL_GetTicksNS() - attack_routine_ns);
#endif
    }

    wk->wu.next_z = ((PLW*)wk->wu.target_adrs)->wu.my_priority - 3;

    if (wk->wu.cg_prio) {
        if (wk->wu.cg_prio == 1) {
            wk->wu.next_z += 4;
        } else {
            wk->wu.next_z -= 4;
        }
    }
#if defined(SF3_UWP)
    SDLApp_RecordUWPCombatDetail("attack_total", wk->wu.id, attack_entry_state, wk->player_number, SDL_GetTicksNS() - attack_begin_ns);
#endif
}

void Attack_00000(PLW* wk) {
    wk->scr_pos_set_flag = 0;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        UWP_ATTACK_CALL("attack_landing", hoken_muriyari_chakuchi(wk));
        wk->wu.rl_flag = wk->wu.rl_waza;
        UWP_ATTACK_CALL("attack_direction", setup_lvdir_after_autodir(wk));
        UWP_ATTACK_CALL("attack_cancel_timer", get_cancel_timer(wk));
        UWP_ATTACK_CALL("attack_animation_init", set_char_move_init(&wk->wu, 4, wk->as->char_ix));
        break;

    case 1:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));

        if (wk->wu.cg_type == 20) {
            wk->wu.routine_no[2] = 4;
            wk->wu.cg_type = 0;
            wk->scr_pos_set_flag = 1;
        }

        break;
    }
}

void Attack_01000(PLW* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        UWP_ATTACK_CALL("attack_landing", hoken_muriyari_chakuchi(wk));
        wk->wu.rl_flag = wk->wu.rl_waza;
        UWP_ATTACK_CALL("attack_direction", setup_lvdir_after_autodir(wk));
        UWP_ATTACK_CALL("attack_cancel_timer", get_cancel_timer(wk));
        UWP_ATTACK_CALL("attack_animation_init", set_char_move_init(&wk->wu, 4, wk->as->char_ix));
        setup_mvxy_data(&wk->wu, wk->as->data_ix);
        break;

    case 1:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));

        if (wk->wu.cg_type == 1) {
            add_mvxy_speed(&wk->wu);
            wk->wu.routine_no[3] = 2;
            wk->wu.cg_type = 0;
            break;
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);
        break;

    case 3:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));
        break;
    }
}

void Attack_02000(PLW* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        UWP_ATTACK_CALL("attack_landing", hoken_muriyari_chakuchi(wk));
        wk->wu.rl_flag = wk->wu.rl_waza;
        UWP_ATTACK_CALL("attack_direction", setup_lvdir_after_autodir(wk));
        UWP_ATTACK_CALL("attack_cancel_timer", get_cancel_timer(wk));
        UWP_ATTACK_CALL("attack_animation_init", set_char_move_init(&wk->wu, 4, wk->as->char_ix));
        setup_mvxy_data(&wk->wu, wk->as->data_ix);
        break;

    case 1:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));

        if (wk->wu.cg_type == 1) {
            add_mvxy_speed(&wk->wu);
            wk->wu.routine_no[3] = 2;
            wk->wu.cg_type = 0;
            break;
        }

        break;

    case 2:
        cal_mvxy_speed(&wk->wu);
        add_mvxy_speed(&wk->wu);
        /* fallthrough */

    case 3:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));

        if (wk->wu.cg_type == 1) {
            reset_mvxy_data(&wk->wu);
            wk->wu.cg_type = 0;
        }

        break;
    }
}

void Attack_03000(PLW* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        UWP_ATTACK_CALL("attack_cancel_timer", get_cancel_timer(wk));
        if ((Bonus_Game_Flag == 20 && wk->bs2_on_car) || (wk->wu.xyz[1].disp.pos <= 0)) {
            UWP_ATTACK_CALL("attack_landing", hoken_muriyari_chakuchi(wk));
            wk->wu.rl_flag = wk->wu.rl_waza;
            UWP_ATTACK_CALL("attack_direction", setup_lvdir_after_autodir(wk));
            Normal_18000_init_unit(wk, wk->wu.pat_status);
        }

        UWP_ATTACK_CALL("attack_animation_init", set_char_move_init(&wk->wu, 4, wk->as->char_ix));
        break;

    case 1:
        if ((wk->wu.mvxy.a[1].sp > 0) && (wk->wu.xyz[1].disp.pos < 16)) {
            add_mvxy_speed(&wk->wu);
            cal_mvxy_speed(&wk->wu);
            break;
        }

        wk->wu.routine_no[3]++;

    case 2:
        jumping_union_process(&wk->wu, 3);

        if (wk->wu.routine_no[3] != 3) {
            check_ja_nmj_dummy_RTNM(wk);

            if (wk->wu.cg_type == 0x40) {
                if (!(wk->spmv_ng_flag & 0x100000) && ja_nmj_rno_change(&wk->wu)) {
                    wk->wu.routine_no[1] = 0;
                    wk->wu.routine_no[3] = 1;
                }

                wk->wu.cg_type = 0;
                break;
            }
        }

        break;

    case 3:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));
        break;
    }
}

s16 ja_nmj_rno_change(WORK* wk) {
    s16 rnum = 0;

    switch (wk->pat_status) {
    case 20:
        wk->routine_no[2] = 21;
        rnum = 1;
        break;

    case 14:
        wk->routine_no[2] = 18;
        rnum = 1;
        break;

    case 26:
        wk->routine_no[2] = 24;
        rnum = 1;
        break;

    case 22:
        wk->routine_no[2] = 22;
        rnum = 1;
        break;

    case 16:
        wk->routine_no[2] = 19;
        rnum = 1;
        break;

    case 28:
        wk->routine_no[2] = 25;
        rnum = 1;
        break;

    case 24:
        wk->routine_no[2] = 23;
        rnum = 1;
        break;

    case 18:
        wk->routine_no[2] = 20;
        rnum = 1;
        break;

    case 30:
        wk->routine_no[2] = 26;
        rnum = 1;
        break;
    }

    return rnum;
}

void check_ja_nmj_dummy_RTNM(PLW* wk) {
    if (wk->wu.xyz[1].disp.pos <= 0) {
        wk->ja_nmj_rno = 0;
        return;
    }

    switch (wk->ja_nmj_rno) {
    case 0:
        if ((wk->wu.cg_ja.atix != 0) || (wk->wu.cg_ja.caix != 0)) {
            wk->ja_nmj_rno = 1;
        }

        break;

    case 1:
        if (((wk->wu.cg_ja.atix == 0) && (wk->wu.cg_ja.caix == 0)) || !wk->wu.att_hit_ok) {
            wk->ja_nmj_cnt = get_cjdR(wk);
            wk->ja_nmj_rno = 2;
        }

        break;

    case 2:
        if (((wk->wu.cg_ja.atix != 0) || (wk->wu.cg_ja.caix != 0)) && wk->wu.att_hit_ok) {
            wk->ja_nmj_rno = 1;
            break;
        }

        if (!--wk->ja_nmj_cnt) {
            wk->ja_nmj_rno = 3;
        }

        break;

    default:
        if ((wk->wu.cg_ja.atix != 0) || (wk->wu.cg_ja.caix != 0)) {
            if (wk->wu.att_hit_ok) {
                wk->ja_nmj_rno = 1;
                break;
            }
        } else if (wk->wu.cg_type == 0) {
            wk->wu.cg_type = 64;
        }

        break;
    }
}

u8 get_cjdR(PLW* wk) {
    s16 w_ix = (wk->wu.kind_of_waza & 6);
    w_ix += ((wk->wu.hf.hit.player & 0xA2) != 0);

    if (wk->wu.att_hit_ok || (wk->wu.hf.hit.player == 0)) {
        goto case0;
    }

    if (wk->wu.hf.hit.player & 3) {
        goto case1;
    }

    if (wk->wu.hf.hit.player & 0xC0) {
        goto case2;
    }

    if (wk->wu.hf.hit.player & 0x30) {
        goto case3;
    }

case0:
    return cjdr_karaburi_table[wk->player_number][w_ix];

case1:
    return cjdr_hits_table[wk->player_number][w_ix];

case2:
    return cjdr_blocking_table[wk->player_number][w_ix];

case3:
    return cjdr_defense_table[wk->player_number][w_ix];
}

void Attack_04000(PLW* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        UWP_ATTACK_CALL("attack_landing", hoken_muriyari_chakuchi(wk));
        wk->wu.rl_flag = wk->wu.rl_waza;
        UWP_ATTACK_CALL("attack_direction", setup_lvdir_after_autodir(wk));
        UWP_ATTACK_CALL("attack_cancel_timer", get_cancel_timer(wk));
        UWP_ATTACK_CALL("attack_animation_init", set_char_move_init(&wk->wu, 4, wk->as->char_ix));
        break;

    case 1:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));

        if (wk->wu.cg_type == 20) {
            wk->wu.routine_no[2] = 1;
            wk->wu.cg_type = 0;
            wk->scr_pos_set_flag = 0;
        }

        break;
    }
}

void Attack_05000(PLW* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.rl_flag = wk->wu.rl_waza;
        UWP_ATTACK_CALL("attack_animation_init", set_char_move_init(&wk->wu, 5, wk->as->char_ix));

        if (wk->wu.xyz[1].disp.pos > 0) {
            wk->wu.routine_no[3] = 2;
            char_move_wca(&wk->wu);
        } else {
            UWP_ATTACK_CALL("attack_landing", hoken_muriyari_chakuchi(wk));
            wk->wu.routine_no[3] = 1;
        }

        wk->cancel_timer = 0;
        break;

    case 1:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));

        if (wk->wu.cg_type == 1) {
            add_mvxy_speed(&wk->wu);
            wk->wu.routine_no[3] = 2;
            wk->wu.cg_type = 0;
            effect_G6_init(&wk->wu, wk->wu.weight_level);
            break;
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);
        break;

    case 3:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));
        break;
    }
}

void Attack_06000(PLW* wk) {
    wk->scr_pos_set_flag = 0;
    Attack_07000(wk);
}

void Attack_07000(PLW* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        UWP_ATTACK_CALL("attack_landing", hoken_muriyari_chakuchi(wk));
        UWP_ATTACK_CALL("attack_cancel_timer", get_cancel_timer(wk));
        UWP_ATTACK_CALL("attack_animation_init", set_char_move_init(&wk->wu, 5, wk->as->char_ix));

        if (wk->wu.cg_type == 20) {
            wk->wu.cg_type = 0;
            wk->wu.rl_flag = wk->wu.rl_waza;
            break;
        }

        break;

    case 1:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));

        if (wk->wu.cg_type == 20) {
            wk->wu.cg_type = 0;
            wk->wu.rl_flag = wk->wu.rl_waza;
        }

        break;
    }
}

void Attack_08000(PLW* wk) {
    s16 ixx;

    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;

        if (wk->wu.xyz[1].disp.pos <= 0) {
            wk->wu.rl_flag = wk->wu.rl_waza;
            wk->wu.xyz[1].disp.pos = 0;

            ixx = ((wk->wu.pat_status - 20) / 2 & 3) + 9;

            if (ixx > 11) {
                ixx = 10;
            }

            setup_mvxy_data(&wk->wu, ixx);
        }

        UWP_ATTACK_CALL("attack_cancel_timer", get_cancel_timer(wk));
        UWP_ATTACK_CALL("attack_animation_init", set_char_move_init(&wk->wu, 4, (s16)((wk->as->char_ix))));
        wk->wu.mvxy.index = wk->as->data_ix;
        break;

    case 1:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));

        if (wk->wu.cg_type == 1) {
            wk->wu.routine_no[3]++;
        }

        if (wk->wu.cg_type == 20) {
            setup_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.cg_type = 0;
            wk->wu.mvxy.index++;
            break;
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);

        if ((wk->wu.routine_no[3] != 3) && (wk->wu.cg_type == 20)) {
            add_to_mvxy_data(&wk->wu, wk->wu.mvxy.index);
            wk->wu.cg_type = 0;
            wk->wu.mvxy.index++;
            break;
        }

        break;

    case 3:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));
        break;
    }
}

void Attack_09000(PLW* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        UWP_ATTACK_CALL("attack_landing", hoken_muriyari_chakuchi(wk));
        UWP_ATTACK_CALL("attack_cancel_timer", get_cancel_timer(wk));
        UWP_ATTACK_CALL("attack_animation_init", set_char_move_init(&wk->wu, 5, wk->as->char_ix));
        setup_mvxy_data(&wk->wu, wk->as->data_ix);

        if (wk->wu.cg_type == 20) {
            wk->wu.cg_type = 0;
            wk->wu.rl_flag = wk->wu.rl_waza;
            break;
        }

        break;

    case 1:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));

        if (wk->wu.cg_type == 20) {
            wk->wu.cg_type = 0;
            wk->wu.rl_flag = wk->wu.rl_waza;
        }

        if (wk->wu.cg_type == 1) {
            add_mvxy_speed(&wk->wu);
            wk->wu.routine_no[3] = 2;
            wk->wu.cg_type = 0;
            break;
        }

        break;

    case 2:
        jumping_union_process(&wk->wu, 3);
        break;

    case 3:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));
        break;
    }
}

void Attack_10000(PLW* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        UWP_ATTACK_CALL("attack_landing", hoken_muriyari_chakuchi(wk));
        wk->wu.rl_flag = wk->wu.rl_waza;
        UWP_ATTACK_CALL("attack_animation_init", set_char_move_init(&wk->wu, 5, wk->as->char_ix));
        setup_mvxy_data(&wk->wu, wk->as->data_ix);
        wk->cancel_timer = 0;
        wk->wu.dir_timer = 3;

        if (wk->wu.cg_type == 20) {
            wk->wu.routine_no[3] = 2;
        }

        grade_add_leap_attack(wk->wu.id);
        break;

    case 1:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));
        if (wk->wu.cg_type != 20) {
            break;
        }

        wk->wu.routine_no[3]++;

    case 2:
        jumping_union_process(&wk->wu, 4);
        if ((wk->wu.routine_no[3] != 4) && wk->wu.hf.hit.player) {
            if ((wk->wu.hf.hit.player & 3) != 0) {
                wk->wu.mvxy.a[0].sp /= 4;
                wk->wu.routine_no[3] = 4;
                break;
            }

            if ((wk->wu.hf.hit.player & 0x30) != 0) {
                wk->wu.mvxy.a[0].sp /= 4;
                wk->wu.mvxy.a[1].sp = 0;
                wk->wu.routine_no[3] = 3;
                break;
            }

            if ((wk->wu.hf.hit.player & 0xC0) != 0) {
                wk->wu.mvxy.a[0].sp /= 2;
                wk->wu.mvxy.a[0].sp = -wk->wu.mvxy.a[0].sp;
                wk->wu.mvxy.a[1].sp = 0;
                wk->wu.routine_no[3] = 4;
            }
        }

        break;

    case 3:
        if (--wk->wu.dir_timer > 0) {
            break;
        }

        wk->wu.routine_no[3] = 4;
        /* fallthrough */

    case 4:
        jumping_union_process(&wk->wu, 5);
        break;

    case 5:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));
        break;
    }
}

void Attack_14000(PLW* wk) {
    wk->scr_pos_set_flag = 0;
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;
        UWP_ATTACK_CALL("attack_landing", hoken_muriyari_chakuchi(wk));
        wk->wu.rl_flag = wk->wu.rl_waza;
        wk->cat_break_ok_timer = 6;
        UWP_ATTACK_CALL("attack_direction", setup_lvdir_after_autodir(wk));
        UWP_ATTACK_CALL("attack_animation_init", set_char_move_init(&wk->wu, 4, wk->as->char_ix));
        break;

    case 1:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));
        break;
    }

    if (wk->cat_break_ok_timer) {
        wk->cat_break_reserve = 1;
    }
}

void Attack_15000(PLW* wk) {
    switch (wk->wu.routine_no[3]) {
    case 0:
        wk->wu.routine_no[3]++;

        if (wk->wu.xyz[1].disp.pos <= 0) {
            wk->wu.rl_flag = wk->wu.rl_waza;
            UWP_ATTACK_CALL("attack_direction", setup_lvdir_after_autodir(wk));
            wk->wu.xyz[1].disp.pos = 0;
            Normal_18000_init_unit(wk, wk->wu.pat_status);
        }

        wk->cat_break_ok_timer = 6;
        UWP_ATTACK_CALL("attack_animation_init", set_char_move_init(&wk->wu, 4, wk->as->char_ix));

        if (wk->cat_break_ok_timer) {
            wk->cat_break_reserve = 1;
            break;
        }

        break;

    case 1:
        if (wk->cat_break_ok_timer) {
            wk->cat_break_reserve = 1;
        }

        if ((wk->wu.mvxy.a[1].sp > 0) && (wk->wu.xyz[1].disp.pos < 0)) {
            add_mvxy_speed(&wk->wu);
            cal_mvxy_speed(&wk->wu);
            break;
        }

        wk->wu.routine_no[3]++;

    case 2:
        jumping_union_process(&wk->wu, 3);

        if ((wk->wu.routine_no[3] != 3) && wk->cat_break_ok_timer) {
            wk->cat_break_reserve = 1;
            break;
        }

        break;

    case 3:
        UWP_ATTACK_CALL("attack_animation", char_move(&wk->wu));
        break;
    }
}

void get_cancel_timer(PLW* wk) {
    if (wk->tc_1st_flag) {
        wk->cancel_timer = 0;
        return;
    }

    if (wk->wu.xyz[1].disp.pos > 0) {
        wk->cancel_timer = 2;
        return;
    }

    wk->cancel_timer = 2;
}

void hoken_muriyari_chakuchi(PLW* wk) {
    if ((Bonus_Game_Flag == 20) && wk->bs2_on_car) {
        wk->wu.xyz[1].disp.pos = bs2_floor[2];
        return;
    }

    wk->wu.xyz[1].disp.pos = 0;
}

void (*const plpat_lv_00[16])(PLW* wk) = { Attack_00000, Attack_01000, Attack_02000, Attack_03000,
                                           Attack_04000, Attack_05000, Attack_06000, Attack_07000,
                                           Attack_08000, Attack_09000, Attack_10000, Attack_00000,
                                           Attack_00000, Attack_00000, Attack_14000, Attack_15000 };

void (*const plxx_extra_attack_table[])() = {
    pl00_extra_attack, pl01_extra_attack, pl02_extra_attack, pl03_extra_attack, pl04_extra_attack,
    pl05_extra_attack, pl06_extra_attack, pl07_extra_attack, pl08_extra_attack, pl09_extra_attack,
    pl10_extra_attack, pl11_extra_attack, pl12_extra_attack, pl13_extra_attack, pl14_extra_attack,
    pl16_extra_attack, pl17_extra_attack, pl18_extra_attack, pl19_extra_attack, pl20_extra_attack
};

const u8 cjdr_karaburi_type3[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };

const u8* cjdr_karaburi_table[20] = {
    cjdr_karaburi_type3, cjdr_karaburi_type3, cjdr_karaburi_type3, cjdr_karaburi_type3, cjdr_karaburi_type3,
    cjdr_karaburi_type3, cjdr_karaburi_type3, cjdr_karaburi_type3, cjdr_karaburi_type3, cjdr_karaburi_type3,
    cjdr_karaburi_type3, cjdr_karaburi_type3, cjdr_karaburi_type3, cjdr_karaburi_type3, cjdr_karaburi_type3,
    cjdr_karaburi_type3, cjdr_karaburi_type3, cjdr_karaburi_type3, cjdr_karaburi_type3, cjdr_karaburi_type3
};

const u8 cjdr_hits_type3[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };

const u8* cjdr_hits_table[20] = {
    cjdr_hits_type3, cjdr_hits_type3, cjdr_hits_type3, cjdr_hits_type3, cjdr_hits_type3,
    cjdr_hits_type3, cjdr_hits_type3, cjdr_hits_type3, cjdr_hits_type3, cjdr_hits_type3,
    cjdr_hits_type3, cjdr_hits_type3, cjdr_hits_type3, cjdr_hits_type3, cjdr_hits_type3,
    cjdr_hits_type3, cjdr_hits_type3, cjdr_hits_type3, cjdr_hits_type3, cjdr_hits_type3,
};

const u8 cjdr_blocking_type0[8] = { 16, 7, 18, 9, 20, 11, 20, 11 };
const u8 cjdr_blocking_type1[8] = { 17, 8, 19, 10, 21, 12, 21, 12 };
const u8 cjdr_blocking_type2[8] = { 18, 9, 20, 11, 22, 13, 22, 13 };

const u8* cjdr_blocking_table[20] = {
    cjdr_blocking_type0, cjdr_blocking_type1, cjdr_blocking_type1, cjdr_blocking_type1, cjdr_blocking_type0,
    cjdr_blocking_type2, cjdr_blocking_type1, cjdr_blocking_type0, cjdr_blocking_type1, cjdr_blocking_type1,
    cjdr_blocking_type1, cjdr_blocking_type0, cjdr_blocking_type1, cjdr_blocking_type2, cjdr_blocking_type0,
    cjdr_blocking_type1, cjdr_blocking_type1, cjdr_blocking_type1, cjdr_blocking_type2, cjdr_blocking_type1
};

const u8 cjdr_defense_type3[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };

const u8* cjdr_defense_table[20] = { cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3,
                                     cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3,
                                     cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3,
                                     cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3,
                                     cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3, cjdr_defense_type3 };

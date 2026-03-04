#include "modding.h"
#include "global.h"
#include "recomputils.h"
#include "recompconfig.h"
#include "globalobjects_api.h"
#include "z64recomp_api.h"
#include "overlays/actors/ovl_En_Test3/z_en_test3.h"

static ActorExtensionId PLAYER_EXT_BUNNY_HOOD_TWEAKS;

typedef struct BunnyEarKinematics {
    /* 0x0 */ Vec3s rot;
    /* 0x6 */ Vec3s angVel;
} BunnyEarKinematics; // size = 0xC

typedef struct BunnyHoodTweaksData {
    Vec3f curPos;
    Vec3f prevPos;
    BunnyEarKinematics kinematics;
    bool isOverrideGetPlayerReq;
    bool isBunnyHoodDrawn;
} BunnyHoodTweaksData;

static bool sIsBunnyHoodEnabled;

static bool sIsZTargetTweakApplied;
static bool sIsSpeedAppliedToForms;
static bool sIsBunnyHoodDrawnOnForms;
static bool sIsVanillaBehavior;
static bool sIsEquipAllowedOnKaleido;
static bool sIsTweakedPhysics;

static int sPlayerAction13Level;
static int sPlayerAction14Level;

#define DEFAULT_SPEED_MULTIPLIER 1.5f

static BunnyHoodTweaksData *getBunnyHoodTweaksData(Player *player) {
    if (player) {
        return z64recomp_get_extended_actor_data(&player->actor, PLAYER_EXT_BUNNY_HOOD_TWEAKS);
    }

    return NULL;
}

// This flag exists entirely for Kafei
static void setOverrideGetPlayerReq(Player *player, bool isOverride) {
    BunnyHoodTweaksData *tweakData = getBunnyHoodTweaksData(player);
    if (tweakData) {
        tweakData->isOverrideGetPlayerReq = isOverride;
    }
}

static bool isBunnyHoodInInventory(void) {
    return INV_CONTENT(ITEM_MASK_BUNNY) == ITEM_MASK_BUNNY;
}

static bool isBunnyHoodEnabledAndInInventory(void) {
    return sIsBunnyHoodEnabled && isBunnyHoodInInventory();
}

static bool isSpeedShouldBeAppliedToPlayer(PlayState *play, Player *player) {
    if (isBunnyHoodEnabledAndInInventory()) {
        BunnyHoodTweaksData *tweakData = getBunnyHoodTweaksData(player);

        if (tweakData) {
            return (tweakData->isOverrideGetPlayerReq || GET_PLAYER(play) == player) &&
                   player->actor.id == ACTOR_PLAYER &&
                   player->currentMask != PLAYER_MASK_BUNNY &&
                   (player->transformation == PLAYER_FORM_HUMAN || sIsSpeedAppliedToForms) &&
                   !sIsVanillaBehavior;
        }
    }

    return false;
}

RECOMP_HOOK("Player_Action_13") void updateAction13Var_on_Player_Action_13(Player *this, PlayState *play) {
    sPlayerAction13Level++;
}

RECOMP_HOOK_RETURN("Player_Action_13") void updateAction13Var_on_return_Player_Action_13(void) {
    sPlayerAction13Level--;
}

RECOMP_HOOK("Player_Action_14") void updateAction13Var_on_Player_Action_14(Player *this, PlayState *play) {
    sPlayerAction14Level++;
}

RECOMP_HOOK_RETURN("Player_Action_14") void updateAction13Var_on_return_Player_Action_14(void) {
    sPlayerAction14Level--;
}

static f32 *sSpeedTarget;
static Player *sPlayerMovementSpeedAndYawPlayer;
static PlayState *sPlayerMovementSpeedAndYawPlay;

RECOMP_HOOK("Player_GetMovementSpeedAndYaw") void updatePlayerSpeed_on_Player_GetMovementSpeedAndYaw(Player *this, f32 *outSpeedTarget, s16 *outYawTarget, f32 speedMode, PlayState *play) {
    sPlayerMovementSpeedAndYawPlayer = this;
    sPlayerMovementSpeedAndYawPlay = play;
    sSpeedTarget = outSpeedTarget;
}

RECOMP_HOOK_RETURN("Player_GetMovementSpeedAndYaw") void updatePlayerSpeed_on_return_Player_GetMovementSpeedAndYaw(void) {
    if (isSpeedShouldBeAppliedToPlayer(sPlayerMovementSpeedAndYawPlay, sPlayerMovementSpeedAndYawPlayer)) {
        if (sPlayerAction13Level != 0 || (sPlayerAction14Level != 0 && sIsZTargetTweakApplied)) {
            *sSpeedTarget *= DEFAULT_SPEED_MULTIPLIER;
        }
    }
}

extern s8 sItemItemActions[];
static s8 sPrevMaskBunnyIA;
static u8 sPrevPlayerHeldIA;
static u8 sPrevPlayerIA;
static bool sIsBunnyHoodToggleInProgress;
static Player *sPlayerUseItemPlayer;

RECOMP_HOOK("Player_UseItem") void disableBunnyMaskItemAction_on_Player_UseItem(PlayState *play, Player *this, ItemId item) {
    if (sIsVanillaBehavior) {
        return;
    }

    sPlayerUseItemPlayer = this;
    sPrevPlayerHeldIA = this->heldItemAction;
    sPrevPlayerIA = this->itemAction;
    sIsBunnyHoodToggleInProgress = false;

    if (GET_PLAYER(play) == this && item == ITEM_MASK_BUNNY && isBunnyHoodInInventory()) {
        sPrevMaskBunnyIA = sItemItemActions[ITEM_MASK_BUNNY];
        sIsBunnyHoodToggleInProgress = true;
        sItemItemActions[ITEM_MASK_BUNNY] = this->heldItemAction = this->itemAction = PLAYER_IA_NONE;
        sIsBunnyHoodEnabled = !sIsBunnyHoodEnabled;

        SfxId sfxId = sIsBunnyHoodEnabled ? NA_SE_PL_CHANGE_ARMS : NA_SE_PL_TAKE_OUT_SHIELD;

        Player_PlaySfx(this, sfxId);
    }
}

RECOMP_HOOK_RETURN("Player_UseItem") void disableBunnyMaskItemAction_on_return_Player_UseItem(void) {
    if (sIsVanillaBehavior) {
        return;
    }

    if (sIsBunnyHoodToggleInProgress) {
        extern s32 sPlayerUseHeldItem;
        extern s32 sPlayerHeldItemButtonIsHeldDown;

        sItemItemActions[ITEM_MASK_BUNNY] = sPrevMaskBunnyIA;
        sPlayerUseItemPlayer->heldItemAction = sPrevPlayerHeldIA;
        sPlayerUseItemPlayer->itemAction = sPrevPlayerIA;
        sPlayerUseHeldItem = false;
        sPlayerHeldItemButtonIsHeldDown = false;
    }
}

static bool isDrawGlobalObjectsBunnyHood(PlayState *play, Player *player) {
    if (isSpeedShouldBeAppliedToPlayer(play, player)) {
        if (player->transformation == PLAYER_FORM_HUMAN) {
            if (player->currentMask == PLAYER_MASK_NONE) {
                return true;
            } else if (sIsBunnyHoodDrawnOnForms && sIsSpeedAppliedToForms) {
                switch (player->currentMask) {
                    case PLAYER_MASK_DEKU:
                    case PLAYER_MASK_GORON:
                    case PLAYER_MASK_ZORA:
                    case PLAYER_MASK_FIERCE_DEITY:
                    case PLAYER_MASK_DEKU + 1: // Fierce Deity Scream
                    case PLAYER_MASK_DEKU + 2: // Goron Scream
                    case PLAYER_MASK_DEKU + 3: // Zora Scream
                    case PLAYER_MASK_DEKU + 4: // Deku Scream
                        return true;
                        break;

                    default:
                        return false;
                        break;
                }
            }
        } else if (sIsBunnyHoodDrawnOnForms) {
            return true;
        }
    }

    return false;
}

extern BunnyEarKinematics sBunnyEarKinematics;

static BunnyEarKinematics sSavedBunnyEarKinematics;

static void saveBunnyHoodKinematics(void) {
    sSavedBunnyEarKinematics = sBunnyEarKinematics;
}

static void loadBunnyHoodKinematics(void) {
    sBunnyEarKinematics = sSavedBunnyEarKinematics;
}

static Player *sPlayerUpdateBunnyEarsPlayer;
static f32 sPlayerUpdateBunnyEarsSpeed;

RECOMP_HOOK("Player_UpdateBunnyEars") void useHeadSpeed_on_Player_UpdateBunnyEars(Player *player) {
    sPlayerUpdateBunnyEarsPlayer = NULL;

    if (sIsTweakedPhysics) {
        BunnyHoodTweaksData *tweakData = getBunnyHoodTweaksData(player);

        if (tweakData) {
            sPlayerUpdateBunnyEarsPlayer = player;

            saveBunnyHoodKinematics();

            sPlayerUpdateBunnyEarsSpeed = player->actor.speed;

            sBunnyEarKinematics = tweakData->kinematics;

            player->actor.speed = CLAMP_MAX(Math_Vec3f_DistXZ(&tweakData->curPos, &tweakData->prevPos), 8.8f);
        }
    }
}

RECOMP_HOOK_RETURN("Player_UpdateBunnyEars") void useHeadSpeed_on_return_Player_UpdateBunnyEars(void) {
    if (sPlayerUpdateBunnyEarsPlayer) {
        BunnyHoodTweaksData *tweakData = getBunnyHoodTweaksData(sPlayerUpdateBunnyEarsPlayer);

        if (tweakData) {
            tweakData->kinematics = sBunnyEarKinematics;
            sPlayerUpdateBunnyEarsPlayer->actor.speed = sPlayerUpdateBunnyEarsSpeed;
            loadBunnyHoodKinematics();
        }
    }
}

RECOMP_HOOK("Player_Update") void removeBunnyHood_on_Player_Update(Player *this, PlayState *play) {
    if (sIsVanillaBehavior) {
        return;
    }

    if (GET_PLAYER(play) == this) {
        if (this->currentMask == PLAYER_MASK_BUNNY) {
            this->currentMask = PLAYER_MASK_NONE;
        }
    }

    BunnyHoodTweaksData *tweakData = getBunnyHoodTweaksData(this);

    if (tweakData) {
        tweakData->isBunnyHoodDrawn = isDrawGlobalObjectsBunnyHood(play, this);

        if (tweakData->isBunnyHoodDrawn) {
            Player_UpdateBunnyEars(this);
        }
    }
}

void Player_DrawBunnyHood(PlayState *play);

RECOMP_HOOK("Player_PostLimbDrawGameplay") void drawBunnyHood_on_Player_PostLimbDrawGameplay(PlayState *play, s32 limbIndex, Gfx **dList1, Gfx **dList2, Vec3s *rot, Player *player) {
    extern Gfx *D_801C0B20[];

    if (limbIndex == PLAYER_LIMB_HEAD) {
        BunnyHoodTweaksData *tweakData = getBunnyHoodTweaksData(player);

        if (tweakData) {
            tweakData->prevPos = tweakData->curPos;
            tweakData->curPos = player->bodyPartsPos[PLAYER_BODYPART_HEAD];

            if (*dList1 && tweakData->isBunnyHoodDrawn) {
                void *bunnyHoodObj = GlobalObjects_getGlobalObject(OBJECT_MASK_RABIT);

                if (sIsTweakedPhysics) {
                    saveBunnyHoodKinematics();
                    sBunnyEarKinematics = tweakData->kinematics;
                    Player_DrawBunnyHood(play);
                    loadBunnyHoodKinematics();
                } else {
                    Player_DrawBunnyHood(play);
                }

                OPEN_DISPS(play->state.gfxCtx);
                gSPSegment(POLY_OPA_DISP++, 0x0A, bunnyHoodObj);
                gSPDisplayList(POLY_OPA_DISP++, D_801C0B20[PLAYER_MASK_BUNNY - 1]);
                CLOSE_DISPS(play->state.gfxCtx);

                // If the bunny hood is the only mask drawn then set it as the equipped mask in the save
                if (player->transformation == PLAYER_FORM_HUMAN) {
                    if (gSaveContext.save.equippedMask == PLAYER_MASK_NONE) {
                        gSaveContext.save.equippedMask = PLAYER_MASK_BUNNY;
                    }
                }
            }
        }
    }
}

RECOMP_HOOK("Play_Main") void updateOptions_on_Play_Main(PlayState *play) {
    bool prevVanillaEnabled = sIsVanillaBehavior;

    sIsZTargetTweakApplied = recomp_get_config_u32("is_z_target_tweak_applied");
    sIsSpeedAppliedToForms = recomp_get_config_u32("is_speed_applied_to_forms");
    sIsBunnyHoodDrawnOnForms = recomp_get_config_u32("is_bunny_hood_drawn_on_forms");
    sIsVanillaBehavior = recomp_get_config_u32("is_vanilla_behavior");
    sIsEquipAllowedOnKaleido = recomp_get_config_u32("is_can_equip_from_kaleido");
    sIsTweakedPhysics = recomp_get_config_u32("is_tweak_bunny_ear_physics");

    bool isVanillaEnabledChanged = sIsVanillaBehavior != prevVanillaEnabled;

    if (sIsVanillaBehavior) {
        if (isVanillaEnabledChanged && gSaveContext.save.equippedMask == PLAYER_MASK_BUNNY) {
            gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
        }
    } else if (!sIsBunnyHoodEnabled && gSaveContext.save.equippedMask == PLAYER_MASK_BUNNY) {
        gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
    }
}

RECOMP_HOOK_RETURN("Play_Init") void modifyItemRestrictions_on_return_Play_Init(void) {
    if (recomp_get_config_u32("is_item_restrictions_modified")) {
        for (PlayerTransformation i = PLAYER_FORM_FIERCE_DEITY; i < PLAYER_FORM_MAX; i++) {
            gPlayerFormItemRestrictions[i][ITEM_MASK_BUNNY] = true;
        }
    }
}

RECOMP_HOOK_RETURN("FileSelect_LoadGame") void equipBunnyHood_on_return_FileSelect_LoadGame(void) {
    sIsBunnyHoodEnabled = gSaveContext.save.equippedMask == PLAYER_MASK_BUNNY;
}

RECOMP_HOOK_RETURN("Sram_InitDebugSave") void disableBunnyHood_on_return_Sram_InitDebugSave(void) {
    sIsBunnyHoodEnabled = false;
}

static PlayState *sPlayerGetMaskPlay;

RECOMP_HOOK("Player_GetMask") void setBunnyMask_on_Player_GetMask(PlayState *play) {
    Player *player = GET_PLAYER(play);

    if (player->currentMask == PLAYER_MASK_NONE && sIsBunnyHoodEnabled) {
        sPlayerGetMaskPlay = play;
        player->currentMask = PLAYER_MASK_BUNNY;
    } else {
        sPlayerGetMaskPlay = NULL;
    }
}

RECOMP_HOOK_RETURN("Player_GetMask") void setBunnyMask_on_return_Player_GetMask(void) {
    if (sPlayerGetMaskPlay) {
        GET_PLAYER(sPlayerGetMaskPlay)->currentMask = PLAYER_MASK_NONE;
    }
}

static PlayState *sFunc80A6F9DCPlay;
static PlayerMask sFunc80A6F9DCMask;

RECOMP_HOOK("func_80A6F9DC") void allowPostmanTimer_on_func_80A6F9DC(Actor *thisx, PlayState *play) {
    Player *player = GET_PLAYER(play);

    if (isSpeedShouldBeAppliedToPlayer(play, player)) {
        sFunc80A6F9DCMask = player->currentMask;
        player->currentMask = PLAYER_MASK_BUNNY;
        sFunc80A6F9DCPlay = play;
    } else {
        sFunc80A6F9DCPlay = NULL;
    }
}

RECOMP_HOOK_RETURN("func_80A6F9DC") void allowPostmanTimer_on_return_func_80A6F9DC(void) {
    if (sFunc80A6F9DCPlay) {
        GET_PLAYER(sFunc80A6F9DCPlay)->currentMask = sFunc80A6F9DCMask;
    }
}

static PlayState *sFunc80A6FBFCPlay;
static PlayerMask sFunc80A6FBFCMask;

RECOMP_HOOK("func_80A6FBFC") void allowPostmanTimerSfx_on_func_80A6FBFC(Actor *thisx, PlayState *play) {
    Player *player = GET_PLAYER(play);

    if (isSpeedShouldBeAppliedToPlayer(play, player)) {
        sFunc80A6FBFCMask = player->currentMask;
        player->currentMask = PLAYER_MASK_BUNNY;
        sFunc80A6FBFCPlay = play;
    } else {
        sFunc80A6FBFCPlay = NULL;
    }
}

RECOMP_HOOK_RETURN("func_80A6FBFC") void allowPostmanTimerSfx_on_return_func_80A6FBFC(void) {
    if (sFunc80A6FBFCPlay) {
        GET_PLAYER(sFunc80A6FBFCPlay)->currentMask = sFunc80A6FBFCMask;
    }
}

static bool isPressingAOnBunnyHood(PlayState *play) {
    Input *input = CONTROLLER1(&play->state);
    PauseContext *pauseCtx = &play->pauseCtx;

    return input->press.button & BTN_A &&
           pauseCtx->pageIndex == PAUSE_MASK &&
           pauseCtx->cursorSlot[PAUSE_MASK] == SLOT_MASK_BUNNY - ITEM_NUM_SLOTS &&
           isBunnyHoodInInventory();
}

RECOMP_HOOK("KaleidoScope_UpdateMaskCursor") void handleBunnyHoodEquip_on_KaleidoScope_UpdateMaskCursor(PlayState *play) {
    if (sIsVanillaBehavior || !sIsEquipAllowedOnKaleido) {
        return;
    }

    if (isPressingAOnBunnyHood(play)) {
        Input *input = CONTROLLER1(&play->state);
        PauseContext *pauseCtx = &play->pauseCtx;

        input->press.button &= (~BTN_A);

        sIsBunnyHoodEnabled = !sIsBunnyHoodEnabled;

        s16 sfxId = sIsBunnyHoodEnabled ? NA_SE_SY_DECIDE : NA_SE_SY_CANCEL;

        Audio_PlaySfx(sfxId);
    }
}

#define MASK_GRID_CELL_WIDTH 32
#define MASK_GRID_CELL_HEIGHT 32
#define MASK_GRID_QUAD_MARGIN 2
#define MASK_GRID_QUAD_WIDTH (MASK_GRID_CELL_WIDTH - (2 * MASK_GRID_QUAD_MARGIN))
#define MASK_GRID_QUAD_HEIGHT (MASK_GRID_CELL_HEIGHT - (2 * MASK_GRID_QUAD_MARGIN))
#define MASK_GRID_SELECTED_QUAD_MARGIN (-2)
#define MASK_GRID_SELECTED_QUAD_WIDTH (MASK_GRID_QUAD_WIDTH - (2 * MASK_GRID_SELECTED_QUAD_MARGIN))
#define MASK_GRID_SELECTED_QUAD_HEIGHT (MASK_GRID_QUAD_HEIGHT - (2 * MASK_GRID_SELECTED_QUAD_MARGIN))
#define MASK_GRID_SELECTED_QUAD_TEX_SIZE 32 // both width and height

// based on DrawEquipSquare from BetterBunnyHood
RECOMP_HOOK("KaleidoScope_DrawMaskSelect") void drawEquipSquare_on_KaleidoScope_DrawMaskSelect(PlayState *play) {
    if (isBunnyHoodEnabledAndInInventory()) {
        PauseContext *pauseCtx = &play->pauseCtx;

        if (pauseCtx->state == PAUSE_STATE_MAIN) {
            OPEN_DISPS(play->state.gfxCtx);

            Gfx_SetupDL42_Opa(play->state.gfxCtx);

            Vtx *masksVtx = GRAPH_ALLOC(play->state.gfxCtx, (4 * 4) * sizeof(Vtx));

            s16 slot = SLOT_MASK_BUNNY - ITEM_NUM_SLOTS;
            s16 slotX = slot % MASK_GRID_COLS;
            s16 slotY = slot / MASK_GRID_COLS;
            s16 initialX = 0 - (MASK_GRID_COLS * MASK_GRID_CELL_WIDTH) / 2;
            s16 initialY = (MASK_GRID_ROWS * MASK_GRID_CELL_HEIGHT) / 2 - 6;
            s16 vtxX = (initialX + (slotX * MASK_GRID_CELL_WIDTH)) + MASK_GRID_QUAD_MARGIN;
            s16 vtxY = (initialY - (slotY * MASK_GRID_CELL_HEIGHT)) + pauseCtx->offsetY - MASK_GRID_QUAD_MARGIN;

            s16 gridSelectedQuadMargin = -2;

            // slightly enlarges the square if the bunny hood is on a C button
            // otherwise, the white square would completely cover this mod's square
            for (int i = 0; i < 3; i++) {
                if (GET_CUR_FORM_BTN_ITEM(i + 1) == ITEM_MASK_BUNNY) {
                    if (GET_CUR_FORM_BTN_SLOT(i + 1) >= ITEM_NUM_SLOTS) {
                        gridSelectedQuadMargin = -4;
                    }
                }
            }

            s16 gridSelectedQuadWidth = (MASK_GRID_QUAD_WIDTH - (2 * gridSelectedQuadMargin));
            s16 gridSelectedQuadHeight = (MASK_GRID_QUAD_HEIGHT - (2 * gridSelectedQuadMargin));

            masksVtx[0].v.ob[0] = masksVtx[2].v.ob[0] =
                vtxX + gridSelectedQuadMargin;
            masksVtx[1].v.ob[0] = masksVtx[3].v.ob[0] =
                masksVtx[0].v.ob[0] + gridSelectedQuadWidth;
            masksVtx[0].v.ob[1] = masksVtx[1].v.ob[1] =
                vtxY - gridSelectedQuadMargin;

            masksVtx[2].v.ob[1] = masksVtx[3].v.ob[1] =
                masksVtx[0].v.ob[1] - gridSelectedQuadHeight;

            masksVtx[0].v.ob[2] = masksVtx[1].v.ob[2] =
                masksVtx[2].v.ob[2] = masksVtx[3].v.ob[2] = 0;

            masksVtx[0].v.flag = masksVtx[1].v.flag = masksVtx[2].v.flag =
                masksVtx[3].v.flag = 0;

            masksVtx[0].v.tc[0] = masksVtx[0].v.tc[1] =
                masksVtx[1].v.tc[1] = masksVtx[2].v.tc[0] = 0;

            masksVtx[1].v.tc[0] = masksVtx[2].v.tc[1] =
                masksVtx[3].v.tc[0] = masksVtx[3].v.tc[1] =
                    MASK_GRID_SELECTED_QUAD_TEX_SIZE * (1 << 5);

            masksVtx[0].v.cn[0] = masksVtx[1].v.cn[0] =
                masksVtx[2].v.cn[0] = masksVtx[3].v.cn[0] =
                    masksVtx[0].v.cn[1] = masksVtx[1].v.cn[1] =
                        masksVtx[2].v.cn[1] = masksVtx[3].v.cn[1] =
                            masksVtx[0].v.cn[2] = masksVtx[1].v.cn[2] =
                                masksVtx[2].v.cn[2] = masksVtx[3].v.cn[2] = 255;

            masksVtx[0].v.cn[3] = masksVtx[1].v.cn[3] =
                masksVtx[2].v.cn[3] = masksVtx[3].v.cn[3] = pauseCtx->alpha;

            extern u8 gEquippedItemOutlineTex[];

            gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 100, 200, 255, 255);
            gSPVertex(POLY_OPA_DISP++, masksVtx, 4, 0);
            POLY_OPA_DISP = Gfx_DrawTexQuadIA8(POLY_OPA_DISP, gEquippedItemOutlineTex, 32, 32, 0);
            gDPPipeSync(POLY_OPA_DISP++);
            CLOSE_DISPS(play->state.gfxCtx);
        }
    }
}

static EnTest3 *sFunc80A3F73CKafei;

RECOMP_HOOK("func_80A3F73C") void setDrawFlag_on_func_80A3F73C(EnTest3 *this, PlayState *play) {
    sFunc80A3F73CKafei = this;
}

RECOMP_HOOK_RETURN("func_80A3F73C") void setDrawFlag_on_return_func_80A3F73C(void) {
    if (sIsBunnyHoodEnabled) {
        Player *playerLink = sFunc80A3F73CKafei->unk_D90;
        if (playerLink && playerLink->actor.category == ACTORCAT_NPC && playerLink->currentMask == PLAYER_MASK_NONE) {
            setOverrideGetPlayerReq(playerLink, true);
        }
    }
}

RECOMP_HOOK("func_80A3F0B0") void unsetDrawFlag_on_func_80A3F0B0(EnTest3 *this, PlayState *play) {
    Player *playerLink = this->unk_D90;

    if (playerLink) {
        setOverrideGetPlayerReq(this->unk_D90, false);
    }
}

RECOMP_HOOK("EnTest3_Destroy") void unsetDrawFlag_on_EnTest3_Destroy(EnTest3 *this, PlayState *play) {
    Player *playerLink = this->unk_D90;

    if (playerLink) {
        setOverrideGetPlayerReq(this->unk_D90, false);
    }
}

RECOMP_HOOK("CutsceneCmd_Misc") void clearBunnyHood_on_CutsceneCmd_Misc(PlayState *play, CutsceneContext *csCtx, CsCmdMisc *cmd) {
    if (cmd->type == CS_MISC_PLAYER_FORM_HUMAN) {
        sIsBunnyHoodEnabled = false;
    }
}

RECOMP_CALLBACK("*", recomp_on_init) void registerActorExtensions(void) {
    PLAYER_EXT_BUNNY_HOOD_TWEAKS = z64recomp_extend_actor(ACTOR_PLAYER, sizeof(BunnyHoodTweaksData));
}

RECOMP_EXPORT bool BunnyHoodTweaks_isPlayerRunSpeedModified(PlayState *play, Player *player) {
    return isSpeedShouldBeAppliedToPlayer(play, player);
}

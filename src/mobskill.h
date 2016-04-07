
/**************************************************************************
*   File: mobskill.h                                 Part of MclandiaMUD *
*  Usage: implementation of skills for mobs                               *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  (c) 2002 Sidewinder & Meo                                              *
*  Mclandia is based on CircleMUD                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

/* local functions */
void do_mob_steal (struct char_data *ch, struct char_data *vict);
void do_mob_bash (struct char_data *ch, struct char_data *vict);
void do_mob_disarm (struct char_data *ch, struct char_data *vict);
void do_mob_kick (struct char_data *ch, struct char_data *vict);
void do_mob_backstab (struct char_data *ch, struct char_data *vict);
void do_mob_springleap(struct char_data *ch, struct char_data *vict);
void do_mob_sommersault(struct char_data *ch, struct char_data *vict);
void do_mob_quivering_palm(struct char_data *ch, struct char_data *vict);
void do_mob_kamehameha(struct char_data *ch, struct char_data *vict);
void do_mob_retreat(struct char_data *ch, struct char_data *vict);
void do_mob_ki_shield(struct char_data *ch, struct char_data *vict);

/* local function with parameter */
bool do_mob_fireweapon (struct char_data *ch, struct char_data *vict);
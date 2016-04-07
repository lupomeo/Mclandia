
/* ************************************************************************
*   File: spec_procs2.c                               Part of MclandiaMUD *
*  Usage: implementation of special procedures for mobiles/objects/rooms  *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  (c) 2002 Sidewinder & Meo                                              *
*  Mclandia is based on CircleMUD                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "constants.h"
#include "mobskill.h"

/* meo home vars */
struct char_data *tmpch;
struct char_data *tmpch1;
int mastime;
int mastime1;

/*   external vars  */
extern struct room_data *world;
extern struct char_data *character_list;
extern struct descriptor_data *descriptor_list;
extern struct index_data *mob_index;
extern struct index_data *obj_index;
extern struct time_info_data time_info;
extern struct spell_info_type spell_info[];
extern int guild_info[][3];
extern char *shot_types[];
extern int shot_damage[][2];
extern char *types_ammo[][2];

/* extern functions */
void add_follower (struct char_data *ch, struct char_data *leader);
void gain_level (struct char_data *ch);
void look_at_char (struct char_data *i, struct char_data *ch);
ACMD (do_drop);
ACMD (do_gen_door);
ACMD (do_say);
int compute_armor_class (struct char_data *ch);
void sort_spells (void);
int compare_spells (const void *x, const void *y);
const char *how_good (int percent);
void list_skills (struct char_data *ch);
struct char_data *find_MobInRoomWithFunc (int room,
										  int (name) (struct char_data * ch,
													  void *me, int cmd,
													  char *argument));
void npc_steal (struct char_data *ch, struct char_data *victim);


/* local functions */
SPECIAL (warrior_class);
SPECIAL (thief_class);
SPECIAL (martial_artist);




/*************************************************************/

SPECIAL (warrior_class)
{
	struct char_data *vict;
	int att_type = 0, hit_roll = 0, to_hit = 0;

	if (cmd || !AWAKE (ch))
		return FALSE;

    /* check ch e' realmente un NPC */
    if (!IS_NPC(ch))
        return FALSE;


	/* if two people are fighting in a room */
	if (FIGHTING (ch) && (FIGHTING (ch)->in_room == ch->in_room))
	{

		vict = FIGHTING (ch);

		if (number (1, 10) == 10)
		{
			act ("$n ruggisce per la rabbia.", 1, ch, 0, 0,
				 TO_ROOM);
		}
		if (GET_POS (ch) == POS_FIGHTING)
		{
            if ( ( IS_MAGIC_USER(vict) || IS_CLERIC(vict) || IS_LINKER(vict) || IS_PSIONIC(vict) ) &&
                GET_POS(vict) >= POS_FIGHTING )
            {
                if (number(1,10) == 1)
                {
                    do_mob_bash (ch, vict);
                    return (TRUE);
                }
            }
            
            if (number (1, 6) < 6)
            {
                /* Uso l'arma da fuoco se ne ha una in mano */
                if ( do_mob_fireweapon(ch,vict) )
                {
                    return (TRUE);
                }
            }

			att_type = number (1, 40);

			hit_roll = number (1, 100) + GET_STR (ch);
			to_hit = (100 - (int) (100 * GET_LEVEL (ch) / 250));
			if (GET_LEVEL (vict) >= LVL_IMMORT)
				hit_roll = 0;

			switch (att_type)
			{
			case 1:
			case 2:
			case 3:
			case 4:
            case 5:
                do_mob_kick (ch, vict);
                return (TRUE);
				break;
			case 6:
			case 7:
            case 8: 
            case 9: 
            case 10:
                do_mob_bash (ch, vict);
                return (TRUE);
				break;
            case 15:
			    do_mob_disarm (ch, vict);
                return (TRUE);
				break;
			default:
                hit (ch, vict, TYPE_UNDEFINED);
				break;
			}

		}
        else 
        {
            act ("$n scatta all'attacco!", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS (ch) = POS_FIGHTING;
        }
        
	}
    
	return (FALSE);
}

SPECIAL (thief_class)
{
	struct char_data *cons;

	if (cmd)
		return (FALSE);

    /* check ch e' realmente un NPC */
    if (!IS_NPC(ch))
        return FALSE;


	if (GET_POS (ch) != POS_STANDING)
		return (FALSE);

	for (cons = world[ch->in_room].people; cons; cons = cons->next_in_room)
    {
		if (!IS_NPC (cons) && (GET_LEVEL (cons) < LVL_IMMORT)
			&& (!number (0, 4)))
		{
			if (number (1, 50) > 25)
			{
				do_mob_backstab (ch, cons);
			}
			else
			{
				do_mob_steal (ch, cons);
			}
			return (TRUE);
		}
    }
	return (FALSE);
}


SPECIAL (martial_artist)
{
    struct char_data *vict;
	int att_type = 0, hit_roll = 0, to_hit = 0;

	if (cmd || !AWAKE (ch))
		return FALSE;

    /* check ch e' realmente un NPC */
    if (!IS_NPC(ch))
        return FALSE;

	/* if two people are fighting in a room */
	if (FIGHTING (ch) && (FIGHTING (ch)->in_room == ch->in_room))
	{
        vict = FIGHTING (ch);

		if (GET_POS (ch) < POS_FIGHTING)
        {
            do_mob_springleap(ch,vict);
            GET_POS (ch) = POS_FIGHTING;
            return (TRUE);
        }
        if ( ( IS_MAGIC_USER(vict) || IS_CLERIC(vict) || IS_LINKER(vict) || IS_PSIONIC(vict) ) &&
                GET_POS(vict) >= POS_FIGHTING )
        {
            if (number(1,10) == 1)
            {
                do_mob_sommersault (ch, vict);
                return (TRUE);
            }
        }
        else if (GET_POS (ch) == POS_FIGHTING)
		{
            att_type = number (1, 40);

			hit_roll = number (1, 100) + GET_DEX (ch);
			to_hit = (100 - (int) (100 * GET_LEVEL (ch) / 250));
			if (GET_LEVEL (vict) >= LVL_IMMORT)
				hit_roll = 0;

			switch (att_type)
			{
			case 1:
			case 2:
			case 3:
			case 4:
            case 5:
                do_mob_kick (ch, vict);
                return (TRUE);
				break;
			case 6:
			case 7:
            case 8: 
            case 9: 
            case 10:
                do_mob_sommersault (ch, vict);
                return (TRUE);
				break;
			case 14:
            case 15:
			case 16:
			case 17:
                do_mob_disarm (ch, vict);
                return (TRUE);
			case 18:
                do_mob_quivering_palm(ch,vict);
                return (TRUE);
                break;
			case 19:
			case 20:
                do_mob_kamehameha(ch, vict);
                return (TRUE);
				break;
			default:
                hit (ch, vict, TYPE_UNDEFINED);
				break;
			}
		}
        else 
        {
            act ("$n scatta all'attacco!", TRUE, ch, 0, 0, TO_ROOM);
			GET_POS (ch) = POS_FIGHTING;
        }
	}
	return (FALSE);
}
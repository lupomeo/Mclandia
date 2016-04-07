
/***************************************************************************
 *   File: trap.c                                    Part of CircleMUD     *
 *  Usage: Trap systems                                                    *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *                                                                         *
 *  (C) 2001 - Sidewinder                                                  *
 *                                                                         *
 *  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 ***************************************************************************/

#define __TRAP_C__

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "spells.h"
#include "screen.h"
#include "constants.h"
#include "trap.h"


/*extern vars */
extern struct descriptor_data *descriptor_list;
extern struct index_data *mob_index;
extern struct room_data *world;
extern struct message_list fight_messages[MAX_MESSAGES];
extern struct obj_data *object_list;
extern char *shot_types[];
extern int shot_damage[][2];
extern char *types_ammo[][2];

/* extern functions */
extern void make_corpse (struct char_data *ch);
extern void change_alignment (struct char_data *ch, struct char_data *victim);
extern void death_cry (struct char_data *ch);
extern void die (struct char_data *ch);
extern int mag_savingthrow (struct char_data *ch, int type, int modifier);
extern void mag_affects (int level, struct char_data *ch,
						 struct char_data *victim, int spellnum,
						 int savetype);

extern int trap_dir[];

/* local functions */
void do_settrap (struct char_data *ch, char *arg, int cmd);
int check_move_trap (struct char_data *ch, int dir);
int check_inside_trap (struct char_data *ch, struct obj_data *obj);
int check_any_trap (struct char_data *ch, struct obj_data *obj);
int check_get_trap (struct char_data *ch, struct obj_data *obj);
int trigger_trap (struct char_data *ch, struct obj_data *obj);
void find_trap_damage (struct char_data *ch, struct obj_data *obj);
void trap_damage (struct char_data *ch, int damtype, int amnt,
				  struct obj_data *obj);
void trap_dam (struct char_data *vict, int damtype, int amnt,
			   struct obj_data *obj);
void trap_poison (struct char_data *vict, struct obj_data *obj);
void trap_sleep (struct char_data *vict, struct obj_data *obj);
void trap_teleport (struct char_data *vict);
void info_msg (struct char_data *ch);

/* routines */

void
do_settrap (struct char_data *ch, char *arg, int cmd)
{

	/* parse for directions */

/* trap that affects all directions is an AE trap */

	/* parse for type       */
	/* parse for level      */

}


int
check_move_trap (struct char_data *ch, int dir)
{
	struct obj_data *obj;

	for (obj = world[ch->in_room].contents; obj; obj = obj->next_content)
	{
		if ((GET_OBJ_TYPE (obj) == ITEM_TRAP) && (GET_TRAP_CHARGES (obj) > 0))
		{
			if (IS_SET (GET_TRAP_EFF (obj), trap_dir[dir]) &&
				IS_SET (GET_TRAP_EFF (obj), TRAP_EFF_MOVE))
			{
				return (trigger_trap (ch, obj));
			}
		}
	}
	return (FALSE);
}

int
check_inside_trap (struct char_data *ch, struct obj_data *obj)
{
	struct obj_data *t;

	for (t = obj->contains; t; t = t->next_content)
	{
		if ((GET_OBJ_TYPE (t) == ITEM_TRAP) &&
			(IS_SET (GET_TRAP_EFF (t), TRAP_EFF_OBJECT)) &&
			(GET_TRAP_CHARGES (t) > 0))
		{
			return (trigger_trap (ch, t));
		}
	}
	return (FALSE);
}

int
check_any_trap (struct char_data *ch, struct obj_data *obj)
{
	if ((GET_OBJ_TYPE (obj) == ITEM_TRAP) && (GET_TRAP_CHARGES (obj) > 0))
	{
		return (trigger_trap (ch, obj));
	}
	return (FALSE);
}

int
check_get_trap (struct char_data *ch, struct obj_data *obj)
{
	if ((GET_OBJ_TYPE (obj) == ITEM_TRAP) &&
		(IS_SET (GET_TRAP_EFF (obj), TRAP_EFF_OBJECT)) &&
		(GET_TRAP_CHARGES (obj) > 0))
	{
		return (trigger_trap (ch, obj));
	}
	return (FALSE);
}

int
trigger_trap (struct char_data *ch, struct obj_data *obj)
{
	struct char_data *vict;

	if (GET_OBJ_TYPE (obj) == ITEM_TRAP)
	{
		if (obj->obj_flags.value[TRAP_CHARGES])
		{
			act ("Senti uno strano rumore...", TRUE, ch, 0, 0, TO_ROOM);
			act ("Senti uno strano rumore...", TRUE, ch, 0, 0, TO_CHAR);
			GET_TRAP_CHARGES (obj) -= 1;

			/* make sure room fire off works! */
			if (IS_SET (GET_TRAP_EFF (obj), TRAP_EFF_ROOM))
			{
				for (vict = world[ch->in_room].people; vict;
					 vict = vict->next_in_room)
				{
					find_trap_damage (vict, obj);
				}				/* end for */
			}					/* end is_set */
			else
			{
				find_trap_damage (ch, obj);
			}					/* end was not fire trap */
			return (TRUE);
		}						/* end trap had charges */
		return (FALSE);
	}
	return (FALSE);
}

void
find_trap_damage (struct char_data *ch, struct obj_data *obj)
{
	/* trap types < 0 are special */
	if (GET_TRAP_DAM_TYPE (obj) >= 0)
	{
		trap_damage (ch, GET_TRAP_DAM_TYPE (obj), 3 * GET_TRAP_LEV (obj),
					 obj);
	}
	else
	{
		trap_damage (ch, GET_TRAP_DAM_TYPE (obj), 0, obj);
	}
}

void
trap_damage (struct char_data *ch, int damtype, int amnt,
			 struct obj_data *obj)
{
	if (GET_LEVEL (ch) < LVL_IMMORT)
	{
		return;
	}

	if (IS_AFFECTED (ch, AFF_SANCTUARY))
	{
		amnt = MAX ((int) (amnt / 2), 0);	/* Max 1/2 damage when sanct'd */
	}

	amnt = MAX (amnt, 0);
	GET_HIT (ch) -= amnt;
	update_pos (ch);
	trap_dam (ch, damtype, amnt, obj);
	info_msg (ch);

	if (GET_POS (ch) == POS_DEAD)
	{
		sprintf (buf, "%s e' stato ucciso da una trappola a %s \r\n",
				 GET_NAME (ch), world[ch->in_room].name);
		mudlog (buf, BRF, LVL_IMMORT, TRUE);
		die (ch);
	}
}

void
trap_dam (struct char_data *vict, int damtype, int amnt, struct obj_data *obj)
{
	char desc[20];
	char buf[132];

	/* easier than dealing with message(ug) */
	switch (damtype)
	{
	case TRAP_DAM_PIERCE:
		strcpy (desc, "pierced");
		break;
	case TRAP_DAM_SLASH:
		strcpy (desc, "sliced");
		break;
	case TRAP_DAM_BLUNT:
		strcpy (desc, "pounded");
		break;
	case TRAP_DAM_FIRE:
		strcpy (desc, "seared");
		break;
	case TRAP_DAM_COLD:
		strcpy (desc, "frozen");
		break;
	case TRAP_DAM_ENERGY:
		strcpy (desc, "shortcuted");
		break;
	case TRAP_DAM_SLEEP:
		strcpy (desc, "knocked out");
		break;
	case TRAP_DAM_TELEPORT:
		strcpy (desc, "transported");
		break;
	case TRAP_DAM_POISON:
		strcpy (desc, "poisoned");
		break;
	default:					/* need a poison trap! */
		strcpy (desc, "blown away");
		break;
	}

	if ((damtype != TRAP_DAM_TELEPORT) &&
		(damtype != TRAP_DAM_SLEEP) && (damtype != TRAP_DAM_POISON))	/* check for poison trap here */
	{
		if (amnt > 0)
		{
			sprintf (buf, "$n is %s by $p!", desc);
			act (buf, TRUE, vict, obj, 0, TO_ROOM);
			sprintf (buf, "You are %s by $p!", desc);
			act (buf, TRUE, vict, obj, 0, TO_CHAR);
		}
		else
		{
			sprintf (buf, "$n is almost %s by $p!", desc);
			act (buf, TRUE, vict, obj, 0, TO_ROOM);
			sprintf (buf, "You are almost %s by $p!", desc);
			act (buf, TRUE, vict, obj, 0, TO_CHAR);
		}
	}

	if (damtype == TRAP_DAM_TELEPORT)
	{
		trap_teleport (vict);
	}
	else if (damtype == TRAP_DAM_SLEEP)
	{
		trap_sleep (vict, obj);
	}
	else if (damtype == TRAP_DAM_POISON)
	{
		trap_poison (vict, obj);
	}

}


void
trap_poison (struct char_data *vict, struct obj_data *obj)
{
	act ("Ti senti male.", FALSE, vict, 0, 0, TO_CHAR);
	act ("$n si sente male.", TRUE, vict, 0, 0, TO_ROOM);

	mag_affects (GET_TRAP_LEV (obj), NULL, vict, SPELL_POISON, SAVING_SPELL);
}

void
trap_sleep (struct char_data *vict, struct obj_data *obj)
{
	if (GET_POS (vict) > POS_SLEEPING)
	{
		act ("Ti senti molto assonnato ..... zzzzzz", FALSE, vict, 0, 0,
			 TO_CHAR);
		act ("$n crolla a terra assonnato.", TRUE, vict, 0, 0, TO_ROOM);
		GET_POS (vict) = POS_SLEEPING;
	}
	mag_affects (GET_TRAP_LEV (obj), NULL, vict, SPELL_SLEEP, SAVING_SPELL);
}

void
trap_teleport (struct char_data *vict)
{
	room_rnum to_room;

	if (mag_savingthrow (vict, SAVING_SPELL, 0))
	{
		send_to_char ("You feel strange, but the effect fades.\r\n", vict);
		return;
	}

	do
	{
		to_room = number (0, top_of_world);
	}
	while (ROOM_FLAGGED (to_room, ROOM_PRIVATE | ROOM_DEATH | ROOM_GODROOM));

	act ("$n slowly fades out of existence and is gone.",
		 FALSE, vict, 0, 0, TO_ROOM);
	char_from_room (vict);
	char_to_room (vict, to_room);
	act ("$n slowly fades into existence.", FALSE, vict, 0, 0, TO_ROOM);
	look_at_room (vict, 0);
}

void
info_msg (struct char_data *ch)
{

	switch (GET_POS (ch))
	{
	case POS_MORTALLYW:
		act ("$n is mortally wounded, and will die soon, if not aided.",
			 TRUE, ch, 0, 0, TO_ROOM);
		act ("You are mortally wounded, and will die soon, if not aided.",
			 FALSE, ch, 0, 0, TO_CHAR);
		break;
	case POS_INCAP:
		act ("$n is incapacitated and will slowly die, if not aided.",
			 TRUE, ch, 0, 0, TO_ROOM);
		act ("You are incapacitated and you will slowly die, if not aided.",
			 FALSE, ch, 0, 0, TO_CHAR);
		break;
	case POS_STUNNED:
		act ("$n is stunned, but will probably regain consciousness.",
			 TRUE, ch, 0, 0, TO_ROOM);
		act ("You're stunned, but you will probably regain consciousness.",
			 FALSE, ch, 0, 0, TO_CHAR);
		break;
	case POS_DEAD:
		act ("$n is dead! R.I.P.", TRUE, ch, 0, 0, TO_ROOM);
		act ("You are dead!  Sorry...", FALSE, ch, 0, 0, TO_CHAR);
		break;
	default:					/* >= POSITION SLEEPING */
		break;
	}
}

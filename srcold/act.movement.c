
/* ************************************************************************
*   File: act.movement.c                                Part of CircleMUD *
*  Usage: movement commands, door handling, & sleep/rest/etc state        *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
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
#include "house.h"
#include "constants.h"

/* external vars  */
extern struct room_data *world;
extern struct char_data *character_list;
extern struct descriptor_data *descriptor_list;
extern struct index_data *obj_index;

/* external functs */
void add_follower (struct char_data *ch, struct char_data *leader);
int special (struct char_data *ch, int cmd, char *arg);
void death_cry (struct char_data *ch);
int find_eq_pos (struct char_data *ch, struct obj_data *obj, char *arg);
room_rnum find_target_room (struct char_data *ch, char *rawroomstr);
extern int check_move_trap (struct char_data *ch, int dir);

/* local functions */
int has_boat (struct char_data *ch);
int find_door (struct char_data *ch, const char *type, char *dir,
			   const char *cmdname);
int has_key (struct char_data *ch, obj_vnum key);
void do_doorcmd (struct char_data *ch, struct obj_data *obj, int door,
				 int scmd);
int ok_pick (struct char_data *ch, obj_vnum keynum, int pickproof, int scmd);
ACMD (do_gen_door);
ACMD (do_enter);
ACMD (do_leave);
ACMD (do_stand);
ACMD (do_sit);
ACMD (do_rest);
ACMD (do_sleep);
ACMD (do_wake);
ACMD (do_follow);
ACMD (do_doorway);


/* simple function to determine if char can walk on water */
int
has_boat (struct char_data *ch)
{
	struct obj_data *obj;
	int i;

/*
  if (ROOM_IDENTITY(ch->in_room) == DEAD_SEA)
    return (1);
*/

	if (GET_LEVEL (ch) > LVL_IMMORT)
		return (1);

	if (AFF_FLAGGED (ch, AFF_WATERWALK))
		return (1);

	/* non-wearable boats in inventory will do it */
	for (obj = ch->carrying; obj; obj = obj->next_content)
		if (GET_OBJ_TYPE (obj) == ITEM_BOAT
			&& (find_eq_pos (ch, obj, NULL) < 0))
			return (1);

	/* and any boat you're wearing will do it too */
	for (i = 0; i < NUM_WEARS; i++)
		if (GET_EQ (ch, i) && GET_OBJ_TYPE (GET_EQ (ch, i)) == ITEM_BOAT)
			return (1);

	return (0);
}



/* do_simple_move assumes
 *    1. That there is no master and no followers.
 *    2. That the direction exists.
 *
 *   Returns :
 *   1 : If succes.
 *   0 : If fail
 */
int
do_simple_move (struct char_data *ch, int dir, int need_specials_check)
{
	room_rnum was_in;
	int need_movement;


	/* aggiunto da meo per settare la pos di fly se l'aff_bitvector e' attivo */

	if (AFF_FLAGGED (ch, AFF_FLY) && GET_POS (ch) == POS_STANDING)
	{
		GET_POS (ch) = POS_FLYING;
	}

	/*
	 * Check for special routines (North is 1 in command list, but 0 here) Note
	 * -- only check if following; this avoids 'double spec-proc' bug
	 */
	if (need_specials_check && special (ch, dir + 1, ""))	/* XXX: Evaluate NULL */
	{
		return (0);
	}

	/* charmed? */
	if (AFF_FLAGGED (ch, AFF_CHARM) && ch->master
		&& ch->in_room == ch->master->in_room)
	{
		send_to_char
			("The thought of leaving your master makes you weep.\r\n", ch);
		act ("$n bursts into tears.", FALSE, ch, 0, 0, TO_ROOM);
		return (0);
	}

	/* if this room or the one we're going to needs a boat, check for one */
	if ((SECT (IN_ROOM (ch)) == SECT_WATER_NOSWIM) ||
		(SECT (EXIT (ch, dir)->to_room) == SECT_WATER_NOSWIM))
	{
		if (GET_POS (ch) != POS_FLYING)
		{
			if (!has_boat (ch))
			{
				send_to_char
					("Hai bisogno di una imbarcazione per andare li'.\r\n",
					 ch);
				return (0);
			}
		}
	}

	/* move points needed is avg. move loss for src and destination sect type */
	need_movement = (movement_loss[SECT (ch->in_room)] +
					 movement_loss[SECT (EXIT (ch, dir)->to_room)]) / 2;

	if (GET_MOVE (ch) < need_movement && !IS_NPC (ch))
	{
		if (need_specials_check && ch->master)
		{
			send_to_char ("Sei esausto.\r\n", ch);
		}
		else
		{
			send_to_char ("Sei esausto.\r\n", ch);
		}
		return (0);
	}

	if (ROOM_FLAGGED (ch->in_room, ROOM_ATRIUM))
	{
		if (!House_can_enter (ch, GET_ROOM_VNUM (EXIT (ch, dir)->to_room)))
		{
			send_to_char
				("Questa e' una proprieta' privata! Vietato passare!\r\n",
				 ch);
			return (0);
		}
	}

	if (ROOM_FLAGGED (EXIT (ch, dir)->to_room, ROOM_TUNNEL) &&
		num_pc_in_room (&(world[EXIT (ch, dir)->to_room])) > 1)
	{
		send_to_char ("Non c'e spazio per priu di una persona!\r\n", ch);
		return (0);
	}

	/* Mortals and low level gods cannot enter greater god rooms. */
	if (ROOM_FLAGGED (EXIT (ch, dir)->to_room, ROOM_GODROOM) &&
		GET_LEVEL (ch) < LVL_GRGOD)
	{
		send_to_char
			("Non sei degno di usare questa stanza che e' consacrata agli dei!\r\n",
			 ch);
		return (0);
	}

	/* Now we know we're allow to go into the room. */
	if (GET_LEVEL (ch) < LVL_IMMORT && !IS_NPC (ch)
		&& (GET_POS (ch) != POS_FLYING))
	{
		GET_MOVE (ch) -= need_movement;
	}

	if (GET_POS (ch) == POS_FLYING)
	{
		sprintf (buf2, "$n vola verso %s.", it_dirs[dir]);
		act (buf2, TRUE, ch, 0, 0, TO_ROOM);
	}
	else if (!AFF_FLAGGED (ch, AFF_SNEAK))
	{
		sprintf (buf2, "$n va verso %s.", it_dirs[dir]);
		act (buf2, TRUE, ch, 0, 0, TO_ROOM);
	}

	was_in = ch->in_room;
	char_from_room (ch);
	char_to_room (ch, world[was_in].dir_option[dir]->to_room);

	if (GET_POS (ch) == POS_FLYING)
	{
		act ("$n arriva volando.", TRUE, ch, 0, 0, TO_ROOM);
	}
	else if (!AFF_FLAGGED (ch, AFF_SNEAK))
	{
		act ("arriva $n.", TRUE, ch, 0, 0, TO_ROOM);
	}

	if (ch->desc != NULL)
	{
		look_at_room (ch, 0);
	}
	if (check_move_trap (ch, dir) != 0)
	{
		return 0;
	}

	if (ROOM_FLAGGED (ch->in_room, ROOM_DEATH) && GET_LEVEL (ch) < LVL_IMMORT)
	{
		log_death_trap (ch);
		death_cry (ch);
		extract_char (ch);
		return (0);
	}
	return (1);
}


int
perform_move (struct char_data *ch, int dir, int need_specials_check)
{
	room_rnum was_in;
	struct follow_type *k, *next;

	if (ch == NULL || dir < 0 || dir >= NUM_OF_DIRS || FIGHTING (ch))
		return (0);
	else if (!EXIT (ch, dir) || EXIT (ch, dir)->to_room == NOWHERE)
		send_to_char ("Non puoi andare da quella parte...\r\n", ch);
	else if (EXIT_FLAGGED (EXIT (ch, dir), EX_CLOSED))
	{
		if (EXIT (ch, dir)->keyword)
		{
			sprintf (buf2, "The %s seems to be closed.\r\n",
					 fname (EXIT (ch, dir)->keyword));
			send_to_char (buf2, ch);
		}
		else
			send_to_char ("Sembra chiusa.\r\n", ch);
	}
	else
	{
		if (!ch->followers)
			return (do_simple_move (ch, dir, need_specials_check));

		was_in = ch->in_room;
		if (!do_simple_move (ch, dir, need_specials_check))
			return (0);

		for (k = ch->followers; k; k = next)
		{
			next = k->next;
			if ((k->follower->in_room == was_in) &&
				(GET_POS (k->follower) >= POS_STANDING))
			{
				act ("Segui $N.\r\n", FALSE, k->follower, 0, ch, TO_CHAR);
				perform_move (k->follower, dir, 1);
			}
		}
		return (1);
	}
	return (0);
}


ACMD (do_move)
{
	/*
	 * This is basically a mapping of cmd numbers to perform_move indices.
	 * It cannot be done in perform_move because perform_move is called
	 * by other functions which do not require the remapping.
	 */
	perform_move (ch, subcmd - 1, 0);
}


int
find_door (struct char_data *ch, const char *type, char *dir,
		   const char *cmdname)
{
	int door;

	if (*dir)
	{							/* a direction was specified */
		if ((door = search_block (dir, dirs, FALSE)) == -1)
		{						/* Partial Match */
			send_to_char ("That's not a direction.\r\n", ch);
			return (-1);
		}
		if (EXIT (ch, door))
		{						/* Braces added according to indent. -gg */
			if (EXIT (ch, door)->keyword)
			{
				if (isname (type, EXIT (ch, door)->keyword))
					return (door);
				else
				{
					sprintf (buf2, "I see no %s there.\r\n", type);
					send_to_char (buf2, ch);
					return (-1);
				}
			}
			else
				return (door);
		}
		else
		{
			sprintf (buf2,
					 "I really don't see how you can %s anything there.\r\n",
					 cmdname);
			send_to_char (buf2, ch);
			return (-1);
		}
	}
	else
	{							/* try to locate the keyword */
		if (!*type)
		{
			sprintf (buf2, "What is it you want to %s?\r\n", cmdname);
			send_to_char (buf2, ch);
			return (-1);
		}
		for (door = 0; door < NUM_OF_DIRS; door++)
			if (EXIT (ch, door))
				if (EXIT (ch, door)->keyword)
					if (isname (type, EXIT (ch, door)->keyword))
						return (door);

		sprintf (buf2, "There doesn't seem to be %s %s here.\r\n", AN (type),
				 type);
		send_to_char (buf2, ch);
		return (-1);
	}
}


int
has_key (struct char_data *ch, obj_vnum key)
{
	struct obj_data *o;

	for (o = ch->carrying; o; o = o->next_content)
		if (GET_OBJ_VNUM (o) == key)
			return (1);

	if (GET_EQ (ch, WEAR_HOLD))
		if (GET_OBJ_VNUM (GET_EQ (ch, WEAR_HOLD)) == key)
			return (1);

	return (0);
}



#define NEED_OPEN	(1 << 0)
#define NEED_CLOSED	(1 << 1)
#define NEED_UNLOCKED	(1 << 2)
#define NEED_LOCKED	(1 << 3)

const char *cmd_door[] = {
	"open",
	"close",
	"unlock",
	"lock",
	"pick"
};

const int flags_door[] = {
	NEED_CLOSED | NEED_UNLOCKED,
	NEED_OPEN,
	NEED_CLOSED | NEED_LOCKED,
	NEED_CLOSED | NEED_UNLOCKED,
	NEED_CLOSED | NEED_LOCKED
};


#define EXITN(room, door)		(world[room].dir_option[door])
#define OPEN_DOOR(room, obj, door)	((obj) ?\
		(TOGGLE_BIT(GET_OBJ_VAL(obj, 1), CONT_CLOSED)) :\
		(TOGGLE_BIT(EXITN(room, door)->exit_info, EX_CLOSED)))
#define LOCK_DOOR(room, obj, door)	((obj) ?\
		(TOGGLE_BIT(GET_OBJ_VAL(obj, 1), CONT_LOCKED)) :\
		(TOGGLE_BIT(EXITN(room, door)->exit_info, EX_LOCKED)))

void
do_doorcmd (struct char_data *ch, struct obj_data *obj, int door, int scmd)
{
	int other_room = 0;
	struct room_direction_data *back = 0;

	sprintf (buf, "$n %ss ", cmd_door[scmd]);
	if (!obj && ((other_room = EXIT (ch, door)->to_room) != NOWHERE))
		if ((back = world[other_room].dir_option[rev_dir[door]]) != NULL)
			if (back->to_room != ch->in_room)
				back = 0;

	switch (scmd)
	{
	case SCMD_OPEN:
	case SCMD_CLOSE:
		OPEN_DOOR (ch->in_room, obj, door);
		if (back)
			OPEN_DOOR (other_room, obj, rev_dir[door]);
		send_to_char (OK, ch);
		break;
	case SCMD_UNLOCK:
	case SCMD_LOCK:
		LOCK_DOOR (ch->in_room, obj, door);
		if (back)
			LOCK_DOOR (other_room, obj, rev_dir[door]);
		send_to_char ("*Click*\r\n", ch);
		break;
	case SCMD_PICK:
		LOCK_DOOR (ch->in_room, obj, door);
		if (back)
			LOCK_DOOR (other_room, obj, rev_dir[door]);
		send_to_char ("The lock quickly yields to your skills.\r\n", ch);
		strcpy (buf, "$n skillfully picks the lock on ");
		break;
	}

	/* Notify the room */
	sprintf (buf + strlen (buf), "%s%s.", ((obj) ? "" : "the "),
			 (obj) ? "$p" : (EXIT (ch, door)->keyword ? "$F" : "door"));
	if (!(obj) || (obj->in_room != NOWHERE))
		act (buf, FALSE, ch, obj, obj ? 0 : EXIT (ch, door)->keyword,
			 TO_ROOM);

	/* Notify the other room */
	if ((scmd == SCMD_OPEN || scmd == SCMD_CLOSE) && back)
	{
		sprintf (buf, "The %s is %s%s from the other side.",
				 (back->keyword ? fname (back->keyword) : "door"),
				 cmd_door[scmd], (scmd == SCMD_CLOSE) ? "d" : "ed");
		if (world[EXIT (ch, door)->to_room].people)
		{
			act (buf, FALSE, world[EXIT (ch, door)->to_room].people, 0, 0,
				 TO_ROOM);
			act (buf, FALSE, world[EXIT (ch, door)->to_room].people, 0, 0,
				 TO_CHAR);
		}
	}
}


int
ok_pick (struct char_data *ch, obj_vnum keynum, int pickproof, int scmd)
{
	int percent;

	percent = number (1, 101);

	if (scmd == SCMD_PICK)
	{
		if (keynum < 0)
			send_to_char ("Odd - you can't seem to find a keyhole.\r\n", ch);
		else if (pickproof)
			send_to_char ("It resists your attempts to pick it.\r\n", ch);
		else if (percent > GET_SKILL (ch, SKILL_PICK_LOCK))
			send_to_char ("You failed to pick the lock.\r\n", ch);
		else
			return (1);
		return (0);
	}
	return (1);
}


#define DOOR_IS_OPENABLE(ch, obj, door)	((obj) ? \
			((GET_OBJ_TYPE(obj) == ITEM_CONTAINER) && \
			OBJVAL_FLAGGED(obj, CONT_CLOSEABLE)) :\
			(EXIT_FLAGGED(EXIT(ch, door), EX_ISDOOR)))
#define DOOR_IS_OPEN(ch, obj, door)	((obj) ? \
			(!OBJVAL_FLAGGED(obj, CONT_CLOSED)) :\
			(!EXIT_FLAGGED(EXIT(ch, door), EX_CLOSED)))
#define DOOR_IS_UNLOCKED(ch, obj, door)	((obj) ? \
			(!OBJVAL_FLAGGED(obj, CONT_LOCKED)) :\
			(!EXIT_FLAGGED(EXIT(ch, door), EX_LOCKED)))
#define DOOR_IS_PICKPROOF(ch, obj, door) ((obj) ? \
			(OBJVAL_FLAGGED(obj, CONT_PICKPROOF)) : \
			(EXIT_FLAGGED(EXIT(ch, door), EX_PICKPROOF)))

#define DOOR_IS_CLOSED(ch, obj, door)	(!(DOOR_IS_OPEN(ch, obj, door)))
#define DOOR_IS_LOCKED(ch, obj, door)	(!(DOOR_IS_UNLOCKED(ch, obj, door)))
#define DOOR_KEY(ch, obj, door)		((obj) ? (GET_OBJ_VAL(obj, 2)) : \
					(EXIT(ch, door)->key))
#define DOOR_LOCK(ch, obj, door)	((obj) ? (GET_OBJ_VAL(obj, 1)) : \
					(EXIT(ch, door)->exit_info))

ACMD (do_gen_door)
{
	int door = -1;
	obj_vnum keynum;
	char type[MAX_INPUT_LENGTH], dir[MAX_INPUT_LENGTH];
	struct obj_data *obj = NULL;
	struct char_data *victim = NULL;

	skip_spaces (&argument);
	if (!*argument)
	{
		sprintf (buf, "%s what?\r\n", cmd_door[subcmd]);
		send_to_char (CAP (buf), ch);
		return;
	}
	two_arguments (argument, type, dir);
	if (!generic_find (type, FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &victim, &obj))
		door = find_door (ch, type, dir, cmd_door[subcmd]);

	if ((obj) || (door >= 0))
	{
		keynum = DOOR_KEY (ch, obj, door);
		if (!(DOOR_IS_OPENABLE (ch, obj, door)))
			act ("You can't $F that!", FALSE, ch, 0, cmd_door[subcmd],
				 TO_CHAR);
		else if (!DOOR_IS_OPEN (ch, obj, door)
				 && IS_SET (flags_door[subcmd], NEED_OPEN))
			send_to_char ("But it's already closed!\r\n", ch);
		else if (!DOOR_IS_CLOSED (ch, obj, door) &&
				 IS_SET (flags_door[subcmd], NEED_CLOSED))
			send_to_char ("But it's currently open!\r\n", ch);
		else if (!(DOOR_IS_LOCKED (ch, obj, door)) &&
				 IS_SET (flags_door[subcmd], NEED_LOCKED))
			send_to_char ("Oh.. it wasn't locked, after all..\r\n", ch);
		else if (!(DOOR_IS_UNLOCKED (ch, obj, door)) &&
				 IS_SET (flags_door[subcmd], NEED_UNLOCKED))
			send_to_char ("It seems to be locked.\r\n", ch);
		else if (!has_key (ch, keynum) && (GET_LEVEL (ch) < LVL_GOD) &&
				 ((subcmd == SCMD_LOCK) || (subcmd == SCMD_UNLOCK)))
			send_to_char ("You don't seem to have the proper key.\r\n", ch);
		else
			if (ok_pick
				(ch, keynum, DOOR_IS_PICKPROOF (ch, obj, door), subcmd))
			do_doorcmd (ch, obj, door, subcmd);
	}
	return;
}



ACMD (do_enter)
{
	int door;

/* Begin Remove Portal */

	struct obj_data *obj;

	skip_spaces (&argument);
	one_argument (argument, buf);

	if (*buf)					/* an argument was supplied, search for door * keyword */
	{

		if (
			(obj =
			 get_obj_in_list_vis (ch, buf, NULL,
								  world[ch->in_room].contents)))
		{
			if (CAN_SEE_OBJ (ch, obj))
			{
				if (GET_OBJ_TYPE (obj) == ITEM_PORTAL)
				{
					if (GET_OBJ_VAL (obj, 0) != NOWHERE)
					{
						send_to_char ("Entri dentro il telnet\r\n", ch);
						act ("$n attraversa dentro la porta telnet.\r\n",
							 FALSE, ch, 0, 0, TO_ROOM);
						char_from_room (ch);
						char_to_room (ch, GET_OBJ_VAL (obj, 0));
						act ("$n esce da una porta telnet.\r\n", FALSE, ch, 0,
							 0, TO_ROOM);
					}
					else if (real_room (GET_OBJ_VAL (obj, 1)) != NOWHERE)
					{
						send_to_char ("Entri dentro il telnet\r\n", ch);
						act ("$n attraversa dentro la porta telnet.\r\n",
							 FALSE, ch, 0, 0, TO_ROOM);
						char_from_room (ch);
						char_to_room (ch, real_room (GET_OBJ_VAL (obj, 1)));
						act ("$n esce da una porta telnet.\r\n", FALSE, ch, 0,
							 0, TO_ROOM);
					}

					look_at_room (ch, 1);
					return;
				}
			}
		}
		for (door = 0; door < NUM_OF_DIRS; door++)
		{
			if (EXIT (ch, door))
			{
				if (EXIT (ch, door)->keyword)
				{
					if (!str_cmp (EXIT (ch, door)->keyword, buf))
					{
						perform_move (ch, door, 1);
						return;
					}
				}
			}
		}

		sprintf (buf2, "There is no %s here.\r\n", buf);
		send_to_char (buf2, ch);
	}
	else if (ROOM_FLAGGED (ch->in_room, ROOM_INDOORS))
	{
		send_to_char ("You are already indoors.\r\n", ch);
	}
	else
	{
		/* try to locate an entrance */
		for (door = 0; door < NUM_OF_DIRS; door++)
		{
			if (EXIT (ch, door))
			{
				if (EXIT (ch, door)->to_room != NOWHERE)
				{
					if (!EXIT_FLAGGED (EXIT (ch, door), EX_CLOSED) &&
						ROOM_FLAGGED (EXIT (ch, door)->to_room, ROOM_INDOORS))
					{
						perform_move (ch, door, 1);
						return;
					}
				}
			}
		}
		send_to_char ("You can't seem to find anything to enter.\r\n", ch);
	}
}


ACMD (do_leave)
{
	int door;

	if (OUTSIDE (ch))
		send_to_char ("You are outside.. where do you want to go?\r\n", ch);
	else
	{
		for (door = 0; door < NUM_OF_DIRS; door++)
			if (EXIT (ch, door))
				if (EXIT (ch, door)->to_room != NOWHERE)
					if (!EXIT_FLAGGED (EXIT (ch, door), EX_CLOSED) &&
						!ROOM_FLAGGED (EXIT (ch, door)->to_room,
									   ROOM_INDOORS))
					{
						perform_move (ch, door, 1);
						return;
					}
		send_to_char ("I see no obvious exits to the outside.\r\n", ch);
	}
}


ACMD (do_stand)
{
	switch (GET_POS (ch))
	{
	case POS_STANDING:
		send_to_char ("Sei gia' in piedi.\r\n", ch);
		break;
	case POS_SITTING:
		send_to_char ("Ti alzi.\r\n", ch);
		act ("$n si alza in piedi.", TRUE, ch, 0, 0, TO_ROOM);
		/* Will be sitting after a successful bash and may still be fighting. */
		GET_POS (ch) = FIGHTING (ch) ? POS_FIGHTING : POS_STANDING;
		break;
	case POS_RESTING:
		send_to_char ("Smetti di riposare e ti alzi.\r\n", ch);
		act ("$n smette di riposare e si alza in piedi.", TRUE, ch, 0, 0,
			 TO_ROOM);
		GET_POS (ch) = POS_STANDING;
		break;
	case POS_SLEEPING:
		send_to_char ("Devi svegliarti prima!\r\n", ch);
		break;
	case POS_FIGHTING:
		send_to_char ("Do you not consider fighting as standing?\r\n", ch);
		break;
	default:
		send_to_char
			("You stop floating around, and put your feet on the ground.\r\n",
			 ch);
		act ("$n stops floating around, and puts $s feet on the ground.",
			 TRUE, ch, 0, 0, TO_ROOM);
		GET_POS (ch) = POS_STANDING;
		break;
	}
}


ACMD (do_sit)
{
	switch (GET_POS (ch))
	{
	case POS_STANDING:
		send_to_char ("Ti siedi.\r\n", ch);
		act ("$n si siede.", FALSE, ch, 0, 0, TO_ROOM);
		GET_POS (ch) = POS_SITTING;
		break;
	case POS_SITTING:
		send_to_char ("Sei gia' seduto.\r\n", ch);
		break;
	case POS_RESTING:
		send_to_char ("Smetti di riposare e ti alzi.\r\n", ch);
		act ("$n smette di riposare.", TRUE, ch, 0, 0, TO_ROOM);
		GET_POS (ch) = POS_SITTING;
		break;
	case POS_SLEEPING:
		send_to_char ("Ti devi alzare prima.\r\n", ch);
		break;
	case POS_FIGHTING:
		send_to_char ("Ti siedi mentre combatti? Sei MATTO?\r\n", ch);
		break;
	default:
		send_to_char ("You stop floating around, and sit down.\r\n", ch);
		act ("$n stops floating around, and sits down.", TRUE, ch, 0, 0,
			 TO_ROOM);
		GET_POS (ch) = POS_SITTING;
		break;
	}
}


ACMD (do_rest)
{
	switch (GET_POS (ch))
	{
	case POS_STANDING:
		send_to_char ("Ti fermi a riposare.\r\n", ch);
		act ("$n si ferma a riposare.", TRUE, ch, 0, 0, TO_ROOM);
		GET_POS (ch) = POS_RESTING;
		break;
	case POS_SITTING:
		send_to_char ("Ti fermi a riposare.\r\n", ch);
		act ("$n riposa.", TRUE, ch, 0, 0, TO_ROOM);
		GET_POS (ch) = POS_RESTING;
		break;
	case POS_RESTING:
		send_to_char ("Stai gia' riposando.\r\n", ch);
		break;
	case POS_SLEEPING:
		send_to_char ("Ti devi alzare prima.\r\n", ch);
		break;
	case POS_FIGHTING:
		send_to_char ("Riposi mentre combatti?  Sei MATTO?\r\n", ch);
		break;
	default:
		send_to_char
			("You stop floating around, and stop to rest your tired bones.\r\n",
			 ch);
		act ("$n stops floating around, and rests.", FALSE, ch, 0, 0,
			 TO_ROOM);
		GET_POS (ch) = POS_SITTING;
		break;
	}
}


ACMD (do_sleep)
{
	switch (GET_POS (ch))
	{
	case POS_STANDING:
	case POS_SITTING:
	case POS_RESTING:
		send_to_char ("Ti metti a dormire.\r\n", ch);
		act ("$n si mette a dormire.", TRUE, ch, 0, 0, TO_ROOM);
		GET_POS (ch) = POS_SLEEPING;
		break;
	case POS_SLEEPING:
		send_to_char ("Stai gia' dormendo.\r\n", ch);
		break;
	case POS_FIGHTING:
		send_to_char ("Dormi mentre combatti?  Sei MATTO?\r\n", ch);
		break;
	default:
		send_to_char ("You stop floating around, and lie down to sleep.\r\n",
					  ch);
		act ("$n stops floating around, and lie down to sleep.", TRUE, ch, 0,
			 0, TO_ROOM);
		GET_POS (ch) = POS_SLEEPING;
		break;
	}
}


ACMD (do_wake)
{
	struct char_data *vict;
	int self = 0;

	one_argument (argument, arg);
	if (*arg)
	{
		if (GET_POS (ch) == POS_SLEEPING)
			send_to_char ("Ti devi prima svegliare forse.\r\n", ch);
		else if ((vict = get_char_vis (ch, arg, NULL, FIND_CHAR_ROOM)) ==
				 NULL)
			send_to_char (NOPERSON, ch);
		else if (vict == ch)
			self = 1;
		else if (AWAKE (vict))
			act ("$E e' gia sveglio.", FALSE, ch, 0, vict, TO_CHAR);
		else if (AFF_FLAGGED (vict, AFF_SLEEP))
			act ("Non puoi svegliare $M!", FALSE, ch, 0, vict, TO_CHAR);
		else if (GET_POS (vict) < POS_SLEEPING)
			act ("$E's in pretty bad shape!", FALSE, ch, 0, vict, TO_CHAR);
		else
		{
			act ("Svegli $M .", FALSE, ch, 0, vict, TO_CHAR);
			act ("Sei stato svegliato da $n.", FALSE, ch, 0, vict,
				 TO_VICT | TO_SLEEP);
			GET_POS (vict) = POS_SITTING;
		}
		if (!self)
			return;
	}
	if (AFF_FLAGGED (ch, AFF_SLEEP))
		send_to_char ("Non puoi svegliarti!\r\n", ch);
	else if (GET_POS (ch) > POS_SLEEPING)
		send_to_char ("Sei gia sveglio...\r\n", ch);
	else
	{
		send_to_char ("Ti svegli e ti siedi.\r\n", ch);
		act ("$n si sveglia.", TRUE, ch, 0, 0, TO_ROOM);
		GET_POS (ch) = POS_SITTING;
	}
}


ACMD (do_follow)
{
	struct char_data *leader;

	one_argument (argument, buf);

	if (*buf)
	{
		if (!(leader = get_char_vis (ch, buf, NULL, FIND_CHAR_ROOM)))
		{
			send_to_char (NOPERSON, ch);
			return;
		}
	}
	else
	{
		send_to_char ("Chi vuoi seguire?\r\n", ch);
		return;
	}

	if (ch->master == leader)
	{
		act ("Stai gia' seguendo $M.", FALSE, ch, 0, leader, TO_CHAR);
		return;
	}
	if (AFF_FLAGGED (ch, AFF_CHARM) && (ch->master))
	{
		act ("But you only feel like following $N!", FALSE, ch, 0, ch->master,
			 TO_CHAR);
	}
	else
	{							/* Not Charmed follow person */
		if (leader == ch)
		{
			if (!ch->master)
			{
				send_to_char ("Stai gia' seguendo te stesso.\r\n", ch);
				return;
			}
			stop_follower (ch);
		}
		else
		{
			if (circle_follow (ch, leader))
			{
				send_to_char
					("Spiacente ma tu segui quello, quello segue quell'altro .....\r\n",
					 ch);
				return;
			}
			if (ch->master)
				stop_follower (ch);
			REMOVE_BIT (AFF_FLAGS (ch), AFF_GROUP);
			add_follower (ch, leader);
		}
	}
}


ACMD (do_doorway)
{
//    room_rnum location;
	struct char_data *target;

	skip_spaces (&argument);

	if (IS_NPC (ch))
		return;

	if (!IS_PSIONIC (ch) && (GET_LEVEL (ch) != LVL_IMPL))
	{
		send_to_char ("Huh?\r\n", ch);
		return;
	}

	if (!GET_SKILL (ch, SPELL_PSI_DOORWAY))
	{
		send_to_char ("Non sei addastrato a farlo!\r\n", ch);
		return;
	}

	one_argument (argument, buf);


	if (!(target = get_char_vis (ch, buf, NULL, FIND_CHAR_WORLD)))
	{
		send_to_char ("Non hai nessuna sensazione su di lui.\r\n", ch);
		return;
	}

	if (GET_LEVEL (target) >= LVL_IMMORT)
	{
		send_to_char ("Non hai il potere di creare un doorway su di lui.\r\n",
					  ch);
		return;
	}

	if (ROOM_FLAGGED
		(IN_ROOM (target),
		 ROOM_NOMAGIC | ROOM_PRIVATE | ROOM_GODROOM | ROOM_HOUSE))
	{
		send_to_char ("Non hai nessuna sensazione su di lui.\r\n", ch);
		return;
	}

	if ((GET_MANA (ch) < 20) && GET_LEVEL (ch) < LVL_IMMORT)
	{
		send_to_char
			("Hai il mal di testa, meglio riposare e riprovare piu' tardi.\n\r",
			 ch);
		return;
	}
	else if (dice (1, 101) > GET_SKILL (ch, SPELL_PSI_DOORWAY))
	{
		send_to_char ("Non riesci a aprirti un portale.\n\r", ch);
		act ("$n sembra svanire per un attimo, e poi ritorna!", FALSE, ch, 0,
			 0, TO_ROOM);
		GET_MANA (ch) -= 10;

		return;
	}
	else
	{
		GET_MANA (ch) -= 20;
		send_to_char
			("Chiudi gli occhi e apri un portale, e poi velocemente entri su di esso.\r\n",
			 ch);
		act
			("$n chiude i suoi occhi e un tremolante portale appare improvvisamente!",
			 FALSE, ch, 0, 0, TO_ROOM);
		act ("$n attraversa dentro il portale e scompare!\n\r", FALSE, ch, 0,
			 0, TO_ROOM);
		char_from_room (ch);
		char_to_room (ch, IN_ROOM (target));
		act ("Un portale appare dietro di te e $n esce fuori!", FALSE, ch, 0,
			 0, TO_ROOM);
		look_at_room (ch, 0);
	}
}


/***************************************************************************
 *   File: ranged.c                                    Part of CircleMUD   *
 *  Usage: Ranged weapons commands                                         *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *                                                                         *
 *  (C) 2001 - Sidewinder                                                  *
 *                                                                         *
 *  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 ***************************************************************************/

#define __RANGED_C__

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
extern void hit (struct char_data *ch, struct char_data *vict, int type);

/*external procedures */
ACMD (do_flee);




/* Local functions */
int skill_roll (struct char_data *ch, int skill_num);
void strike_missile (struct char_data *ch, struct char_data *tch,
					 struct obj_data *missile, int dir, int attacktype);
void miss_missile (struct char_data *ch, struct char_data *tch,
				   struct obj_data *missile, int dir, int attacktype);
void mob_reaction (struct char_data *ch, struct char_data *vict, int dir);
void fire_missile (struct char_data *ch, char arg1[MAX_INPUT_LENGTH],
				   struct obj_data *missile, int pos, int range, int dir);
void shrapnel_damage (struct char_data *ch, struct char_data *vict,
					  int missile_type, int damage);


/* local commands */
ACMD (do_load_weapon);
ACMD (do_autofire);
ACMD (do_fire);
ACMD (do_throw);
//ACMD(do_shoot);  //da usare piu avanti...

/*=======================================================*/

int
skill_roll (struct char_data *ch, int skill_num)
{
	if (number (0, 101) > GET_SKILL (ch, skill_num))
		return FALSE;
	else
		return TRUE;
}


void
strike_missile (struct char_data *ch, struct char_data *tch,
				struct obj_data *missile, int dir, int attacktype)
{
	int dam;
	//extern struct str_app_type str_app[];

	dam = str_app[STRENGTH_APPLY_INDEX (ch)].todam;
	dam += dice (missile->obj_flags.value[1], missile->obj_flags.value[2]);
	dam += GET_DAMROLL (ch);

	damage (ch, tch, dam, attacktype);

	send_to_char ("You hit!\r\n", ch);
	sprintf (buf, "$P flies in from the %s and strikes %s.",
			 dirs[rev_dir[dir]], GET_NAME (tch));
	act (buf, FALSE, tch, 0, missile, TO_ROOM);
	sprintf (buf, "$P flies in from the %s and hits YOU!",
			 dirs[rev_dir[dir]]);
	act (buf, FALSE, tch, 0, missile, TO_CHAR);
	return;
}


void
miss_missile (struct char_data *ch, struct char_data *tch,
			  struct obj_data *missile, int dir, int attacktype)
{
	sprintf (buf, "$P flies in from the %s and hits the ground!",
			 dirs[rev_dir[dir]]);
	act (buf, FALSE, tch, 0, missile, TO_ROOM);
	act (buf, FALSE, tch, 0, missile, TO_CHAR);
	send_to_char ("You missed!\r\n", ch);
}


void
mob_reaction (struct char_data *ch, struct char_data *vict, int dir)
{
	if (IS_NPC (vict) && !FIGHTING (vict) && GET_POS (vict) > POS_STUNNED)
	{

		/* can remember so charge! */
		if (IS_SET (MOB_FLAGS (vict), MOB_MEMORY))
		{
			remember (vict, ch);
			sprintf (buf, "$n urla dal dolore!");
			act (buf, FALSE, vict, 0, 0, TO_ROOM);
			if (GET_POS (vict) == POS_STANDING)
			{

				if (IN_ROOM (ch) != IN_ROOM (vict))
				{
					if (IS_SET (MOB_FLAGS (vict), MOB_WIMPY))
					{
						if (dice (1, 100) < 10)
						{
							act ("$n inciampa mentre cerca di scappare!",
								 FALSE, vict, 0, 0, TO_ROOM);
							GET_POS (vict) = POS_SITTING;
						}
						else
						{
							do_flee (vict, NULL, 0, 0);
						}
					}
					else
					{
						if (!do_simple_move (vict, rev_dir[dir], TRUE))
						{
							act ("$n inciampa mentre cerca di correre!",
								 FALSE, vict, 0, 0, TO_ROOM);
							GET_POS (vict) = POS_SITTING;
						}
						else
						{
							act ("$n corre verso $N e attacca!",
								 FALSE, vict, 0, ch, TO_NOTVICT);
							act ("$n corre verso di te e ti attacca!",
								 FALSE, vict, 0, ch, TO_VICT);
							act ("Corri verso $N e attacchi!",
								 FALSE, vict, 0, ch, TO_CHAR);

							char_from_room (vict);
							char_to_room (vict, ch->in_room);

							act ("$n corre verso $N e attacca!",
								 FALSE, vict, 0, ch, TO_NOTVICT);
							hit (vict, ch, TYPE_UNDEFINED);
						}
					}
				}
			}
			else
			{
				GET_POS (vict) = POS_STANDING;
			}
			/* can't remember so try to run away */
		}
		else
		{
			if (dice (1, 100) < 30)
			{
				do_flee (vict, NULL, 0, 0);
			}
		}
	}
}


void
fire_missile (struct char_data *ch, char arg1[MAX_INPUT_LENGTH],
			  struct obj_data *missile, int pos, int range, int dir)
{
	bool shot = FALSE, found = FALSE;
	int attacktype;
	int room, nextroom, distance;
	struct char_data *vict;

	if (ROOM_FLAGGED (ch->in_room, ROOM_PEACEFUL))
	{
		send_to_char
			("Questa stanza e' pacifica, che piacevole sensazione...\r\n",
			 ch);
		return;
	}

	room = ch->in_room;

	if CAN_GO2
		(room, dir) nextroom = EXIT2 (room, dir)->to_room;
	else
		nextroom = NOWHERE;

	if (GET_OBJ_TYPE (missile) == ITEM_GRENADE)
	{
		sprintf (buf, "Lanci %s verso %s!\r\n",
				 GET_EQ (ch, WEAR_WIELD)->short_description, dirs[dir]);
		send_to_char (buf, ch);

		sprintf (buf, "$n lancia %s in direzione %s.",
				 GET_EQ (ch, WEAR_WIELD)->short_description, dirs[dir]);
		act (buf, FALSE, ch, 0, 0, TO_ROOM);
		sprintf (buf, "%s vola dentro da %s.\r\n",
				 missile->short_description, dirs[rev_dir[dir]]);
		send_to_room (buf, nextroom);
		obj_to_room (unequip_char (ch, pos), nextroom);
		return;
	}

	for (distance = 1; ((nextroom != NOWHERE) && (distance <= range));
		 distance++)
	{

		for (vict = world[nextroom].people; vict; vict = vict->next_in_room)
		{
			if ((isname (arg1, GET_NAME (vict))) && (CAN_SEE (ch, vict)))
			{
				found = TRUE;
				break;
			}
		}

		if (found == 1)
		{

			/* Daniel Houghton's missile modification */
			if (missile && ROOM_FLAGGED (vict->in_room, ROOM_PEACEFUL))
			{
				send_to_char ("Nah.  Lasciali in pace.\r\n", ch);
				return;
			}

			switch (GET_OBJ_TYPE (missile))
			{
			case ITEM_THROW:
				sprintf (buf, "Lanci %s verso %s!\r\n",
						 GET_EQ (ch, WEAR_WIELD)->short_description,
						 dirs[dir]);
				send_to_char (buf, ch);

				sprintf (buf, "$n lancia %s in direzione %s.",
						 GET_EQ (ch, WEAR_WIELD)->short_description,
						 dirs[dir]);
				act (buf, FALSE, ch, 0, 0, TO_ROOM);
				attacktype = SKILL_THROW;
				break;
			default:
				attacktype = TYPE_UNDEFINED;
				break;
			}

			if (attacktype != TYPE_UNDEFINED)
				shot = skill_roll (ch, attacktype);
			else
				shot = FALSE;

			if (shot == TRUE)
			{
				strike_missile (ch, vict, missile, dir, attacktype);
				if ((number (0, 1)) || (attacktype == SKILL_THROW))
					obj_to_char (unequip_char (ch, pos), vict);
				else
					extract_obj (unequip_char (ch, pos));
			}
			else
			{
				/* ok missed so move missile into new room */
				miss_missile (ch, vict, missile, dir, attacktype);
				if ((!number (0, 2)) || (attacktype == SKILL_THROW))
					obj_to_room (unequip_char (ch, pos), vict->in_room);
				else
					extract_obj (unequip_char (ch, pos));
			}

			/* either way mob remembers */
			mob_reaction (ch, vict, dir);
			WAIT_STATE (ch, PULSE_VIOLENCE);
			return;

		}

		room = nextroom;
		if CAN_GO2
			(room, dir) nextroom = EXIT2 (room, dir)->to_room;
		else
			nextroom = NOWHERE;
	}

	send_to_char ("Non trovo il bersaglio!\r\n", ch);
	return;

}


void
tick_grenade (void)
{
	struct obj_data *i, *tobj;
	struct char_data *tch, *next_tch;
	int s, t, dam, door;
	/* grenades are activated by pulling the pin - ie, setting the
	   one of the extra flag bits. After the pin is pulled the grenade
	   starts counting down. once it reaches zero, it explodes. */

	for (i = object_list; i; i = i->next)
	{

		if (OBJ_FLAGGED (i, ITEM_LIVE_GRENADE))
		{
			/* update ticks */
			if (i->obj_flags.value[0] > 0)
				i->obj_flags.value[0] -= 1;
			else
			{
				t = 0;

				/* blow it up */
				/* checks to see if inside containers */
				/* to avoid possible infinite loop add a counter variable */
				s = 0;			/* we'll jump out after 5 containers deep and just delete
								   the grenade */

				for (tobj = i; tobj; tobj = tobj->in_obj)
				{
					s++;
					if (tobj->in_room != NOWHERE)
					{
						t = tobj->in_room;
						break;
					}
					else
					{
						if ((tch = tobj->carried_by))
						{
							t = tch->in_room;
							break;
						}
						else
						{
							if ((tch = tobj->worn_by))
							{
								t = tch->in_room;
								break;
							}
						}
					}
					if (s == 5)
						break;
				}

				/* then truly this grenade is nowhere?!? */
				if (t <= 0)
				{
					sprintf (buf,
							 "SYSERR: serious problem, grenade truly in nowhere\r\n");
					log (buf);
					extract_obj (i);
				}
				else			/* ok we have a room to blow up */
				{
					/* peaceful rooms */
					if (ROOM_FLAGGED (t, ROOM_PEACEFUL))
					{
						sprintf (buf,
								 "Senti %s esplode inoccuamente, con un sonoro POP!\n\r",
								 i->short_description);
						send_to_room (buf, t);
						extract_obj (i);
						return;
					}

					dam = dice (i->obj_flags.value[1], i->obj_flags.value[2]);

					sprintf (buf,
							 "OH NO! - %s esplode!  KABOOOOOOOOOM!!!\r\n",
							 i->short_description);
					send_to_room (buf, t);

					for (door = 0; door < NUM_OF_DIRS; door++)
						if (CAN_GO2 (t, door))
							send_to_room
								("Senti un forte rombo di una esplosione!\r\n",
								 world[t].dir_option[door]->to_room);

					for (tch = world[t].people; tch; tch = next_tch)
					{
						next_tch = tch->next_in_room;

						if (GET_POS (tch) <= POS_DEAD)
						{
							log ("SYSERR: Attempt to damage a corpse.");
							return;	/* -je, 7/7/92 */
						}

						/* You can't damage an immortal! */
						if (IS_NPC (tch) || (GET_LEVEL (tch) < LVL_IMMORT))
						{

							GET_HIT (tch) -= dam;
							act ("$n e' coinvolto nell'esplosione!", TRUE,
								 tch, 0, 0, TO_ROOM);
							act ("Sei coinvolto nell'esplosione!", TRUE, tch,
								 0, 0, TO_CHAR);
							update_pos (tch);

							if (GET_POS (tch) <= POS_DEAD)
							{
								make_corpse (tch);
								death_cry (tch);
								extract_char (tch);
							}
						}

					}
					/* ok hit all the people now get rid of the grenade and 
					   any container it might have been in */

					extract_obj (i);
					return;

				}
			}					/* end else stmt that took care of explosions */
		}						/* end if stmt that took care of live grenades */
	}							/* end loop that searches the mud for objects. */

	return;

}


void
shrapnel_damage (struct char_data *ch, struct char_data *vict,
				 int missile_type, int damage)
{
	struct char_data *tch, *next_tch;
	int door;
	room_vnum t;

	t = IN_ROOM (vict);

	if (ROOM_FLAGGED (t, ROOM_PEACEFUL))
	{
		sprintf (buf,
				 "Senti %s esplode inoccuamente, con un sonoro POP!\n\r",
				 shot_types[missile_type]);
		send_to_room (buf, t);

		return;
	}

	sprintf (buf,
			 "OH NO! - %s esplode!  KABOOOOOOOOOM!!!\r\n",
			 shot_types[missile_type]);
	send_to_room (buf, t);

	for (door = 0; door < NUM_OF_DIRS; door++)
		if (CAN_GO2 (t, door))
			send_to_room
				("Senti un forte rombo di una esplosione!\r\n",
				 world[t].dir_option[door]->to_room);

	for (tch = world[t].people; tch; tch = next_tch)
	{
		next_tch = tch->next_in_room;

		if (GET_POS (tch) <= POS_DEAD)
		{
			log ("SYSERR: Attempt to damage a corpse.");
			return;				/* -je, 7/7/92 */
		}

		/* You can't damage an immortal! */
		if (IS_NPC (tch) || (GET_LEVEL (tch) < LVL_IMMORT))
		{
			if (tch == ch)
			{
				GET_HIT (tch) -= (damage / 4);
			}
			else if (tch != vict)
			{
				GET_HIT (tch) -= (damage / 2);
			}
			else
			{
				GET_HIT (tch) -= damage;
			}
			act ("$n e' coinvolto nell'esplosione!", TRUE,
				 tch, 0, 0, TO_ROOM);
			act ("Sei coinvolto nell'esplosione!", TRUE, tch, 0, 0, TO_CHAR);
			update_pos (tch);

			if (GET_POS (tch) <= POS_DEAD)
			{
				make_corpse (tch);
				death_cry (tch);
				extract_char (tch);
			}
			else
			{
				send_to_char ("Rimani sordito dall'esplosione.\r\n", tch);
				WAIT_STATE (tch, PULSE_VIOLENCE * 3);
			}
		}

	}
	/* ok hit all the people now get rid of the grenade and 
	   any container it might have been in */
	return;

}


ACMD (do_load_weapon)
{
	/* arg1 = fire weapon arg2 = missiles */

	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];

	struct obj_data *missile;
	struct obj_data *weapon = GET_EQ (ch, WEAR_HOLD);


	int num_needed = 0;
	//int num_left = 0; // da togliere?
	int num_ammo = 0;
	two_arguments (argument, arg1, arg2);

	if (!*arg1 || !*arg2)
	{
		send_to_char ("Usa RELOAD <weapon> <ammo>", ch);
		return;
	}
	if (!weapon)
	{
		send_to_char ("Devi tenere l'arma prima di caricare.", ch);
		return;
	}
	if (GET_OBJ_TYPE (weapon) != ITEM_FIREWEAPON)
	{
		send_to_char ("Questo coso non usa munizioni!", ch);
		return;
	}

	missile = get_obj_in_list_vis (ch, arg2, NULL, ch->carrying);
	if (!missile)
	{
		send_to_char ("Cosa cerchi di provare come munizioni?", ch);
		return;
	}
	if (GET_OBJ_TYPE (missile) != ITEM_MISSILE)
	{
		send_to_char ("Non e' una munizione!", ch);
		return;
	}
	if (GET_OBJ_VAL (missile, 0) != GET_OBJ_VAL (weapon, 0))
	{
		send_to_char ("Queste munizioni non vanno bene con questa arma!", ch);
		return;
	}
	num_needed = GET_OBJ_VAL (weapon, 2) - GET_OBJ_VAL (weapon, 3);
	if (!num_needed)
	{
		send_to_char ("E' gia carica.", ch);
		return;
	}
	num_ammo = GET_OBJ_VAL (missile, 3);
	if (!num_ammo)
	{
		/* shouldn't really get here.. this one's for Murphy :) */
		send_to_char ("E' vuoto!", ch);
		extract_obj (missile);
		return;
	}
	if (num_ammo <= num_needed)
	{
		GET_OBJ_VAL (weapon, 3) += num_ammo;
		extract_obj (missile);
	}
	else
	{
		GET_OBJ_VAL (weapon, 3) += num_needed;
		GET_OBJ_VAL (missile, 3) -= num_needed;
	}
	act ("Carichi $p", FALSE, ch, weapon, 0, TO_CHAR);
	act ("$n carica $p", FALSE, ch, weapon, 0, TO_ROOM);
}


ACMD (do_autofire)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	struct obj_data *weapon;

	half_chop (argument, arg1, arg2);

	if (!*arg1 || !*arg2)
	{
		send_to_char ("Usa: AUTOFIRE <arma> <on|off>", ch);
		return;
	}

	weapon = GET_EQ (ch, WEAR_HOLD);

	if (!weapon)
	{
		sprintf (buf2, "Non stai tenendo %s!\r\n", arg1);
		send_to_char (buf2, ch);
		return;
	}

	if ((GET_OBJ_TYPE (weapon) != ITEM_FIREWEAPON))
	{
		send_to_char
			("Quello che stai tenendo in mano non e' un arma da fuoco!\r\n",
			 ch);
		return;
	}

	if (!IS_AUTO_FIREWEAPON (weapon))
	{
		send_to_char ("Non e' una arma automatica!\r\n", ch);
		return;
	}

	if (!strcmp ("on", arg2) || !strcmp ("off", arg2))
	{
		if (!strcmp ("off", arg2))
		{
			GET_OBJ_VAL (weapon, 1) = 0;
			send_to_char ("L'autofire della tua arma e' disabilitato!\r\n",
						  ch);
			return;
		}
		else
		{
			GET_OBJ_VAL (weapon, 1) = 1;
			send_to_char ("L'autofire della tua arma e' abilitato!\r\n", ch);
			return;
		}
	}
	else
	{
		send_to_char ("Usa: AUTOFIRE <arma> <on|off>", ch);
		return;
	}

}


ACMD (do_fire)
{								/* fire <weapon> <mob> [direction] */

	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	char arg3[MAX_INPUT_LENGTH];
	char argt[MAX_INPUT_LENGTH];

	struct obj_data *weapon;
	struct char_data *vict;

	int target_room = -1;
	int source_room = -1;
	int far_room = 0;
	int num1 = 0;
	int num2 = 0;
	int roll = 0;
	int dmg = 0;
	int i = 0;
	int ripple = 1;
	//int bits = 0; //da togliere

	half_chop (argument, arg1, argt);
	half_chop (argt, arg2, arg3);

	/* arg1 = weapon arg2= victim arg3= direction */

	if (!*arg1 || !*arg2)
	{
		send_to_char ("Usa: FIRE <arma> <vittima> [direzione]", ch);
		return;
	}

	if (ROOM_FLAGGED (ch->in_room, ROOM_PEACEFUL))
	{
		send_to_char
			("Questa stanza e' pacifica, che piacevole sensazione...\r\n",
			 ch);
		return;
	}

	if (!GET_SKILL (ch, SKILL_SHOOT))
	{
		send_to_char ("Non sei addestrato ad usare questa arma.\r\n", ch);
		return;
	}

	weapon = GET_EQ (ch, WEAR_HOLD);

	if (!weapon)
	{
		sprintf (buf2, "Non stai tenendo %s!\r\n", arg1);
		send_to_char (buf2, ch);
		return;
	}

	if ((GET_OBJ_TYPE (weapon) != ITEM_FIREWEAPON))
	{
		send_to_char ("Non puoi sparare con questo affare!", ch);
		return;
	}

	if (GET_OBJ_VAL (weapon, 3) <= 0)
	{
		send_to_char ("E' scarica.", ch);
		return;
	}

	if (*arg3)
	{							/* attempt to fire direction x */
		far_room = -1;
		switch (arg3[0])
		{
		case 'n':
		case 'N':
			far_room = 0;
			break;

		case 'e':
		case 'E':
			far_room = 1;
			break;

		case 's':
		case 'S':
			far_room = 2;
			break;

		case 'w':
		case 'W':
			far_room = 3;
			break;

		case 'u':
		case 'U':
			far_room = 4;
			break;

		case 'd':
		case 'D':
			far_room = 5;
			break;
		}

		if (far_room == -1)
		{
			send_to_char
				("Direzione invalida. Le direzioni valide sono N, E, S, W, U, D.",
				 ch);
			return;
		}

		if (CAN_GO (ch, far_room))
		{
			target_room = world[ch->in_room].dir_option[far_room]->to_room;
		}
		if (target_room == -1)
		{
			send_to_char ("Non esiste nulla da sparare in questa direzione.",
						  ch);
			return;
		}

		source_room = IN_ROOM (ch);
		ch->in_room = target_room;

		vict = get_char_room_vis (ch, arg2, 0);

		ch->in_room = source_room;

		if (!vict)
		{
			sprintf (buf, "Non vedo %s in quella parte.", arg2);
			send_to_char (buf, ch);
			return;
		}
	}
	else
	{

		target_room = IN_ROOM (ch);
		vict = get_char_room_vis (ch, arg2, 0);
		if ((!*arg2) || (!vict))
		{
			act ("Chi vuoi cercare di sparare a $p?", FALSE, ch, weapon, 0,
				 TO_CHAR);
			return;
		}
	}


	/* ok.. got a loaded weapon, the victim is identified */

	/* Sto usando una arma automatica? */
	if (GET_OBJ_VAL (weapon, 1))
	{
		if (IS_AUTO_FIREWEAPON (weapon))
		{
			ripple = 3;
			strcpy (argt, "una raffica");
		}
		else
		{
			ripple = 1;
			strcpy (argt, shot_types[GET_OBJ_VAL (weapon, 0)]);
		}
	}
	else
	{
		ripple = 1;
		strcpy (argt, shot_types[GET_OBJ_VAL (weapon, 0)]);
	}

	num1 = GET_DEX (ch) - GET_DEX (vict);
	if (num1 > 25)
		num1 = 25;
	if (num1 < -25)
		num1 = -25;
	num2 = GET_SKILL (ch, SKILL_SHOOT);
	num2 = (int) (num2 * .75);
	if (num2 < 1)
		num2 = 1;


	//sprintbit((long) GET_OBJ_VAL(weapon, 0), shot_types, argt);

	for (i = 0; i < ripple; i++)
	{
		if (GET_OBJ_VAL (weapon, 1))
		{
			roll = number (1, 101) + 10;	/* -10%  se usi un arma automatica */
		}
		else
		{
			roll = number (1, 101);
		}
		if (GET_OBJ_VAL (weapon, 3) <= 0)
		{
			send_to_char ("E' scarica.", ch);
			break;
		}
		else if ((num1 + num2) >= roll)
		{

			/* we hit
			   1) print message in room.
			   2) print rush message for mob in mob's room.
			   3) trans mob
			   4) set mob to fighting ch
			 */
			sprintf (buf, "Colpisci $N con %s sparata da $p!", argt);
			act (buf, FALSE, ch, weapon, vict, TO_CHAR);
			sprintf (buf, "$n ti colpisce con %s sparata da $p!", argt);
			act (buf, FALSE, ch, weapon, vict, TO_VICT);
			sprintf (buf, "$n colpisce $N con %s sparata da $p!", argt);
			act (buf, FALSE, ch, weapon, vict, TO_NOTVICT);
			GET_OBJ_VAL (weapon, 3) -= 1;

			if (IS_WARRIOR (ch))
			{
				dmg =
					dice (shot_damage[GET_OBJ_VAL (weapon, 0)][0],
						  shot_damage[GET_OBJ_VAL (weapon, 0)][1]) +
					(GET_LEVEL (ch) / 10);
			}
			else
			{
				dmg =
					dice (shot_damage[GET_OBJ_VAL (weapon, 0)][0],
						  shot_damage[GET_OBJ_VAL (weapon, 0)][1]) +
					(GET_LEVEL (ch) / 12);
			}

			if (GET_OBJ_VAL (weapon, 1))
			{
				dmg -= (GET_LEVEL (ch) / 10);
			}

			if (GET_OBJ_VAL (weapon, 0) == MISSILE_ROCKET
				|| GET_OBJ_VAL (weapon, 0) == MISSILE_GRENADE)
			{
				dmg =
					dice (shot_damage[GET_OBJ_VAL (weapon, 0)][0],
						  shot_damage[GET_OBJ_VAL (weapon, 0)][1]) +
					(GET_LEVEL (ch) / 10);
				shrapnel_damage (ch, vict, GET_OBJ_VAL (weapon, 0), dmg);
			}
			else
			{
				damage (ch, vict, dmg, TYPE_UNDEFINED);
			}

			if (IN_ROOM (ch) != IN_ROOM (vict))
			{
				act ("$n corre verso $N e attacca!",
					 FALSE, vict, 0, ch, TO_NOTVICT);
				act ("$n corre verso di te e ti attacca!",
					 FALSE, vict, 0, ch, TO_VICT);
				act ("Corri verso $N e attacchi!",
					 FALSE, vict, 0, ch, TO_CHAR);

				char_from_room (vict);
				char_to_room (vict, ch->in_room);

				act ("$n corre verso $N e attacca!",
					 FALSE, vict, 0, ch, TO_NOTVICT);
				hit (vict, ch, TYPE_UNDEFINED);
			}
		}
		else
		{

			/* we missed
			   1) print miss message
			 */
			sprintf (buf, "Spari %s a $N e manchi!", argt);
			act (buf, FALSE, ch, weapon, vict, TO_CHAR);
			sprintf (buf, "$n ti spara %s e manca!", argt);
			act (buf, FALSE, ch, weapon, vict, TO_VICT);
			sprintf (buf, "$n spara %s a $N e manca!", argt);
			act (buf, FALSE, ch, weapon, vict, TO_NOTVICT);
			GET_OBJ_VAL (weapon, 3) -= 1;

			if (GET_OBJ_VAL (weapon, 0) == MISSILE_ROCKET
				|| GET_OBJ_VAL (weapon, 0) == MISSILE_GRENADE)
			{
				dmg =
					dice (shot_damage[GET_OBJ_VAL (weapon, 0)][0],
						  shot_damage[GET_OBJ_VAL (weapon, 0)][1]) / 2;
				shrapnel_damage (ch, vict, GET_OBJ_VAL (weapon, 0), dmg);
			}
		}
	}

	if (!FIGHTING (ch))
	{
		return;
	}
	// in ogni caso il mob reagisce
	mob_reaction (ch, vict, far_room);
	WAIT_STATE (ch, PULSE_VIOLENCE);
}



ACMD (do_throw)
{

//  sample format: throw monkey east
//    this would throw a throwable or grenade object wielded
//    into the room 1 east of the pc's current room. The chance
//    to hit the monkey would be calculated based on the pc's skill.
//    if the wielded object is a grenade then it does not 'hit' for
//    damage, it is merely dropped into the room. (the timer is set
//    with the 'pull pin' command.) 

	struct obj_data *missile;
	int dir, range = 1;
	char arg2[MAX_INPUT_LENGTH];
	char arg1[MAX_INPUT_LENGTH];
	two_arguments (argument, arg1, arg2);

// / only two types of throwing objects: 
//    ITEM_THROW - knives, stones, etc
//    ITEM_GRENADE - calls tick_grenades.c . 

	if (!(GET_EQ (ch, WEAR_WIELD)))
	{
		send_to_char ("Devi impugnare qualcosa prima!\r\n", ch);
		return;
	}

	missile = GET_EQ (ch, WEAR_WIELD);

	if (!((GET_OBJ_TYPE (missile) == ITEM_THROW) ||
		  (GET_OBJ_TYPE (missile) == ITEM_GRENADE)))
	{
		send_to_char ("Devi impugnare un arma da lancio, prima!\r\n", ch);
		return;
	}

	if (GET_OBJ_TYPE (missile) == ITEM_GRENADE)
	{
		if (!*arg1)
		{
			send_to_char ("Usa: throw <direzione>\r\n", ch);
			return;
		}
		if ((dir = search_block (arg1, dirs, FALSE)) < 0)
		{
			send_to_char ("Quale direzione?\r\n", ch);
			return;
		}
	}
	else
	{

		two_arguments (argument, arg1, arg2);

		if (!*arg1 || !*arg2)
		{
			send_to_char ("Usa: throw <qualcuno> <direzione>\r\n", ch);
			return;
		}

		// arg2 must be a direction 

		if ((dir = search_block (arg2, dirs, FALSE)) < 0)
		{
			send_to_char ("Quale direzione?\r\n", ch);
			return;
		}
	}

	// make sure we can go in the direction throwing. 
	if (!CAN_GO (ch, dir))
	{
		send_to_char ("Qualcosa blocca la strada!\r\n", ch);
		return;
	}

	fire_missile (ch, arg1, missile, WEAR_WIELD, range, dir);

}

//  Da non usare per il momento... perche e' adatta solo per le  armi tipo arco e frecce

/*
ACMD(do_shoot)
{ 
   struct obj_data *missile;
   char arg2[MAX_INPUT_LENGTH];
   char arg1[MAX_INPUT_LENGTH];
   int dir, range;
 
   if (!GET_EQ(ch, WEAR_WIELD)) { 
     send_to_char("You aren't wielding a shooting weapon!\r\n", ch);
     return;
   }
 
   if (!GET_EQ(ch, WEAR_HOLD)) { 
     send_to_char("You need to be holding a missile!\r\n", ch);
     return;
   }
 
   missile = GET_EQ(ch, WEAR_HOLD);
 
   if ((GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_SLING) &&
       (GET_OBJ_TYPE(missile) == ITEM_ROCK))
        range = GET_EQ(ch, WEAR_WIELD)->obj_flags.value[0];
   else 
     if ((GET_OBJ_TYPE(missile) == ITEM_ARROW) &&
         (GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_BOW))
          range = GET_EQ(ch, WEAR_WIELD)->obj_flags.value[0];
   else 
     if ((GET_OBJ_TYPE(missile) == ITEM_BOLT) &&
         (GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) == ITEM_CROSSBOW))
          range = GET_EQ(ch, WEAR_WIELD)->obj_flags.value[0];
 
   else {
     send_to_char("You should wield a missile weapon and hold a missile!\r\n", ch);
     return;
   }
 
   two_arguments(argument, arg1, arg2);
 
   if (!*arg1 || !*arg2) {
     send_to_char("You should try: shoot <someone> <direction>\r\n", ch);
     return;
   }
 
   if (IS_DARK(ch->in_room)) { 
     send_to_char("You can't see that far.\r\n", ch);
     return;
   } 
 
   if ((dir = search_block(arg2, dirs, FALSE)) < 0) {
     send_to_char("What direction?\r\n", ch);
     return;
   }
 
   if (!CAN_GO(ch, dir)) { 
     send_to_char("Something blocks the way!\r\n", ch);
     return;
   }
 
   if (range > 3) 
     range = 3;
   if (range < 1)
     range = 1;
 
   fire_missile(ch, arg1, missile, WEAR_HOLD, range, dir);
 
}
*/

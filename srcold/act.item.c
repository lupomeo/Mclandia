
/* ************************************************************************
*   File: act.item.c                                    Part of CircleMUD *
*  Usage: object handling routines -- get/drop and container handling     *
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
#include "constants.h"


/* extern variables */
extern room_rnum donation_room_1;
#if 0
extern room_rnum donation_room_2;	/* uncomment if needed! */
extern room_rnum donation_room_3;	/* uncomment if needed! */
#endif
extern struct obj_data *obj_proto;
extern struct room_data *world;

/* extern funct */
extern int check_get_trap (struct char_data *ch, struct obj_data *obj);
extern int check_use_fireweapon (struct char_data *ch);

/* local functions */
int can_take_obj (struct char_data *ch, struct obj_data *obj);
void get_check_money (struct char_data *ch, struct obj_data *obj);
int perform_get_from_room (struct char_data *ch, struct obj_data *obj);
void get_from_room (struct char_data *ch, char *arg, int amount);
void perform_give_gold (struct char_data *ch, struct char_data *vict,
						int amount);
void perform_give (struct char_data *ch, struct char_data *vict,
				   struct obj_data *obj);
int perform_drop (struct char_data *ch, struct obj_data *obj, byte mode,
				  const char *sname, room_rnum RDR);
void perform_drop_gold (struct char_data *ch, int amount, byte mode,
						room_rnum RDR);
struct char_data *give_find_vict (struct char_data *ch, char *arg);
void weight_change_object (struct obj_data *obj, int weight);
void perform_put (struct char_data *ch, struct obj_data *obj,
				  struct obj_data *cont);
void name_from_drinkcon (struct obj_data *obj);
void get_from_container (struct char_data *ch, struct obj_data *cont,
						 char *arg, int mode, int amount);
void name_to_drinkcon (struct obj_data *obj, int type);
void wear_message (struct char_data *ch, struct obj_data *obj, int where);
void perform_wear (struct char_data *ch, struct obj_data *obj, int where);
int find_eq_pos (struct char_data *ch, struct obj_data *obj, char *arg);
void perform_get_from_container (struct char_data *ch, struct obj_data *obj,
								 struct obj_data *cont, int mode);
void perform_remove (struct char_data *ch, int pos);
ACMD (do_remove);
ACMD (do_put);
ACMD (do_get);
ACMD (do_drop);
ACMD (do_give);
ACMD (do_drink);
ACMD (do_eat);
ACMD (do_pour);
ACMD (do_wear);
ACMD (do_wield);
ACMD (do_grab);
ACMD (do_pull);
ACMD (do_disarm);


void
perform_put (struct char_data *ch, struct obj_data *obj,
			 struct obj_data *cont)
{
	if (GET_OBJ_WEIGHT (cont) + GET_OBJ_WEIGHT (obj) > GET_OBJ_VAL (cont, 0))
		act ("$p non entra in $P.", FALSE, ch, obj, cont, TO_CHAR);
	else
	{
		obj_from_char (obj);
		obj_to_obj (obj, cont);

		act ("$n mette $p in $P.", TRUE, ch, obj, cont, TO_ROOM);

		/* Yes, I realize this is strange until we have auto-equip on rent. -gg */
		if (IS_OBJ_STAT (obj, ITEM_NODROP)
			&& !IS_OBJ_STAT (cont, ITEM_NODROP))
		{
			SET_BIT (GET_OBJ_EXTRA (cont), ITEM_NODROP);
			act ("Hai una strana sensazione appena metti $p in $P.", FALSE,
				 ch, obj, cont, TO_CHAR);
		}
		else
			act ("Metti $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
	}
}


/* The following put modes are supported by the code below:

	1) put <object> <container>
	2) put all.<object> <container>
	3) put all <container>

	<container> must be in inventory or on ground.
	all objects to be put into container must be in inventory.
*/

ACMD (do_put)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	char arg3[MAX_INPUT_LENGTH];
	struct obj_data *obj, *next_obj, *cont;
	struct char_data *tmp_char;
	int obj_dotmode, cont_dotmode, found = 0, howmany = 1;
	char *theobj, *thecont;

	argument = two_arguments (argument, arg1, arg2);
	one_argument (argument, arg3);

	if (*arg3 && is_number (arg1))
	{
		howmany = atoi (arg1);
		theobj = arg2;
		thecont = arg3;
	}
	else
	{
		theobj = arg1;
		thecont = arg2;
	}
	obj_dotmode = find_all_dots (theobj);
	cont_dotmode = find_all_dots (thecont);

	if (!*theobj)
		send_to_char ("Metti cosa in cosa?\r\n", ch);
	else if (cont_dotmode != FIND_INDIV)
		send_to_char
			("Puoi mettere cose in un solo contenitore alla volta.\r\n", ch);
	else if (!*thecont)
	{
		sprintf (buf, "Cosa vuoi mettere in %s ?\r\n",
				 ((obj_dotmode == FIND_INDIV) ? "it" : "them"));
		send_to_char (buf, ch);
	}
	else
	{
		generic_find (thecont, FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &tmp_char,
					  &cont);
		if (!cont)
		{
			sprintf (buf, "Non vedi %s %s qui.\r\n", AN (thecont), thecont);
			send_to_char (buf, ch);
		}
		else if (GET_OBJ_TYPE (cont) != ITEM_CONTAINER)
			act ("$p non e' un contenitore.", FALSE, ch, cont, 0, TO_CHAR);
		else if (OBJVAL_FLAGGED (cont, CONT_CLOSED))
			send_to_char ("Faresti meglio ad aprirlo prima!\r\n", ch);
		else
		{
			if (obj_dotmode == FIND_INDIV)
			{					/* put <obj> <container> */
				if (!
					(obj =
					 get_obj_in_list_vis (ch, theobj, NULL, ch->carrying)))
				{
					sprintf (buf, "Non hai %s %s.\r\n", AN (theobj), theobj);
					send_to_char (buf, ch);
				}
				else if (obj == cont)
					send_to_char
						("Provi a metterlo in se stesso ma fallisci.\r\n",
						 ch);
				else
				{
					struct obj_data *next_obj;
					while (obj && howmany--)
					{
						next_obj = obj->next_content;
						perform_put (ch, obj, cont);
						obj =
							get_obj_in_list_vis (ch, theobj, NULL, next_obj);
					}
				}
			}
			else
			{
				for (obj = ch->carrying; obj; obj = next_obj)
				{
					next_obj = obj->next_content;
					if (obj != cont && CAN_SEE_OBJ (ch, obj) &&
						(obj_dotmode == FIND_ALL
						 || isname (theobj, obj->name)))
					{
						found = 1;
						perform_put (ch, obj, cont);
					}
				}
				if (!found)
				{
					if (obj_dotmode == FIND_ALL)
						send_to_char
							("Non sembri avere nulla in cui metterlo.\r\n",
							 ch);
					else
					{
						sprintf (buf, "Non sembri avere alcun %s .\r\n",
								 theobj);
						send_to_char (buf, ch);
					}
				}
			}
		}
	}
}



int
can_take_obj (struct char_data *ch, struct obj_data *obj)
{
	if (IS_CARRYING_N (ch) >= CAN_CARRY_N (ch))
	{
		act ("$p: non puoi trasportare cosi' tanti oggetti.", FALSE, ch, obj,
			 0, TO_CHAR);
		return (0);
	}
	else if ((IS_CARRYING_W (ch) + GET_OBJ_WEIGHT (obj)) > CAN_CARRY_W (ch))
	{
		act ("$p: non puoi portare tutto questo peso.", FALSE, ch, obj, 0,
			 TO_CHAR);
		return (0);
	}
	else if (!(CAN_WEAR (obj, ITEM_WEAR_TAKE)))
	{
		act ("$p: non puoi prenderlo!", FALSE, ch, obj, 0, TO_CHAR);
		return (0);
	}
	return (1);
}


void
get_check_money (struct char_data *ch, struct obj_data *obj)
{
	int value = GET_OBJ_VAL (obj, 0);

	if (GET_OBJ_TYPE (obj) != ITEM_MONEY || value <= 0)
		return;

	extract_obj (obj);

	GET_GOLD (ch) += value;

	if (value == 1)
		send_to_char ("C'era 1 moneta d'oro.\r\n", ch);
	else
	{
		sprintf (buf, "C'erano %d monete d'oro.\r\n", value);
		send_to_char (buf, ch);
	}
}


void
perform_get_from_container (struct char_data *ch, struct obj_data *obj,
							struct obj_data *cont, int mode)
{
	if (mode == FIND_OBJ_INV || can_take_obj (ch, obj))
	{
		if (IS_CARRYING_N (ch) >= CAN_CARRY_N (ch))
			act ("$p: non puoi prendere in mano altri oggetti.", FALSE, ch,
				 obj, 0, TO_CHAR);
		else
		{
			obj_from_obj (obj);
			obj_to_char (obj, ch);
			act ("Prendi $p da $P.", FALSE, ch, obj, cont, TO_CHAR);
			act ("$n prende $p da $P.", TRUE, ch, obj, cont, TO_ROOM);
			get_check_money (ch, obj);
		}
	}
}


void
get_from_container (struct char_data *ch, struct obj_data *cont,
					char *arg, int mode, int howmany)
{
	struct obj_data *obj, *next_obj;
	int obj_dotmode, found = 0;

	obj_dotmode = find_all_dots (arg);

	if (OBJVAL_FLAGGED (cont, CONT_CLOSED))
		act ("$p e' chiusa.", FALSE, ch, cont, 0, TO_CHAR);
	else if (obj_dotmode == FIND_INDIV)
	{
		if (!(obj = get_obj_in_list_vis (ch, arg, NULL, cont->contains)))
		{
			sprintf (buf, "Non sembra esserci %s %s in $p.", AN (arg), arg);
			act (buf, FALSE, ch, cont, 0, TO_CHAR);
		}
		else
		{
			struct obj_data *obj_next;
			while (obj && howmany--)
			{
				obj_next = obj->next_content;
				if (check_get_trap (ch, obj))
				{
					return;
				}
				perform_get_from_container (ch, obj, cont, mode);
				obj = get_obj_in_list_vis (ch, arg, NULL, obj_next);
			}
		}
	}
	else
	{
		if (obj_dotmode == FIND_ALLDOT && !*arg)
		{
			send_to_char ("Prendi tutto di cosa?\r\n", ch);
			return;
		}
		for (obj = cont->contains; obj; obj = next_obj)
		{
			next_obj = obj->next_content;
			if (CAN_SEE_OBJ (ch, obj) &&
				(obj_dotmode == FIND_ALL || isname (arg, obj->name)))
			{
				if (check_get_trap (ch, obj))
				{
					return;
				}
				found = 1;
				perform_get_from_container (ch, obj, cont, mode);
			}
		}
		if (!found)
		{
			if (obj_dotmode == FIND_ALL)
			{
				act ("$p sembra essere vuoto.", FALSE, ch, cont, 0, TO_CHAR);
			}
			else
			{
				sprintf (buf, "Non trovi alcun %s in $p.", arg);
				act (buf, FALSE, ch, cont, 0, TO_CHAR);
			}
		}
	}
}


int
perform_get_from_room (struct char_data *ch, struct obj_data *obj)
{
	if (can_take_obj (ch, obj))
	{
		if (check_get_trap (ch, obj))
		{
			return (0);
		}
		obj_from_room (obj);
		obj_to_char (obj, ch);
		act ("Prendi $p.", FALSE, ch, obj, 0, TO_CHAR);
		act ("$n prende $p.", TRUE, ch, obj, 0, TO_ROOM);
		get_check_money (ch, obj);
		return (1);
	}
	return (0);
}


void
get_from_room (struct char_data *ch, char *arg, int howmany)
{
	struct obj_data *obj, *next_obj;
	int dotmode, found = 0;

	dotmode = find_all_dots (arg);

	if (dotmode == FIND_INDIV)
	{
		if (!
			(obj =
			 get_obj_in_list_vis (ch, arg, NULL,
								  world[ch->in_room].contents)))
		{
			sprintf (buf, "Non vedi %s %s qui.\r\n", AN (arg), arg);
			send_to_char (buf, ch);
		}
		else
		{
			struct obj_data *obj_next;
			while (obj && howmany--)
			{
				obj_next = obj->next_content;
				perform_get_from_room (ch, obj);
				obj = get_obj_in_list_vis (ch, arg, NULL, obj_next);
			}
		}
	}
	else
	{
		if (dotmode == FIND_ALLDOT && !*arg)
		{
			send_to_char ("Prendi tutto di cosa?\r\n", ch);
			return;
		}
		for (obj = world[ch->in_room].contents; obj; obj = next_obj)
		{
			next_obj = obj->next_content;
			if (CAN_SEE_OBJ (ch, obj) &&
				(dotmode == FIND_ALL || isname (arg, obj->name)))
			{
				found = 1;
				perform_get_from_room (ch, obj);
			}
		}
		if (!found)
		{
			if (dotmode == FIND_ALL)
			{
				send_to_char ("Non c'e' nulla qui.\r\n", ch);
			}
			else
			{
				sprintf (buf, "Non c'e' alcun %s qui.\r\n", arg);
				send_to_char (buf, ch);
			}
		}
	}
}



ACMD (do_get)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	char arg3[MAX_INPUT_LENGTH];

	int cont_dotmode, found = 0, mode;
	struct obj_data *cont;
	struct char_data *tmp_char;

	argument = two_arguments (argument, arg1, arg2);
	one_argument (argument, arg3);

	if (IS_CARRYING_N (ch) >= CAN_CARRY_N (ch))
		send_to_char ("HAi le braccia piene!\r\n", ch);
	else if (!*arg1)
		send_to_char ("Prendi cosa?\r\n", ch);
	else if (!*arg2)
		get_from_room (ch, arg1, 1);
	else if (is_number (arg1) && !*arg3)
		get_from_room (ch, arg2, atoi (arg1));
	else
	{
		int amount = 1;
		if (is_number (arg1))
		{
			amount = atoi (arg1);
			strcpy (arg1, arg2);
			strcpy (arg2, arg3);
		}
		cont_dotmode = find_all_dots (arg2);
		if (cont_dotmode == FIND_INDIV)
		{
			mode =
				generic_find (arg2, FIND_OBJ_INV | FIND_OBJ_ROOM, ch,
							  &tmp_char, &cont);
			if (!cont)
			{
				sprintf (buf, "Non hai %s %s.\r\n", AN (arg2), arg2);
				send_to_char (buf, ch);
			}
			else if (GET_OBJ_TYPE (cont) != ITEM_CONTAINER)
				act ("$p non e' un contenitore.", FALSE, ch, cont, 0,
					 TO_CHAR);
			else
				get_from_container (ch, cont, arg1, mode, amount);
		}
		else
		{
			if (cont_dotmode == FIND_ALLDOT && !*arg2)
			{
				send_to_char ("Prendi tutto da cosa?\r\n", ch);
				return;
			}
			for (cont = ch->carrying; cont; cont = cont->next_content)
			{
				if (CAN_SEE_OBJ (ch, cont) &&
					(cont_dotmode == FIND_ALL || isname (arg2, cont->name)))
				{

					if (GET_OBJ_TYPE (cont) == ITEM_CONTAINER)
					{
						found = 1;
						get_from_container (ch, cont, arg1, FIND_OBJ_INV,
											amount);
					}
					else if (cont_dotmode == FIND_ALLDOT)
					{
						found = 1;
						act ("$p non e' un contenitore.", FALSE, ch, cont, 0,
							 TO_CHAR);
					}
				}
			}
			for (cont = world[ch->in_room].contents; cont;
				 cont = cont->next_content)
			{
				if (CAN_SEE_OBJ (ch, cont) &&
					(cont_dotmode == FIND_ALL || isname (arg2, cont->name)))
				{
					if (GET_OBJ_TYPE (cont) == ITEM_CONTAINER)
					{
						get_from_container (ch, cont, arg1, FIND_OBJ_ROOM,
											amount);
						found = 1;
					}
					else if (cont_dotmode == FIND_ALLDOT)
					{
						act ("$p non e' un contenitore.", FALSE, ch, cont, 0,
							 TO_CHAR);
						found = 1;
					}
				}
			}
			if (!found)
			{
				if (cont_dotmode == FIND_ALL)
					send_to_char
						("You can't seem to find any containers.\r\n", ch);
				else
				{
					sprintf (buf, "You can't seem to find any %ss here.\r\n",
							 arg2);
					send_to_char (buf, ch);
				}
			}
		}
	}
}


void
perform_drop_gold (struct char_data *ch, int amount, byte mode, room_rnum RDR)
{
	struct obj_data *obj;

	if (amount <= 0)
		send_to_char ("Heh heh heh.. stiamo scherzando oggi, eh?\r\n", ch);
	else if (GET_GOLD (ch) < amount)
		send_to_char ("Non hai tutti i soldi!\r\n", ch);
	else
	{
		if (mode != SCMD_JUNK)
		{
			WAIT_STATE (ch, PULSE_VIOLENCE);	/* to prevent coin-bombing */
			obj = create_money (amount);
			if (mode == SCMD_DONATE)
			{
				send_to_char
					("Tiri in aria delle monete e queste spariscono in una nuvoletta di fumo!\r\n",
					 ch);
				act
					("$n tira in aria delle monete e queste spariscono in una nuvoletta di fumo!",
					 FALSE, ch, 0, 0, TO_ROOM);
				obj_to_room (obj, RDR);
				act ("$p appare in una nuvola di fumo arancione!", 0, 0, obj,
					 0, TO_ROOM);
			}
			else
			{
				sprintf (buf, "Posi %s\r\n.", money_desc (amount));
				send_to_char (buf, ch);
				sprintf (buf, "$n posa %s.", money_desc (amount));
				act (buf, TRUE, ch, 0, 0, TO_ROOM);
				obj_to_room (obj, ch->in_room);
			}
		}
		else
		{
			sprintf (buf,
					 "$n posa %s che spariscono in una nuvoletta di fumo!",
					 money_desc (amount));
			act (buf, FALSE, ch, 0, 0, TO_ROOM);
			send_to_char
				("Posi delle monete che spariscono in una nuvoletta di fumo!\r\n",
				 ch);
		}
		GET_GOLD (ch) -= amount;
	}
}


#define VANISH(mode) ((mode == SCMD_DONATE || mode == SCMD_JUNK) ? \
		      "  Sparisce in una nuvoletta di fumo!" : "")

int
perform_drop (struct char_data *ch, struct obj_data *obj,
			  byte mode, const char *sname, room_rnum RDR)
{
	int value;

	if (IS_OBJ_STAT (obj, ITEM_NODROP))
	{
		if (mode == SCMD_DROP)
		{
			act ("Non puoi posare $p, e' MALEDETTO!", FALSE, ch, obj, 0,
				 TO_CHAR);
		}
		else
		{
			sprintf (buf, "Can't %s $p, is CURSED!", sname);
			act (buf, FALSE, ch, obj, 0, TO_CHAR);
		}
		return (0);
	}
	if (mode == SCMD_DROP)
	{
		act ("Posi $p", FALSE, ch, obj, 0, TO_CHAR);
		act ("$n posa $p", TRUE, ch, obj, 0, TO_ROOM);
		obj_from_char (obj);
	}
	else
	{
		sprintf (buf, "You %s $p.%s", sname, VANISH (mode));
		act (buf, FALSE, ch, obj, 0, TO_CHAR);
		sprintf (buf, "$n %ss $p.%s", sname, VANISH (mode));
		act (buf, TRUE, ch, obj, 0, TO_ROOM);
		obj_from_char (obj);
	}


	if ((mode == SCMD_DONATE) && IS_OBJ_STAT (obj, ITEM_NODONATE))
		mode = SCMD_JUNK;

	switch (mode)
	{
	case SCMD_DROP:
		obj_to_room (obj, ch->in_room);
		return (0);
	case SCMD_DONATE:
		obj_to_room (obj, RDR);
		act ("$p si materializza improvvisamente nella stanza!", FALSE,
			 0, obj, 0, TO_ROOM);
		return (0);
	case SCMD_JUNK:
		value = MAX (1, MIN (200, GET_OBJ_COST (obj) / 16));
		extract_obj (obj);
		return (value);
	default:
		log ("SYSERR: Incorrect argument %d passed to perform_drop.", mode);
		break;
	}

	return (0);
}



ACMD (do_drop)
{
	struct obj_data *obj, *next_obj;
	room_rnum RDR = 0;
	byte mode = SCMD_DROP;
	int dotmode, amount = 0, multi;
	const char *sname;

	switch (subcmd)
	{
	case SCMD_JUNK:
		sname = "junk";
		mode = SCMD_JUNK;
		break;
	case SCMD_DONATE:
		sname = "donate";
		mode = SCMD_DONATE;
		switch (number (0, 2))
		{
		case 0:
			mode = SCMD_JUNK;
			break;
		case 1:
		case 2:
			RDR = real_room (donation_room_1);
			break;

/*    
	  case 3: RDR = real_room(donation_room_2); break;
      case 4: RDR = real_room(donation_room_3); break;
*/
		}
		if (RDR == NOWHERE)
		{
			send_to_char ("Spiacente, non puoi donare niente.\r\n", ch);
			return;
		}
		break;
	default:
		sname = "drop";
		break;
	}

	argument = one_argument (argument, arg);

	if (!*arg)
	{
		sprintf (buf, "Cosa vuoi fare %s?\r\n", sname);
		send_to_char (buf, ch);
		return;
	}
	else if (is_number (arg))
	{
		multi = atoi (arg);
		one_argument (argument, arg);
		if (!str_cmp ("coins", arg) || !str_cmp ("coin", arg)
			|| !str_cmp ("moneta", arg) || !str_cmp ("monete", arg))
		{
			perform_drop_gold (ch, multi, mode, RDR);
		}
		else if (multi <= 0)
			send_to_char ("Si, questo ha senso.\r\n", ch);
		else if (!*arg)
		{
			sprintf (buf, "Cosa vuoi fare da %s %d of?\r\n", sname, multi);
			send_to_char (buf, ch);
		}
		else if (!(obj = get_obj_in_list_vis (ch, arg, NULL, ch->carrying)))
		{
			sprintf (buf, "Non hai nessuna %ss.\r\n", arg);
			send_to_char (buf, ch);
		}
		else
		{
			do
			{
				next_obj =
					get_obj_in_list_vis (ch, arg, NULL, obj->next_content);
				amount += perform_drop (ch, obj, mode, sname, RDR);
				obj = next_obj;
			}
			while (obj && --multi);
		}
	}
	else
	{
		dotmode = find_all_dots (arg);

		/* Can't junk or donate all */
		if ((dotmode == FIND_ALL)
			&& (subcmd == SCMD_JUNK || subcmd == SCMD_DONATE))
		{
			if (subcmd == SCMD_JUNK)
				send_to_char
					("Go to the dump if you want to junk EVERYTHING!\r\n",
					 ch);
			else
				send_to_char
					("Go do the donation room if you want to donate EVERYTHING!\r\n",
					 ch);
			return;
		}
		if (dotmode == FIND_ALL)
		{
			if (!ch->carrying)
				send_to_char ("You don't seem to be carrying anything.\r\n",
							  ch);
			else
				for (obj = ch->carrying; obj; obj = next_obj)
				{
					next_obj = obj->next_content;
					amount += perform_drop (ch, obj, mode, sname, RDR);
				}
		}
		else if (dotmode == FIND_ALLDOT)
		{
			if (!*arg)
			{
				sprintf (buf, "What do you want to %s all of?\r\n", sname);
				send_to_char (buf, ch);
				return;
			}
			if (!(obj = get_obj_in_list_vis (ch, arg, NULL, ch->carrying)))
			{
				sprintf (buf, "You don't seem to have any %ss.\r\n", arg);
				send_to_char (buf, ch);
			}
			while (obj)
			{
				next_obj =
					get_obj_in_list_vis (ch, arg, NULL, obj->next_content);
				amount += perform_drop (ch, obj, mode, sname, RDR);
				obj = next_obj;
			}
		}
		else
		{
			if (!(obj = get_obj_in_list_vis (ch, arg, NULL, ch->carrying)))
			{
				sprintf (buf, "You don't seem to have %s %s.\r\n", AN (arg),
						 arg);
				send_to_char (buf, ch);
			}
			else
				amount += perform_drop (ch, obj, mode, sname, RDR);
		}
	}

	if (amount && (subcmd == SCMD_JUNK))
	{
		send_to_char ("Sei stato ricompensato dagli dei!\r\n", ch);
		act ("$n e' stato ricompensato dagli dei!", TRUE, ch, 0, 0, TO_ROOM);
		GET_GOLD (ch) += amount;
	}
}


void
perform_give (struct char_data *ch, struct char_data *vict,
			  struct obj_data *obj)
{
	if (IS_OBJ_STAT (obj, ITEM_NODROP))
	{
		act ("You can't let go of $p!!  Yeech!", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}
	if (IS_CARRYING_N (vict) >= CAN_CARRY_N (vict))
	{
		act ("$N vedi che $S ha le mani piene.", FALSE, ch, 0, vict, TO_CHAR);
		return;
	}
	if (GET_OBJ_WEIGHT (obj) + IS_CARRYING_W (vict) > CAN_CARRY_W (vict))
	{
		act ("$E e' sovraccaricot.", FALSE, ch, 0, vict, TO_CHAR);
		return;
	}
	obj_from_char (obj);
	obj_to_char (obj, vict);
	act ("Dai $p a $N.", FALSE, ch, obj, vict, TO_CHAR);
	act ("$n ti da' $p.", FALSE, ch, obj, vict, TO_VICT);
	act ("$n da' $p a $N.", TRUE, ch, obj, vict, TO_NOTVICT);
}

/* utility function for give */
struct char_data *
give_find_vict (struct char_data *ch, char *arg)
{
	struct char_data *vict;

	if (!*arg)
	{
		send_to_char ("a chi?\r\n", ch);
		return (NULL);
	}
	else if (!(vict = get_char_vis (ch, arg, NULL, FIND_CHAR_ROOM)))
	{
		send_to_char (NOPERSON, ch);
		return (NULL);
	}
	else if (vict == ch)
	{
		send_to_char ("What's the point of that?\r\n", ch);
		return (NULL);
	}
	else
		return (vict);
}


void
perform_give_gold (struct char_data *ch, struct char_data *vict, int amount)
{
	if (amount <= 0)
	{
		send_to_char ("Heh heh heh ... stiamo scherzando oggi, eh?\r\n", ch);
		return;
	}
	if ((GET_GOLD (ch) < amount)
		&& (IS_NPC (ch) || (GET_LEVEL (ch) < LVL_GOD)))
	{
		send_to_char ("Non hai tutti quei soldi!\r\n", ch);
		return;
	}
	send_to_char (OK, ch);
	sprintf (buf, "$n ti da' %d %s d'oro.", amount,
			 amount == 1 ? "moneta" : "monete");
	act (buf, FALSE, ch, 0, vict, TO_VICT);
	sprintf (buf, "$n da' %s a $N.", money_desc (amount));
	act (buf, TRUE, ch, 0, vict, TO_NOTVICT);
	if (IS_NPC (ch) || (GET_LEVEL (ch) < LVL_GOD))
		GET_GOLD (ch) -= amount;
	GET_GOLD (vict) += amount;
}


ACMD (do_give)
{
	int amount, dotmode;
	struct char_data *vict;
	struct obj_data *obj, *next_obj;

	argument = one_argument (argument, arg);

	if (!*arg)
		send_to_char ("Dai cosa a chi?\r\n", ch);
	else if (is_number (arg))
	{
		amount = atoi (arg);
		argument = one_argument (argument, arg);
		if (!str_cmp ("coins", arg) || !str_cmp ("coin", arg)
			|| !str_cmp ("moneta", arg) || !str_cmp ("monete", arg))
		{
			one_argument (argument, arg);
			if ((vict = give_find_vict (ch, arg)) != NULL)
				perform_give_gold (ch, vict, amount);
			return;
		}
		else if (!*arg)
		{						/* Give multiple code. */
			sprintf (buf, "What do you want to give %d of?\r\n", amount);
			send_to_char (buf, ch);
		}
		else if (!(vict = give_find_vict (ch, argument)))
		{
			return;
		}
		else if (!(obj = get_obj_in_list_vis (ch, arg, NULL, ch->carrying)))
		{
			sprintf (buf, "Non sembri avere alcun %ss.\r\n", arg);
			send_to_char (buf, ch);
		}
		else
		{
			while (obj && amount--)
			{
				next_obj =
					get_obj_in_list_vis (ch, arg, NULL, obj->next_content);
				perform_give (ch, vict, obj);
				obj = next_obj;
			}
		}
	}
	else
	{
		one_argument (argument, buf1);
		if (!(vict = give_find_vict (ch, buf1)))
			return;
		dotmode = find_all_dots (arg);
		if (dotmode == FIND_INDIV)
		{
			if (!(obj = get_obj_in_list_vis (ch, arg, NULL, ch->carrying)))
			{
				sprintf (buf, "Non sembri avere alcun %s %s.\r\n", AN (arg),
						 arg);
				send_to_char (buf, ch);
			}
			else
				perform_give (ch, vict, obj);
		}
		else
		{
			if (dotmode == FIND_ALLDOT && !*arg)
			{
				send_to_char ("Tutto di cosa?\r\n", ch);
				return;
			}
			if (!ch->carrying)
				send_to_char ("You don't seem to be holding anything.\r\n",
							  ch);
			else
				for (obj = ch->carrying; obj; obj = next_obj)
				{
					next_obj = obj->next_content;
					if (CAN_SEE_OBJ (ch, obj) &&
						((dotmode == FIND_ALL || isname (arg, obj->name))))
						perform_give (ch, vict, obj);
				}
		}
	}
}



void
weight_change_object (struct obj_data *obj, int weight)
{
	struct obj_data *tmp_obj;
	struct char_data *tmp_ch;

	if (obj->in_room != NOWHERE)
	{
		GET_OBJ_WEIGHT (obj) += weight;
	}
	else if ((tmp_ch = obj->carried_by))
	{
		obj_from_char (obj);
		GET_OBJ_WEIGHT (obj) += weight;
		obj_to_char (obj, tmp_ch);
	}
	else if ((tmp_obj = obj->in_obj))
	{
		obj_from_obj (obj);
		GET_OBJ_WEIGHT (obj) += weight;
		obj_to_obj (obj, tmp_obj);
	}
	else
	{
		log ("SYSERR: Unknown attempt to subtract weight from an object.");
	}
}



void
name_from_drinkcon (struct obj_data *obj)
{
	char *new_name, *cur_name, *next;
	const char *liqname;
	int liqlen, cpylen;

	if (!obj
		|| (GET_OBJ_TYPE (obj) != ITEM_DRINKCON
			&& GET_OBJ_TYPE (obj) != ITEM_FOUNTAIN))
		return;

	liqname = drinknames[GET_OBJ_VAL (obj, 2)];
	if (!isname (liqname, obj->name))
	{
		log ("SYSERR: Can't remove liquid '%s' from '%s' (%d) item.", liqname,
			 obj->name, obj->item_number);
		return;
	}

	liqlen = strlen (liqname);
	CREATE (new_name, char, strlen (obj->name) - strlen (liqname));	/* +1 for NUL, -1 for space */

	for (cur_name = obj->name; cur_name; cur_name = next)
	{
		if (*cur_name == ' ')
			cur_name++;

		if ((next = strchr (cur_name, ' ')))
			cpylen = next - cur_name;
		else
			cpylen = strlen (cur_name);

		if (!strn_cmp (cur_name, liqname, liqlen))
			continue;

		if (*new_name)
			strcat (new_name, " ");
		strncat (new_name, cur_name, cpylen);
	}

	if (GET_OBJ_RNUM (obj) < 0
		|| obj->name != obj_proto[GET_OBJ_RNUM (obj)].name)
		free (obj->name);
	obj->name = new_name;
}



void
name_to_drinkcon (struct obj_data *obj, int type)
{
	char *new_name;

	if (!obj
		|| (GET_OBJ_TYPE (obj) != ITEM_DRINKCON
			&& GET_OBJ_TYPE (obj) != ITEM_FOUNTAIN))
		return;

	CREATE (new_name, char,
			strlen (obj->name) + strlen (drinknames[type]) + 2);
	sprintf (new_name, "%s %s", obj->name, drinknames[type]);
	if (GET_OBJ_RNUM (obj) < 0
		|| obj->name != obj_proto[GET_OBJ_RNUM (obj)].name)
		free (obj->name);
	obj->name = new_name;
}



ACMD (do_drink)
{
	struct obj_data *temp;
	struct affected_type af;
	int amount, weight;
	int on_ground = 0;

	one_argument (argument, arg);

	if (IS_NPC (ch))			/* Cannot use GET_COND() on mobs. */
		return;

	if (!*arg)
	{
		send_to_char ("Bevi da cosa?\r\n", ch);
		return;
	}
	if (!(temp = get_obj_in_list_vis (ch, arg, NULL, ch->carrying)))
	{
		if (!
			(temp =
			 get_obj_in_list_vis (ch, arg, NULL,
								  world[ch->in_room].contents)))
		{
			send_to_char ("Non lo trovi!\r\n", ch);
			return;
		}
		else
			on_ground = 1;
	}
	if ((GET_OBJ_TYPE (temp) != ITEM_DRINKCON) &&
		(GET_OBJ_TYPE (temp) != ITEM_FOUNTAIN))
	{
		send_to_char ("Non puoi bere da li'!\r\n", ch);
		return;
	}
	if (on_ground && (GET_OBJ_TYPE (temp) == ITEM_DRINKCON))
	{
		send_to_char ("You have to be holding that to drink from it.\r\n",
					  ch);
		return;
	}
	if ((GET_COND (ch, DRUNK) > 10) && (GET_COND (ch, THIRST) > 0))
	{
		/* The pig is drunk */
		send_to_char ("Provi a bere ma non trovi la tua bocca.\r\n", ch);
		act ("$n prova a bere ma manca la sua mouth!", TRUE, ch, 0, 0,
			 TO_ROOM);
		return;
	}
	if ((GET_COND (ch, FULL) > 20) && (GET_COND (ch, THIRST) > 0))
	{
		send_to_char ("Il tuo stomaco non puo' contenere piu' nulla!\r\n",
					  ch);
		return;
	}
	if (!GET_OBJ_VAL (temp, 1))
	{
		send_to_char ("E' vuoto.\r\n", ch);
		return;
	}
	if (subcmd == SCMD_DRINK)
	{
		sprintf (buf, "$n beve %s da $p.", drinks[GET_OBJ_VAL (temp, 2)]);
		act (buf, TRUE, ch, temp, 0, TO_ROOM);

		sprintf (buf, "Bevi %s.\r\n", drinks[GET_OBJ_VAL (temp, 2)]);
		send_to_char (buf, ch);

		if (drink_aff[GET_OBJ_VAL (temp, 2)][DRUNK] > 0)
			amount =
				(25 -
				 GET_COND (ch, THIRST)) / drink_aff[GET_OBJ_VAL (temp,
																 2)][DRUNK];
		else
			amount = number (3, 10);

	}
	else
	{
		act ("$n sorseggia da $p.", TRUE, ch, temp, 0, TO_ROOM);
		sprintf (buf, "Ha il sapore come %s.\r\n",
				 drinks[GET_OBJ_VAL (temp, 2)]);
		send_to_char (buf, ch);
		amount = 1;
	}

	amount = MIN (amount, GET_OBJ_VAL (temp, 1));

	/* You can't subtract more than the object weighs */
	weight = MIN (amount, GET_OBJ_WEIGHT (temp));

	weight_change_object (temp, -weight);	/* Subtract amount */

	gain_condition (ch, DRUNK,
					(int) ((int) drink_aff[GET_OBJ_VAL (temp, 2)][DRUNK] *
						   amount) / 4);

	gain_condition (ch, FULL,
					(int) ((int) drink_aff[GET_OBJ_VAL (temp, 2)][FULL] *
						   amount) / 4);

	gain_condition (ch, THIRST,
					(int) ((int) drink_aff[GET_OBJ_VAL (temp, 2)][THIRST] *
						   amount) / 4);

	if (GET_COND (ch, DRUNK) > 10)
		send_to_char ("Ti senti ubriaco.\r\n", ch);

	if (GET_COND (ch, THIRST) > 20)
		send_to_char ("Non hai piu' sete.\r\n", ch);

	if (GET_COND (ch, FULL) > 20)
		send_to_char ("Sei pieno.\r\n", ch);

	if (GET_OBJ_VAL (temp, 3))
	{							/* The crap was poisoned ! */
		send_to_char ("Oops, ha uno strano sapore!\r\n", ch);
		act ("$n tossisce ed emette strani suoni.", TRUE, ch, 0, 0, TO_ROOM);

		af.type = SPELL_POISON;
		af.duration = amount * 3;
		af.modifier = 0;
		af.location = APPLY_NONE;
		af.bitvector = AFF_POISON;
		affect_join (ch, &af, FALSE, FALSE, FALSE, FALSE);
	}
	/* empty the container, and no longer poison. */
	GET_OBJ_VAL (temp, 1) -= amount;
	if (!GET_OBJ_VAL (temp, 1))
	{							/* The last bit */
		name_from_drinkcon (temp);
		GET_OBJ_VAL (temp, 2) = 0;
		GET_OBJ_VAL (temp, 3) = 0;
	}
	return;
}



ACMD (do_eat)
{
	struct obj_data *food;
	struct affected_type af;
	int amount;

	one_argument (argument, arg);

	if (IS_NPC (ch))			/* Cannot use GET_COND() on mobs. */
		return;

	if (!*arg)
	{
		send_to_char ("Mangiare cosa?\r\n", ch);
		return;
	}
	if (!(food = get_obj_in_list_vis (ch, arg, NULL, ch->carrying)))
	{
		sprintf (buf, "Non sembri avere %s %s.\r\n", AN (arg), arg);
		send_to_char (buf, ch);
		return;
	}
	if (subcmd == SCMD_TASTE && ((GET_OBJ_TYPE (food) == ITEM_DRINKCON) ||
								 (GET_OBJ_TYPE (food) == ITEM_FOUNTAIN)))
	{
		do_drink (ch, argument, 0, SCMD_SIP);
		return;
	}
	if ((GET_OBJ_TYPE (food) != ITEM_FOOD) && (GET_LEVEL (ch) < LVL_GOD))
	{
		send_to_char ("Non puoi mangiarlo!\r\n", ch);
		return;
	}
	if (GET_COND (ch, FULL) > 20)
	{							/* Stomach full */
		send_to_char
			("Sei troppo pieno non riesci a mangiare piu' nulla!\r\n", ch);
		return;
	}
	if (subcmd == SCMD_EAT)
	{
		act ("Mangi $p.", FALSE, ch, food, 0, TO_CHAR);
		act ("$n mangia $p.", TRUE, ch, food, 0, TO_ROOM);
	}
	else
	{
		act ("Mordi un piccolo bocconcino di $p.", FALSE, ch, food, 0,
			 TO_CHAR);
		act ("$n assaggia un pezzettino di $p.", TRUE, ch, food, 0, TO_ROOM);
	}

	amount = (subcmd == SCMD_EAT ? GET_OBJ_VAL (food, 0) : 1);

	gain_condition (ch, FULL, amount);

	if (GET_COND (ch, FULL) > 20)
		send_to_char ("Sei sazio.\r\n", ch);

	if (GET_OBJ_VAL (food, 3) && (GET_LEVEL (ch) < LVL_IMMORT))
	{
		/* The crap was poisoned ! */
		send_to_char ("Oops, ha uno strano sapore!\r\n", ch);
		act ("$n tossisce ed emette strani rumori.", FALSE, ch, 0, 0,
			 TO_ROOM);

		af.type = SPELL_POISON;
		af.duration = amount * 2;
		af.modifier = 0;
		af.location = APPLY_NONE;
		af.bitvector = AFF_POISON;
		affect_join (ch, &af, FALSE, FALSE, FALSE, FALSE);
	}
	if (subcmd == SCMD_EAT)
		extract_obj (food);
	else
	{
		if (!(--GET_OBJ_VAL (food, 0)))
		{
			send_to_char ("There's nothing left now.\r\n", ch);
			extract_obj (food);
		}
	}
}


ACMD (do_pour)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	struct obj_data *from_obj = NULL, *to_obj = NULL;
	int amount;

	two_arguments (argument, arg1, arg2);

	if (subcmd == SCMD_POUR)
	{
		if (!*arg1)
		{						/* No arguments */
			send_to_char ("Da dove vuoi svuotare?\r\n", ch);
			return;
		}
		if (!(from_obj = get_obj_in_list_vis (ch, arg1, NULL, ch->carrying)))
		{
			send_to_char ("Non lo trovo!\r\n", ch);
			return;
		}
		if (GET_OBJ_TYPE (from_obj) != ITEM_DRINKCON)
		{
			send_to_char ("Non puoi svuotare da questo!\r\n", ch);
			return;
		}
	}
	if (subcmd == SCMD_FILL)
	{
		if (!*arg1)
		{						/* no arguments */
			send_to_char ("Cosa vuoi riempire? E con cosa?\r\n", ch);
			return;
		}
		if (!(to_obj = get_obj_in_list_vis (ch, arg1, NULL, ch->carrying)))
		{
			send_to_char ("Non lo trovi!\r\n", ch);
			return;
		}
		if (GET_OBJ_TYPE (to_obj) != ITEM_DRINKCON)
		{
			act ("Non puoi riempire $p!", FALSE, ch, to_obj, 0, TO_CHAR);
			return;
		}
		if (!*arg2)
		{						/* no 2nd argument */
			act ("Con cosa vuoi riempire $p?", FALSE, ch, to_obj, 0, TO_CHAR);
			return;
		}
		if (!
			(from_obj =
			 get_obj_in_list_vis (ch, arg2, NULL,
								  world[ch->in_room].contents)))
		{
			sprintf (buf, "Non sembra esserci %s %s qui.\r\n", AN (arg2),
					 arg2);
			send_to_char (buf, ch);
			return;
		}
		if (GET_OBJ_TYPE (from_obj) != ITEM_FOUNTAIN)
		{
			act ("Non puoi riempire nulla da $p.", FALSE, ch, from_obj, 0,
				 TO_CHAR);
			return;
		}
	}
	if (GET_OBJ_VAL (from_obj, 1) == 0)
	{
		act ("e' vuoto.", FALSE, ch, from_obj, 0, TO_CHAR);
		return;
	}
	if (subcmd == SCMD_POUR)
	{							/* pour */
		if (!*arg2)
		{
			send_to_char ("Where do you want it?  Out or in what?\r\n", ch);
			return;
		}
		if (!str_cmp (arg2, "out"))
		{
			act ("$n svuota $p.", TRUE, ch, from_obj, 0, TO_ROOM);
			act ("Svuoti $p.", FALSE, ch, from_obj, 0, TO_CHAR);

			weight_change_object (from_obj, -GET_OBJ_VAL (from_obj, 1));	/* Empty */

			name_from_drinkcon (from_obj);
			GET_OBJ_VAL (from_obj, 1) = 0;
			GET_OBJ_VAL (from_obj, 2) = 0;
			GET_OBJ_VAL (from_obj, 3) = 0;

			return;
		}
		if (!(to_obj = get_obj_in_list_vis (ch, arg2, NULL, ch->carrying)))
		{
			send_to_char ("Non lo trovi!\r\n", ch);
			return;
		}
		if ((GET_OBJ_TYPE (to_obj) != ITEM_DRINKCON) &&
			(GET_OBJ_TYPE (to_obj) != ITEM_FOUNTAIN))
		{
			send_to_char ("You can't pour anything into that.\r\n", ch);
			return;
		}
	}
	if (to_obj == from_obj)
	{
		send_to_char ("A most unproductive effort.\r\n", ch);
		return;
	}
	if ((GET_OBJ_VAL (to_obj, 1) != 0) &&
		(GET_OBJ_VAL (to_obj, 2) != GET_OBJ_VAL (from_obj, 2)))
	{
		send_to_char ("C'e' gia' un altro liquido dentro!\r\n", ch);
		return;
	}
	if (!(GET_OBJ_VAL (to_obj, 1) < GET_OBJ_VAL (to_obj, 0)))
	{
		send_to_char ("Non c'e' piu' spazio.\r\n", ch);
		return;
	}
	if (subcmd == SCMD_POUR)
	{
		sprintf (buf, "You pour the %s into the %s.",
				 drinks[GET_OBJ_VAL (from_obj, 2)], arg2);
		send_to_char (buf, ch);
	}
	if (subcmd == SCMD_FILL)
	{
		act ("Riempi $p da $P.", FALSE, ch, to_obj, from_obj, TO_CHAR);
		act ("$n riempie $p da $P.", TRUE, ch, to_obj, from_obj, TO_ROOM);
	}
	/* New alias */
	if (GET_OBJ_VAL (to_obj, 1) == 0)
		name_to_drinkcon (to_obj, GET_OBJ_VAL (from_obj, 2));

	/* First same type liq. */
	GET_OBJ_VAL (to_obj, 2) = GET_OBJ_VAL (from_obj, 2);

	/* Then how much to pour */
	GET_OBJ_VAL (from_obj, 1) -= (amount =
								  (GET_OBJ_VAL (to_obj, 0) -
								   GET_OBJ_VAL (to_obj, 1)));

	GET_OBJ_VAL (to_obj, 1) = GET_OBJ_VAL (to_obj, 0);

	if (GET_OBJ_VAL (from_obj, 1) < 0)
	{							/* There was too little */
		GET_OBJ_VAL (to_obj, 1) += GET_OBJ_VAL (from_obj, 1);
		amount += GET_OBJ_VAL (from_obj, 1);
		name_from_drinkcon (from_obj);
		GET_OBJ_VAL (from_obj, 1) = 0;
		GET_OBJ_VAL (from_obj, 2) = 0;
		GET_OBJ_VAL (from_obj, 3) = 0;
	}
	/* Then the poison boogie */
	GET_OBJ_VAL (to_obj, 3) =
		(GET_OBJ_VAL (to_obj, 3) || GET_OBJ_VAL (from_obj, 3));

	/* And the weight boogie */
	weight_change_object (from_obj, -amount);
	weight_change_object (to_obj, amount);	/* Add weight */
}



void
wear_message (struct char_data *ch, struct obj_data *obj, int where)
{
	const char *wear_messages[][2] = {
		{"$n accende $p.",
		 "Accendi $p."},

		{"$n infila $p all'anulare destro.",
		 "Infili $p all'anulare sinistro."},

		{"$n infila $p all'anulare sinistro.",
		 "Infili $p all'anulare sinistro."},

		{"$n mette $p intorno al collo.",
		 "Ti metti $p intorno al collo."},

		{"$n mette $p intorno al collo.",
		 "Ti metti $p intorno al collo."},

		{"$n indossa $p sul corpo.",
		 "Indossi $p sul corpo."},

		{"$n si mette $p in testa.",
		 "Ti metti $p in testa."},

		{"$n si mette $p sulle gambe.",
		 "Ti metti $p sulle gambe."},

		{"$n si mette $p ai piedi.",
		 "Ti metti $p ai piedi."},

		{"$n si mette $p sulle mani.",
		 "Ti metti $p sulle mani."},

		{"$n si mette $p sulle braccia.",
		 "Ti metti $p sulle braccia."},

		{"$n usa $p come scudo.",
		 "Usi $p come scudo."},

		{"$n indossa $p intorno al corpo.",
		 "Indossi $p intorno al corpo."},

		{"$n si mette $p intorno alla vita.",
		 "Ti metti $p intorno alla vita."},

		{"$n si mette $p al polso destro.",
		 "Ti metti $p al polso destro."},

		{"$n si mette $p al polso sinistro.",
		 "Ti metti $p al polso sinistro."},

		{"$n impugna $p.",
		 "Impugni $p."},

		{"$n afferra $p.",
		 "Afferri $p."},

		{"$n si mette $p sulla schiena.",
		 "Ti metti $p sulla schiena."},

		{"$n si mette $p sulla spalla destra.",
		 "Ti metti $p sulla spalla destra."},

		{"$n si mette $p sulla spalla sinistra.",
		 "Ti metti $p sulla spalla sinistra."},

		{"$n si mette $p all'orecchio destro.",
		 "Ti metti $p all'orecchio destro."},

		{"$n si mette $p all'orecchio sinistro.",
		 "Ti metti $p all'orecchio sinistro."},

		{"$n si mette $p sugli occhi.",
		 "Ti metti $p sugli occhi."}
	};

	act (wear_messages[where][0], TRUE, ch, obj, 0, TO_ROOM);
	act (wear_messages[where][1], FALSE, ch, obj, 0, TO_CHAR);
}



void
perform_wear (struct char_data *ch, struct obj_data *obj, int where)
{
	/*
	 * ITEM_WEAR_TAKE is used for objects that do not require special bits
	 * to be put into that position (e.g. you can hold any object, not just
	 * an object with a HOLD bit.)
	 */

	int wear_bitvectors[] = {
		ITEM_WEAR_TAKE, ITEM_WEAR_FINGER, ITEM_WEAR_FINGER, ITEM_WEAR_NECK,
		ITEM_WEAR_NECK, ITEM_WEAR_BODY, ITEM_WEAR_HEAD, ITEM_WEAR_LEGS,
		ITEM_WEAR_FEET, ITEM_WEAR_HANDS, ITEM_WEAR_ARMS, ITEM_WEAR_SHIELD,
		ITEM_WEAR_ABOUT, ITEM_WEAR_WAIST, ITEM_WEAR_WRIST, ITEM_WEAR_WRIST,
		ITEM_WEAR_WIELD, ITEM_WEAR_TAKE, ITEM_WEAR_BACK, ITEM_WEAR_SHOULDER,
		ITEM_WEAR_SHOULDER, ITEM_WEAR_EAR, ITEM_WEAR_EAR, ITEM_WEAR_EYE
	};

	const char *already_wearing[] = {
		"Stai gia' usando una luce.\r\n",
		"YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
		"Stai gia' indossando qualcosa su entrambe le dita delle mani.\r\n",
		"YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
		"Non puoi mettere altro intorno al collo.\r\n",
		"Stai gia' indossando qualcosa sul corpo.\r\n",
		"Stai gia' indossando qualcosa sulla tua testa.\r\n",
		"Stai gia' indossando qualcosa sulle gambe.\r\n",
		"Porti gia' delle calzature ai piedi.\r\n",
		"Hai gia' dei guanti alle mani.\r\n",
		"Stai gia' indossando qualcosa sulle braccia.\r\n",
		"Stai gia' usando uno scudo.\r\n",
		"Stai gia' indossando qualcosa intorno al corpo.\r\n",
		"You already have something around your waist.\r\n",
		"YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
		"Hai gia' entrambi i polsi occupati.\r\n",
		"Stai gia' impugnando qualcosa.\r\n",
		"Hai gia' qualcosa in mano.\r\n",
		"Hai gia' qualcosa sulla schiena.\r\n",
		"YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
		"Hai gia' entrambe le spalle occupate.\r\n",
		"YOU SHOULD NEVER SEE THIS MESSAGE.  PLEASE REPORT.\r\n",
		"Hai gia' entrambi gli orecchi occupati.\r\n",
		"Stai gia' usando qualcosa sugli occhi.\r\n",
	};

	/* first, make sure that the wear position is valid. */
	if (!CAN_WEAR (obj, wear_bitvectors[where]))
	{
		act ("Non puoi metterlo $p li'.", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}
	/* for neck, finger, and wrist, try pos 2 if pos 1 is already full */
	if ((where == WEAR_FINGER_R) || (where == WEAR_NECK_1)
		|| (where == WEAR_WRIST_R) || (where == WEAR_SHOULDER_R)
		|| (where == WEAR_EAR_R))
		if (GET_EQ (ch, where))
			where++;

	if (GET_EQ (ch, where))
	{
		send_to_char (already_wearing[where], ch);
		return;
	}
	wear_message (ch, obj, where);
	obj_from_char (obj);
	equip_char (ch, obj, where);
}



int
find_eq_pos (struct char_data *ch, struct obj_data *obj, char *arg)
{
	int where = -1;

	const char *keywords[] = {
		"!RESERVED!",
		"finger",
		"!RESERVED!",
		"neck",
		"!RESERVED!",
		"body",
		"head",
		"legs",
		"feet",
		"hands",
		"arms",
		"shield",
		"about",
		"waist",
		"wrist",
		"!RESERVED!",
		"!RESERVED!",
		"!RESERVED!",
		"back",
		"!RESERVED!",
		"shoulder",
		"!RESERVED!",
		"ear",
		"eyes",
		"\n"
	};

	if (!arg || !*arg)
	{
		if (CAN_WEAR (obj, ITEM_WEAR_FINGER))
			where = WEAR_FINGER_R;
		if (CAN_WEAR (obj, ITEM_WEAR_NECK))
			where = WEAR_NECK_1;
		if (CAN_WEAR (obj, ITEM_WEAR_BODY))
			where = WEAR_BODY;
		if (CAN_WEAR (obj, ITEM_WEAR_HEAD))
			where = WEAR_HEAD;
		if (CAN_WEAR (obj, ITEM_WEAR_LEGS))
			where = WEAR_LEGS;
		if (CAN_WEAR (obj, ITEM_WEAR_FEET))
			where = WEAR_FEET;
		if (CAN_WEAR (obj, ITEM_WEAR_HANDS))
			where = WEAR_HANDS;
		if (CAN_WEAR (obj, ITEM_WEAR_ARMS))
			where = WEAR_ARMS;
		if (CAN_WEAR (obj, ITEM_WEAR_SHIELD))
			where = WEAR_SHIELD;
		if (CAN_WEAR (obj, ITEM_WEAR_ABOUT))
			where = WEAR_ABOUT;
		if (CAN_WEAR (obj, ITEM_WEAR_WAIST))
			where = WEAR_WAIST;
		if (CAN_WEAR (obj, ITEM_WEAR_WRIST))
			where = WEAR_WRIST_R;
		if (CAN_WEAR (obj, ITEM_WEAR_BACK))
			where = WEAR_BACK;
		if (CAN_WEAR (obj, ITEM_WEAR_SHOULDER))
			where = WEAR_SHOULDER_R;
		if (CAN_WEAR (obj, ITEM_WEAR_EAR))
			where = WEAR_EAR_R;
		if (CAN_WEAR (obj, ITEM_WEAR_EYE))
			where = WEAR_EYES;
	}
	else
	{
		if ((where = search_block (arg, keywords, FALSE)) < 0)
		{
			sprintf (buf, "'%s'?  Quale parte del tuo corpo e' quella?\r\n",
					 arg);
			send_to_char (buf, ch);
		}
	}

	return (where);
}



ACMD (do_wear)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	struct obj_data *obj, *next_obj;
	int where, dotmode, items_worn = 0;

	two_arguments (argument, arg1, arg2);

	if (!*arg1)
	{
		send_to_char ("Indossi cosa?\r\n", ch);
		return;
	}
	dotmode = find_all_dots (arg1);

	if (*arg2 && (dotmode != FIND_INDIV))
	{
		send_to_char
			("You can't specify the same body location for more than one item!\r\n",
			 ch);
		return;
	}
	if (dotmode == FIND_ALL)
	{
		for (obj = ch->carrying; obj; obj = next_obj)
		{
			next_obj = obj->next_content;
			if (CAN_SEE_OBJ (ch, obj)
				&& (where = find_eq_pos (ch, obj, 0)) >= 0)
			{
				items_worn++;
				perform_wear (ch, obj, where);
			}
		}
		if (!items_worn)
			send_to_char ("Non vedi niente di indossabile.\r\n", ch);
	}
	else if (dotmode == FIND_ALLDOT)
	{
		if (!*arg1)
		{
			send_to_char ("Indossare tutto cosa?\r\n", ch);
			return;
		}
		if (!(obj = get_obj_in_list_vis (ch, arg1, NULL, ch->carrying)))
		{
			sprintf (buf, "Non vedo nessun %ss.\r\n", arg1);
			send_to_char (buf, ch);
		}
		else
			while (obj)
			{
				next_obj =
					get_obj_in_list_vis (ch, arg1, NULL, obj->next_content);
				if ((where = find_eq_pos (ch, obj, 0)) >= 0)
					perform_wear (ch, obj, where);
				else
					act ("Non puoi indossare $p.", FALSE, ch, obj, 0,
						 TO_CHAR);
				obj = next_obj;
			}
	}
	else
	{
		if (!(obj = get_obj_in_list_vis (ch, arg1, NULL, ch->carrying)))
		{
			sprintf (buf, "You don't seem to have %s %s.\r\n", AN (arg1),
					 arg1);
			send_to_char (buf, ch);
		}
		else
		{
			if ((where = find_eq_pos (ch, obj, arg2)) >= 0)
				perform_wear (ch, obj, where);
			else if (!*arg2)
				act ("Non puoi indossare $p.", FALSE, ch, obj, 0, TO_CHAR);
		}
	}
}



ACMD (do_wield)
{
	struct obj_data *obj;

	one_argument (argument, arg);

	if (!*arg)
		send_to_char ("Impugnare cosa?\r\n", ch);
	else if (!(obj = get_obj_in_list_vis (ch, arg, NULL, ch->carrying)))
	{
		sprintf (buf, "Non vedo niente da impugnare %s %s.\r\n", AN (arg),
				 arg);
		send_to_char (buf, ch);
	}
	else
	{
		if (!CAN_WEAR (obj, ITEM_WEAR_WIELD))
			send_to_char ("Non puoi impugnare questo.\r\n", ch);
		else if (GET_OBJ_WEIGHT (obj) >
				 str_app[STRENGTH_APPLY_INDEX (ch)].wield_w)
			send_to_char ("E' troppo pesante da usare.\r\n", ch);
		else
			perform_wear (ch, obj, WEAR_WIELD);
	}
}



ACMD (do_grab)
{
	struct obj_data *obj;

	one_argument (argument, arg);

	if (!*arg)
		send_to_char ("Tenere cosa?\r\n", ch);
	else if (!(obj = get_obj_in_list_vis (ch, arg, NULL, ch->carrying)))
	{
		sprintf (buf, "You don't seem to have %s %s.\r\n", AN (arg), arg);
		send_to_char (buf, ch);
	}
	else
	{
		if (GET_OBJ_TYPE (obj) == ITEM_LIGHT)
			perform_wear (ch, obj, WEAR_LIGHT);
		else
		{
			if (!CAN_WEAR (obj, ITEM_WEAR_HOLD)
				&& GET_OBJ_TYPE (obj) != ITEM_WAND
				&& GET_OBJ_TYPE (obj) != ITEM_STAFF
				&& GET_OBJ_TYPE (obj) != ITEM_SCROLL
				&& GET_OBJ_TYPE (obj) != ITEM_POTION)
				send_to_char ("Non puoi tenere questo.\r\n", ch);
			else
				perform_wear (ch, obj, WEAR_HOLD);
		}
	}
}



void
perform_remove (struct char_data *ch, int pos)
{
	struct obj_data *obj;

	if (!(obj = GET_EQ (ch, pos)))
		log ("SYSERR: perform_remove: bad pos %d passed.", pos);
	else if (IS_OBJ_STAT (obj, ITEM_NODROP))
		act ("Non puoi rimuovere $p, e' stato MALEDETTO!", FALSE, ch, obj, 0,
			 TO_CHAR);
	else if (IS_CARRYING_N (ch) >= CAN_CARRY_N (ch))
		act ("$p: non puoi usare tanti oggetti!", FALSE, ch, obj, 0, TO_CHAR);
	else
	{
		obj_to_char (unequip_char (ch, pos), ch);
		act ("Smetti di usare $p.", FALSE, ch, obj, 0, TO_CHAR);
		act ("$n smette di usare $p.", TRUE, ch, obj, 0, TO_ROOM);
	}
}



ACMD (do_remove)
{
	int i, dotmode, found;

	one_argument (argument, arg);

	if (!*arg)
	{
		send_to_char ("Togliere cosa?\r\n", ch);
		return;
	}
	dotmode = find_all_dots (arg);

	if (dotmode == FIND_ALL)
	{
		found = 0;
		for (i = 0; i < NUM_WEARS; i++)
			if (GET_EQ (ch, i))
			{
				perform_remove (ch, i);
				found = 1;
			}
		if (!found)
			send_to_char ("Non stai usando niente.\r\n", ch);
	}
	else if (dotmode == FIND_ALLDOT)
	{
		if (!*arg)
			send_to_char ("Togliere tutto di cosa?\r\n", ch);
		else
		{
			found = 0;
			for (i = 0; i < NUM_WEARS; i++)
				if (GET_EQ (ch, i) && CAN_SEE_OBJ (ch, GET_EQ (ch, i)) &&
					isname (arg, GET_EQ (ch, i)->name))
				{
					perform_remove (ch, i);
					found = 1;
				}
			if (!found)
			{
				sprintf (buf, "You don't seem to be using any %ss.\r\n", arg);
				send_to_char (buf, ch);
			}
		}
	}
	else
	{
		/* Returns object pointer but we don't need it, just true/false. */
		if (!get_object_in_equip_vis (ch, arg, ch->equipment, &i))
		{
			sprintf (buf, "You don't seem to be using %s %s.\r\n", AN (arg),
					 arg);
			send_to_char (buf, ch);
		}
		else
			perform_remove (ch, i);
	}
}


ACMD (do_pull)
{

	one_argument (argument, arg);

	if (!*arg)
	{
		send_to_char ("Tirare cosa?\r\n", ch);
		return;
	}
	/* for now only pull pin will work and must be wielding a grenade */
	if ((!str_cmp (arg, "pin")))
	{
		if (GET_EQ (ch, WEAR_WIELD))
		{
			if (GET_OBJ_TYPE (GET_EQ (ch, WEAR_WIELD)) == ITEM_GRENADE)
			{
				sprintf (buf, "Tiri via il pin, %s e' attiva!\r\n",
						 GET_EQ (ch, WEAR_WIELD)->short_description);
				send_to_char (buf, ch);
				SET_BIT (GET_OBJ_EXTRA (GET_EQ (ch, WEAR_WIELD)),
						 ITEM_LIVE_GRENADE);
				sprintf (buf, "$n tira il pin di %s",
						 GET_EQ (ch, WEAR_WIELD)->short_description);
				act (buf, FALSE, ch, 0, 0, TO_ROOM);
			}
			else
			{
				send_to_char ("Questa non e' una granata!\r\n", ch);
			}
		}
		else
		{
			send_to_char ("Non stai impugnando niente!\r\n", ch);
		}
	}

	/* put other 'pull' options here later */
	return;
}

ACMD (do_disarm)
{
	struct char_data *vict;
	struct obj_data *weap;
	byte percent;

	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_DISARM))
	{
		send_to_char ("Non hai idea su come fare.\r\n", ch);
		return;
	}

	if (check_use_fireweapon (ch) == TRUE ||
		(weap = GET_EQ (ch, WEAR_WIELD)) > 0)
	{
		send_to_char ("Non puoi disarmare con le mani occupate!\r\n", ch);
		return;
	}

	one_argument (argument, arg);

	if (!(vict = get_char_vis (ch, arg, NULL, FIND_CHAR_ROOM)))
	{
		if (FIGHTING (ch) && IN_ROOM (ch) == IN_ROOM (FIGHTING (ch)))
		{
			vict = FIGHTING (ch);
		}
		else
		{
			send_to_char ("Disarmi chi?\r\n", ch);
			return;
		}
	}

	if (vict == ch)
	{
		send_to_char ("Non e' divertente, oggi...\r\n", ch);
		return;
	}

	if (check_use_fireweapon (vict) == FALSE &&
		!(weap = GET_EQ (vict, WEAR_WIELD)))
	{
		send_to_char ("E' gia disarmato!\r\n", ch);
		return;
	}

	percent = number (1, 101);

	if (percent >
		(GET_SKILL (ch, SKILL_DISARM) + GET_DEX (ch) - GET_DEX (vict)))
	{
		damage (ch, vict, 0, SKILL_DISARM);
		act ("Fallisci nel tentativo di disarmare $N.", FALSE, ch, 0, vict,
			 TO_CHAR);
		act ("$n fallisce nel tentativo di disarmarti.", FALSE, ch, 0, vict,
			 TO_VICT);
		act ("$n fallisce nel tentativo di disarmare $N.", FALSE, ch, 0, vict,
			 TO_NOTVICT);
		learned_from_mistake (ch, SKILL_DISARM, 0, 95);
	}
	else
	{
		damage (ch, vict, 0, SKILL_DISARM);
		if (check_use_fireweapon (vict))
		{
			weap = GET_EQ (vict, WEAR_HOLD);

			act ("Con una mossa spettacolare disarmi $p dalle mani di $N.",
				 FALSE, ch, weap, vict, TO_CHAR);
			act
				("$n ti disarma $p dalle tue mani con un abile gioco delle mani!",
				 FALSE, ch, weap, vict, TO_VICT);
			act
				("$n disarma $p dalle mani di $N con un abile gioco delle mani!",
				 FALSE, ch, weap, vict, TO_NOTVICT);
			obj_to_room (unequip_char (vict, WEAR_HOLD), vict->in_room);
		}
		else if (GET_EQ (vict, WEAR_WIELD))
		{
			weap = GET_EQ (vict, WEAR_WIELD);

			act ("Con una mossa spettacolare disarmi $p dalle mani di $N.",
				 FALSE, ch, weap, vict, TO_CHAR);
			act
				("$n ti disarma $p dalle tue mani con un abile gioco delle mani!",
				 FALSE, ch, weap, vict, TO_VICT);
			act
				("$n disarma $p dalle mani di $N con un abile gioco delle mani!",
				 FALSE, ch, weap, vict, TO_NOTVICT);
			obj_to_room (unequip_char (vict, WEAR_WIELD), vict->in_room);
		}
	}

	WAIT_STATE (ch, PULSE_VIOLENCE);
}

/* ************************************************************************
*   File: act.comm.c                                    Part of CircleMUD *
*  Usage: Player-level communication commands                             *
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
#include "screen.h"

/* extern variables */
extern int level_can_shout;
extern int holler_move_cost;
extern struct room_data *world;
extern struct descriptor_data *descriptor_list;
extern struct char_data *character_list;

/* local functions */
void perform_tell (struct char_data *ch, struct char_data *vict, char *arg);
int is_tell_ok (struct char_data *ch, struct char_data *vict);
ACMD (do_say);
ACMD (do_gsay);
ACMD (do_tell);
ACMD (do_reply);
ACMD (do_spec_comm);
ACMD (do_write);
ACMD (do_page);
ACMD (do_gen_comm);
ACMD (do_qcomm);

ACMD(do_pray);


ACMD (do_say)
{
	skip_spaces (&argument);

	if (!*argument)
		send_to_char ("SI', ma cosa vorresti dire?\r\n", ch);
	else
	{
		sprintf (buf, "$n dice, '%s'", argument);
		act (buf, FALSE, ch, 0, 0, TO_ROOM);

		if (!IS_NPC (ch) && PRF_FLAGGED (ch, PRF_NOREPEAT))
			send_to_char (OK, ch);
		else
		{
			delete_doubledollar (argument);
			sprintf (buf, "Dici, '%s'\r\n", argument);
			send_to_char (buf, ch);
		}
	}
}


ACMD (do_gsay)
{
	struct char_data *k;
	struct follow_type *f;

	skip_spaces (&argument);

	if (!AFF_FLAGGED (ch, AFF_GROUP))
	{
		send_to_char ("Ma non sei mebro di nessun gruppo!\r\n", ch);
		return;
	}
	if (!*argument)
		send_to_char ("Cosa vuoi dire al gruppo?\r\n", ch);
	else
	{
		if (ch->master)
			k = ch->master;
		else
			k = ch;

		sprintf (buf, "$n dice al gruppo, '%s'", argument);

		if (AFF_FLAGGED (k, AFF_GROUP) && (k != ch))
			act (buf, FALSE, ch, 0, k, TO_VICT | TO_SLEEP);
		for (f = k->followers; f; f = f->next)
			if (AFF_FLAGGED (f->follower, AFF_GROUP) && (f->follower != ch))
				act (buf, FALSE, ch, 0, f->follower, TO_VICT | TO_SLEEP);

		if (PRF_FLAGGED (ch, PRF_NOREPEAT))
			send_to_char (OK, ch);
		else
		{
			sprintf (buf, "Dici al gruppo, '%s'\r\n", argument);
			send_to_char (buf, ch);
		}
	}
}


void
perform_tell (struct char_data *ch, struct char_data *vict, char *arg)
{
	send_to_char (CCRED (vict, C_NRM), vict);
	sprintf (buf, "$n ti dice, '%s'", arg);
	act (buf, FALSE, ch, 0, vict, TO_VICT | TO_SLEEP);
	send_to_char (CCNRM (vict, C_NRM), vict);

	if (!IS_NPC (ch) && PRF_FLAGGED (ch, PRF_NOREPEAT))
		send_to_char (OK, ch);
	else
	{
		send_to_char (CCRED (ch, C_CMP), ch);
		sprintf (buf, "Dici a $N, '%s'", arg);
		act (buf, FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
		act ("$n dice qualcosa a $N", 0, ch, 0, vict, TO_NOTVICT | TO_SLEEP);
		send_to_char (CCNRM (ch, C_CMP), ch);
	}

	if (!IS_NPC (vict) && !IS_NPC (ch))
	{
		GET_LAST_TELL (vict) = GET_IDNUM (ch);
	}
}

int
is_tell_ok (struct char_data *ch, struct char_data *vict)
{
	if (ch == vict)
	{
		send_to_char ("Provi a dirti qualcosa.\r\n", ch);
	}
	else if (!IS_NPC (ch) && PRF_FLAGGED (ch, PRF_NOTELL))
	{
		send_to_char ("Non puoi dire nulla quando hai notell on.\r\n", ch);
	}
	else if (ROOM_FLAGGED (ch->in_room, ROOM_SOUNDPROOF))
	{
		send_to_char ("I muri sembrano assorbire le tue parole.\r\n", ch);
	}
	else if (!IS_NPC (vict) && !vict->desc)	/* linkless */
	{
		act ("$E e' al momento disconesso.", FALSE, ch, 0, vict,
			 TO_CHAR | TO_SLEEP);
	}
	else if (PLR_FLAGGED (vict, PLR_WRITING))
	{
		act ("Sta scrivendo un messaggio e non puo' sentirti al momento.",
			 FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
	}
	else if ((!IS_NPC (vict) && PRF_FLAGGED (vict, PRF_NOTELL))
			 || ROOM_FLAGGED (vict->in_room, ROOM_SOUNDPROOF))
	{
		act ("Non puo' sentirti.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
	}
	else if (PRF_FLAGGED (vict, PRF_AFK))
	{

		act ("E' momentanemanete lontano dalla tastiera.", FALSE, ch, 0, vict,
			 TO_CHAR | TO_SLEEP);
	}
	else if (GET_LEVEL (ch) < LVL_IMMORT && (vict->in_room != ch->in_room))
	{
		act ("Non c'e' $N qui.", FALSE, ch, 0, vict, TO_CHAR | TO_SLEEP);
	}
	else
	{
		return (TRUE);
	}

	return (FALSE);
}

/*
 * Yes, do_tell probably could be combined with whisper and ask, but
 * called frequently, and should IMHO be kept as tight as possible.
 */
ACMD (do_tell)
{
	struct char_data *vict = NULL;

	half_chop (argument, buf, buf2);

	if (!*buf || !*buf2)
	{
		send_to_char ("A chi speri di dire qualcosa??\r\n", ch);
	}
	else if (GET_LEVEL (ch) < LVL_IMMORT
			 && !(vict = get_player_vis (ch, buf, NULL, FIND_CHAR_WORLD)))
	{
		send_to_char (NOPERSON, ch);
	}
	else if (GET_LEVEL (ch) >= LVL_IMMORT
			 && !(vict = get_char_vis (ch, buf, NULL, FIND_CHAR_WORLD)))
	{
		send_to_char (NOPERSON, ch);
	}
	else if (is_tell_ok (ch, vict))
	{
		perform_tell (ch, vict, buf2);
	}
}


ACMD (do_reply)
{
	struct char_data *tch = character_list;

	if (IS_NPC (ch))
		return;

	skip_spaces (&argument);

	if (GET_LAST_TELL (ch) == NOBODY)
		send_to_char ("You have no-one to reply to!\r\n", ch);
	else if (!*argument)
		send_to_char ("What is your reply?\r\n", ch);
	else
	{
		/*
		 * Make sure the person you're replying to is still playing by searching
		 * for them.  Note, now last tell is stored as player IDnum instead of
		 * a pointer, which is much better because it's safer, plus will still
		 * work if someone logs out and back in again.
		 */

		/*
		 * XXX: A descriptor list based search would be faster although
		 *      we could not find link dead people.  Not that they can
		 *      hear tells anyway. :) -gg 2/24/98
		 */
		while (tch != NULL
			   && (IS_NPC (tch) || GET_IDNUM (tch) != GET_LAST_TELL (ch)))
			tch = tch->next;

		if (tch == NULL)
			send_to_char ("They are no longer playing.\r\n", ch);
		else if (is_tell_ok (ch, tch))
			perform_tell (ch, tch, argument);
	}
}


ACMD (do_spec_comm)
{
	struct char_data *vict;
	const char *action_sing, *action_plur, *action_others;

	switch (subcmd)
	{
	case SCMD_WHISPER:
		action_sing = "sussuri";
		action_plur = "sussurano";
		action_others = "$n sussura qualcosa a $N.";
		break;

	case SCMD_ASK:
		action_sing = "domandi";
		action_plur = "domandano";
		action_others = "$n domanda a $N un interrogativo.";
		break;

	default:
		action_sing = "oops";
		action_plur = "oopses";
		action_others = "$n si impappina cercando di parlare a $N.";
		break;
	}

	half_chop (argument, buf, buf2);

	if (!*buf || !*buf2)
	{
		sprintf (buf, "Whom do you want to %s.. and what??\r\n", action_sing);
		send_to_char (buf, ch);
	}
	else if (!(vict = get_char_vis (ch, buf, NULL, FIND_CHAR_ROOM)))
		send_to_char (NOPERSON, ch);
	else if (vict == ch)
		send_to_char ("Non puoi portare la tua bocca al tuo orecchio...\r\n",
					  ch);
	else
	{
		sprintf (buf, "$n ti %s, '%s'", action_plur, buf2);
		act (buf, FALSE, ch, 0, vict, TO_VICT);
		if (PRF_FLAGGED (ch, PRF_NOREPEAT))
			send_to_char (OK, ch);
		else
		{
			sprintf (buf, "Tu %s %s, '%s'\r\n", action_sing, GET_NAME (vict),
					 buf2);
			send_to_char (buf, ch);
		}
		act (action_others, FALSE, ch, 0, vict, TO_NOTVICT);
	}
}



#define MAX_NOTE_LENGTH 1000	/* arbitrary */

ACMD (do_write)
{
	struct obj_data *paper, *pen = NULL;
	char *papername, *penname;

	papername = buf1;
	penname = buf2;

	two_arguments (argument, papername, penname);

	if (!ch->desc)
		return;

	if (!*papername)
	{							/* nothing was delivered */
		send_to_char
			("Scrivere?  Con cosa?  SU cosa?  Cosa vuoi provare di fare?!?\r\n",
			 ch);
		return;
	}
	if (*penname)
	{							/* there were two arguments */
		if (!
			(paper = get_obj_in_list_vis (ch, papername, NULL, ch->carrying)))
		{
			sprintf (buf, "Non hai %s.\r\n", papername);
			send_to_char (buf, ch);
			return;
		}
		if (!(pen = get_obj_in_list_vis (ch, penname, NULL, ch->carrying)))
		{
			sprintf (buf, "Non hai %s.\r\n", penname);
			send_to_char (buf, ch);
			return;
		}
	}
	else
	{							/* there was one arg.. let's see what we can find */
		if (!
			(paper = get_obj_in_list_vis (ch, papername, NULL, ch->carrying)))
		{
			sprintf (buf, "Non c'e' %s nel tuo inventario.\r\n", papername);
			send_to_char (buf, ch);
			return;
		}
		if (GET_OBJ_TYPE (paper) == ITEM_PEN)
		{						/* oops, a pen.. */
			pen = paper;
			paper = NULL;
		}
		else if (GET_OBJ_TYPE (paper) != ITEM_NOTE)
		{
			send_to_char ("Queste oggetti non vanno bene per scrivere.\r\n",
						  ch);
			return;
		}
		/* One object was found.. now for the other one. */
		if (!GET_EQ (ch, WEAR_HOLD))
		{
			sprintf (buf, "Non puoi scrivere con %s %s da soli.\r\n",
					 AN (papername), papername);
			send_to_char (buf, ch);
			return;
		}
		if (!CAN_SEE_OBJ (ch, GET_EQ (ch, WEAR_HOLD)))
		{
			send_to_char
				("Gli oggetti sulle tue mani sono invisibili!  Yeech!!\r\n",
				 ch);
			return;
		}
		if (pen)
			paper = GET_EQ (ch, WEAR_HOLD);
		else
			pen = GET_EQ (ch, WEAR_HOLD);
	}


	/* ok.. now let's see what kind of stuff we've found */
	if (GET_OBJ_TYPE (pen) != ITEM_PEN)
		act ("$p non va bene per scrivere.", FALSE, ch, pen, 0, TO_CHAR);
	else if (GET_OBJ_TYPE (paper) != ITEM_NOTE)
		act ("Non puoi scrivere sopra $p.", FALSE, ch, paper, 0, TO_CHAR);
	else if (paper->action_description)
		send_to_char ("C'e scritto qualcosa sopra.\r\n", ch);
	else
	{
		/* we can write - hooray! */
		send_to_char
			("Scrivi le tue note.  Termina con '@' su una nuova riga.\r\n",
			 ch);
		act ("$n inizia a scrivere delle note.", TRUE, ch, 0, 0, TO_ROOM);
		string_write (ch->desc, &paper->action_description, MAX_NOTE_LENGTH,
					  0, NULL);
	}
}



ACMD (do_page)
{
	struct descriptor_data *d;
	struct char_data *vict;

	half_chop (argument, arg, buf2);

	if (IS_NPC (ch))
		send_to_char ("Monsters can't page.. go away.\r\n", ch);
	else if (!*arg)
		send_to_char ("Whom do you wish to page?\r\n", ch);
	else
	{
		sprintf (buf, "\007\007*$n* %s", buf2);
		if (!str_cmp (arg, "all"))
		{
			if (GET_LEVEL (ch) > LVL_GOD)
			{
				for (d = descriptor_list; d; d = d->next)
					if (STATE (d) == CON_PLAYING && d->character)
						act (buf, FALSE, ch, 0, d->character, TO_VICT);
			}
			else
				send_to_char
					("You will never be godly enough to do that!\r\n", ch);
			return;
		}
		if ((vict = get_char_vis (ch, arg, NULL, FIND_CHAR_WORLD)) != NULL)
		{
			act (buf, FALSE, ch, 0, vict, TO_VICT);
			if (PRF_FLAGGED (ch, PRF_NOREPEAT))
				send_to_char (OK, ch);
			else
				act (buf, FALSE, ch, 0, vict, TO_CHAR);
		}
		else
			send_to_char ("There is no such person in the game!\r\n", ch);
	}
}


/**********************************************************************
 * generalized communication func, originally by Fred C. Merkel (Torg) *
  *********************************************************************/

ACMD (do_gen_comm)
{
	struct descriptor_data *i;
	char color_on[24];
	char buf_meo1[30];
	char buf_meo2[30];

	/* Array of flags which must _not_ be set in order for comm to be heard */
	int channels[] = {
		0,
		PRF_DEAF,
		PRF_NOGOSS,
		PRF_NOAUCT,
		PRF_NOGRATZ,
		0
	};

	/*
	 * com_msgs: [0] Message if you can't perform the action because of noshout
	 *           [1] name of the action
	 *           [2] message if you're not on the channel
	 *           [3] a color string.
	 */
	const char *com_msgs[][4] = {
		{"You cannot holler!!\r\n",
		 "holler",
		 "",
		 KYEL},

		{"You cannot shout!!\r\n",
		 "shout",
		 "Turn off your noshout flag first!\r\n",
		 KYEL},

		{"You cannot gossip!!\r\n",
		 "gossip",
		 "You aren't even on the channel!\r\n",
		 KYEL},

		{"You cannot auction!!\r\n",
		 "auction",
		 "You aren't even on the channel!\r\n",
		 KMAG},

		{"You cannot congratulate!\r\n",
		 "congrat",
		 "You aren't even on the channel!\r\n",
		 KGRN}
	};

	/* to keep pets, etc from being ordered to shout */
	if (!ch->desc)
		return;

	if (PLR_FLAGGED (ch, PLR_NOSHOUT))
	{
		send_to_char (com_msgs[subcmd][0], ch);
		return;
	}
	if (ROOM_FLAGGED (ch->in_room, ROOM_SOUNDPROOF))
	{
		send_to_char ("I muri sembrano assorbire le tue parole.\r\n", ch);
		return;
	}
	/* level_can_shout defined in config.c */
	if (GET_LEVEL (ch) < level_can_shout)
	{
		sprintf (buf1, "Devi essere almeno %d livello prima di poter %s.\r\n",
				 level_can_shout, com_msgs[subcmd][1]);
		send_to_char (buf1, ch);
		return;
	}
	/* make sure the char is on the channel */
	if (PRF_FLAGGED (ch, channels[subcmd]))
	{
		send_to_char (com_msgs[subcmd][2], ch);
		return;
	}
	/* skip leading spaces */
	skip_spaces (&argument);

	/* make sure that there is something there to say! */
	if (!*argument)
	{
		sprintf (buf1, "Si, %s, caro, %s, si possiamo fare, ma COSA???\r\n",
				 com_msgs[subcmd][1], com_msgs[subcmd][1]);
		send_to_char (buf1, ch);
		return;
	}
	if (subcmd == SCMD_HOLLER)
	{
		if (GET_MOVE (ch) < holler_move_cost)
		{
			send_to_char ("Sei troppo stanco per urlare.\r\n", ch);
			return;
		}
		else
			GET_MOVE (ch) -= holler_move_cost;
	}
	/* set up the color on code */

	strcpy (color_on, com_msgs[subcmd][3]);
	strcpy (buf_meo1, com_msgs[subcmd][1]);
	strcpy (buf_meo2, com_msgs[subcmd][1]);

	if (strcmp (buf_meo1, "gossip") == 0)
	{
		strcpy (buf_meo1, "dici");
		strcpy (buf_meo2, "vi dice");
	}

	if (strcmp (buf_meo1, "shout") == 0 || strcmp (buf_meo1, "holler") == 0)
	{
		strcpy (buf_meo1, "urli");
		strcpy (buf_meo2, "urla");
	}

	/* first, set up strings to be given to the communicator */
	if (PRF_FLAGGED (ch, PRF_NOREPEAT))
		send_to_char (OK, ch);
	else
	{
		if (COLOR_LEV (ch) >= C_CMP)
			sprintf (buf1, "%sTu %s, '%s'%s", color_on, buf_meo1, argument,
					 KNRM);
		else
			sprintf (buf1, "Tu %s, '%s'", buf_meo1, argument);

		act (buf1, FALSE, ch, 0, 0, TO_CHAR | TO_SLEEP);
	}

	sprintf (buf, "$n %s, '%s'", buf_meo2, argument);

	/* now send all the strings out */
	for (i = descriptor_list; i; i = i->next)
	{
		if (STATE (i) == CON_PLAYING && i != ch->desc && i->character &&
			!PRF_FLAGGED (i->character, channels[subcmd]) &&
			!PLR_FLAGGED (i->character, PLR_WRITING) &&
			!ROOM_FLAGGED (i->character->in_room, ROOM_SOUNDPROOF))
		{

			if (subcmd == SCMD_SHOUT &&
				((world
				  [ch->in_room].zone != world[i->character->in_room].zone)
				 || !AWAKE (i->character)))
				continue;

			if (COLOR_LEV (i->character) >= C_NRM)
				send_to_char (color_on, i->character);
			act (buf, FALSE, ch, 0, i->character, TO_VICT | TO_SLEEP);
			if (COLOR_LEV (i->character) >= C_NRM)
				send_to_char (KNRM, i->character);
		}
	}
}


ACMD (do_qcomm)
{
	struct descriptor_data *i;

	if (!PRF_FLAGGED (ch, PRF_QUEST))
	{
		send_to_char ("Non fai parte della quest!\r\n", ch);
		return;
	}
	skip_spaces (&argument);

	if (!*argument)
	{
		sprintf (buf, "%s?  Si, bello, %s noi possiamo fare, ma COSA??\r\n",
				 CMD_NAME, CMD_NAME);
		CAP (buf);
		send_to_char (buf, ch);
	}
	else
	{
		if (PRF_FLAGGED (ch, PRF_NOREPEAT))
			send_to_char (OK, ch);
		else
		{
			if (subcmd == SCMD_QSAY)
				sprintf (buf, "You quest-say, '%s'", argument);
			else
				strcpy (buf, argument);
			act (buf, FALSE, ch, 0, argument, TO_CHAR);
		}

		if (subcmd == SCMD_QSAY)
			sprintf (buf, "$n quest-says, '%s'", argument);
		else
			strcpy (buf, argument);

		for (i = descriptor_list; i; i = i->next)
			if (STATE (i) == CON_PLAYING && i != ch->desc &&
				PRF_FLAGGED (i->character, PRF_QUEST))
				act (buf, 0, ch, 0, i->character, TO_VICT | TO_SLEEP);
	}
}


ACMD (do_pray)
{
	struct descriptor_data *i;
	char buf_me[30];
	char buf_you[30];

	/* to keep pets, etc from being ordered to shout */
	if (!ch->desc)
		return;

	/* skip leading spaces */
	skip_spaces (&argument);

	/* make sure that there is something there to say! */
	if (!*argument)
	{
		send_to_char ("Si, pregare, caro, pregare, si possiamo fare, ma COSA???\r\n", ch);
		return;
	}
	
	/* set up the color on code */

	strcpy (buf1, "pray");
	

	if (strcmp (buf1, "pray") == 0)
	{
		strcpy (buf_me, "preghi");
		strcpy (buf_you, "prega");
	}

	
	/* first, set up strings to be given to the communicator */
	if (PRF_FLAGGED (ch, PRF_NOREPEAT))
		send_to_char (OK, ch);
	else
	{
		if (COLOR_LEV (ch) >= C_CMP)
			sprintf (buf1, "%sTu %s, '%s'%s", KYEL, buf_me, argument,
					 KNRM);
		else
			sprintf (buf1, "Tu %s, '%s'", buf_me, argument);

		act (buf1, FALSE, ch, 0, 0, TO_CHAR | TO_SLEEP);
		
		act ("$n sta pregando gli dei", FALSE, ch, 0, 0, TO_ROOM);
	}

	sprintf (buf, "$n %s, '%s'", buf_you, argument);

	/* now send all the strings out */
	for (i = descriptor_list; i; i = i->next)
	{
		if (STATE (i) == CON_PLAYING && i != ch->desc && i->character &&
			GET_REAL_LEVEL(i->character) >= LVL_IMMORT )
		{

			if (COLOR_LEV (i->character) >= C_NRM)
				send_to_char (KYEL, i->character);
			act (buf, FALSE, ch, 0, i->character, TO_VICT | TO_SLEEP);
			if (COLOR_LEV (i->character) >= C_NRM)
				send_to_char (KNRM, i->character);
		}
	}
}

/* ************************************************************************
*   File: act.other.c                                   Part of CircleMUD *
*  Usage: Miscellaneous player-level commands                             *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define __ACT_OTHER_C__

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
#include "house.h"
#include "constants.h"

/* extern variables */
extern struct room_data *world;
extern struct descriptor_data *descriptor_list;
extern struct spell_info_type spell_info[];
extern struct index_data *mob_index;
extern char *class_abbrevs[];
extern int free_rent;
extern int pt_allowed;
extern int max_filesize;
extern int nameserver_is_slow;
extern int auto_save;
extern int track_through_doors;


/* extern procedures */
void list_skills (struct char_data *ch);
void appear (struct char_data *ch);
void write_aliases (struct char_data *ch);
void perform_immort_vis (struct char_data *ch);
SPECIAL (shop_keeper);
ACMD (do_gen_comm);
void die (struct char_data *ch);
void Crash_rentsave (struct char_data *ch, int cost);
char *find_exdesc (char *word, struct extra_descr_data *list);

/* local functions */
ACMD (do_quit);
ACMD (do_save);
ACMD (do_not_here);
ACMD (do_sneak);
ACMD (do_hide);
ACMD (do_steal);
ACMD (do_practice);
ACMD (do_visible);
ACMD (do_title);
ACMD (do_gain);
int perform_group (struct char_data *ch, struct char_data *vict);
void disband_group (struct char_data *ch, struct char_data *vict);
void print_group (struct char_data *ch);
int check_group (struct char_data *ch);
ACMD (do_group);
ACMD (do_ungroup);
ACMD (do_report);
ACMD (do_split);
ACMD (do_use);
ACMD (do_wimpy);
ACMD (do_display);
ACMD (do_gen_write);
ACMD (do_gen_tog);
ACMD (do_first_aid);
ACMD (do_ki_shield);


ACMD (do_quit)
{
	if (IS_NPC (ch) || !ch->desc)
	{
		return;
	}

	if (subcmd != SCMD_QUIT && GET_LEVEL (ch) < LVL_IMMORT)
	{
		send_to_char ("Devi solo digitare quit, per uscire!\r\n", ch);
	}
	else if (GET_POS (ch) == POS_FIGHTING)
	{
		send_to_char ("Non puoi!  Stai combattendo per la tua vita!\r\n", ch);
	}
	else if (GET_POS (ch) < POS_STUNNED)
	{
		send_to_char ("Stai morendo prima del tempo...\r\n", ch);
		die (ch);
	}
	else
	{
		int loadroom = ch->in_room;

		act ("$n ha lasciato il gioco.", TRUE, ch, 0, 0, TO_ROOM);
		sprintf (buf, "%s e' uscito dal gioco.", GET_NAME (ch));
		mudlog (buf, NRM, MAX (LVL_IMMORT, GET_INVIS_LEV (ch)), TRUE);
		send_to_char ("Arrivederci, vieni presto a ritrovarci!\r\n", ch);

		/*  We used to check here for duping attempts, but we may as well
		 *  do it right in extract_char(), since there is no check if a
		 *  player rents out and it can leave them in an equally screwy
		 *  situation.
		 */

		if (free_rent)
		{
			Crash_rentsave (ch, 0);
		}

		extract_char (ch);		/* Char is saved in extract char */

		/* If someone is quitting in their house, let them load back here */
		if (ROOM_FLAGGED (loadroom, ROOM_HOUSE))
		{
			save_char (ch, loadroom);
		}
	}
}


ACMD (do_save)
{
	if (IS_NPC (ch) || !ch->desc)
		return;

	/* Only tell the char we're saving if they actually typed "save" */
	if (cmd)
	{
		/*
		 * This prevents item duplication by two PC's using coordinated saves
		 * (or one PC with a house) and system crashes. Note that houses are
		 * still automatically saved without this enabled. This code assumes
		 * that guest immortals aren't trustworthy. If you've disabled guest
		 * immortal advances from mortality, you may want < instead of <=.
		 */
		if (auto_save && GET_LEVEL (ch) <= LVL_IMMORT)
		{
			send_to_char ("Saving aliases.\r\n", ch);
			write_aliases (ch);
			return;
		}
		sprintf (buf, "Saving %s and aliases.\r\n", GET_NAME (ch));
		send_to_char (buf, ch);
	}

	write_aliases (ch);
	save_char (ch, NOWHERE);
	Crash_crashsave (ch);
	if (ROOM_FLAGGED (ch->in_room, ROOM_HOUSE_CRASH))
		House_crashsave (GET_ROOM_VNUM (IN_ROOM (ch)));
}


/* generic function for commands which are normally overridden by
   special procedures - i.e., shop commands, mail commands, etc. */
ACMD (do_not_here)
{
	send_to_char ("Sorry, but you cannot do that here!\r\n", ch);
}



ACMD (do_sneak)
{
	struct affected_type af;
	byte percent;

	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_SNEAK))
	{
		send_to_char ("You have no idea how to do that.\r\n", ch);
		return;
	}
	send_to_char ("Okay, you'll try to move silently for a while.\r\n", ch);
	if (AFF_FLAGGED (ch, AFF_SNEAK))
		affect_from_char (ch, SKILL_SNEAK);

	percent = number (1, 101);	/* 101% is a complete failure */

	if (percent >
		GET_SKILL (ch, SKILL_SNEAK) + dex_app_skill[GET_DEX (ch)].sneak)
		return;

	af.type = SKILL_SNEAK;
	af.duration = GET_LEVEL (ch);
	af.modifier = 0;
	af.location = APPLY_NONE;
	af.bitvector = AFF_SNEAK;
	affect_to_char (ch, &af);
}



ACMD (do_hide)
{
	byte percent;

	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_HIDE))
	{
		send_to_char ("You have no idea how to do that.\r\n", ch);
		return;
	}

	send_to_char ("You attempt to hide yourself.\r\n", ch);

	if (AFF_FLAGGED (ch, AFF_HIDE))
		REMOVE_BIT (AFF_FLAGS (ch), AFF_HIDE);

	percent = number (1, 101);	/* 101% is a complete failure */

	if (percent >
		GET_SKILL (ch, SKILL_HIDE) + dex_app_skill[GET_DEX (ch)].hide)
		return;

	SET_BIT (AFF_FLAGS (ch), AFF_HIDE);
}




ACMD (do_steal)
{
	struct char_data *vict;
	struct obj_data *obj;
	char vict_name[MAX_INPUT_LENGTH], obj_name[MAX_INPUT_LENGTH];
	int percent, gold, eq_pos, pcsteal = 0, ohoh = 0;

	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_STEAL))
	{
		send_to_char ("You have no idea how to do that.\r\n", ch);
		return;
	}
	if (ROOM_FLAGGED (IN_ROOM (ch), ROOM_PEACEFUL))
	{
		send_to_char
			("This room just has such a peaceful, easy feeling...\r\n", ch);
		return;
	}

	two_arguments (argument, obj_name, vict_name);

	if (!(vict = get_char_vis (ch, vict_name, NULL, FIND_CHAR_ROOM)))
	{
		send_to_char ("Steal what from who?\r\n", ch);
		return;
	}
	else if (vict == ch)
	{
		send_to_char ("Come on now, that's rather stupid!\r\n", ch);
		return;
	}

	/* 101% is a complete failure */
	percent = number (1, 101) - dex_app_skill[GET_DEX (ch)].p_pocket;

	if (GET_POS (vict) < POS_SLEEPING)
		percent = -1;			/* ALWAYS SUCCESS, unless heavy object. */

	if (!pt_allowed && !IS_NPC (vict))
		pcsteal = 1;

	if (!AWAKE (vict))			/* Easier to steal from sleeping people. */
		percent -= 50;

	/* NO NO With Imp's and Shopkeepers, and if player thieving is not allowed */
	if (GET_LEVEL (vict) >= LVL_IMMORT || pcsteal ||
		GET_MOB_SPEC (vict) == shop_keeper)
		percent = 101;			/* Failure */

	if (str_cmp (obj_name, "coins") && str_cmp (obj_name, "gold"))
	{

		if (!(obj = get_obj_in_list_vis (ch, obj_name, NULL, vict->carrying)))
		{

			for (eq_pos = 0; eq_pos < NUM_WEARS; eq_pos++)
				if (GET_EQ (vict, eq_pos) &&
					(isname (obj_name, GET_EQ (vict, eq_pos)->name)) &&
					CAN_SEE_OBJ (ch, GET_EQ (vict, eq_pos)))
				{
					obj = GET_EQ (vict, eq_pos);
					break;
				}
			if (!obj)
			{
				act ("$E hasn't got that item.", FALSE, ch, 0, vict, TO_CHAR);
				return;
			}
			else
			{					/* It is equipment */
				if ((GET_POS (vict) > POS_STUNNED))
				{
					send_to_char ("Steal the equipment now?  Impossible!\r\n",
								  ch);
					return;
				}
				else
				{
					act ("You unequip $p and steal it.", FALSE, ch, obj, 0,
						 TO_CHAR);
					act ("$n steals $p from $N.", FALSE, ch, obj, vict,
						 TO_NOTVICT);
					obj_to_char (unequip_char (vict, eq_pos), ch);
				}
			}
		}
		else
		{						/* obj found in inventory */

			percent += GET_OBJ_WEIGHT (obj);	/* Make heavy harder */

			if (percent > GET_SKILL (ch, SKILL_STEAL))
			{
				ohoh = TRUE;
				send_to_char ("Oops..\r\n", ch);
				act ("$n tried to steal something from you!", FALSE, ch, 0,
					 vict, TO_VICT);
				act ("$n tries to steal something from $N.", TRUE, ch, 0,
					 vict, TO_NOTVICT);
			}
			else
			{					/* Steal the item */
				if (IS_CARRYING_N (ch) + 1 < CAN_CARRY_N (ch))
				{
					if (IS_CARRYING_W (ch) + GET_OBJ_WEIGHT (obj) <
						CAN_CARRY_W (ch))
					{
						obj_from_char (obj);
						obj_to_char (obj, ch);
						send_to_char ("Got it!\r\n", ch);
					}
				}
				else
					send_to_char ("You cannot carry that much.\r\n", ch);
			}
		}
	}
	else
	{							/* Steal some coins */
		if (AWAKE (vict) && (percent > GET_SKILL (ch, SKILL_STEAL)))
		{
			ohoh = TRUE;
			send_to_char ("Oops..\r\n", ch);
			act ("You discover that $n has $s hands in your wallet.", FALSE,
				 ch, 0, vict, TO_VICT);
			act ("$n tries to steal gold from $N.", TRUE, ch, 0, vict,
				 TO_NOTVICT);
		}
		else
		{
			/* Steal some gold coins */
			gold = (int) ((GET_GOLD (vict) * number (1, 10)) / 100);
			gold = MIN (1782, gold);
			if (gold > 0)
			{
				GET_GOLD (ch) += gold;
				GET_GOLD (vict) -= gold;
				if (gold > 1)
				{
					sprintf (buf, "Bingo!  You got %d gold coins.\r\n", gold);
					send_to_char (buf, ch);
				}
				else
				{
					send_to_char
						("You manage to swipe a solitary gold coin.\r\n", ch);
				}
			}
			else
			{
				send_to_char ("You couldn't get any gold...\r\n", ch);
			}
		}
	}

	if (ohoh && IS_NPC (vict) && AWAKE (vict))
		hit (vict, ch, TYPE_UNDEFINED);
}



ACMD (do_practice)
{
	if (IS_NPC (ch))
		return;

	one_argument (argument, arg);

	if (*arg)
		send_to_char ("You can only practice skills in your guild.\r\n", ch);
	else
		list_skills (ch);
}

ACMD (do_gain)
{
	/* non fa nulla, serve per spec_proc SPECIAL(master) */
	return;
}

ACMD (do_visible)
{
	if (GET_LEVEL (ch) >= LVL_IMMORT)
	{
		perform_immort_vis (ch);
		return;
	}

	if AFF_FLAGGED
		(ch, AFF_INVISIBLE)
	{
		appear (ch);
		send_to_char ("Rompi l'incantesimo di invisibilita'.\r\n", ch);
	}
	else
		send_to_char ("Sei gia' visibile.\r\n", ch);
}



ACMD (do_title)
{
	skip_spaces (&argument);
	delete_doubledollar (argument);

	if (IS_NPC (ch))
		send_to_char ("Your title is fine... go away.\r\n", ch);
	else if (PLR_FLAGGED (ch, PLR_NOTITLE))
		send_to_char
			("You can't title yourself -- you shouldn't have abused it!\r\n",
			 ch);
	else if (strstr (argument, "(") || strstr (argument, ")"))
		send_to_char ("Titles can't contain the ( or ) characters.\r\n", ch);
	else if (strlen (argument) > MAX_TITLE_LENGTH)
	{
		sprintf (buf, "Sorry, titles can't be longer than %d characters.\r\n",
				 MAX_TITLE_LENGTH);
		send_to_char (buf, ch);
	}
	else
	{
		set_title (ch, argument);
		sprintf (buf, "Okay, you're now %s %s.\r\n", GET_NAME (ch),
				 GET_TITLE (ch));
		send_to_char (buf, ch);
	}
}


int
perform_group (struct char_data *ch, struct char_data *vict)
{
	if (AFF_FLAGGED (vict, AFF_GROUP) || !CAN_SEE (ch, vict))
		return (0);

	SET_BIT (AFF_FLAGS (vict), AFF_GROUP);
	if (ch != vict)
		act ("$N e' entrato come membro nel tuo gruppo.", FALSE, ch, 0, vict,
			 TO_CHAR);
	act ("Sei entrato nel gruppo di $n.", FALSE, ch, 0, vict, TO_VICT);
	act ("$N e' entrato nel gruppo di $n.", FALSE, ch, 0, vict, TO_NOTVICT);
	return (1);
}


void
print_group (struct char_data *ch)
{
	struct char_data *k;
	struct follow_type *f;
    
	if (!AFF_FLAGGED (ch, AFF_GROUP))
		send_to_char ("Ma non sei membro di nessun gruppo!\r\n", ch);
	else
	{
		send_to_char ("Il tuo gruppo e' composto da:\r\n", ch);

		k = (ch->master ? ch->master : ch);
        
        if (k == ch)
        {
            if (check_group(ch)==0)
            {
                send_to_char("Non ci sono piu' membri, il gruppo e' disbandato.\r\n", ch);
                REMOVE_BIT (AFF_FLAGS (ch), AFF_GROUP);
                return;
            }
        }

		if (AFF_FLAGGED (k, AFF_GROUP))
		{
			sprintf (buf,
					 "     [H:%3d%% M:%3d%% V:%3d%%] [%2d %s] $N (Leader del gruppo)",
					 (GET_HIT (k) * 100) / GET_MAX_HIT (k),
					 (GET_MANA (k) * 100) / GET_MAX_MANA (k),
					 (GET_MOVE (k) * 100) / GET_MAX_MOVE (k), GET_LEVEL (k),
					 CLASS_ABBR (k));
			act (buf, FALSE, ch, 0, k, TO_CHAR);
		}

		for (f = k->followers; f; f = f->next)
		{
			if (!AFF_FLAGGED (f->follower, AFF_GROUP))
				continue;

			sprintf (buf, "     [H:%3d%% M:%3d%% V:%3d%%] [%2d %s] $N",
					 (GET_HIT (f->follower) * 100) /
					 GET_MAX_HIT (f->follower),
					 (GET_MANA (f->follower) * 100) /
					 GET_MAX_MANA (f->follower),
					 (GET_MOVE (f->follower) * 100) /
					 GET_MAX_MOVE (f->follower), GET_LEVEL (f->follower),
					 CLASS_ABBR (f->follower));
			act (buf, FALSE, ch, 0, f->follower, TO_CHAR);
		}
	}
}

int 
check_group (struct char_data *ch)
{
    struct char_data *k;
	struct follow_type *f;
    int count = 0;

	if (!AFF_FLAGGED (ch, AFF_GROUP))
		return 0;
	else
	{
		//k = (ch->master ? ch->master : ch);
        k = ch;

		for (f = k->followers; f; f = f->next)
		{
			if (!AFF_FLAGGED (f->follower, AFF_GROUP))
            {
				continue;
            }
			count++;
		}
	}
    return count;
}



ACMD (do_group)
{
	struct char_data *vict;
	struct follow_type *f;
	int found;

	one_argument (argument, buf);

	if (!*buf)
	{
		print_group (ch);
		return;
	}

	if (ch->master)
	{
		act
			("You can not enroll group members without being head of a group.",
			 FALSE, ch, 0, 0, TO_CHAR);
		return;
	}

	if (!str_cmp (buf, "all"))
	{
		//perform_group (ch, ch);
        send_to_char("Formi un nuovo gruppo.\r\n",ch);
        SET_BIT (AFF_FLAGS (ch), AFF_GROUP);
		for (found = 0, f = ch->followers; f; f = f->next)
        {
			found += perform_group (ch, f->follower);
        }
		if (!found)
        {
			send_to_char
				("Tutti quelli che ti seguono sono nel tuo gruppo.\r\n", ch);
        }
		return;
	}

	if (!(vict = get_char_vis (ch, buf, NULL, FIND_CHAR_ROOM)))
    {
		send_to_char (NOPERSON, ch);
    }
	else if ((vict->master != ch) && (vict != ch))
    {
		act ("$N Deve seguirti per poter entrare nel tuo gruppo.", FALSE, ch,
			 0, vict, TO_CHAR);
    }
	else
	{
		if (!AFF_FLAGGED (vict, AFF_GROUP))
		{
			if (!AFF_FLAGGED (ch, AFF_GROUP))
			{
				SET_BIT (AFF_FLAGS (ch), AFF_GROUP);
                send_to_char("Formi un nuovo gruppo.\r\n",ch);
                //perform_group (ch, ch);	//Se ch non e' nel gruppo, viene addato automaticamente
			}
			perform_group (ch, vict);
		}
		else
		{
			
            if (ch != vict)
			{
				act ("$N non e' piu' membro del tuo gruppo.", FALSE, ch, 0,
					 vict, TO_CHAR);
			}
			act ("Sei stato buttato fuori dal gruppo di $n!", FALSE, ch, 0,
				 vict, TO_VICT);
			act ("$N e' stato calciato fuori dal gruppo di $n!", FALSE, ch, 0,
				 vict, TO_NOTVICT);
			REMOVE_BIT (AFF_FLAGS (vict), AFF_GROUP);
            if (!AFF_FLAGGED (vict, AFF_CHARM))
            {
		        stop_follower (vict);
            }
            if (check_group(ch) == 0)
            {
                send_to_char("Non ci sono piu' membri, il gruppo e' disbandato.\r\n", ch);
                REMOVE_BIT (AFF_FLAGS (ch), AFF_GROUP);
            }
		}
	}
}


ACMD (do_ungroup)
{
	struct follow_type *f, *next_fol;
	struct char_data *tch;

	one_argument (argument, buf);

	if (!*buf)
	{
		if (ch->master || !(AFF_FLAGGED (ch, AFF_GROUP)))
		{
			send_to_char ("MA non guidi nessun gruppo!\r\n", ch);
			return;
		}
		sprintf (buf2, "%s ha lasciato il tuo gruppo.\r\n", GET_NAME (ch));
		for (f = ch->followers; f; f = next_fol)
		{
			next_fol = f->next;
			if (AFF_FLAGGED (f->follower, AFF_GROUP))
			{
				REMOVE_BIT (AFF_FLAGS (f->follower), AFF_GROUP);
				send_to_char (buf2, f->follower);
				if (!AFF_FLAGGED (f->follower, AFF_CHARM))
					stop_follower (f->follower);
			}
		}

		REMOVE_BIT (AFF_FLAGS (ch), AFF_GROUP);
		send_to_char ("Lasci il gruppo.\r\n", ch);
		return;
	}
	if (!(tch = get_char_vis (ch, buf, NULL, FIND_CHAR_ROOM)))
	{
		send_to_char ("Non c'e nessuna persona!\r\n", ch);
		return;
	}
	if (tch->master != ch)
	{
		send_to_char ("Quella persona non ti segue!\r\n", ch);
		return;
	}

	if (!AFF_FLAGGED (tch, AFF_GROUP))
	{
		send_to_char ("Quella persona non e' del tuo gruppo.\r\n", ch);
		return;
	}

	REMOVE_BIT (AFF_FLAGS (tch), AFF_GROUP);

	act ("$N non e' piu' membro del tuo gruppo.", FALSE, ch, 0, tch, TO_CHAR);
	act ("Sei stato buttato fuori dal gruppo di $n!", FALSE, ch, 0, tch,
		 TO_VICT);
	act ("$N e' stato calciato fuori dal gruppo di $n!", FALSE, ch, 0, tch,
		 TO_NOTVICT);
	if (!AFF_FLAGGED (tch, AFF_CHARM))
		stop_follower (tch);
}




ACMD (do_report)
{
	struct char_data *k;
	struct follow_type *f;

	if (!AFF_FLAGGED (ch, AFF_GROUP))
	{
		send_to_char ("MA non sei membro di nessun gruppo!\r\n", ch);
		return;
	}
	sprintf (buf, "%s riporta: H:%d%%, M:%d%%, V:%d%%\r\n",
			 GET_NAME (ch),
			 (GET_HIT (ch) * 100) / GET_MAX_HIT (ch),
			 (GET_MANA (ch) * 100) / GET_MAX_MANA (ch),
			 (GET_MOVE (ch) * 100) / GET_MAX_MOVE (ch));

	CAP (buf);

	k = (ch->master ? ch->master : ch);

	for (f = k->followers; f; f = f->next)
	{
		if (AFF_FLAGGED (f->follower, AFF_GROUP) && f->follower != ch)
		{
			send_to_char (buf, f->follower);
		}
	}
	if (k != ch)
	{
		send_to_char (buf, k);
	}


	sprintf (buf, "Riporti al gruppo: H:%d%%, M:%d%%, V:%d%%\r\n",
			 (GET_HIT (ch) * 100) / GET_MAX_HIT (ch),
			 (GET_MANA (ch) * 100) / GET_MAX_MANA (ch),
			 (GET_MOVE (ch) * 100) / GET_MAX_MOVE (ch));
	send_to_char (buf, ch);
}



ACMD (do_split)
{
	int amount, num, share, rest;
	struct char_data *k;
	struct follow_type *f;

	if (IS_NPC (ch))
		return;

	one_argument (argument, buf);

	if (is_number (buf))
	{
		amount = atoi (buf);
		if (amount <= 0)
		{
			send_to_char ("Spiacente, non puoi farlo.\r\n", ch);
			return;
		}
		if (amount > GET_GOLD (ch))
		{
			send_to_char ("Non hai tutti questi soldi.\r\n", ch);
			return;
		}
		k = (ch->master ? ch->master : ch);

		if (AFF_FLAGGED (k, AFF_GROUP) && (k->in_room == ch->in_room))
			num = 1;
		else
			num = 0;

		for (f = k->followers; f; f = f->next)
			if (AFF_FLAGGED (f->follower, AFF_GROUP) &&
				(!IS_NPC (f->follower)) &&
				(f->follower->in_room == ch->in_room))
				num++;

		if (num && AFF_FLAGGED (ch, AFF_GROUP))
		{
			share = amount / num;
			rest = amount % num;
		}
		else
		{
			send_to_char ("Con chi speri di dividere i soldi?\r\n", ch);
			return;
		}

		GET_GOLD (ch) -= share * (num - 1);

		sprintf (buf, "%s divide %d monete; ricevi %d.\r\n", GET_NAME (ch),
				 amount, share);
		if (rest)
		{
			sprintf (buf + strlen (buf),
					 "%d monet%s non %s divisibil%s, quindi %s"
					 " si tiene l%s monet%s.\r\n", rest,
					 (rest == 1) ? "a" : "e", (rest == 1) ? "e'" : "sono",
					 (rest == 1) ? "e" : "i", GET_NAME (ch),
					 (rest == 1) ? "a" : "e", (rest == 1) ? "a" : "e");
		}
		if (AFF_FLAGGED (k, AFF_GROUP) && (k->in_room == ch->in_room)
			&& !(IS_NPC (k)) && k != ch)
		{
			GET_GOLD (k) += share;
			send_to_char (buf, k);
		}
		for (f = k->followers; f; f = f->next)
		{
			if (AFF_FLAGGED (f->follower, AFF_GROUP) &&
				(!IS_NPC (f->follower)) &&
				(f->follower->in_room == ch->in_room) && f->follower != ch)
			{
				GET_GOLD (f->follower) += share;
				send_to_char (buf, f->follower);
			}
		}
		sprintf (buf,
				 "Dividi %d monete tra  %d membri -- %d monete ciascuno.\r\n",
				 amount, num, share);
		if (rest)
		{
			sprintf (buf + strlen (buf),
					 "%d monet%s non %s divisibil%s, quindi ti tieni"
					 " l%s monet%s.\r\n", rest, (rest == 1) ? "a" : "e",
					 (rest == 1) ? "e'" : "sono", (rest == 1) ? "e" : "i",
					 (rest == 1) ? "a" : "e", (rest == 1) ? "a" : "e");
			GET_GOLD (ch) += rest;
		}
		send_to_char (buf, ch);
	}
	else
	{
		send_to_char ("Quante monete speravi di dividere con il gruppo?\r\n",
					  ch);
		return;
	}
}



ACMD (do_use)
{
	struct obj_data *mag_item;

	half_chop (argument, arg, buf);
	if (!*arg)
	{
		sprintf (buf2, "What do you want to %s?\r\n", CMD_NAME);
		send_to_char (buf2, ch);
		return;
	}
	mag_item = GET_EQ (ch, WEAR_HOLD);

	if (!mag_item || !isname (arg, mag_item->name))
	{
		switch (subcmd)
		{
		case SCMD_RECITE:
		case SCMD_QUAFF:
			if (!
				(mag_item =
				 get_obj_in_list_vis (ch, arg, NULL, ch->carrying)))
			{
				sprintf (buf2, "You don't seem to have %s %s.\r\n", AN (arg),
						 arg);
				send_to_char (buf2, ch);
				return;
			}
			break;
		case SCMD_USE:
			sprintf (buf2, "You don't seem to be holding %s %s.\r\n",
					 AN (arg), arg);
			send_to_char (buf2, ch);
			return;
		default:
			log ("SYSERR: Unknown subcmd %d passed to do_use.", subcmd);
			return;
		}
	}
	switch (subcmd)
	{
	case SCMD_QUAFF:
		if (GET_OBJ_TYPE (mag_item) != ITEM_POTION)
		{
			send_to_char ("You can only quaff potions.\r\n", ch);
			return;
		}
		break;
	case SCMD_RECITE:
		if (GET_OBJ_TYPE (mag_item) != ITEM_SCROLL)
		{
			send_to_char ("Puoi solo connettere chip.\r\n", ch);
			return;
		}
		break;
	case SCMD_USE:
		if ((GET_OBJ_TYPE (mag_item) != ITEM_WAND) &&
			(GET_OBJ_TYPE (mag_item) != ITEM_STAFF))
		{
			send_to_char ("You can't seem to figure out how to use it.\r\n",
						  ch);
			return;
		}
		break;
	}

	mag_objectmagic (ch, mag_item, buf);
}



ACMD (do_wimpy)
{
	int wimp_lev;

	/* 'wimp_level' is a player_special. -gg 2/25/98 */
	if (IS_NPC (ch))
		return;

	one_argument (argument, arg);

	if (!*arg)
	{
		if (GET_WIMP_LEV (ch))
		{
			sprintf (buf, "Your current wimp level is %d hit points.\r\n",
					 GET_WIMP_LEV (ch));
			send_to_char (buf, ch);
			return;
		}
		else
		{
			send_to_char
				("At the moment, you're not a wimp.  (sure, sure...)\r\n",
				 ch);
			return;
		}
	}
	if (isdigit (*arg))
	{
		if ((wimp_lev = atoi (arg)) != 0)
		{
			if (wimp_lev < 0)
				send_to_char
					("Heh, heh, heh.. we are jolly funny today, eh?\r\n", ch);
			else if (wimp_lev > GET_MAX_HIT (ch))
				send_to_char
					("That doesn't make much sense, now does it?\r\n", ch);
			else if (wimp_lev > (GET_MAX_HIT (ch) / 2))
				send_to_char
					("You can't set your wimp level above half your hit points.\r\n",
					 ch);
			else
			{
				sprintf (buf,
						 "Okay, you'll wimp out if you drop below %d hit points.\r\n",
						 wimp_lev);
				send_to_char (buf, ch);
				GET_WIMP_LEV (ch) = wimp_lev;
			}
		}
		else
		{
			send_to_char
				("Okay, you'll now tough out fights to the bitter end.\r\n",
				 ch);
			GET_WIMP_LEV (ch) = 0;
		}
	}
	else
		send_to_char
			("Specify at how many hit points you want to wimp out at.  (0 to disable)\r\n",
			 ch);
}


ACMD (do_display)
{
	size_t i;

	if (IS_NPC (ch))
	{
		send_to_char ("Mosters don't need displays.  Go away.\r\n", ch);
		return;
	}
	skip_spaces (&argument);

	if (!*argument)
	{
		send_to_char
			("Usage: prompt { H | M | V | G | X | E } | all | none }\r\n",
			 ch);
		return;
	}
	if (!str_cmp (argument, "on") || !str_cmp (argument, "all"))
		SET_BIT (PRF_FLAGS (ch),
				 PRF_DISPHP | PRF_DISPMANA | PRF_DISPMOVE | PRF_DISPEXP);
	else if (!str_cmp (argument, "off") || !str_cmp (argument, "none"))
		REMOVE_BIT (PRF_FLAGS (ch),
					PRF_DISPHP | PRF_DISPMANA | PRF_DISPMOVE | PRF_DISPEXP |
					PRF_DISPEXITS | PRF_DISPGOLD);
	else
	{

		for (i = 0; i < strlen (argument); i++)
		{
			switch (LOWER (argument[i]))
			{
			case 'h':
				if (PRF_FLAGGED (ch, PRF_DISPHP))
					REMOVE_BIT (PRF_FLAGS (ch), PRF_DISPHP);
				else
					SET_BIT (PRF_FLAGS (ch), PRF_DISPHP);
				break;
			case 'm':
				if (PRF_FLAGGED (ch, PRF_DISPMANA))
					REMOVE_BIT (PRF_FLAGS (ch), PRF_DISPMANA);
				else
					SET_BIT (PRF_FLAGS (ch), PRF_DISPMANA);
				break;
			case 'v':
				if (PRF_FLAGGED (ch, PRF_DISPMOVE))
					REMOVE_BIT (PRF_FLAGS (ch), PRF_DISPMOVE);
				else
					SET_BIT (PRF_FLAGS (ch), PRF_DISPMOVE);
				break;
			case 'g':
				if (PRF_FLAGGED (ch, PRF_DISPGOLD))
					REMOVE_BIT (PRF_FLAGS (ch), PRF_DISPGOLD);
				else
					SET_BIT (PRF_FLAGS (ch), PRF_DISPGOLD);
				break;
			case 'x':
				if (PRF_FLAGGED (ch, PRF_DISPEXP))
					REMOVE_BIT (PRF_FLAGS (ch), PRF_DISPEXP);
				else
					SET_BIT (PRF_FLAGS (ch), PRF_DISPEXP);
				break;
			case 'e':
				if (PRF_FLAGGED (ch, PRF_DISPEXITS))
					REMOVE_BIT (PRF_FLAGS (ch), PRF_DISPEXITS);
				else
					SET_BIT (PRF_FLAGS (ch), PRF_DISPEXITS);
				break;
			default:
				send_to_char
					("Usage: prompt { H | M | V | X | E } | all | none }\r\n",
					 ch);
				return;
			}
		}
		send_to_char (OK, ch);
	}
}

ACMD (do_gen_write)
{
	FILE *fl;
	char *tmp, buf[MAX_STRING_LENGTH];
	const char *filename;
	struct stat fbuf;
	time_t ct;

	switch (subcmd)
	{
	case SCMD_BUG:
		filename = BUG_FILE;
		break;
	case SCMD_TYPO:
		filename = TYPO_FILE;
		break;
	case SCMD_IDEA:
		filename = IDEA_FILE;
		break;
	default:
		return;
	}

	ct = time (0);
	tmp = asctime (localtime (&ct));

	if (IS_NPC (ch))
	{
		send_to_char ("Monsters can't have ideas - Go away.\r\n", ch);
		return;
	}

	skip_spaces (&argument);
	delete_doubledollar (argument);

	if (!*argument)
	{
		send_to_char ("That must be a mistake...\r\n", ch);
		return;
	}
	sprintf (buf, "%s %s: %s", GET_NAME (ch), CMD_NAME, argument);
	mudlog (buf, CMP, LVL_IMMORT, FALSE);

	if (stat (filename, &fbuf) < 0)
	{
		perror ("SYSERR: Can't stat() file");
		return;
	}
	if (fbuf.st_size >= max_filesize)
	{
		send_to_char
			("Sorry, the file is full right now.. try again later.\r\n", ch);
		return;
	}
	if (!(fl = fopen (filename, "a")))
	{
		perror ("SYSERR: do_gen_write");
		send_to_char ("Could not open the file.  Sorry.\r\n", ch);
		return;
	}
	fprintf (fl, "%-8s (%6.6s) [%5d] %s\n", GET_NAME (ch), (tmp + 4),
			 GET_ROOM_VNUM (IN_ROOM (ch)), argument);
	fclose (fl);
	send_to_char ("Okay.  Thanks!\r\n", ch);
}



#define TOG_OFF 0
#define TOG_ON  1

#define PRF_TOG_CHK(ch,flag) ((TOGGLE_BIT(PRF_FLAGS(ch), (flag))) & (flag))

ACMD (do_gen_tog)
{
	long result;

	const char *tog_messages[][2] = {
		{"You are now safe from summoning by other players.\r\n",
		 "You may now be summoned by other players.\r\n"},
		{"Nohassle disabled.\r\n",
		 "Nohassle enabled.\r\n"},
		{"Brief mode off.\r\n",
		 "Brief mode on.\r\n"},
		{"Compact mode off.\r\n",
		 "Compact mode on.\r\n"},
		{"You can now hear tells.\r\n",
		 "You are now deaf to tells.\r\n"},
		{"You can now hear auctions.\r\n",
		 "You are now deaf to auctions.\r\n"},
		{"You can now hear shouts.\r\n",
		 "You are now deaf to shouts.\r\n"},
		{"You can now hear gossip.\r\n",
		 "You are now deaf to gossip.\r\n"},
		{"You can now hear the congratulation messages.\r\n",
		 "You are now deaf to the congratulation messages.\r\n"},
		{"You can now hear the Wiz-channel.\r\n",
		 "You are now deaf to the Wiz-channel.\r\n"},
		{"You are no longer part of the Quest.\r\n",
		 "Okay, you are part of the Quest!\r\n"},
		{"You will no longer see the room flags.\r\n",
		 "You will now see the room flags.\r\n"},
		{"You will now have your communication repeated.\r\n",
		 "You will no longer have your communication repeated.\r\n"},
		{"HolyLight mode off.\r\n",
		 "HolyLight mode on.\r\n"},





		
			{"Nameserver_is_slow changed to NO; IP addresses will now be resolved.\r\n",
			 "Nameserver_is_slow changed to YES; sitenames will no longer be resolved.\r\n"},
		{"Autoexits disabled.\r\n",
		 "Autoexits enabled.\r\n"},
		{"Ritorni alla tastiera.\r\n",
		 "Ti allontani dalla tastiera.\r\n"},
		{"Will no longer track through doors.\r\n",
		 "Will now track through doors.\r\n"}
	};


	if (IS_NPC (ch))
		return;

	switch (subcmd)
	{
	case SCMD_NOSUMMON:
		result = PRF_TOG_CHK (ch, PRF_SUMMONABLE);
		break;
	case SCMD_NOHASSLE:
		result = PRF_TOG_CHK (ch, PRF_NOHASSLE);
		break;
	case SCMD_BRIEF:
		result = PRF_TOG_CHK (ch, PRF_BRIEF);
		break;
	case SCMD_COMPACT:
		result = PRF_TOG_CHK (ch, PRF_COMPACT);
		break;
	case SCMD_NOTELL:
		result = PRF_TOG_CHK (ch, PRF_NOTELL);
		break;
	case SCMD_NOAUCTION:
		result = PRF_TOG_CHK (ch, PRF_NOAUCT);
		break;
	case SCMD_DEAF:
		result = PRF_TOG_CHK (ch, PRF_DEAF);
		break;
	case SCMD_NOGOSSIP:
		result = PRF_TOG_CHK (ch, PRF_NOGOSS);
		break;
	case SCMD_NOGRATZ:
		result = PRF_TOG_CHK (ch, PRF_NOGRATZ);
		break;
	case SCMD_NOWIZ:
		result = PRF_TOG_CHK (ch, PRF_NOWIZ);
		break;
	case SCMD_QUEST:
		result = PRF_TOG_CHK (ch, PRF_QUEST);
		break;
	case SCMD_ROOMFLAGS:
		result = PRF_TOG_CHK (ch, PRF_ROOMFLAGS);
		break;
	case SCMD_NOREPEAT:
		result = PRF_TOG_CHK (ch, PRF_NOREPEAT);
		break;
	case SCMD_HOLYLIGHT:
		result = PRF_TOG_CHK (ch, PRF_HOLYLIGHT);
		break;
	case SCMD_SLOWNS:
		result = (nameserver_is_slow = !nameserver_is_slow);
		break;
	case SCMD_AUTOEXIT:
		result = PRF_TOG_CHK (ch, PRF_AUTOEXIT);
		break;
	case SCMD_AFK:
		result = PRF_TOG_CHK (ch, PRF_AFK);
		if (PRF_FLAGGED (ch, PRF_AFK))
			act ("$n si e' allontanato dall tastiera.", TRUE, ch, 0, 0,
				 TO_ROOM);
		else
			act ("$n e' tornato alla tastiera.", TRUE, ch, 0, 0, TO_ROOM);
		break;
	case SCMD_TRACK:
		result = (track_through_doors = !track_through_doors);
		break;
	default:
		log ("SYSERR: Unknown subcmd %d in do_gen_toggle.", subcmd);
		return;
	}

	if (result)
		send_to_char (tog_messages[subcmd][TOG_ON], ch);
	else
		send_to_char (tog_messages[subcmd][TOG_OFF], ch);

	return;
}
ACMD (do_push)
{
	char *desc;
	int room_boundary, old_room;
	char arg1[MAX_STRING_LENGTH];
	char arg2[MAX_STRING_LENGTH];

	two_arguments (argument, arg1, arg2);

	if (!*arg1)
	{
		send_to_char ("Premere cosa?\r\n", ch);
		return;
	}

	if (!str_cmp (arg1, "button")
		&& ROOM_FLAGGED (ch->in_room, ROOM_ELEVATOR))
	{
		if ((desc = find_exdesc (arg2, world[ch->in_room].ex_description)))
		{
			room_boundary = atoi (desc);
			old_room = world[ch->in_room].dir_option[3]->to_room;
			if (world[old_room].number < room_boundary)
			{
				act ("$n preme il bottone e l'ascensore inizia a salire",
					 TRUE, ch, 0, 0, TO_ROOM);
				send_to_char
					("premi il bottone e l'ascensore inizia a salire.\r\n",
					 ch);
			}
			else if (world[old_room].number > room_boundary)
			{
				act ("$n preme il bottone e l'ascensore inizia a scendere",
					 TRUE, ch, 0, 0, TO_ROOM);
				send_to_char
					("premi il bottone e l'ascensore inizia a scendere.\r\n",
					 ch);
			}
			world[old_room].dir_option[1]->to_room = -1;
			world[ch->in_room].dir_option[3]->to_room =
				real_room (room_boundary);
			world[real_room (room_boundary)].dir_option[1]->to_room =
				ch->in_room;
		}
		else
			send_to_char ("Quel bottone non c'e'.\r\n", ch);

	}
	else
		if (!str_cmp (arg1, "button")
			&& ROOM_FLAGGED (ch->in_room, ROOM_ENTRANCE))
	{
		if (
			(desc =
			 find_exdesc ("VNUM_e", world[ch->in_room].ex_description)))
		{

			room_boundary = atoi (desc);
			if (world[ch->in_room].dir_option[1]->to_room ==
				real_room (room_boundary))
			{
				send_to_char ("L'ascensore e' gia' qui!\r\n", ch);
				return;
			}
			old_room =
				world[real_room (room_boundary)].dir_option[3]->to_room;
			world[old_room].dir_option[1]->to_room = -1;
			world[real_room (room_boundary)].dir_option[3]->to_room =
				ch->in_room;
			world[ch->in_room].dir_option[1]->to_room =
				real_room (room_boundary);
			send_to_char ("L'ascensore e' arrivato.\r\n", ch);
		}
	}
	else
		send_to_char ("Quel bottone non esiste.\r\n", ch);
	return;
}


ACMD (do_first_aid)
{
	struct affected_type af;
	int exp_level = 0;

	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_FIRST_AID))
	{
		send_to_char ("Non hai idea su come fare.\r\n", ch);
		return;
	}

	if (!affected_by_spell (ch, SKILL_FIRST_AID))
	{
		send_to_char ("Cerchi di medicare alla meglio le tue ferite.\n\r",
					  ch);
	}
	else
	{
		send_to_char ("Devi aspettare ancora un po` prima di poter medicare "
					  "ancora le tue ferite.\n\r", ch);
		return;
	}

	exp_level = GET_LEVEL (ch);


	if (number (1, 101) < GET_SKILL (ch, SKILL_FIRST_AID))
	{
		GET_HIT (ch) += number (1, 4) + exp_level;
		if (GET_HIT (ch) > GET_MAX_HIT (ch))
			GET_HIT (ch) = GET_MAX_HIT (ch);

		af.duration = 6;
	}
	else
	{
		af.duration = 3;
		if (GET_SKILL (ch, SKILL_FIRST_AID) < 95 &&
			GET_SKILL (ch, SKILL_FIRST_AID) > 0)
		{
			if (number (1, 101) > GET_SKILL (ch, SKILL_FIRST_AID))
			{
				GET_SKILL (ch, SKILL_FIRST_AID) += 1;
			}
		}
	}

	af.type = SKILL_FIRST_AID;
	af.modifier = 0;
	af.location = APPLY_NONE;
	af.bitvector = 0;
	affect_to_char (ch, &af);
	return;
}

ACMD (do_ki_shield)
{
	struct affected_type af;
	int exp_level = 0;

	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_KI_SHIELD))
	{
		send_to_char ("Non hai idea su come fare.\r\n", ch);
		return;
	}

	if (!affected_by_spell (ch, SKILL_KI_SHIELD))
	{
		send_to_char ("Espandi la tua aura.\n\r", ch);
	}
	else
	{
		send_to_char ("Devi aspettare ancora un po` prima di poter esandere "
					  "la tua aura.\n\r", ch);
		return;
	}

	if (number (1, 101) < GET_SKILL (ch, SKILL_KI_SHIELD))
	{
		act("$n fa espandere la sua aura.", TRUE, ch, 0, 0, TO_ROOM);
		send_to_char ("Sei circondato da una aura luminosa.\n\r", ch);
		
        af.type = SKILL_KI_SHIELD;
        af.duration = 4;
	    af.bitvector = AFF_SANCTUARY;
	    af.modifier = 0;
	    af.location = APPLY_NONE;
	}
	else
	{
		
        act("$n fa espandere la sua aura, ma si dissolve subito.",TRUE, ch, 0, 0, TO_ROOM);
		send_to_char ("La tua aura tremola e si dissolve subito.\n\r", ch);
        learned_from_mistake(ch,SKILL_KI_SHIELD,0,95);
        af.type = SKILL_KI_SHIELD;
        af.duration = 2;
	    af.bitvector = 0;
	    af.modifier = 0;
	    af.location = APPLY_NONE;
	}

	affect_to_char (ch, &af);
	return;
}




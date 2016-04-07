
/* ************************************************************************
*   File: act.offensive.c                               Part of CircleMUD *
*  Usage: player-level commands of an offensive nature                    *
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

/* extern variables */
extern struct room_data *world;
extern struct descriptor_data *descriptor_list;
extern int pk_allowed;
extern const char *dirs[];
extern const char *it_dirs[];

/* extern functions */
void raw_kill (struct char_data *ch);
void check_killer (struct char_data *ch, struct char_data *vict);
int compute_armor_class (struct char_data *ch);
int check_use_fireweapon (struct char_data *ch);



/* local functions */
ACMD (do_assist);
ACMD (do_hit);
ACMD (do_kill);
ACMD (do_backstab);
ACMD (do_order);
ACMD (do_flee);
ACMD (do_bash);
ACMD (do_rescue);
ACMD (do_kick);
ACMD (do_stop);
ACMD (do_springleap);
ACMD (do_sommersault);
ACMD (do_quivering_palm);
ACMD (do_kamehameha);

ACMD (do_assist)
{
	struct char_data *helpee, *opponent;

	if (FIGHTING (ch))
	{
		send_to_char
			("Stai gia combattendo!  Come puoi assistere qualcuno?\r\n", ch);
		return;
	}
	one_argument (argument, arg);

	if (!*arg)
		send_to_char ("chi speri di assistere?\r\n", ch);
	else if (!(helpee = get_char_vis (ch, arg, NULL, FIND_CHAR_ROOM)))
		send_to_char (NOPERSON, ch);
	else if (helpee == ch)
		send_to_char ("Non puoi assistere se stesso!\r\n", ch);
	else
	{
		/*
		 * Hit the same enemy the person you're helping is.
		 */
		if (FIGHTING (helpee))
			opponent = FIGHTING (helpee);
		else
			for (opponent = world[ch->in_room].people;
				 opponent && (FIGHTING (opponent) != helpee);
				 opponent = opponent->next_in_room)
				;

		if (!opponent)
			act ("MA nessuno sta combattendo con $M!", FALSE, ch, 0, helpee,
				 TO_CHAR);
		else if (!CAN_SEE (ch, opponent))
			act ("Non vedi nessuno che combatte con $M!", FALSE, ch, 0,
				 helpee, TO_CHAR);
		else if (!pk_allowed && !IS_NPC (opponent))	/* prevent accidental pkill */
			act ("Usa 'murder' se vuoi realmente attaccare $N.", FALSE,
				 ch, 0, opponent, TO_CHAR);
		else
		{
			send_to_char ("Ti unisci al combattimento!\r\n", ch);
			act ("$N ti assiste!", 0, helpee, 0, ch, TO_CHAR);
			act ("$n assiste $N.", FALSE, ch, 0, helpee, TO_NOTVICT);
			hit (ch, opponent, TYPE_UNDEFINED);
		}
	}
}


ACMD (do_hit)
{
	struct char_data *vict;

	one_argument (argument, arg);

	if (!*arg)
		send_to_char ("Colpisci chi?\r\n", ch);
	else if (!(vict = get_char_vis (ch, arg, NULL, FIND_CHAR_ROOM)))
		send_to_char ("Non c'e' nessuno da colpire.\r\n", ch);
	else if (vict == ch)
	{
		send_to_char ("Colpisci te stesso...OUCH!.\r\n", ch);
		act ("$n colpisce se' stesso, e dice: 'OUCH!'", FALSE, ch, 0, vict,
			 TO_ROOM);
	}
	else if (AFF_FLAGGED (ch, AFF_CHARM) && (ch->master == vict))
		act ("$N e' solo un buon amico e non puoi colpirlo!.", FALSE, ch, 0,
			 vict, TO_CHAR);
	else
	{
		if (!pk_allowed)
		{
			if (!IS_NPC (vict) && !IS_NPC (ch))
			{
				if (subcmd != SCMD_MURDER)
				{
					send_to_char
						("Usa 'murder' per colpire un altro giocatore.\r\n",
						 ch);
					return;
				}
				else
				{
					check_killer (ch, vict);
				}
			}
			if (AFF_FLAGGED (ch, AFF_CHARM) && !IS_NPC (ch->master)
				&& !IS_NPC (vict))
				return;			/* you can't order a charmed pet to attack a
								   * player */
		}
		if ((GET_POS (ch) == POS_STANDING || GET_POS (ch) == POS_FLYING)
			&& (vict != FIGHTING (ch)))
		{
			hit (ch, vict, TYPE_UNDEFINED);
			WAIT_STATE (ch, PULSE_VIOLENCE + 2);
		}
		else
			send_to_char ("Stai facendo quello che puoi!\r\n", ch);
	}
}



ACMD (do_kill)
{
	struct char_data *vict;

	if ((GET_LEVEL (ch) < LVL_IMPL) || IS_NPC (ch))
	{
		do_hit (ch, argument, cmd, subcmd);
		return;
	}
	one_argument (argument, arg);

	if (!*arg)
	{
		send_to_char ("Uccidere chi?\r\n", ch);
	}
	else
	{
		if (!(vict = get_char_vis (ch, arg, NULL, FIND_CHAR_ROOM)))
			send_to_char ("Non c'e nessuno qui.\r\n", ch);
		else if (ch == vict)
			send_to_char ("Tua madre e' triste per te.. :(\r\n", ch);
		else
		{
			act ("Tagli $N a pezzi!  Ah! Il SANGUE!", FALSE, ch, 0, vict,
				 TO_CHAR);
			act ("$N ti taglia a pezzi!", FALSE, vict, 0, ch, TO_CHAR);
			act ("$n trucida brutalmente $N!", FALSE, ch, 0, vict,
				 TO_NOTVICT);
			raw_kill (vict);
		}
	}
}



ACMD (do_backstab)
{
	struct char_data *vict;
	int percent, prob;

	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_BACKSTAB))
	{
		send_to_char ("Non hai idea di come fare.\r\n", ch);
		return;
	}

	one_argument (argument, buf);

	if (!(vict = get_char_vis (ch, buf, NULL, FIND_CHAR_ROOM)))
	{
		send_to_char ("Backstab who?\r\n", ch);
		return;
	}
	if (vict == ch)
	{
		send_to_char ("Cerchi di strisciare di nascosto te stesso?\r\n", ch);
		return;
	}
	if (!GET_EQ (ch, WEAR_WIELD))
	{
		send_to_char ("DEvi impugnare una arma per avere successo.\r\n", ch);
		return;
	}
	if (GET_OBJ_VAL (GET_EQ (ch, WEAR_WIELD), 3) != TYPE_PIERCE - TYPE_HIT)
	{
		send_to_char
			("Solo le armi da penetrazione possono essere usate per il backstabbing.\r\n",
			 ch);
		return;
	}
	if (FIGHTING (vict))
	{
		send_to_char
			("Non puoi pugnalare una persona che combatte -- E' gia in allerta!\r\n",
			 ch);
		return;
	}

	if (MOB_FLAGGED (vict, MOB_AWARE) && AWAKE (vict))
	{
		act ("Noti $N balzarti alle spalle!", FALSE, vict, 0, ch, TO_CHAR);
		act ("$e nota il tuo balzo a $m!", FALSE, vict, 0, ch, TO_VICT);
		act ("$n nota $N balzare a $m!", FALSE, vict, 0, ch, TO_NOTVICT);
		hit (vict, ch, TYPE_UNDEFINED);
		return;
	}

	percent = number (1, 101);	/* 101% is a complete failure */
	prob = GET_SKILL (ch, SKILL_BACKSTAB);

	if (AWAKE (vict) && (percent > prob))
		damage (ch, vict, 0, SKILL_BACKSTAB);
	else
		hit (ch, vict, SKILL_BACKSTAB);

	WAIT_STATE (ch, 2 * PULSE_VIOLENCE);
}


ACMD (do_order)
{
	char name[MAX_INPUT_LENGTH], message[MAX_INPUT_LENGTH];
	bool found = FALSE;
	room_rnum org_room;
	struct char_data *vict;
	struct follow_type *k;

	half_chop (argument, name, message);

	if (!*name || !*message)
	{
		send_to_char ("Ordini CHI di fare COSA?\r\n", ch);
	}
	else if (!(vict = get_char_vis (ch, name, NULL, FIND_CHAR_ROOM))
			 && !is_abbrev (name, "followers"))
	{
		send_to_char ("Quella persona non e' qui.\r\n", ch);
	}
	else if (ch == vict)
	{
		send_to_char ("Ovviamente sei schitzofrenico.\r\n", ch);
	}
	else
	{
		if (AFF_FLAGGED (ch, AFF_CHARM))
		{
			send_to_char
				("Non puoi dare ordini, sei sotto la sua merce'.\r\n", ch);
			return;
		}
		if (vict)
		{
			sprintf (buf, "$N ti ordina di '%s'", message);
			act (buf, FALSE, vict, 0, ch, TO_CHAR);
			act ("$n da a $N un ordine.", FALSE, ch, 0, vict, TO_ROOM);

			if ((vict->master != ch) || !AFF_FLAGGED (vict, AFF_CHARM))
			{
				act ("$n se ne sbatte del tuo ordine.", FALSE, vict, 0, 0,
					 TO_ROOM);
			}
			else
			{
				send_to_char (OK, ch);
				command_interpreter (vict, message);
			}
		}
		else					/* This is order "followers" */
		{
			sprintf (buf, "$n emana l'ordine '%s'.", message);
			act (buf, FALSE, ch, 0, 0, TO_ROOM);

			org_room = ch->in_room;

			for (k = ch->followers; k; k = k->next)
			{
				if (org_room == k->follower->in_room)
				{
					if (AFF_FLAGGED (k->follower, AFF_CHARM))
					{
						found = TRUE;
						command_interpreter (k->follower, message);
					}
				}
			}
			if (found)
			{
				send_to_char (OK, ch);
			}
			else
			{
				send_to_char ("Nobody here is a loyal subject of yours!\r\n",
							  ch);
			}
		}
	}
}



ACMD (do_flee)
{
	int i, attempt, loss;
	struct char_data *was_fighting;
	int last_pos;
    int percent, prob;

	if (GET_POS (ch) < POS_FIGHTING)
	{
		send_to_char
			("Sei in una brutta situazione, impossibilitato a fuggire!\r\n",
			 ch);
		return;
	}

	for (i = 0; i < 6; i++)
	{
		attempt = number (0, NUM_OF_DIRS - 1);	/* Select a random direction */
		if (CAN_GO (ch, attempt) &&
			!ROOM_FLAGGED (EXIT (ch, attempt)->to_room, ROOM_DEATH))
		{
			act ("$n e' nel panico, e cerca di scappare!", TRUE, ch, 0, 0,
				 TO_ROOM);
			last_pos = IN_ROOM (ch);
			was_fighting = FIGHTING (ch);
            if (do_simple_move (ch, attempt, TRUE))
			{
				if ( !IS_NPC(ch))
                {
                    /* spostata all'interno per evitare il syserr sui mob a causa del GET_SKILL */
                    if (GET_SKILL(ch,SKILL_RETREAT) > 0)
                    {
                        percent = number (1, 101);	/* 101% is a complete failure */
	                    prob = (GET_SKILL (ch, SKILL_RETREAT) + GET_DEX (ch));

                        if (percent > prob)
	                    {
		                    send_to_char("Non riesci a ritirarti  e vai nel panico!",ch);
                            learned_from_mistake (ch, SKILL_RETREAT, 0, 90);
	                    }
	                    else
                        {
                            send_to_char ("Ti ritiri dal combattimento.\r\n", ch);
                            sprintf (buf, "%s si ritira dalla lotta con %s\r\n", GET_NAME (ch),
							    it_dirs[attempt]);
					        send_to_room (buf, last_pos);
                            return;
                        }
                    }
                }
                send_to_char("scappi con la coda nelle cambe",ch);
                if (IS_NPC (ch))
				{
					sprintf (buf, "%s scappa a %s", GET_NAME (ch),
							 it_dirs[attempt]);
					send_to_room (buf, last_pos);
				}

				if (was_fighting && !IS_NPC (ch))
				{
					loss =
						GET_MAX_HIT (was_fighting) - GET_HIT (was_fighting);
					loss *= GET_LEVEL (was_fighting);
					gain_exp (ch, -loss);
				}
			}
			else
			{
				act ("$n cerca di scappare, ma non puo'!", TRUE, ch, 0, 0,
					 TO_ROOM);
			}
			return;
		}
	}
	send_to_char ("PANICO!  Non puoi scappare!\r\n", ch);
}


ACMD (do_bash)
{
	struct char_data *vict;
	int percent, prob;

	one_argument (argument, arg);

	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_BASH))
	{
		send_to_char ("Non hai idea di come fare.\r\n", ch);
		return;
	}

	if (ROOM_FLAGGED (IN_ROOM (ch), ROOM_PEACEFUL))
	{
		send_to_char
			("Questa stanza e' pacifica, piacevole sensazione...\r\n", ch);
		return;
	}

	if (!GET_EQ (ch, WEAR_WIELD))
	{
		if (!check_use_fireweapon (ch))
		{
			send_to_char ("Devi impugnare una arma per avere successo.\r\n",
						  ch);
			return;
		}
	}

	if (!(vict = get_char_vis (ch, arg, NULL, FIND_CHAR_ROOM)))
	{
		if (FIGHTING (ch) && IN_ROOM (ch) == IN_ROOM (FIGHTING (ch)))
		{
			vict = FIGHTING (ch);
		}
		else
		{
			send_to_char ("Bash who?\r\n", ch);
			return;
		}
	}

	if (vict == ch)
	{
		send_to_char ("Oggi non e' divertente...\r\n", ch);
		return;
	}
	percent = number (1, 101);	/* 101% is a complete failure */
	prob = (GET_SKILL (ch, SKILL_BASH) + GET_DEX (ch));

	if (MOB_FLAGGED (vict, MOB_NOBASH))
	{
		damage (ch, vict, 0, SKILL_BASH);
		act
			("$n rimbalza contro $N e cade per terra.",
			 FALSE, ch, 0, vict, TO_ROOM);
		act ("Rimbalzi contro $N e cadi per terra!!",
			 FALSE, ch, 0, vict, TO_CHAR);
		act
			("$n rimbalza contro te e cade per terra.",
			 FALSE, ch, 0, vict, TO_VICT);

		GET_POS (ch) = POS_SITTING;
		WAIT_STATE (ch, PULSE_VIOLENCE * 3);
		return;
	}

	if (percent > prob)
	{
		damage (ch, vict, 0, SKILL_BASH);
		GET_POS (ch) = POS_SITTING;
		learned_from_mistake (ch, SKILL_BASH, 0, 90);
	}
	else
	{
		/*
		 * If we bash a player and they wimp out, they will move to the previous
		 * room before we set them sitting.  If we try to set the victim sitting
		 * first to make sure they don't flee, then we can't bash them!  So now
		 * we only set them sitting if they didn't flee. -gg 9/21/98
		 */
		if (damage (ch, vict, number (1, GET_STR (ch)), SKILL_BASH) > 0)	/* -1 = dead, 0 = miss */
		{
			WAIT_STATE(vict, PULSE_VIOLENCE);
            if (IN_ROOM(ch) == IN_ROOM(vict))
			{
				GET_POS (vict) = POS_SITTING;
			}
		}
	}
	WAIT_STATE (ch, PULSE_VIOLENCE * 2);
}


ACMD (do_rescue)
{
	struct char_data *vict, *tmp_ch;
	int percent, prob;

	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_RESCUE))
	{
		send_to_char ("Non hai idea su come fare.\r\n", ch);
		return;
	}

	one_argument (argument, arg);

	if (!(vict = get_char_vis (ch, arg, NULL, FIND_CHAR_ROOM)))
	{
		send_to_char ("Chi o cosa vuoi salvare?\r\n", ch);
		return;
	}
	if (vict == ch)
	{
		send_to_char ("Non fai prima a scappare?\r\n", ch);
		return;
	}
	if (FIGHTING (ch) == vict)
	{
		send_to_char
			("Perche' vuoi salvare qualcuno che cerca di ucciderti?\r\n", ch);
		return;
	}
	for (tmp_ch = world[ch->in_room].people; tmp_ch &&
		 (FIGHTING (tmp_ch) != vict); tmp_ch = tmp_ch->next_in_room);

	if (!tmp_ch)
	{
		act ("Ma nessuno sta combattento con $M!", FALSE, ch, 0, vict,
			 TO_CHAR);
		return;
	}
	percent = number (1, 101);	/* 101% is a complete failure */
	prob = GET_SKILL (ch, SKILL_RESCUE);

	if (percent > prob)
	{
		send_to_char ("Fallisci il salvataggio!\r\n", ch);
		learned_from_mistake (ch, SKILL_RESCUE, 0, 90);
		return;
	}
	send_to_char ("GERONIMO!!!!  Al salvataggio...\r\n", ch);
	act ("Sei setato salvato da $N, sei confuso!", FALSE, vict, 0, ch,
		 TO_CHAR);
	act ("$n salva eriocamente $N!", FALSE, ch, 0, vict, TO_NOTVICT);

	if (FIGHTING (vict) == tmp_ch)
		stop_fighting (vict);
	if (FIGHTING (tmp_ch))
		stop_fighting (tmp_ch);
	if (FIGHTING (ch))
		stop_fighting (ch);

	set_fighting (ch, tmp_ch);
	set_fighting (tmp_ch, ch);

	if (IS_MARTIAL (ch))
	{
		WAIT_STATE (ch, PULSE_VIOLENCE * 1);
	}
	else
	{
		WAIT_STATE (ch, PULSE_VIOLENCE * 2);
	}
}



ACMD (do_kick)
{
	struct char_data *vict;
	int percent, prob;

	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_KICK))
	{
		send_to_char ("Non hai idea su come fare.\r\n", ch);
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
			send_to_char ("Calciare chi?\r\n", ch);
			return;
		}
	}
	if (vict == ch)
	{
		send_to_char ("Non e' divertente, oggi...\r\n", ch);
		return;
	}
	/* 101% is a complete failure */
	percent =
		((10 - (compute_armor_class (vict) / 10)) * 2) + number (1, 101);
	prob = GET_SKILL (ch, SKILL_KICK);

	if (percent > prob)
	{
		damage (ch, vict, 0, SKILL_KICK);
		learned_from_mistake (ch, SKILL_KICK, 0, 90);
	}
	else
		damage (ch, vict, GET_LEVEL (ch) / 2, SKILL_KICK);

	if (IS_MARTIAL (ch))
	{
		WAIT_STATE (ch, PULSE_VIOLENCE * 1);
	}
	else
	{
		WAIT_STATE (ch, PULSE_VIOLENCE * 3);
	}

}

ACMD (do_stop)
{
	stop_fighting (ch);
	send_to_char ("Smetti di combattere.\r\n", ch);
}


ACMD (do_springleap)
{
	struct char_data *vict;
	int percent, prob;


	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_SPRING_LEAP))
	{
		send_to_char ("Non hai idea su come fare.\r\n", ch);
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
			send_to_char ("Spring-leap a chi?\r\n", ch);
			return;
		}
	}
	if (vict == ch)
	{
		send_to_char ("Non e' divertente, oggi...\r\n", ch);
		return;
	}

    if ( GET_POS(ch) > POS_SITTING || !FIGHTING(ch))
    {
        send_to_char( "Non sei nella giusta posizione per farlo!\n\r", ch );
        return;
    }

	/* 101% is a complete failure */
	percent =
		((10 - (compute_armor_class (vict) / 10)) * 2) + number (1, 101);
	prob = GET_SKILL (ch, SKILL_SPRING_LEAP);

	act ("$n fa un'abile mossa allungando una gamba verso $N.", FALSE,
		 ch, 0, vict, TO_NOTVICT);
	act ("Sali a gamba tesa verso $N.", FALSE, ch, 0, vict, TO_CHAR);
	act ("$n salta a gamba tesa verso te.", FALSE, ch, 0, vict, TO_VICT);

	if (MOB_FLAGGED (vict, MOB_NOBASH))
	{
		damage (ch, vict, 0, SKILL_SPRING_LEAP);
		act
			("$n rimbalza a gamba tesa contro $N e cade rovinosamente a terra.",
			 FALSE, ch, 0, vict, TO_NOTVICT);
		act ("Rimbalzi a gamba tesa contro $N e cadi rovinosamente a terra.",
			 FALSE, ch, 0, vict, TO_CHAR);
		act
			("$n rimbalza a gamba tesa contro di te e cade rovinosamente a terra.",
			 FALSE, ch, 0, vict, TO_VICT);
		GET_POS (ch) = POS_SITTING;
		WAIT_STATE (ch, PULSE_VIOLENCE * 2);
		return;
	}

	if (percent > prob)
	{
		damage (ch, vict, 0, SKILL_SPRING_LEAP);
		send_to_char ("Rovini a terra rumorosamente.\n\r", ch);
		act ("$n rovina a terra rumorosamente", FALSE, ch, 0, 0, TO_ROOM);
		learned_from_mistake (ch, SKILL_SPRING_LEAP, 0, 90);
		GET_POS (ch) = POS_SITTING;

	}
	else
	{
		damage (ch, vict, GET_LEVEL (ch) / 2 + 10, SKILL_KICK);
        WAIT_STATE (vict, PULSE_VIOLENCE);
		if (IN_ROOM (ch) == IN_ROOM (vict))
		{
			act ("$n cade a terra con violenza per la forza dell'impeto.",
				 FALSE, vict, 0, 0, TO_ROOM);
			GET_POS (vict) = POS_SITTING;
            GET_POS (ch) = POS_FIGHTING;
		}

	}
	WAIT_STATE (ch, PULSE_VIOLENCE * 2);
}

ACMD (do_sommersault)
{
	struct char_data *vict;
	int percent, prob;


	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_SOMMERSAULT))
	{
		send_to_char ("Non hai idea su come fare.\r\n", ch);
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
			send_to_char ("Esegui il sommersault a chi?\r\n", ch);
			return;
		}
	}
	if (vict == ch)
	{
		send_to_char ("Non e' divertente, oggi...\r\n", ch);
		return;
	}


	/* 101% is a complete failure */
	percent =
		((10 - (compute_armor_class (vict) / 10)) * 2) + number (1, 101);
	prob = GET_SKILL (ch, SKILL_SOMMERSAULT);

	act
		("$n fa una rotazione su se stesso allungando una gamba all'indietro verso $N.",
		 FALSE, ch, 0, vict, TO_NOTVICT);
	act ("Ruoti su se stesso allungando una gamba all'indietro verso $N.",
		 FALSE, ch, 0, vict, TO_CHAR);
	act
		("$n fa una rotazione su se stesso allungando una gamba all'indietro verso te.",
		 FALSE, ch, 0, vict, TO_VICT);

    if (MOB_FLAGGED (vict, MOB_NOBASH))
	{
		damage (ch, vict, 0, SKILL_SOMMERSAULT);
		act ("La gamba di $n contro $N e perde l'equilibro.",
			 FALSE, ch, 0, vict, TO_NOTVICT);
		act ("La tua gamba rimbalza contro $N e perdi l'equilibro.",
			 FALSE, ch, 0, vict, TO_CHAR);
		act ("La gamba di $n rimbalza su di te e perde l'equilibro.",
			 FALSE, ch, 0, vict, TO_VICT);
		GET_POS (ch) = POS_SITTING;
		WAIT_STATE (ch, PULSE_VIOLENCE * 2);
		return;
	}

	if (percent > prob)
	{
		damage (ch, vict, 0, SKILL_SOMMERSAULT);
		send_to_char ("Cadi a terra rumorosamente.\n\r", ch);
		act ("$n cade a terra rumorosamente", FALSE, ch, 0, 0, TO_ROOM);
		learned_from_mistake (ch, SKILL_SOMMERSAULT, 0, 90);
		GET_POS (ch) = POS_SITTING;
		WAIT_STATE (ch, PULSE_VIOLENCE * 1);
	}
	else
	{
		damage (ch, vict, (GET_LEVEL (ch) / 4) + 10, SKILL_KICK);
        WAIT_STATE (vict, PULSE_VIOLENCE);
        if (IN_ROOM (ch) == IN_ROOM (vict))
		{
			act ("$n rovina a terra.",
				 FALSE, vict, 0, 0, TO_ROOM);
			GET_POS (vict) = POS_SITTING;
            GET_POS (ch) = POS_FIGHTING;			
		}
	}
    WAIT_STATE (ch, PULSE_VIOLENCE * 1);
}


ACMD (do_quivering_palm)
{
	struct char_data *victim;
	struct affected_type af;
	byte percent;

	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_QUIVER_PALM))
	{
		send_to_char ("Non hai idea su come fare.\r\n", ch);
		return;
	}

	if (!IS_MARTIAL (ch))
	{
		send_to_char ("Non sei un Martial artist!\n\r", ch);
		return;
	}

	one_argument (argument, arg);

	if (!(victim = get_char_vis (ch, arg, NULL, FIND_CHAR_ROOM)))
	{
		if (FIGHTING (ch) && IN_ROOM (ch) == IN_ROOM (FIGHTING (ch)))
		{
			victim = FIGHTING (ch);
		}
		else
		{
			send_to_char ("Esegui il palmo vibrante a chi?\r\n", ch);
			return;
		}
	}
	if (victim == ch)
	{
		send_to_char ("Non e' divertente, oggi...\r\n", ch);
		return;
	}

	/*
	   if (!IS_HUMANOID(victim) ) 
	   {
	   send_to_char( "Rispondono alle vibrazioni solo gli umanoidi.\n\r", ch);
	   return;
	   } 
	 */

	send_to_char ("Cominci a generare una vibrazione con il palmo della tua "
				  "mano.\n\r", ch);

	if (affected_by_spell (ch, SKILL_QUIVER_PALM))
	{
		send_to_char ("Puoi farlo solo una volta alla settimana.\n\r", ch);
		return;
	}

	percent = number (1, 101);

	if (percent > GET_SKILL (ch, SKILL_QUIVER_PALM))
	{
		send_to_char ("La vibrazione si spegne inefficace.\n\r", ch);
		if (GET_POS (victim) > POS_DEAD)
		{
			learned_from_mistake (ch, SKILL_QUIVER_PALM, 0, 95);
		}
		WAIT_STATE (ch, PULSE_VIOLENCE * 2);
	}
	else
	{
		damage (ch, victim, GET_MAX_HIT (victim) * 20, SKILL_QUIVER_PALM);
		WAIT_STATE (ch, PULSE_VIOLENCE);
	}

	af.type = SKILL_QUIVER_PALM;
	af.duration = (24 * 7);
	af.modifier = 0;
	af.location = APPLY_NONE;
	af.bitvector = 0;
	affect_to_char (ch, &af);
}

ACMD (do_kamehameha)
{
	struct char_data *vict;
	//struct affected_type af;
	byte percent;

	if (IS_NPC (ch) || !GET_SKILL (ch, SKILL_KAMEHAMEHA))
	{
		send_to_char ("Non hai idea su come fare.\r\n", ch);
		return;
	}

	if (!IS_MARTIAL (ch))
	{
		send_to_char ("Non sei un Martial Artist!\n\r", ch);
		return;
	}

	if (GET_MOVE (ch) < 25)
	{
		send_to_char ("Sei troppo stanco per generare l'onda energetica.\r\n",
					  ch);
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
			send_to_char ("Esegui l'onda kamehameha a chi?\r\n", ch);
			return;
		}
	}
	if (vict == ch)
	{
		send_to_char ("Non e' divertente, oggi...\r\n", ch);
		return;
	}

/*
	if (affected_by_spell (ch, SKILL_KAMEHAMEHA))
	{
		send_to_char ("Puoi farlo solo una volta al giorno.\n\r", ch);
		return;
	}
*/
	send_to_char
		("Le tue mani si illuminano e cominci a generare una sfera di forza "
		 "nei palmi.\n\r", ch);
	act
		("Le mani di $n si illuminano e comincia a generare una sfera di forza nei palmi.",
		 FALSE, ch, 0, 0, TO_ROOM);

	percent = number (1, 101);

	if (percent > GET_SKILL (ch, SKILL_KAMEHAMEHA))
	{
		send_to_char
			("La sfera energetica si dissolve inefficace nelle mani.\n\r",
			 ch);
		act ("La sfera energetica di $n si dissolve inefficace.", FALSE, ch,
			 0, 0, TO_ROOM);
		learned_from_mistake (ch, SKILL_KAMEHAMEHA, 0, 95);
	}
	else
	{

		act ("$n lancia la sfera energetica verso $N.", FALSE, ch, 0, vict,
			 TO_NOTVICT);
		act ("Lanci la sfera energetica verso $N.", FALSE, ch, 0, vict,
			 TO_CHAR);
		act ("$n lancia la sfera energetica verso te.", FALSE, ch, 0, vict,
			 TO_VICT);
		damage (ch, vict, GET_LEVEL (ch), SKILL_KAMEHAMEHA);	// il danno e' proporzionale al livello
	}

	WAIT_STATE (ch, PULSE_VIOLENCE);

/*
	af.type = SKILL_KAMEHAMEHA;
	af.duration = 24;
	af.modifier = 0;
	af.location = APPLY_NONE;
	af.bitvector = 0;
	affect_to_char (ch, &af);
*/
	GET_MOVE (ch) -= 25;
}

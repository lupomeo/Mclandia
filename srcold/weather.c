
/* ************************************************************************
*   File: weather.c                                     Part of CircleMUD *
*  Usage: functions handling time and the weather                         *
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
#include "handler.h"
#include "interpreter.h"
#include "db.h"

extern struct time_info_data time_info;

void weather_and_time (int mode);
void another_hour (int mode);
void weather_change (void);
void astral_body_update (void);
void moon_update (void);
int get_moon_phase (int pos);
void show_moon_nodes (struct astral_body moon, const char *moon_name);
void show_moon_sky (struct astral_body moon, const char *moon_name,
					struct char_data *ch);
void show_astral_body (struct char_data *ch);

void
weather_and_time (int mode)
{
	another_hour (mode);
	if (mode)
	{
		weather_change ();
	}
}

void
another_hour (int mode)
{
	time_info.hours++;
	/* da usare solo per debug */
	/*
	   sprintf(buf,"Time:%d:00 ",time_info.hours);
	   send_to_all(buf);
	 */

	if (mode)
	{
		astral_body_update ();

		switch (time_info.hours)
		{
		case 7:
			send_to_sector ("Si spengono le luci della citta'.\r\n",
							SECT_CITY);
			break;
		case 12:
			send_to_sector
				("Lungo le vie della citta' si sente odore di cibo.\r\n",
				 SECT_CITY);
			break;
		case 18:
			send_to_sector ("Si accendono le luci della citta'.\r\n",
							SECT_CITY);
			break;

		default:
			break;
		}
	}
	if (time_info.hours > 23)	/* Changed by HHS due to bug ??? */
	{
		time_info.hours -= 24;
		time_info.day++;
		moon_update ();

		if (time_info.day > 34)
		{
			time_info.day = 0;
			time_info.month++;

			if (time_info.month > 16)
			{
				time_info.month = 0;
				time_info.year++;
			}
		}
	}
}

/*****************************************************
 * Aggiorna le fasi lunari delle due lune
 * La prima luna, Trammel, orbita in 20 gg
 * La seconda, Felucca, orbita in 36 gg
 *****************************************************/
void
moon_update (void)
{
	weather_info.moon1.abs_pos = degree_sum (weather_info.moon1.abs_pos, 18);
	weather_info.moon2.abs_pos = degree_sum (weather_info.moon2.abs_pos, 10);

	/* check delle fasi lunari */
	/* luna 1 Trammel */
	weather_info.moon1.phase = get_moon_phase (weather_info.moon1.abs_pos);
	/* luna 2 Felucca */
	weather_info.moon2.phase = get_moon_phase (weather_info.moon2.abs_pos);

}

int
get_moon_phase (int pos)
{
	if (pos <= 23)
	{
		return MOON_DARK;
	}
	else if (pos > 23 && pos <= 68)
	{
		return MOON_CRESCENT_RISE;
	}
	else if (pos > 68 && pos <= 113)
	{
		return MOON_RISE;
	}
	else if (pos > 113 && pos <= 158)
	{
		return MOON_PARTIAL_RISE;
	}
	else if (pos > 158 && pos <= 203)
	{
		return MOON_FULL;
	}
	else if (pos > 203 && pos <= 248)
	{
		return MOON_PARTIAL_FALL;
	}
	else if (pos > 248 && pos <= 293)
	{
		return MOON_FALL;
	}
	else if (pos > 293 && pos <= 338)
	{
		return MOON_CRESCENT_FALL;
	}
	else
	{
		return MOON_DARK;
	}
}

#define MIMIMUM_CHECK_ARC 7

void
show_moon_nodes (struct astral_body moon, const char *moon_name)
{
	char moon_phase[256];
	char buf[512];
	int node_moon_rise, node_moon_zenith, node_moon_fall;

	if (moon.phase != MOON_DARK)
	{
		switch (moon.phase)
		{
		case MOON_FULL:
			strcpy (moon_phase, "faccia piena");
			break;
		case MOON_PARTIAL_FALL:
			strcpy (moon_phase, "faccia parziale decrescente");
			break;
		case MOON_FALL:
			strcpy (moon_phase, "mezzaluna decrescente");
			break;
		case MOON_CRESCENT_FALL:
			strcpy (moon_phase, "falce decrescente");
			break;
		case MOON_CRESCENT_RISE:
			strcpy (moon_phase, "falce crescente");
			break;
		case MOON_RISE:
			strcpy (moon_phase, "mezzaluna crescente");
			break;
		case MOON_PARTIAL_RISE:
			strcpy (moon_phase, "faccia parziale crescente");
		default:
			break;
		}

		node_moon_rise = degree_sum (moon.abs_pos, -90);
		node_moon_zenith = degree_sum (moon.abs_pos, 0);
		node_moon_fall = degree_sum (moon.abs_pos, 90);

		if (check_degree (moon.rel_pos, node_moon_rise, MIMIMUM_CHECK_ARC))
		{
			if (weather_info.sky == SKY_CLOUDLESS)
			{
				sprintf (buf, "La %s della %s sorge ad est.\r\n", moon_phase,
						 moon_name);
				send_to_outdoor (buf);
			}
			else if (weather_info.sky == SKY_CLOUDY)
			{
				sprintf (buf,
						 "La %s della %s sbuca tra le nuvole ad est.\r\n",
						 moon_phase, moon_name);
				send_to_outdoor (buf);
			}
		}
		else
			if (check_degree
				(moon.rel_pos, node_moon_zenith, MIMIMUM_CHECK_ARC))
		{
			if (weather_info.sky == SKY_CLOUDLESS)
			{
				sprintf (buf, "La %s della %s e' sopra la tua testa.\r\n",
						 moon_phase, moon_name);
				send_to_outdoor (buf);
			}
			else if (weather_info.sky == SKY_CLOUDY)
			{
				sprintf (buf,
						 "La %s della %s sbuca tra le nuvole sopra la tua testa.\r\n",
						 moon_phase, moon_name);
				send_to_outdoor (buf);
			}
		}
		else
			if (check_degree
				(moon.rel_pos, node_moon_fall, MIMIMUM_CHECK_ARC))
		{
			if (weather_info.sky == SKY_CLOUDLESS)
			{
				sprintf (buf,
						 "La %s della %s tramonta lentamente ad ovest.\r\n",
						 moon_phase, moon_name);
				send_to_outdoor (buf);
			}
			else if (weather_info.sky == SKY_CLOUDY)
			{
				sprintf (buf,
						 "La %s della %s tramonta tra le nuvole ad ovest.\r\n",
						 moon_phase, moon_name);
				send_to_outdoor (buf);
			}
		}

	}
}


void
show_moon_sky (struct astral_body moon, const char *moon_name,
			   struct char_data *ch)
{
	char moon_phase[256];
	char buffer[512];
	int node_moon_rise, node_moon_zenith, node_moon_fall;

	if (moon.phase != MOON_DARK)
	{
		switch (moon.phase)
		{
		case MOON_FULL:
			strcpy (moon_phase, "faccia piena");
			break;
		case MOON_PARTIAL_FALL:
			strcpy (moon_phase, "faccia parziale decrescente");
			break;
		case MOON_FALL:
			strcpy (moon_phase, "mezzaluna decrescente");
			break;
		case MOON_CRESCENT_FALL:
			strcpy (moon_phase, "falce decrescente");
			break;
		case MOON_CRESCENT_RISE:
			strcpy (moon_phase, "falce crescente");
			break;
		case MOON_RISE:
			strcpy (moon_phase, "mezzaluna crescente");
			break;
		case MOON_PARTIAL_RISE:
			strcpy (moon_phase, "faccia parziale crescente");
		default:
			break;
		}

		node_moon_rise = degree_sum (moon.abs_pos, -90);
		node_moon_zenith = degree_sum (moon.abs_pos, 0);
		node_moon_fall = degree_sum (moon.abs_pos, 90);

		if (check_in_arc
			(moon.rel_pos, node_moon_rise, degree_sum (node_moon_rise, 45)))
		{
			if (weather_info.sky == SKY_CLOUDLESS)
			{
				sprintf (buffer, "La %s della %s e' in basso ad est.\r\n",
						 moon_phase, moon_name);
				send_to_char (buffer, ch);
			}
			else if (weather_info.sky == SKY_CLOUDY)
			{
				sprintf (buffer,
						 "La %s della %s sbuca tra le nuvole in basso ad est.\r\n",
						 moon_phase, moon_name);
				send_to_char (buffer, ch);
			}
		}
		else
			if (check_in_arc
				(moon.rel_pos, degree_sum (node_moon_rise, 45),
				 degree_sum (node_moon_zenith, -10)))
		{
			if (weather_info.sky == SKY_CLOUDLESS)
			{
				sprintf (buffer, "La %s della %s sta in alto ad est.\r\n",
						 moon_phase, moon_name);
				send_to_char (buffer, ch);
			}
			else if (weather_info.sky == SKY_CLOUDY)
			{
				sprintf (buffer,
						 "La %s della %s sbuca tra le nuvole in alto ad est.\r\n",
						 moon_phase, moon_name);
				send_to_char (buffer, ch);
			}
		}
		else
			if (check_in_arc
				(moon.rel_pos, degree_sum (node_moon_zenith, -10),
				 degree_sum (node_moon_zenith, 10)))
		{
			if (weather_info.sky == SKY_CLOUDLESS)
			{
				sprintf (buffer, "La %s della %s e' sopra la tua testa.\r\n",
						 moon_phase, moon_name);
				send_to_char (buffer, ch);
			}
			else if (weather_info.sky == SKY_CLOUDY)
			{
				sprintf (buffer,
						 "La %s della %s sbuca tra le nuvole sopra la tua testa.\r\n",
						 moon_phase, moon_name);
				send_to_char (buffer, ch);
			}
		}
		else
			if (check_in_arc
				(moon.rel_pos, degree_sum (node_moon_zenith, 10),
				 degree_sum (node_moon_fall, -45)))
		{
			if (weather_info.sky == SKY_CLOUDLESS)
			{
				sprintf (buffer, "La %s della %s sta in alto ad ovest.\r\n",
						 moon_phase, moon_name);
				send_to_char (buffer, ch);
			}
			else if (weather_info.sky == SKY_CLOUDY)
			{
				sprintf (buffer,
						 "La %s della %s sbuca tra le nuvole in alto ad ovest.\r\n",
						 moon_phase, moon_name);
				send_to_char (buffer, ch);
			}
		}
		else
			if (check_in_arc
				(moon.rel_pos, degree_sum (node_moon_fall, -45),
				 node_moon_fall))
		{
			if (weather_info.sky == SKY_CLOUDLESS)
			{
				sprintf (buffer, "La %s della %s sta in basso ad ovest.\r\n",
						 moon_phase, moon_name);
				send_to_char (buffer, ch);
			}
			else if (weather_info.sky == SKY_CLOUDY)
			{
				sprintf (buffer,
						 "La %s della %s sbuca tra le nuvole in basso ad ovest.\r\n",
						 moon_phase, moon_name);
				send_to_char (buffer, ch);
			}
		}

	}
}


void
show_astral_body (struct char_data *ch)
{

	switch (weather_info.sky)
	{
	case SKY_LIGHTNING:
		if (weather_info.sunlight == SUN_DARK)
		{
			send_to_char
				("Vedi i lampi del temporale lacerare il buio della notte...\r\n",
				 ch);
		}
		else
		{
			send_to_char ("Vedi i lampi del temporale...\r\n", ch);
		}
		return;
		break;

	case SKY_RAINING:
		if (weather_info.sunlight == SUN_DARK)
		{
			send_to_char
				("Sta piovendo e vedi nel buio le nuvole temporalesche...\r\n",
				 ch);
		}
		else
		{
			send_to_char
				("Sta piovendo e non noti altro che nuvole scure...\r\n", ch);
		}
		return;
		break;

	case SKY_CLOUDY:
		switch (time_info.hours)
		{
		case 5:
			sprintf (buf, "l'aurora a est");
			break;
		case 6:
			sprintf (buf, "l'alba a est");
			break;
		case 7:
		case 8:
			sprintf (buf, "il sole e' in basso a est");
			break;
		case 9:
		case 10:
		case 11:
			sprintf (buf, "il sole e' in alto a est");
			break;
		case 12:
			sprintf (buf, "il sole e' sopra la tua testa");
			break;
		case 13:
		case 14:
		case 15:
			sprintf (buf, "il sole e' in alto a ovest");
			break;
		case 16:
		case 17:
			sprintf (buf, "il sole e' in basso a ovest");
			break;
		case 18:
			sprintf (buf, "il tramonto a ovest");
			break;
		case 19:
			sprintf (buf, "il crepuscolo a ovest");
			break;
		default:
			sprintf (buf, "le stelle scintillare");
			break;
		}
		sprintf (buf2, "Tra le nuvole intravedi %s.\r\n", buf);
		send_to_char (buf2, ch);

		break;

	default:					/* SKY_CLOUDLESS */
		switch (time_info.hours)
		{

		case 5:
			sprintf (buf, "l'aurora a est");
			break;
		case 6:
			sprintf (buf, "l'alba a est");
			break;
		case 7:
		case 8:
			sprintf (buf, "il sole e' in basso a est");
			break;
		case 9:
		case 10:
		case 11:
			sprintf (buf, "il sole e' in alto a est");
			break;
		case 12:
			sprintf (buf, "il sole e' sopra la tua testa");
			break;
		case 13:
		case 14:
		case 15:
			sprintf (buf, "il sole e' in alto a ovest");
			break;
		case 16:
		case 17:
			sprintf (buf, "il sole e' in basso a ovest");
			break;
		case 18:
			sprintf (buf, "il tramonto a ovest");
			break;
		case 19:
			sprintf (buf, "il crepuscolo a ovest");
			break;
		default:
			sprintf (buf, "le stelle scintillare nel cielo terso");
			break;
		}
		sprintf (buf2, "Vedi %s.\r\n", buf);
		send_to_char (buf2, ch);

		break;
	}							/* end switch weather_info.sky */

	/* Luna azzurra Trammel */
	if (weather_info.moon1.phase != MOON_DARK)
	{
		show_moon_sky (weather_info.moon1, "piccola luna azzurra", ch);
	}

	/* Luna arancione Felucca */
	if (weather_info.moon2.phase != MOON_DARK)
	{
		show_moon_sky (weather_info.moon2, "luna arancione", ch);
	}
}



void
astral_body_update (void)
{


	weather_info.sun.rel_pos = degree_sum (weather_info.sun.rel_pos, 15);
	weather_info.moon1.rel_pos = degree_sum (weather_info.moon1.rel_pos, 15);
	weather_info.moon2.rel_pos = degree_sum (weather_info.moon2.rel_pos, 15);

	/*
	   ora 0 = rel_pos 0
	   ora 6 = rel_pos 90
	   ora 12 = rel_pos 180
	   ora 18 = rel_pos 270
	 */

	switch (weather_info.sun.rel_pos)
	{
	case 90:

		weather_info.sunlight = SUN_RISE;
		if (weather_info.sky == SKY_CLOUDLESS)
		{
			send_to_outdoor ("Il sole sorge a est.\r\n");
		}
		else if (weather_info.sky == SKY_CLOUDY)
		{
			send_to_outdoor ("Il sole sbuca tra le nuvole a est.\r\n");
		}
		else
		{
			send_to_outdoor ("Vedi schiarire a est.\r\n");
		}
		break;

	case 105:
		weather_info.sunlight = SUN_LIGHT;
		send_to_outdoor ("Il giorno e' iniziato.\r\n");
		break;

	case 180:
		if (weather_info.sky == SKY_CLOUDLESS)
		{
			send_to_outdoor ("Il sole e' sopra la testa.\r\n");
		}
		else if (weather_info.sky == SKY_CLOUDY)
		{
			send_to_outdoor
				("Il sole sbuca tra le nuvole, sopra la testa.\r\n");
		}
		break;

	case 270:
		weather_info.sunlight = SUN_SET;
		if (weather_info.sky == SKY_CLOUDLESS)
		{
			send_to_outdoor ("Il sole tramonta lentamente ad ovest.\r\n");
		}
		else if (weather_info.sky == SKY_CLOUDY)
		{
			send_to_outdoor
				("Il sole tramonta lentamente ad ovest, colorando di rosso le nubi.\r\n");
		}
		else
		{
			send_to_outdoor ("Vedi calare l'oscurita'.\r\n");
		}
		break;

	case 285:
		weather_info.sunlight = SUN_DARK;
		send_to_outdoor ("La notte stende il suo velo oscuro.\r\n");
		break;

	default:
		break;
	}

	show_moon_nodes (weather_info.moon1, "piccola luna azzurra");
	show_moon_nodes (weather_info.moon2, "luna arancione");
}


void
weather_change (void)
{
	int diff, change;
	if ((time_info.month >= 9) && (time_info.month <= 16))
		diff = (weather_info.pressure > 985 ? -2 : 2);
	else
		diff = (weather_info.pressure > 1015 ? -2 : 2);

	weather_info.change += (dice (1, 4) * diff + dice (2, 6) - dice (2, 6));

	weather_info.change = MIN (weather_info.change, 12);
	weather_info.change = MAX (weather_info.change, -12);

	weather_info.pressure += weather_info.change;

	weather_info.pressure = MIN (weather_info.pressure, 1040);
	weather_info.pressure = MAX (weather_info.pressure, 960);

	change = 0;

	switch (weather_info.sky)
	{
	case SKY_CLOUDLESS:
		if (weather_info.pressure < 990)
			change = 1;
		else if (weather_info.pressure < 1010)
			if (dice (1, 4) == 1)
				change = 1;
		break;
	case SKY_CLOUDY:
		if (weather_info.pressure < 970)
			change = 2;
		else if (weather_info.pressure < 990)
		{
			if (dice (1, 4) == 1)
				change = 2;
			else
				change = 0;
		}
		else if (weather_info.pressure > 1030)
			if (dice (1, 4) == 1)
				change = 3;

		break;
	case SKY_RAINING:
		if (weather_info.pressure < 970)
		{
			if (dice (1, 4) == 1)
				change = 4;
			else
				change = 0;
		}
		else if (weather_info.pressure > 1030)
			change = 5;
		else if (weather_info.pressure > 1010)
			if (dice (1, 4) == 1)
				change = 5;

		break;
	case SKY_LIGHTNING:
		if (weather_info.pressure > 1010)
			change = 6;
		else if (weather_info.pressure > 990)
			if (dice (1, 4) == 1)
				change = 6;

		break;
	default:
		change = 0;
		weather_info.sky = SKY_CLOUDLESS;
		break;
	}

	switch (change)
	{
	case 0:
		break;
	case 1:
		send_to_outdoor ("Il cielo sta rannuvolando.\r\n");
		weather_info.sky = SKY_CLOUDY;
		break;
	case 2:
		send_to_outdoor ("Sta cominciando a piovere.\r\n");
		weather_info.sky = SKY_RAINING;
		break;
	case 3:
		send_to_outdoor
			("Le nuvole scorrono via veloci spinte dal vento da sud.\r\n");
		weather_info.sky = SKY_CLOUDLESS;
		break;
	case 4:
		send_to_outdoor ("Sta cominciando a lampeggiare e tuonare.\r\n");
		weather_info.sky = SKY_LIGHTNING;
		break;
	case 5:
		send_to_outdoor ("Sta smettendo di piovere.\r\n");
		weather_info.sky = SKY_CLOUDY;
		break;
	case 6:
		send_to_outdoor ("Ha smesso di lampeggiare e tuonare.\r\n");
		weather_info.sky = SKY_RAINING;
		break;
	default:
		break;
	}
}


/* ************************************************************************
*   File: train.c                                       Parte di Mclandia *
*  Usage: gestione del servizio ferroviario e fluviale                    *
*                                                                         *
*  All rights reserved. 										          *
*                      Amedeo de Longis                                   *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"

int counter = 0;
int initt = 0;

#define NUM_OF_FERRYS 2


/*******************Variabili esterne*******************/

extern struct room_data *world;
extern struct time_info_data time_info;

int created = FALSE;

struct ferry_info
{

	int f_from_room;			//VNUM della stazione di partenza

	int ferry_room;				//VNUM del treno

	int f_to_room;				//VNUM della stazione di arrivo

	int f_from_room_dir_from_room;	//Direzione dalla FROM_ROOM al treno

	int f_from_room_dir_from_ferry;	//Direzione dal trenno alla FROM_ROOM

	int f_to_room_dir_from_room;	//Direzione dalla TO_ROOM al treno

	int f_to_room_dir_from_ferry;	//Direction from the ferry to TO_ROOM

	int f_time_arrive_from_room;	// non usato

	int f_time_arrive_from_room2;	// non usato

	int f_time_leave_from_room;	// non usato

	int f_time_leave_from_room2;	// non usato

	int f_time_arrive_to_room;	// non usato

	int f_time_arrive_to_room2;	// non usato

	int f_time_leave_to_room;	// non usato

	int f_time_leave_to_room2;	// non usato

	char *arrive_messg_from;	// Messaggio al treno all'arrivo alla FROM_ROOM

	char *arrive_dock_from;		// Messagio alla stazione all'arrivo alla FROM_ROOM

	char *leave_messg_from;		// Messaggio al treno quando parte dalla FROM_ROOM

	char *leave_dock_from;		// Messaggio alla stazione quando si parte dalla FROM_ROOM

	char *arrive_messg_to;		// Messagio al treno all'arrivo alla TO_ROOM

	char *arrive_dock_to;		// Messaggio alla stazione all'arrivo alla TO_ROOM

	char *leave_messg_to;		// Messaggio al treno quando parte dalla TO_ROOM

	char *leave_dock_to;		// Messaggio alla stazione quando parte dalla TO_ROOM
};

struct ferry_info ferrys[NUM_OF_FERRYS];


/*******************Inizializzazione dei Treni*******************/


void
init_train (void)
{

//IL TRENO

	ferrys[0].f_from_room = 3144;

	ferrys[0].ferry_room = 3145;

	ferrys[0].f_to_room = 5406;

	ferrys[0].f_from_room_dir_from_room = UP;

	ferrys[0].f_from_room_dir_from_ferry = DOWN;

	ferrys[0].f_to_room_dir_from_room = UP;

	ferrys[0].f_to_room_dir_from_ferry = DOWN;

	ferrys[0].f_time_arrive_from_room = 1;

	ferrys[0].f_time_arrive_from_room2 = 13;

	ferrys[0].f_time_leave_from_room = 3;

	ferrys[0].f_time_leave_from_room2 = 15;

	ferrys[0].f_time_arrive_to_room = 7;

	ferrys[0].f_time_arrive_to_room2 = 19;

	ferrys[0].f_time_leave_to_room = 9;

	ferrys[0].f_time_leave_to_room2 = 21;

	ferrys[0].arrive_messg_from =
		"Il treno e' arrivato alla Stazione di Mclandia ed i portelli sono aperti.\r\n"
        "Arrivederci al prossimo viaggio.\r\n";

	ferrys[0].arrive_dock_from =
		"Il treno diretto a NewLand arriva sferragliando sul primo binario.\r\n"
        "I signori passeggeri sono pregati di salire in carrozza.\r\n";

	ferrys[0].arrive_messg_to =
		"Il treno arriva a NewLand e si aprono gli sportelli per permetterti di scendere\r\n";

	ferrys[0].arrive_dock_to =
		"Il treno arriva e gli sportelli si aprono per accogliere i nuovi passeggeri\r\n";

	ferrys[0].leave_messg_from =
		"I portelli vengono chiusi e senti il treno per NewLand partire dolcemente \r\n"
        "e poi prendere velocita'.\r\n";

	ferrys[0].leave_dock_from =
		"Si chiudono i portelloni e vedi il treno per NewLand partire e allontanrsi\r\n";

	ferrys[0].leave_messg_to =
		"I portelli vengono chiusi e senti che il treno parte dolcemente.\r\n";

	ferrys[0].leave_dock_to =
		"Vedi Il treno per Mclandia partire e allontanarsi velocemente.\r\n";



// IL BATTELLO

	ferrys[1].f_from_room = 3049;

	ferrys[1].ferry_room = 3072;

	ferrys[1].f_to_room = 5443;

	ferrys[1].f_from_room_dir_from_room = DOWN;

	ferrys[1].f_from_room_dir_from_ferry = UP;

	ferrys[1].f_to_room_dir_from_room = DOWN;

	ferrys[1].f_to_room_dir_from_ferry = UP;

	ferrys[1].f_time_arrive_from_room = 1;

	ferrys[1].f_time_arrive_from_room2 = 13;

	ferrys[1].f_time_leave_from_room = 3;

	ferrys[1].f_time_leave_from_room2 = 15;

	ferrys[1].f_time_arrive_to_room = 7;

	ferrys[1].f_time_arrive_to_room2 = 19;

	ferrys[1].f_time_leave_to_room = 9;

	ferrys[1].f_time_leave_to_room2 = 21;

	ferrys[1].arrive_messg_from =
		"Vedi apparire il porticciolo di McLandia. Il battello attracca e tira giu' la paserella.\r\n";

	ferrys[1].arrive_dock_from =
		"Il battello fluviale arriva lentamente, attracca e tira giu' la passerella\r\n"
        "per accogliere i nuovi passeggeri\r\n";

	ferrys[1].arrive_messg_to =
		"Vedi apparire il poticciolo di NewLand. Il battello attracca e tira giu'\r\n"
        "la passerella per permettere ai passeggeri di scendere.\r\n";

	ferrys[1].arrive_dock_to =
		"Il battello fluviale arriva lentamente, attracca e tira giu' la passerella\r\n"
        "per accogliere i nuovi passeggeri\r\n\r\n";

	ferrys[1].leave_messg_from =
		"Il battello molla gli ormeggi ed inizia la navigazione allontandosi dal porticciolo di McLandia.\r\n";

	ferrys[1].leave_dock_from =
		"Il battello molla gli ormeggi ed inizia la navigazione. Lo vedi allontanarsi dalla tua vista.\r\n";

	ferrys[1].leave_messg_to =
		"Il battello molla gli ormeggi ed inizia la navigazione. Vedi la citta' allontanarsi.\r\n";

	ferrys[1].leave_dock_to =
		"Il battello molla gli ormeggi ed inizia la navigazione scomparendo all'orizzonte Vedi\r\n";

}

/*******************FUNZIONI*******************/


void
create_exit (int vnum_from, int vnum_to, int from_dir)
{
	char cebuf[128];
	int rnum_from, rnum_to;

	if ((rnum_from = real_room (vnum_from)) == NOWHERE)
	{
		sprintf (cebuf, "SYSERR: Ferry: Couldn't find the 'from' room #%d.",
				 vnum_from);
		log (cebuf);
	}
	else if ((rnum_to = real_room (vnum_to)) == NOWHERE)
	{
		sprintf (cebuf, "SYSERR: Ferry: Couldn't find the 'to' room #%d.",
				 vnum_to);
		log (cebuf);
	}
	else if (world[rnum_from].dir_option[from_dir] == NULL)
	{
		CREATE (world[rnum_from].dir_option[from_dir],
				struct room_direction_data, 1);
		world[rnum_from].dir_option[from_dir]->to_room = rnum_to;
	}
	else
	{
		sprintf (cebuf, "SYSERR: Ferry overwriting exit in room #%d.",
				 world[rnum_from].number);
		log (cebuf);
		world[rnum_from].dir_option[from_dir]->to_room = rnum_to;
	}
}

void
remove_exit (int vnum_from, int vnum_to, int from_dir)
{
	char rebuf[128];
	int rnum_from, rnum_to;

	if ((rnum_from = real_room (vnum_from)) == NOWHERE)
	{
		sprintf (rebuf, "SYSERR: Ferry: Couldn't find the 'from' room #%d.",
				 vnum_from);
		log (rebuf);
	}
	else if ((rnum_to = real_room (vnum_to)) == NOWHERE)
	{
		sprintf (rebuf, "SYSERR: Ferry: Couldn't find the 'to' room #%d.",
				 vnum_to);
		log (rebuf);
	}
	else if (world[rnum_from].dir_option[from_dir] == NULL)
	{
		sprintf (rebuf,
				 "SYSERR: Trying to remove non-existant exit in room #%d.",
				 world[rnum_from].number);
		log (rebuf);
	}
	else
	{
		free (world[rnum_from].dir_option[from_dir]);
		world[rnum_from].dir_option[from_dir] = NULL;
	}
}



void
update_train (void)
{

	int onferrynum = 0;			// CICLO TRENI #

	if (initt == 0)
	{
		init_train ();
		initt = 1;
	}

	for (onferrynum = 0; onferrynum < NUM_OF_FERRYS; onferrynum++)
	{

		if (counter == 1)
		{
			create_exit (ferrys[onferrynum].f_from_room,
						 ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_from_room_dir_from_room);

			create_exit (ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_from_room,
						 ferrys[onferrynum].f_from_room_dir_from_ferry);

			send_to_room (ferrys[onferrynum].arrive_dock_from,
						  real_room (ferrys[onferrynum].f_from_room));

			send_to_room (ferrys[onferrynum].arrive_messg_from,
						  real_room (ferrys[onferrynum].ferry_room));


		}

		if (counter == 3)
		{

			create_exit (ferrys[onferrynum].f_to_room,
						 ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_to_room_dir_from_room);

			create_exit (ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_to_room,
						 ferrys[onferrynum].f_to_room_dir_from_ferry);

			send_to_room (ferrys[onferrynum].arrive_dock_to,
						  real_room (ferrys[onferrynum].f_to_room));

			send_to_room (ferrys[onferrynum].arrive_messg_to,
						  real_room (ferrys[onferrynum].ferry_room));

		}


		if (counter == 2)
		{

			remove_exit (ferrys[onferrynum].f_from_room,
						 ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_from_room_dir_from_room);

			remove_exit (ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_from_room,
						 ferrys[onferrynum].f_from_room_dir_from_ferry);

			send_to_room (ferrys[onferrynum].leave_dock_from,
						  real_room (ferrys[onferrynum].f_from_room));

			send_to_room (ferrys[onferrynum].leave_messg_from,
						  real_room (ferrys[onferrynum].ferry_room));

		}


		if (counter == 4)
		{

			remove_exit (ferrys[onferrynum].f_to_room,
						 ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_to_room_dir_from_room);

			remove_exit (ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_to_room,
						 ferrys[onferrynum].f_to_room_dir_from_ferry);

			send_to_room (ferrys[onferrynum].leave_dock_to,
						  real_room (ferrys[onferrynum].f_to_room));

			send_to_room (ferrys[onferrynum].leave_messg_to,
						  real_room (ferrys[onferrynum].ferry_room));


		}

		// SECONDA PARTENZA ...

		if (counter == 5)
		{

			create_exit (ferrys[onferrynum].f_from_room,
						 ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_from_room_dir_from_room);

			create_exit (ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_from_room,
						 ferrys[onferrynum].f_from_room_dir_from_ferry);

			send_to_room (ferrys[onferrynum].arrive_dock_from,
						  real_room (ferrys[onferrynum].f_from_room));

			send_to_room (ferrys[onferrynum].arrive_messg_from,
						  real_room (ferrys[onferrynum].ferry_room));

		}


		if (counter == 7)
		{

			create_exit (ferrys[onferrynum].f_to_room,
						 ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_to_room_dir_from_room);

			create_exit (ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_to_room,
						 ferrys[onferrynum].f_to_room_dir_from_ferry);

			send_to_room (ferrys[onferrynum].arrive_dock_to,
						  real_room (ferrys[onferrynum].f_to_room));

			send_to_room (ferrys[onferrynum].arrive_messg_to,
						  real_room (ferrys[onferrynum].ferry_room));

		}


		if (counter == 6)
		{

			remove_exit (ferrys[onferrynum].f_from_room,
						 ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_from_room_dir_from_room);

			remove_exit (ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_from_room,
						 ferrys[onferrynum].f_from_room_dir_from_ferry);

			send_to_room (ferrys[onferrynum].leave_dock_from,
						  real_room (ferrys[onferrynum].f_from_room));

			send_to_room (ferrys[onferrynum].leave_messg_from,
						  real_room (ferrys[onferrynum].ferry_room));

		}


		if (counter == 8)
		{


			remove_exit (ferrys[onferrynum].f_to_room,
						 ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_to_room_dir_from_room);

			remove_exit (ferrys[onferrynum].ferry_room,
						 ferrys[onferrynum].f_to_room,
						 ferrys[onferrynum].f_to_room_dir_from_ferry);

			send_to_room (ferrys[onferrynum].leave_dock_to,
						  real_room (ferrys[onferrynum].f_to_room));

			send_to_room (ferrys[onferrynum].leave_messg_to,
						  real_room (ferrys[onferrynum].ferry_room));


		}


	}
	++counter;
	if (counter > 8)
		counter = 0;
	return;
}

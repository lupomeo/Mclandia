
/************************************************
McLandia Release History

Build 103

-   Regen sia per HP sia per Ram
    gain corrisponde a gain() ogni 10 sec

-   Regen intriseco per Cyborg (Ram) e Alieno (hp)
    il gain corrisponte a gain() ogni 10 sec
    
    Nota: gain() e' quella funzione su limits.c

-   Inserito questo file per tenere traccia delle modifiche

-   Cambiate stringhe associate ai flag object come ANTI_CLERIC ecc..

-   Ridefiniti Prompt e info, al posto di Mana diventa RAM

-   Corretti i bonus delle armi da fuoco dati dal livello

-   Rimosso il check sul lag per i MOB, 
    fino a quando non si trova una 	alternativa migliore (Anche LeU non fa il check)

-   Inserito calcolo Cap level su calcolo xp; il gap e' riconfigurabile
	su config.c
    E inoltre puo essere attivato o disabilitato al volo

-   Modificato parser oggetti, ora legge i flag alfabetici oltre al numero anche per
    l'attivazione dell' object affect.



Build 102


-   cambiata chiamata macro per il calcolo della hole

-   cambiato ordine di check per evitare il syserr relativo a una 
    skill che non esistono sui mob nel Flee

-   Nuovi nomi per giorni e mesi

-   traduzione messaggi di combattimento.

-   Assegnate ai terroristi le nuove spec_proc del warrior

-   Il sommersault fa cadere i mob vistyo che lo springleap funziona solo se eri a terra.


Build 101


- 	Nuova skill RETREAT: quando fai 'flee', viene fatto il check sulla skill
	e se riesce, ti ritiri altrimenti scappi a gambe levate

- 	Cambiato alcuni nomi delle skill dei psi per raggruppare in un senso piu 
	logico:
	'psi shield' diventa 'mind shield' visto che ci sono altre spell 
	che iniziano per mind
	'mind nightvision' diventa 'night sight' in modo da uniformare con le 
	altre spell del psionico che riguardano la vista come 'aura sight' e 
	'great sight'
	
-	nuova skill martial artist (45 lvl in su) 'ki shield', come sanctuary, ma 
	solo su se stesso e dura per 4 tick (nuovo comando "kishield" causa 
	bug parser sullo spazio), se fallisce non puo riprovare per 2 tick
	
- 	migliorata Spec_proc dei warrior 
   	Se sei un VW, SA, Li o Psi, preferiscono basharti
   
- 	rifinita spec_proc Martial artist, ora si comportano con piu naturalezza.
	(appena posso ti mando il prototipo del di un Mob MA)

-	Migliorata la gestione della "memoria" dei mob. Se muori ucciso da un mob, 
	questo dimentichera per sempre che hai attaccato.

- 	Riveduta tabella danni MA (riduzione danno MA di alto livello visto che
	hanno 4 attacchi)

- 	Ridotto drasticamente bonus danni armi fuoco sia per PC sia per NPC

- 	Attacco multiplo ai mob di tipo MA


Build 100

-   Aggiunto niovo comando per implementor per verificare il numero di build dell'exe

-   rimossi riferimenti a GET_SKILL in mobskill perche i mob hanno una struttura 
    differente da quella dei PC e corretti i messaggi relativi alle skill dei mob.

-   corretto bug su skill springleap che non permetteva di saltare da seduti

-   nuova spec_proc per martial_artist (se vuoi provarlo modifica pure qualche mob 
    e lo assegni, Pero occhio che al momento il danno e' quello indicato alla scheda 
    del mob e quindi cerca di proporzionarlo al livello del mob 
    Al momento i mob non hanno ancora gli attacchi multipli.


-   rimossa una chiamata a CrashSave perche invece di correggere il bug, ha peggiorato 
    il tutto. Suggerimento, al momento e' consigliabileche TUTTI si rentano e non 
    lasciano cadere  il socket e non ricollegare. 



******************************************************/
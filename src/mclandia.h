
/************************************************
McLandia Release History

Build 110
-	modificato pray
-	modifica skill verso mostri no_bash aggiungendo una percentuale di successo 
	proporzionale alla diff di livello tra ch e mob


Build 109

-   Ottimizzazione codice e "pulizia"



build 108

-   Ripistriniata vecchia curva diff_cap per il calcolo di assegnamento XP 
    a singolo PC che e' meno penalizzante. Rimane comunque la curva per il calcolo
    di contributo XP nel gruppo a favore di alti livelli.


Build 107

-   Modificata Spec Proc del vigile napoletano

-   Cambiata funzione curva level cap

-   Aggiunta funzione calcolo gain cap per i gruppi e cap per gruppi
    esso vale piu o meno (e' una funzione lineare):
    2 membri 250000
    3        350000
    4        435000
    5        525000
    6        600000
    7        700000
    8        800000
    9        900000
    10+     1000000

-   aggiunto offset per il mud_year in modo da visualizzare un anno piu credibile circa 2000 in su

-   corretto baco doorway nel caso in cui il target e' se stesso.

-   corretta procedura skill harm. Aggiunto un check che se  il danno e' maggiore degli hp della vittima
    il danno e' limitato al numero degli hp della vittima meno 1d4 come descritto nell'help


Build 106

-   Modificato buffering msg informativi PS / PSYCHIC e tengono 
    conto se esiste realmente quella spell specifica.

-   modificato Poofin/Poofout che ora puoi spostare il tuo "nome", non solo a inizio
    frase ma addiritura all'interno della frase. Il segnaposto e' $n

-   Aggiunto il learn from mistake anche nelle routine di casting delle spell


Build 105

-   migliorata gestione gruppi.

-   Nuovo comando PS per vedere le spell in esecuzione
    lo mostra in stile ps unixiano....

-   nuovo comando PSYCHIC per mostrare le spell psi attive

-   corretto bug spell area che colpiva tutti del gruppo tranne il caster e il capogruppo

-   Nuova tabella Titles/Xp per livelli per classi su level.h
    cosi e' molto piu leggibile


Build 104

-   aggiunto level cap anche in Perform_group_gain() 

-   corretta funzione errata in utils.c sostituito min() con MIN()

-   aggiunta descrizione classi mob nel parsing degli mob tipo E

-   inibito recall durante combattimento!

-   aggiunta descrizione danno armi da fuoco e da lancio nella spell ID/ Stat

-   rivista in toto la valutazione del danno da spell di area che ora
    tiene conto se il gruppo fa parte del caster o no
    
-   aggiunti sanity check nelle spec_proc critiche

-   implementato gwho (group who)!!!

.


Build 103

-   Regen sia per HP sia per Ram
    gain corrisponde a gain() ogni 10 sec

-   Regen intriseco per Cyborg (Ram) e Alieno (hp)
    il gain corrisponte a gain()/2 ogni 10 sec
    
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
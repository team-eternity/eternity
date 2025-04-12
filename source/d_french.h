//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//------------------------------------------------------------------------------
//
// Purpose: Printed strings, french translation.
//  NOTE: In BOOM, there are additional strings that have been
//        defined that are not included here.  See d_strings.h for
//        a suggested alternative.
//
// Authors: James Haley
//

#ifndef D_FRENCH_H__
#define D_FRENCH_H__
[]
//
// D_Main.C
//
constexpr const char D_DEVSTR[]    = "MODE DEVELOPPEMENT ON.\n";
constexpr const char D_CDROM[]     = "VERSION CD-ROM: DEFAULT.CFG DANS C:\\DOOMDATA\n";

//
//  M_Menu.C
//
#define PRESSKEY    "APPUYEZ SUR UNE TOUCHE."
#define PRESSYN     "APPUYEZ SUR Y OU N"
constexpr const char QUITMSG[]     = "VOUS VOULEZ VRAIMENT\nQUITTER CE SUPER JEU?";
constexpr const char LOADNET[]     = "VOUS NE POUVEZ PAS CHARGER\nUN JEU EN RESEAU!\n\n" PRESSKEY;
constexpr const char QLOADNET[]    = "CHARGEMENT RAPIDE INTERDIT EN RESEAU!\n\n" PRESSKEY;
constexpr const char QSAVESPOT[]   = "VOUS N'AVEZ PAS CHOISI UN EMPLACEMENT!\n\n" PRESSKEY;
constexpr const char SAVEDEAD[]    = "VOUS NE POUVEZ PAS SAUVER SI VOUS NE JOUEZ "
                                     "PAS!\n\n" PRESSKEY;
constexpr const char QSPROMPT[]    = "SAUVEGARDE RAPIDE DANS LE FICHIER \n\n'%s'?\n\n" PRESSYN;
constexpr const char QLPROMPT[]    = "VOULEZ-VOUS CHARGER LA SAUVEGARDE"
                                     "\n\n'%s'?\n\n" PRESSYN;
constexpr const char NEWGAME[]     = "VOUS NE POUVEZ PAS LANCER\n"
                                     "UN NOUVEAU JEU SUR RESEAU.\n\n" PRESSKEY;
constexpr const char NIGHTMARE[]   = "VOUS CONFIRMEZ? CE NIVEAU EST\n"
                                     "VRAIMENT IMPITOYABLE!n" PRESSYN;
constexpr const char SWSTRING[]    = "CECI EST UNE VERSION SHAREWARE DE DOOM.\n\n"
                                     "VOUS DEVRIEZ COMMANDER LA TRILOGIE COMPLETE.\n\n" PRESSKEY;
constexpr const char MSGOFF[]      = "MESSAGES OFF";
constexpr const char MSGON[]       = "MESSAGES ON";
constexpr const char NETEND[]      = "VOUS NE POUVEZ PAS METTRE FIN A UN JEU SUR "
                                     "RESEAU!\n\n" PRESSKEY;
constexpr const char ENDGAME[]     = "VOUS VOULEZ VRAIMENT METTRE FIN AU JEU?\n\n" PRESSYN;

constexpr const char DOSY[]        = "(APPUYEZ SUR Y POUR REVENIR AU OS.)";

constexpr const char DETAILHI[]    = "GRAPHISMES MAXIMUM ";
constexpr const char DETAILLO[]    = "GRAPHISMES MINIMUM ";
constexpr const char GAMMALVL0[]   = "CORRECTION GAMMA OFF";
constexpr const char GAMMALVL1[]   = "CORRECTION GAMMA NIVEAU 1";
constexpr const char GAMMALVL2[]   = "CORRECTION GAMMA NIVEAU 2";
constexpr const char GAMMALVL3[]   = "CORRECTION GAMMA NIVEAU 3";
constexpr const char GAMMALVL4[]   = "CORRECTION GAMMA NIVEAU 4";
constexpr const char EMPTYSTRING[] = "EMPLACEMENT VIDE";

//
//  P_inter.C
//
constexpr const char GOTARMOR[]    = "ARMURE RECUPEREE.";
constexpr const char GOTMEGA[]     = "MEGA-ARMURE RECUPEREE!";
constexpr const char GOTHTHBONUS[] = "BONUS DE SANTE RECUPERE.";
constexpr const char GOTARMBONUS[] = "BONUS D'ARMURE RECUPERE.";
constexpr const char GOTSTIM[]     = "STIMPACK RECUPERE.";
constexpr const char GOTMEDINEED[] = "MEDIKIT RECUPERE. VOUS EN AVEZ VRAIMENT BESOIN!";
constexpr const char GOTMEDIKIT[]  = "MEDIKIT RECUPERE.";
constexpr const char GOTSUPER[]    = "SUPERCHARGE!";

constexpr const char GOTBLUECARD[] = "CARTE MAGNETIQUE BLEUE RECUPEREE.";
constexpr const char GOTYELWCARD[] = "CARTE MAGNETIQUE JAUNE RECUPEREE.";
constexpr const char GOTREDCARD[]  = "CARTE MAGNETIQUE ROUGE RECUPEREE.";
constexpr const char GOTBLUESKUL[] = "CLEF CRANE BLEUE RECUPEREE.";
constexpr const char GOTYELWSKUL[] = "CLEF CRANE JAUNE RECUPEREE.";
constexpr const char GOTREDSKULL[] = "CLEF CRANE ROUGE RECUPEREE.";

constexpr const char GOTINVUL[]    = "INVULNERABILITE!";
constexpr const char GOTBERSERK[]  = "BERSERK!";
constexpr const char GOTINVIS[]    = "INVISIBILITE PARTIELLE ";
constexpr const char GOTSUIT[]     = "COMBINAISON ANTI-RADIATIONS ";
constexpr const char GOTMAP[]      = "CARTE INFORMATIQUE ";
constexpr const char GOTVISOR[]    = "VISEUR A AMPLIFICATION DE LUMIERE ";
constexpr const char GOTMSPHERE[]  = "MEGASPHERE!";

constexpr const char GOTCLIP[]     = "CHARGEUR RECUPERE.";
constexpr const char GOTCLIPBOX[]  = "BOITE DE BALLES RECUPEREE.";
constexpr const char GOTROCKET[]   = "ROQUETTE RECUPEREE.";
constexpr const char GOTROCKBOX[]  = "CAISSE DE ROQUETTES RECUPEREE.";
constexpr const char GOTCELL[]     = "CELLULE D'ENERGIE RECUPEREE.";
constexpr const char GOTCELLBOX[]  = "PACK DE CELLULES D'ENERGIE RECUPERE.";
constexpr const char GOTSHELLS[]   = "4 CARTOUCHES RECUPEREES.";
constexpr const char GOTSHELLBOX[] = "BOITE DE CARTOUCHES RECUPEREE.";
constexpr const char GOTBACKPACK[] = "SAC PLEIN DE MUNITIONS RECUPERE!";

constexpr const char GOTBFG9000[]  = "VOUS AVEZ UN BFG9000!  OH, OUI!";
constexpr const char GOTCHAINGUN[] = "VOUS AVEZ LA MITRAILLEUSE!";
constexpr const char GOTCHAINSAW[] = "UNE TRONCONNEUSE!";
constexpr const char GOTLAUNCHER[] = "VOUS AVEZ UN LANCE-ROQUETTES!";
constexpr const char GOTPLASMA[]   = "VOUS AVEZ UN FUSIL A PLASMA!";
constexpr const char GOTSHOTGUN[]  = "VOUS AVEZ UN FUSIL!";
constexpr const char GOTSHOTGUN2[] = "VOUS AVEZ UN SUPER FUSIL!";

//
// P_Doors.C
//
#define PD_BLUEO    "IL VOUS FAUT UNE CLEF BLEUE"
#define PD_REDO     "IL VOUS FAUT UNE CLEF ROUGE"
#define PD_YELLOWO  "IL VOUS FAUT UNE CLEF JAUNE"
#define PD_BLUEK    PD_BLUEO
#define PD_REDK     PD_REDO
#define PD_YELLOWK  PD_YELLOWO

//
//  G_game.C
//
constexpr const char GGSAVED[]     = "JEU SAUVEGARDE.";

//
//  HU_stuff.C
//
constexpr const char HUSTR_MSGU[]  = "[MESSAGE NON ENVOYE]";

constexpr const char HUSTR_E1M1[]  = "E1M1: HANGAR";
constexpr const char HUSTR_E1M2[]  = "E1M2: USINE NUCLEAIRE ";
constexpr const char HUSTR_E1M3[]  = "E1M3: RAFFINERIE DE TOXINES ";
constexpr const char HUSTR_E1M4[]  = "E1M4: CENTRE DE CONTROLE ";
constexpr const char HUSTR_E1M5[]  = "E1M5: LABORATOIRE PHOBOS ";
constexpr const char HUSTR_E1M6[]  = "E1M6: TRAITEMENT CENTRAL ";
constexpr const char HUSTR_E1M7[]  = "E1M7: CENTRE INFORMATIQUE ";
constexpr const char HUSTR_E1M8[]  = "E1M8: ANOMALIE PHOBOS ";
constexpr const char HUSTR_E1M9[]  = "E1M9: BASE MILITAIRE ";

constexpr const char HUSTR_E2M1[]  = "E2M1: ANOMALIE DEIMOS ";
constexpr const char HUSTR_E2M2[]  = "E2M2: ZONE DE CONFINEMENT ";
constexpr const char HUSTR_E2M3[]  = "E2M3: RAFFINERIE";
constexpr const char HUSTR_E2M4[]  = "E2M4: LABORATOIRE DEIMOS ";
constexpr const char HUSTR_E2M5[]  = "E2M5: CENTRE DE CONTROLE ";
constexpr const char HUSTR_E2M6[]  = "E2M6: HALLS DES DAMNES ";
constexpr const char HUSTR_E2M7[]  = "E2M7: CUVES DE REPRODUCTION ";
constexpr const char HUSTR_E2M8[]  = "E2M8: TOUR DE BABEL ";
constexpr const char HUSTR_E2M9[]  = "E2M9: FORTERESSE DU MYSTERE ";

constexpr const char HUSTR_E3M1[]  = "E3M1: DONJON DE L'ENFER ";
constexpr const char HUSTR_E3M2[]  = "E3M2: BOURBIER DU DESESPOIR ";
constexpr const char HUSTR_E3M3[]  = "E3M3: PANDEMONIUM";
constexpr const char HUSTR_E3M4[]  = "E3M4: MAISON DE LA DOULEUR ";
constexpr const char HUSTR_E3M5[]  = "E3M5: CATHEDRALE PROFANE ";
constexpr const char HUSTR_E3M6[]  = "E3M6: MONT EREBUS";
constexpr const char HUSTR_E3M7[]  = "E3M7: LIMBES";
constexpr const char HUSTR_E3M8[]  = "E3M8: DIS";
constexpr const char HUSTR_E3M9[]  = "E3M9: CLAPIERS";

constexpr const char HUSTR_1[]     = "NIVEAU 1: ENTREE ";
constexpr const char HUSTR_2[]     = "NIVEAU 2: HALLS SOUTERRAINS ";
constexpr const char HUSTR_3[]     = "NIVEAU 3: LE FEU NOURRI ";
constexpr const char HUSTR_4[]     = "NIVEAU 4: LE FOYER ";
constexpr const char HUSTR_5[]     = "NIVEAU 5: LES EGOUTS ";
constexpr const char HUSTR_6[]     = "NIVEAU 6: LE BROYEUR ";
constexpr const char HUSTR_7[]     = "NIVEAU 7: L'HERBE DE LA MORT";
constexpr const char HUSTR_8[]     = "NIVEAU 8: RUSES ET PIEGES ";
constexpr const char HUSTR_9[]     = "NIVEAU 9: LE PUITS ";
constexpr const char HUSTR_10[]    = "NIVEAU 10: BASE DE RAVITAILLEMENT ";
constexpr const char HUSTR_11[]    = "NIVEAU 11: LE CERCLE DE LA MORT!";
  
constexpr const char HUSTR_12[]    = "NIVEAU 12: L'USINE ";
constexpr const char HUSTR_13[]    = "NIVEAU 13: LE CENTRE VILLE";
constexpr const char HUSTR_14[]    = "NIVEAU 14: LES ANTRES PROFONDES ";
constexpr const char HUSTR_15[]    = "NIVEAU 15: LA ZONE INDUSTRIELLE ";
constexpr const char HUSTR_16[]    = "NIVEAU 16: LA BANLIEUE";
constexpr const char HUSTR_17[]    = "NIVEAU 17: LES IMMEUBLES";
constexpr const char HUSTR_18[]    = "NIVEAU 18: LA COUR ";
constexpr const char HUSTR_19[]    = "NIVEAU 19: LA CITADELLE ";
constexpr const char HUSTR_20[]    = "NIVEAU 20: JE T'AI EU!";
  
constexpr const char HUSTR_21[]    = "NIVEAU 21: LE NIRVANA";
constexpr const char HUSTR_22[]    = "NIVEAU 22: LES CATACOMBES ";
constexpr const char HUSTR_23[]    = "NIVEAU 23: LA GRANDE FETE ";
constexpr const char HUSTR_24[]    = "NIVEAU 24: LE GOUFFRE ";
constexpr const char HUSTR_25[]    = "NIVEAU 25: LES CHUTES DE SANG";
constexpr const char HUSTR_26[]    = "NIVEAU 26: LES MINES ABANDONNEES ";
constexpr const char HUSTR_27[]    = "NIVEAU 27: CHEZ LES MONSTRES ";
constexpr const char HUSTR_28[]    = "NIVEAU 28: LE MONDE DE L'ESPRIT ";
constexpr const char HUSTR_29[]    = "NIVEAU 29: LA LIMITE ";
constexpr const char HUSTR_30[]    = "NIVEAU 30: L'ICONE DU PECHE ";
  
constexpr const char HUSTR_31[]    = "NIVEAU 31: WOLFENSTEIN";
constexpr const char HUSTR_32[]    = "NIVEAU 32: LE MASSACRE";


constexpr const char HUSTR_CHATMACRO1[]  = "JE SUIS PRET A LEUR EN FAIRE BAVER!";
constexpr const char HUSTR_CHATMACRO2[]  = "JE VAIS BIEN.";
constexpr const char HUSTR_CHATMACRO3[]  = "JE N'AI PAS L'AIR EN FORME!";
constexpr const char HUSTR_CHATMACRO4[]  = "AU SECOURS!";
constexpr const char HUSTR_CHATMACRO5[]  = "TU CRAINS!";
constexpr const char HUSTR_CHATMACRO6[]  = "LA PROCHAINE FOIS, MINABLE...";
constexpr const char HUSTR_CHATMACRO7[]  = "VIENS ICI!";
constexpr const char HUSTR_CHATMACRO8[]  = "JE VAIS M'EN OCCUPER.";
constexpr const char HUSTR_CHATMACRO9[]  = "OUI";
constexpr const char HUSTR_CHATMACRO0[]  = "NON";

constexpr const char HUSTR_TALKTOSELF1[] = "VOUS PARLEZ TOUT SEUL ";
constexpr const char HUSTR_TALKTOSELF2[] = "QUI EST LA?";
constexpr const char HUSTR_TALKTOSELF3[] = "VOUS VOUS FAITES PEUR ";
constexpr const char HUSTR_TALKTOSELF4[] = "VOUS COMMENCEZ A DELIRER ";
constexpr const char HUSTR_TALKTOSELF5[] = "VOUS ETES LARGUE...";

constexpr const char HUSTR_MESSAGESENT[] = "[MESSAGE ENVOYE]";

// The following should NOT be changed unless it seems
// just AWFULLY necessary

constexpr const char HUSTR_PLRGREEN[]    = "VERT: ";
constexpr const char HUSTR_PLRINDIGO[]   = "INDIGO: ";
constexpr const char HUSTR_PLRBROWN[]    = "BRUN: ";
constexpr const char HUSTR_PLRRED[]      = "ROUGE: ";

constexpr const char HUSTR_KEYGREEN      = 'g'; /* french key should be "V" */
constexpr const char HUSTR_KEYINDIGO     = 'i';
constexpr const char HUSTR_KEYBROWN      = 'b';
constexpr const char HUSTR_KEYRED        = 'r';

//
//  AM_map.C
//

constexpr const char AMSTR_FOLLOWON[]     = "MODE POURSUITE ON";
constexpr const char AMSTR_FOLLOWOFF[]    = "MODE POURSUITE OFF";

constexpr const char AMSTR_GRIDON[]       = "GRILLE ON";
constexpr const char AMSTR_GRIDOFF[]      = "GRILLE OFF";

constexpr const char AMSTR_MARKEDSPOT[]   = "REPERE MARQUE ";
constexpr const char AMSTR_MARKSCLEARED[] = "REPERES EFFACES ";

//
//  ST_stuff.C
//

constexpr const char STSTR_MUS[]         = "CHANGEMENT DE MUSIQUE ";
constexpr const char STSTR_NOMUS[]       = "IMPOSSIBLE SELECTION";
constexpr const char STSTR_DQDON[]       = "INVULNERABILITE ON ";
constexpr const char STSTR_DQDOFF[]      = "INVULNERABILITE OFF";

constexpr const char STSTR_KFAADDED[]    = "ARMEMENT MAXIMUM! ";
constexpr const char STSTR_FAADDED[]     = "ARMES (SAUF CLEFS) AJOUTEES";

constexpr const char STSTR_NCON[]        = "BARRIERES ON";
constexpr const char STSTR_NCOFF[]       = "BARRIERES OFF";

constexpr const char STSTR_BEHOLD[]      = " inVuln, Str, Inviso, Rad, Allmap, or Lite-amp";
constexpr const char STSTR_BEHOLDX[]     = "AMELIORATION ACTIVEE";

constexpr const char STSTR_CHOPPERS[]    = "... DOESN'T SUCK - GM";
constexpr const char STSTR_CLEV[]        = "CHANGEMENT DE NIVEAU...";

//
//  F_Finale.C
//
constexpr const char E1TEXT[] =
   "APRES AVOIR VAINCU LES GROS MECHANTS\n"
   "ET NETTOYE LA BASE LUNAIRE, VOUS AVEZ\n"
   "GAGNE, NON? PAS VRAI? OU EST DONC VOTRE\n"
   " RECOMPENSE ET VOTRE BILLET DE\n"
   "RETOUR? QU'EST-QUE CA VEUT DIRE?CE"
   "N'EST PAS LA FIN ESPEREE!\n"
   "\n" 
   "CA SENT LA VIANDE PUTREFIEE, MAIS\n"
   "ON DIRAIT LA BASE DEIMOS. VOUS ETES\n"
   "APPAREMMENT BLOQUE AUX PORTES DE L'ENFER.\n"
   "LA SEULE ISSUE EST DE L'AUTRE COTE.\n"
   "\n"
   "POUR VIVRE LA SUITE DE DOOM, JOUEZ\n"
   "A 'AUX PORTES DE L'ENFER' ET A\n"
   "L'EPISODE SUIVANT, 'L'ENFER'!\n";

constexpr const char E2TEXT[] =
   "VOUS AVEZ REUSSI. L'INFAME DEMON\n"
   "QUI CONTROLAIT LA BASE LUNAIRE DE\n"
   "DEIMOS EST MORT, ET VOUS AVEZ\n"
   "TRIOMPHE! MAIS... OU ETES-VOUS?\n"
   "VOUS GRIMPEZ JUSQU'AU BORD DE LA\n"
   "LUNE ET VOUS DECOUVREZ L'ATROCE\n"
   "VERITE.\n" 
   "\n"
   "DEIMOS EST AU-DESSUS DE L'ENFER!\n"
   "VOUS SAVEZ QUE PERSONNE NE S'EN\n"
   "EST JAMAIS ECHAPPE, MAIS CES FUMIERS\n"
   "VONT REGRETTER DE VOUS AVOIR CONNU!\n"
   "VOUS REDESCENDEZ RAPIDEMENT VERS\n"
   "LA SURFACE DE L'ENFER.\n"
   "\n" 
   "VOICI MAINTENANT LE CHAPITRE FINAL DE\n"
   "DOOM! -- L'ENFER.";

constexpr const char E3TEXT[] =
   "LE DEMON ARACHNEEN ET REPUGNANT\n"
   "QUI A DIRIGE L'INVASION DES BASES\n"
   "LUNAIRES ET SEME LA MORT VIENT DE SE\n"
   "FAIRE PULVERISER UNE FOIS POUR TOUTES.\n"
   "\n"
   "UNE PORTE SECRETE S'OUVRE. VOUS ENTREZ.\n"
   "VOUS AVEZ PROUVE QUE VOUS POUVIEZ\n"
   "RESISTER AUX HORREURS DE L'ENFER.\n"
   "IL SAIT ETRE BEAU JOUEUR, ET LORSQUE\n"
   "VOUS SORTEZ, VOUS REVOYEZ LES VERTES\n"
   "PRAIRIES DE LA TERRE, VOTRE PLANETE.\n"
   "\n"
   "VOUS VOUS DEMANDEZ CE QUI S'EST PASSE\n"
   "SUR TERRE PENDANT QUE VOUS AVEZ\n"
   "COMBATTU LE DEMON. HEUREUSEMENT,\n"
   "AUCUN GERME DU MAL N'A FRANCHI\n"
   "CETTE PORTE AVEC VOUS...";



// after level 6, put this:

constexpr const char C1TEXT[] =
   "VOUS ETES AU PLUS PROFOND DE L'ASTROPORT\n" 
   "INFESTE DE MONSTRES, MAIS QUELQUE CHOSE\n" 
   "NE VA PAS. ILS ONT APPORTE LEUR PROPRE\n" 
   "REALITE, ET LA TECHNOLOGIE DE L'ASTROPORT\n" 
   "EST AFFECTEE PAR LEUR PRESENCE.\n" 
   "\n"
   "DEVANT VOUS, VOUS VOYEZ UN POSTE AVANCE\n" 
   "DE L'ENFER, UNE ZONE FORTIFIEE. SI VOUS\n" 
   "POUVEZ PASSER, VOUS POURREZ PENETRER AU\n" 
   "COEUR DE LA BASE HANTEE ET TROUVER \n" 
   "L'INTERRUPTEUR DE CONTROLE QUI GARDE LA \n" 
   "POPULATION DE LA TERRE EN OTAGE.";

// After level 11, put this:

constexpr const char C2TEXT[] =
   "VOUS AVEZ GAGNE! VOTRE VICTOIRE A PERMIS\n" 
   "A L'HUMANITE D'EVACUER LA TERRE ET \n"
   "D'ECHAPPER AU CAUCHEMAR. VOUS ETES \n"
   "MAINTENANT LE DERNIER HUMAIN A LA SURFACE \n"
   "DE LA PLANETE. VOUS ETES ENTOURE DE \n"
   "MUTANTS CANNIBALES, D'EXTRATERRESTRES \n"
   "CARNIVORES ET D'ESPRITS DU MAL. VOUS \n"
   "ATTENDEZ CALMEMENT LA MORT, HEUREUX \n"
   "D'AVOIR PU SAUVER VOTRE RACE.\n"
   "MAIS UN MESSAGE VOUS PARVIENT SOUDAIN\n"
   "DE L'ESPACE: \"NOS CAPTEURS ONT LOCALISE\n"
   "LA SOURCE DE L'INVASION EXTRATERRESTRE.\n"
   "SI VOUS Y ALLEZ, VOUS POURREZ PEUT-ETRE\n"
   "LES ARRETER. LEUR BASE EST SITUEE AU COEUR\n"
   "DE VOTRE VILLE NATALE, PRES DE L'ASTROPORT.\n"
   "VOUS VOUS RELEVEZ LENTEMENT ET PENIBLEMENT\n"
   "ET VOUS REPARTEZ POUR LE FRONT.";

// After level 20, put this:

constexpr const char C3TEXT[] =
   "VOUS ETES AU COEUR DE LA CITE CORROMPUE,\n"
   "ENTOURE PAR LES CADAVRES DE VOS ENNEMIS.\n"
   "VOUS NE VOYEZ PAS COMMENT DETRUIRE LA PORTE\n"
   "DES CREATURES DE CE COTE. VOUS SERREZ\n"
   "LES DENTS ET PLONGEZ DANS L'OUVERTURE.\n"
   "\n"
   "IL DOIT Y AVOIR UN MOYEN DE LA FERMER\n"
   "DE L'AUTRE COTE. VOUS ACCEPTEZ DE\n"
   "TRAVERSER L'ENFER POUR LE FAIRE?";

// After level 29, put this:

constexpr const char C4TEXT[] =
   "LE VISAGE HORRIBLE D'UN DEMON D'UNE\n"
   "TAILLE INCROYABLE S'EFFONDRE DEVANT\n"
   "VOUS LORSQUE VOUS TIREZ UNE SALVE DE\n"
   "ROQUETTES DANS SON CERVEAU. LE MONSTRE\n"
   "SE RATATINE, SES MEMBRES DECHIQUETES\n"
   "SE REPANDANT SUR DES CENTAINES DE\n"
   "KILOMETRES A LA SURFACE DE L'ENFER.\n"
   "\n"
   "VOUS AVEZ REUSSI. L'INVASION N'AURA.\n"
   "PAS LIEU. LA TERRE EST SAUVEE. L'ENFER\n"
   "EST ANEANTI. EN VOUS DEMANDANT OU IRONT\n"
   "MAINTENANT LES DAMNES, VOUS ESSUYEZ\n"
   "VOTRE FRONT COUVERT DE SUEUR ET REPARTEZ\n"
   "VERS LA TERRE. SA RECONSTRUCTION SERA\n"
   "BEAUCOUP PLUS DROLE QUE SA DESTRUCTION.\n";

// Before level 31, put this:

constexpr const char C5TEXT[] =
   "FELICITATIONS! VOUS AVEZ TROUVE LE\n"
   "NIVEAU SECRET! IL SEMBLE AVOIR ETE\n"
   "CONSTRUIT PAR LES HUMAINS. VOUS VOUS\n"
   "DEMANDEZ QUELS PEUVENT ETRE LES\n"
   "HABITANTS DE CE COIN PERDU DE L'ENFER.";

// Before level 32, put this:

constexpr const char C6TEXT[] =
   "FELICITATIONS! VOUS AVEZ DECOUVERT\n"
   "LE NIVEAU SUPER SECRET! VOUS FERIEZ\n"
   "MIEUX DE FONCER DANS CELUI-LA!\n";

//
// Character cast strings F_FINALE.C
//
constexpr const char CC_ZOMBIE[]   = "ZOMBIE";
constexpr const char CC_SHOTGUN[]  = "TYPE AU FUSIL";
constexpr const char CC_HEAVY[]    = "MEC SUPER-ARME";
constexpr const char CC_IMP[]      = "DIABLOTIN";
constexpr const char CC_DEMON[]    = "DEMON";
constexpr const char CC_LOST[]     = "AME PERDUE";
constexpr const char CC_CACO[]     = "CACODEMON";
constexpr const char CC_HELL[]     = "CHEVALIER DE L'ENFER";
constexpr const char CC_BARON[]    = "BARON DE L'ENFER";
constexpr const char CC_ARACH[]    = "ARACHNOTRON";
constexpr const char CC_PAIN[]     = "ELEMENTAIRE DE LA DOULEUR";
constexpr const char CC_REVEN[]    = "REVENANT";
constexpr const char CC_MANCU[]    = "MANCUBUS";
constexpr const char CC_ARCH[]     = "ARCHI-INFAME";
constexpr const char CC_SPIDER[]   = "L'ARAIGNEE CERVEAU";
constexpr const char CC_CYBER[]    = "LE CYBERDEMON";
constexpr const char CC_HERO[]     = "NOTRE HEROS";



#endif

//----------------------------------------------------------------------------
//
// $Log: d_french.h,v $
// Revision 1.3  1998/05/04  21:34:04  thldrmn
// commenting and reformatting
//
// Revision 1.2  1998/01/26  19:26:25  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:52  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

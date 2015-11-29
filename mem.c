#include "mem.h"
#include <stdlib.h>
#include <math.h>

/* mem_heap est un pointeur vers la structure allouée */
/* En réalité, ce pointeur n'existe pas */
void *mem_heap=0;

/* mem_free_zone est un pointeur vers les zones libres */
/* en réalité, ce pointeur est stockée dans les ZL tant qu'il y en a */
void *mem_free_zone=0;

/* Cette variable correspond à la taille de la mémoire allouée */
/* En réalité cette variable n'existe pas */
unsigned long mem_size;


/****************************************************************/
/* Définition de la structure de donnée de la mémoire virtuelle */

typedef struct free_zone free_zone_t;

struct free_zone {
	/* taille de la zone libre */
	unsigned long size; 	
	/* pointeur vers la prochaine zone libre */
	free_zone_t *next; 
};

/****************************************************************/


static unsigned long r_size(unsigned long size);
static int not_ok(void *zone, unsigned long size);
static int mem_fusion(void *mem);


/* Fonction d'allocation de la zone mémoire 
 * et initialise les structures de données 
 * utilisées par le gestionnaire
 *
 * @ret 0 si allocation OK, 1 sinon */
int mem_init() {
	mem_heap = malloc(HEAP_SIZE);

	if ( mem_heap == NULL ) {
		return 1;
	}
 
	mem_free_zone = mem_heap;
	mem_size = HEAP_SIZE;
	((free_zone_t *) mem_free_zone)->size = HEAP_SIZE;
	((free_zone_t *) mem_free_zone)->next = (free_zone_t *) mem_free_zone;
	
	return 0;
}


/* Fonction de libération des structures
 * et de la zone mémoire utilisée
 *
 * @ret 0 si désallocation OK, 1 sinon */
int mem_destroy() {
	if ( mem_heap == NULL ) {
		return 1; 
	} else {
		free(mem_heap);
		return 0;
	}
}

/* Fonction de calcul de la taille réelle du bloc à allouer
 * 
 * @param unsigned long size: taille souhaitée
 *
 * @ret le minimume de size et sizeof(free_zone_t)
 */
static unsigned long r_size(unsigned long size) {
	if ( size >= sizeof(free_zone_t) ) {
		return size;
	} else {
		return sizeof(free_zone_t);
	}
}


/* Fonction vérifiant que la zone fournie en paramètre convient 
 * À une allocation de size
 *
 * @param void *zone: pointeur vers la zone libre à matcher
 * @param unsigned long size taille de bloc souhaitée
 *
 * @ret 0 ssi la zone convient pour l'allocation d'un bloc de taille size
 * (c'est-à-dire si la taille est suffisante et la taille du bloc libre restant 
 * est soit nulle soit suffisament grande pour stocker les infos sur 
 * la zone libre restante)
 */
static int not_ok(void *zone, unsigned long size) {
	if ( ((free_zone_t *) zone)->size < size ) {
		/* Taille de bloc insuffisante */
	       	return 1;
	} else if ( ((free_zone_t *) zone)->size == size ) {
		/* Taille parfaite */
		return 0;
	} else if ( ((free_zone_t *) zone)->size - size < sizeof(free_zone_t) ) {
		/* Taille du bloc plus grande que la taille demandée mais 
		 * laisse un bloc vide trop petit pour stocker les infos sur
		 * le nouveau bloc libre */
		return 1;
	} else {
		return 0;
	}
}


/* Allocation d'une zone mémoire de taille
 * size initialement libre
 *
 * @param unsigned long size: taille de bloc souhaitée
 *
 * @ret pointeur vers la zone mémoire allouée (de taille réelle real_size)
 * @ret (void *) 0 en cas d'erreur ou s'il n'existe 
 * 	plus de d'emplacement libre de taille size 
 */
void *mem_alloc(unsigned long size) {
	
	free_zone_t *senti = (free_zone_t *) mem_free_zone;
	free_zone_t *prev = (free_zone_t *) mem_free_zone;
	
	/* Vérification de la validité de la taille 
	 * Ainsi que de la mémoire libre */
	if ( mem_free_zone == NULL || size <= 0 ) {
		return (void *) 0;
	}

	/* mem_heap est non nul à partir d'ici */

	/* Calcul de la taille réelle du bloc à allouer */
	unsigned long real_size = r_size(size);

	/* Recherche de la première zone mémoire de taille suffisante si existante */

	/* Vérification du premier bloc */
	if ( senti->size < real_size ) {
		senti = senti->next;
	}

	/* Recherche du premier bloc libre qui convient 
	   Si le bloc a une taille strictement supérieure à real_size 
	   Il faut qu'il reste au moins sizeof(free_zone_t) */
	while ( senti != mem_free_zone && not_ok((void *) senti, real_size) ) {
		senti = senti->next;
	}

	/* Allocation de la totalité d'un bloc */
	if ( senti->size == real_size ) {

		/* Affectation de la totalité du heap restant */
		if ( senti->next == senti ) {
			mem_free_zone = NULL;
		} else {
			/* rechaînage de la dernière zone vers mem_heap */
			mem_free_zone = (void *) (senti->next);
			prev = senti->next;
			/* recherche de la dernière zone libre */
			while ( prev->next != senti ) {
				prev = prev->next;
			}
			/* rechainage */
			prev->next = (free_zone_t *) mem_free_zone;
		}

		return (void *) senti;

	} else if ( senti->size > real_size ) {

		/* Allocation du dernier bloc restant */
		if ( senti->next == senti ) {
			mem_free_zone = (char *) senti + real_size;
			((free_zone_t *) mem_free_zone)->size = (senti->size - real_size);
			((free_zone_t *) mem_free_zone)->next = (free_zone_t *) mem_free_zone;
		} else {
			/* rechaînage de la dernière zone vers mem_heap */
			prev = senti->next;
			/* recherche de la dernière zone libre */
			while ( prev->next != senti ) {
				prev = prev->next;
			}
			/* rechaînage & modification du bloc partiellement alloué */
			prev->next = (free_zone_t *) ( (char *) senti + real_size);
			prev = senti;
			senti = (free_zone_t *) ( (char *) senti + real_size);
			senti->size = prev->size - real_size;
			senti->next = prev->next;
			senti = prev;
		}

		return (void *) senti;

	} else {
		/* Aucun bloc ne convient */
		return (void *) 0;
	}
}


/* Fonction de fusion des zones libres adjacentes
 * appelée à chaque libération de bloc 
 *
 * @param void *mem: pointeur vers la chaîne circulaire des ZL
 *
 * @return 0 si OK
 */
static int mem_fusion(void *mem) {

	free_zone_t *senti = (free_zone_t *) mem;

	/* Vérification pour tous les blocs de mem au dernier */
	while ( senti->next != mem ) {
		/* Si les blocs sont adjacents */
		if ( (void *) ((char*) senti + senti->size) == (void *) senti->next ) {
			senti->size = senti->size + senti->next->size;
			/* réécriture de mem_free_zone pour ne pas qu'il pointe sur un bloc fusionné */
			if ( (void *) senti->next == mem_free_zone ) {
				mem_free_zone = (void *) senti;
			}
			/* Rechaînage */
		       	senti->next = (senti->next)->next;
		} else {
			senti = senti->next;
		}
	}

	/* Vérification pour mem.prev et mem */
	if ( (void *) ((char*) senti + senti->size) == (void *) senti->next ) {
		senti->size = senti->size + senti->next->size;
		if ( (void *) senti->next == mem_free_zone ) {
			mem_free_zone = (void *) senti;
		}
	       	senti->next = (senti->next)->next;
	} else {
		senti = senti->next;
	}

	return 0;

}




/* Libération de la zone d'adresse zone et de taille
 * size
 *
 * @param void *zone : zone à libérer
 * @param unsigned long size: taille supposée du bloc à libérer
 *
 * @ret 0 si tout s'est bien passé, 1 sinon */
int mem_free(void *zone, unsigned long size) {

	/* Cas à ne pas traiter */
	if ( zone == NULL || size == 0 || size > mem_size || zone < mem_heap || zone > (void * ) ((char *) mem_heap + mem_size) ) {
		return 1;
	}
	
	/* Calcul de la taille réelle du bloc à libérer */
	unsigned long real_size = r_size(size);
		
	/* Si le bloc dépasse de la mémoire allouée (heap), alors return 1 */
	if ( ((char *) zone + real_size) > ((char *) mem_heap + mem_size) ) {
		return 1;
	}

	/* Cas où le heap est vide */
	if ( mem_free_zone == NULL ) {

		mem_free_zone = zone;
		((free_zone_t *) mem_free_zone)->size = real_size;
		((free_zone_t *) mem_free_zone)->next = (free_zone_t *) mem_free_zone;
		return 0;

	} else {

		/* Cas où le heap ne contient qu'un bloc */
		if ( ((free_zone_t *) mem_free_zone)->next == (free_zone_t *) mem_free_zone ) {
			((free_zone_t *) mem_free_zone)->next = (free_zone_t *) zone;
			((free_zone_t *) zone)->size = real_size;
			((free_zone_t *) zone)->next = (free_zone_t *) mem_free_zone;
			if ( zone < mem_free_zone ) {
				mem_free_zone = zone;
			}
			mem_fusion(mem_free_zone);
			return 0;
		} else {
			free_zone_t *senti = (free_zone_t *) mem_free_zone;
			free_zone_t *prev = NULL;
	
			/* Recherche du premier bloc libre suivant zone */
			if ( (void *) senti < zone ) {
				/* Si ce n'est pas mem_heap */
				do {
					senti = senti->next;
				} while ( senti != mem_free_zone && (void *) senti < zone );

				if ( (void *) senti < zone ) {
					/* cas où aucun bloc de la ZL n'est après zone dans la mémoire */
					/* zone doit être inséré en queue de chaîne de ZL */
					while ( (void *) senti->next != mem_free_zone ) {
						senti = senti->next;
					}
					senti->next = (free_zone_t *) zone;
				       	senti->next->size = real_size;
					senti->next->next = (free_zone_t *) mem_free_zone;
					mem_fusion(mem_free_zone);
					return 0;
				} else {
					/* Recherche du précédent */
					prev = senti;
					while ( prev->next != senti ) {
						prev = prev->next;
					}

					/* Insertion de zone entre prev et next */
					prev->next = (free_zone_t *) zone;
					prev->next->size = real_size;
					prev->next->next = senti;
					mem_fusion(mem_free_zone);
					return 0;
				}

			/* Insertion en tête de ZL */
			} else {
				/* Recherche de l'élément précédent de la chaîne circulaire */
				prev = (free_zone_t *) mem_free_zone;
				while ( prev->next != (free_zone_t *) mem_free_zone ) {
					prev = prev->next;
				}
				prev->next = zone;
				prev->next->size = real_size;
				prev->next->next = (free_zone_t *) mem_free_zone;
				mem_free_zone = zone;
				mem_fusion(mem_free_zone);
				return 0;
			}
		}
	}
}


/* Affichage de la liste des blocs libres en utilisant 
 * la fonction print passée en paramètre
 *
 * @ret 0 si OK, 1 sinon */
int mem_show(void (*print)(void *zone, unsigned long size)) {
	
	free_zone_t *senti = (free_zone_t *) mem_free_zone;
	if (senti == NULL) return 1;
	free_zone_t *cur = senti;
	do {
		print((void *) cur, cur->size);
		cur = cur->next;
	} while (cur != senti);
	
	return 0;
}





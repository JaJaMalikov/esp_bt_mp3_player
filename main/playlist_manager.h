// playlist_manager.h
#ifndef PLAYLIST_MANAGER_H
#define PLAYLIST_MANAGER_H

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise le gestionnaire de playlist.
 *        Scanne la carte SD et génère un ordre aléatoire à chaque démarrage.
 */
esp_err_t playlist_manager_init(void);

/**
 * @brief Récupère le chemin du prochain fichier MP3 dans l'ordre shuffle.
 */
const char *playlist_manager_get_next(void);

/**
 * @brief Recupere le chemin du fichier precedent dans l'ordre shuffle.
 */
const char *playlist_manager_get_prev(void);

/**
 * @brief Réinitialise l'index de lecture à zéro (début de playlist).
 */
void playlist_manager_reset(void);

/**
 * @brief Retourne le nombre total de fichiers trouvés.
 */
size_t playlist_manager_get_track_count(void);

/**
 * @brief Retourne l'index courant dans la playlist.
 */
size_t playlist_manager_get_current_index(void);

/**
 * @brief Retourne le chemin du morceau actuellement sélectionné.
 */
const char *playlist_manager_get_current_track(void);

/**
 * @brief Positionne l'index courant sur le fichier donne (nom seul).
 */
esp_err_t playlist_manager_set_current_by_name(const char *filename);

#ifdef __cplusplus
}
#endif

#endif // PLAYLIST_MANAGER_H


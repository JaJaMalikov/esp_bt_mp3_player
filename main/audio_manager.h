// audio_manager.h
#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Démarre le pipeline audio :
 *        lit un fichier MP3 depuis la carte SD et l’envoie en Bluetooth A2DP.
 */
esp_err_t audio_manager_start(void);

/**
 * @brief Arrête et libère complètement le pipeline audio.
 */
esp_err_t audio_manager_stop(void);

/**
 * @brief Passe immédiatement au morceau suivant.
 */
esp_err_t audio_manager_next(void);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_MANAGER_H


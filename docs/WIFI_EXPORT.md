# Export Wi-Fi des entraînements

Au démarrage, le SpeedCoach crée le réseau `SpeedCoach_Wi-Fi` (mot de passe `SpeedCoach`) avant d'initialiser les capteurs et sert la page `http://192.168.4.1`. La page liste les activités enregistrées et télécharge le fichier `Workout.tcx` compatible avec les outils d'entraînement. Le point d'accès peut donc être utilisé pendant le montage sur table, même sans MPU6050.

Avant une séance, la page permet de choisir Libre, Temps unique, Distance unique ou Intervalles, puis l'affichage du coach : allure, vitesse ou courbe d'accélération en direct. Le formulaire permet de démarrer la séance depuis le téléphone. Les séances à temps et distance s'arrêtent à leur objectif ; les intervalles s'arrêtent après le nombre de répétitions configuré. La courbe dépend du MPU6050 et n'affichera pas de données si le capteur est absent.

Le niveau de batterie est lu sur GPIO2 et actualisé à l'ouverture de la page. Si la batterie ou son diviseur n'est pas raccordé, le niveau apparaît comme indisponible. Le statut de charge reste inconnu : le brochage documenté ne fournit pas d'entrée de détection du chargeur.

Le point d'accès et le serveur web s'arrêtent au début d'une activité. Ils redémarrent après l'arrêt et la fermeture des fichiers. Les entraînements sont conservés dans la partition SPIFFS interne `storage`.

Si le réseau n'apparaît pas, redémarrer la carte et consulter le moniteur série à 115200 bauds. Le journal affiche `AP started` si le point d'accès a démarré ou `Wi-Fi export AP failed` avec la cause si l'initialisation a échoué. Vérifier également que la carte est bien un ESP32-C3 et que l'alimentation est stable. Le Wi-Fi est maintenant initialisé avant l'IMU, mais une erreur fatale d'initialisation de l'écran, de NVS ou de la mémoire flash peut toujours redémarrer le firmware.

Le pourcentage de batterie lu par l'application vient de l'entrée ADC GPIO2 avec un diviseur configuré dans `ui_status_bar.c`. Il ne peut être affiché correctement que si le diviseur et la batterie sont réellement présents et si le rapport du diviseur correspond au montage. Aucun signal d'état du chargeur n'est défini dans le brochage actuel : l'état « en charge » n'est donc pas disponible de façon fiable avec le matériel décrit par ce dépôt.

## Première mise en service du stockage

La table de partitions réserve `0x1F0000` octets à SPIFFS et suppose une mémoire flash de 4 Mo. Le build génère une image SPIFFS initiale `storage.bin`, mais ne la flashe pas automatiquement pour éviter d'effacer des activités à chaque mise à jour du firmware. Pour une nouvelle installation, flasher cette image une fois dans la partition nommée `storage` avec `parttool.py` après avoir généré la table de partitions. Ne pas reflasher cette image après avoir commencé à enregistrer des activités : cela remplacerait le contenu du stockage.

Le montage SPIFFS est volontairement configuré sans formatage automatique. Si la partition `storage` n'a jamais été initialisée, le point d'accès peut démarrer mais l'enregistrement et la liste des fichiers ne seront pas disponibles avant le flash de l'image initiale.

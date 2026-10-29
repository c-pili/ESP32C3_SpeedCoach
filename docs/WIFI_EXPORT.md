# Export Wi-Fi des entraînements

Au démarrage, le SpeedCoach crée le réseau `SpeedCoach_Wi-Fi` (mot de passe `SpeedCoach`) et sert la page `http://192.168.4.1`. La page liste les activités enregistrées et télécharge le fichier `Workout.tcx` compatible avec les outils d'entraînement.

Le point d'accès et le serveur web s'arrêtent au début d'une activité. Ils redémarrent après l'arrêt et la fermeture des fichiers. Les entraînements sont conservés dans la partition SPIFFS interne `storage`.

## Première mise en service du stockage

La table de partitions réserve `0x1F0000` octets à SPIFFS et suppose une mémoire flash de 4 Mo. Le build génère une image SPIFFS initiale `storage.bin`, mais ne la flashe pas automatiquement pour éviter d'effacer des activités à chaque mise à jour du firmware. Pour une nouvelle installation, flasher cette image une fois dans la partition nommée `storage` avec `parttool.py` après avoir généré la table de partitions. Ne pas reflasher cette image après avoir commencé à enregistrer des activités : cela remplacerait le contenu du stockage.

Le montage SPIFFS est volontairement configuré sans formatage automatique. Si la partition `storage` n'a jamais été initialisée, le point d'accès peut démarrer mais l'enregistrement et la liste des fichiers ne seront pas disponibles avant le flash de l'image initiale.

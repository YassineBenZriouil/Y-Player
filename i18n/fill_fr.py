"""Fill i18n/yplayer_fr.ts with the French translations below (run after lupdate adds strings).

    deps/qt/6.9.3/msvc2022_64/bin/lupdate.exe -locations relative -no-obsolete -target-language fr_FR \
        app core engine platform ui -ts i18n/yplayer_fr.ts
    python i18n/fill_fr.py

Strings missing here stay "unfinished" and show in English, so new UI text never breaks the build.
"""

import sys
import xml.etree.ElementTree as ET
from pathlib import Path

TS = Path(__file__).resolve().parent / "yplayer_fr.ts"

FR = {
    # About, diagnostics
    "<h3>Y-Player %1</h3><p>A fast, hardware-accelerated video player for Windows.</p><p>Built with Qt %2 and libmpv (client API %3).<br>Licensed under the GNU GPL, version 3 or later.</p>":
        "<h3>Y-Player %1</h3><p>Un lecteur vidéo rapide pour Windows, avec accélération matérielle.</p><p>Conçu avec Qt %2 et libmpv (API client %3).<br>Distribué sous licence GNU GPL, version 3 ou ultérieure.</p>",
    "File": "Fichier", "Container": "Conteneur", "Video codec": "Codec vidéo", "Hardware decoder": "Décodeur matériel",
    "Video output": "Sortie vidéo", "GPU context": "Contexte GPU", "Width": "Largeur", "Height": "Hauteur",
    "Pixel format": "Format de pixel", "Hardware surface format": "Format de surface matérielle",
    "Color matrix": "Matrice de couleurs", "Primaries (source)": "Primaires (source)", "Transfer (source)": "Transfert (source)",
    "Primaries (output)": "Primaires (sortie)", "Transfer (output)": "Transfert (sortie)", "Container FPS": "IPS du conteneur",
    "Measured FPS": "IPS mesurées", "Display FPS": "IPS de l'écran", "Video bitrate": "Débit vidéo", "Audio codec": "Codec audio",
    "Sample rate": "Fréquence d'échantillonnage", "Channels": "Canaux", "Sample format": "Format d'échantillon",
    "Audio output": "Sortie audio", "Audio device": "Périphérique audio", "Audio bitrate": "Débit audio",
    "Dropped frames (output)": "Images perdues (sortie)", "Dropped frames (decoder)": "Images perdues (décodeur)",
    "Delayed frames": "Images retardées", "A/V offset (s)": "Décalage A/V (s)", "Buffered ahead (s)": "Mis en mémoire tampon (s)",
    "Upscaler": "Mise à l'échelle", "Tone mapping": "Mappage des tons", "Video filters": "Filtres vidéo",
    "Audio filters": "Filtres audio", "Shaders": "Shaders", "Diagnostics": "Diagnostics", "Property": "Propriété",
    "Value": "Valeur", "Copy to Clipboard": "Copier dans le presse-papiers",
    # Main window
    "Video": "Vidéo", "No file loaded": "Aucun fichier chargé", "Status": "État", "Screenshot saved: %1": "Capture enregistrée : %1",
    "Video output could not be initialized; see the log for details.": "Impossible d'initialiser la sortie vidéo ; voir le journal pour plus de détails.",
    "Seek": "Position", "Left and Right arrows seek, Home and End jump to the ends": "Les flèches gauche et droite avancent ou reculent, Début et Fin vont aux extrémités",
    "Position": "Position", "Volume": "Volume", "Cannot open: %1": "Impossible d'ouvrir : %1", "&Pause": "&Pause", "&Play": "&Lecture",
    "Resumed at %1": "Reprise à %1", "Volume %1%": "Volume %1 %", "Speed %1×": "Vitesse %1×", "Audio delay %1": "Décalage audio %1",
    "Subtitle delay %1": "Décalage des sous-titres %1", "Audio": "Audio", "Subtitles": "Sous-titres", "%1: no tracks": "%1 : aucune piste",
    "Subtitles: off": "Sous-titres : désactivés", "Track %1": "Piste %1", "A-B repeat: %1 – %2": "Répétition A-B : %1 – %2",
    "A-B repeat: A = %1 (press again to set B)": "Répétition A-B : A = %1 (appuyez à nouveau pour définir B)",
    "A-B repeat off": "Répétition A-B désactivée", "Repeat off": "Répétition désactivée", "Repeat one": "Répéter le fichier",
    "Repeat all": "Tout répéter", "Shuffle on": "Lecture aléatoire activée", "Shuffle off": "Lecture aléatoire désactivée",
    "Aspect ratio %1:1": "Format %1:1", "Aspect ratio: default": "Format : par défaut", "Rotation %1°": "Rotation %1°",
    "Nothing to capture": "Rien à capturer", "Cannot create the screenshot folder %1": "Impossible de créer le dossier de captures %1",
    "Open Files": "Ouvrir des fichiers", "Add Files": "Ajouter des fichiers",
    "Open a video first, then load its subtitles": "Ouvrez d'abord une vidéo, puis chargez ses sous-titres",
    "Load Subtitle File": "Charger un fichier de sous-titres", "Load Playlist": "Charger une liste de lecture",
    "The playlist is empty": "La liste de lecture est vide", "Save Playlist": "Enregistrer la liste de lecture",
    "Playlist saved": "Liste de lecture enregistrée", "Cannot save the playlist: %1": "Impossible d'enregistrer la liste de lecture : %1",
    "%1 fps": "%1 i/s", "software decoding": "décodage logiciel", "hardware decoding (%1)": "décodage matériel (%1)",
    "Seek &Forward %1 s": "&Avancer de %1 s", "Seek &Backward %1 s": "&Reculer de %1 s", "Seek Forward %1 s": "Avancer de %1 s",
    "Seek Backward %1 s": "Reculer de %1 s", "Restart Y-Player to change the language": "Redémarrez Y-Player pour changer de langue",
    "Export Settings": "Exporter les paramètres", "Settings (*.ini)": "Paramètres (*.ini)", "Settings exported": "Paramètres exportés",
    "Cannot write %1": "Impossible d'écrire %1", "Import Settings": "Importer des paramètres",
    "\"%1\" is not a Y-Player settings file": "« %1 » n'est pas un fichier de paramètres Y-Player", "Settings imported": "Paramètres importés",
    "Profile: %1": "Profil : %1", "Save Current Settings as Profile...": "Enregistrer les paramètres actuels comme profil...",
    "Save Profile": "Enregistrer le profil", "Profile name:": "Nom du profil :", "Profile \"%1\" saved": "Profil « %1 » enregistré",
    "Delete Profile": "Supprimer un profil", "Open a file first": "Ouvrez d'abord un fichier", "Bookmark added at %1": "Signet ajouté à %1",
    "(no bookmarks for this file)": "(aucun signet pour ce fichier)", "Delete Bookmark": "Supprimer un signet",
    "Delete All Bookmarks for This File": "Supprimer tous les signets de ce fichier", "Resume playback on": "Reprise de lecture activée",
    "Resume playback off": "Reprise de lecture désactivée", "About Y-Player": "À propos de Y-Player",
    "Runs on the CPU: with hardware decoding it adds a GPU-to-CPU copy per frame": "S'exécute sur le processeur : avec le décodage matériel, ajoute une copie GPU vers CPU par image",
    # Menus
    "&File": "&Fichier", "&Recent Files": "Fichiers &récents", "&Playback": "&Lecture", "S&peed": "&Vitesse", "&A-B Repeat": "Répétition &A-B",
    "&Bookmarks": "&Signets", "&Chapters": "&Chapitres", "Re&peat": "Ré&pétition", "Off": "Désactivé", "Repeat One": "Répéter le fichier",
    "Repeat All": "Tout répéter", "&Audio": "&Audio", "&Track": "&Piste", "Output &Device": "&Périphérique de sortie", "&Channels": "&Canaux",
    "Automatic": "Automatique", "Stereo": "Stéréo", "5.1 Surround": "Surround 5.1", "7.1 Surround": "Surround 7.1", "Mono": "Mono",
    "&Subtitles": "&Sous-titres", "&Video": "&Vidéo", "&Aspect Ratio": "&Format d'image", "Default": "Par défaut", "Custom...": "Personnalisé...",
    "Custom Aspect Ratio": "Format personnalisé", "Width:height or a ratio (e.g. 21:9 or 2.4):": "Largeur:hauteur ou rapport (ex. 21:9 ou 2.4) :",
    "\"%1\" is not a valid aspect ratio": "« %1 » n'est pas un format valide", "&Rotate": "&Rotation", "%1°": "%1°", "&Profiles": "P&rofils",
    "Vie&w": "&Affichage", "&Tools": "&Outils", "&Help": "&Aide", "(empty)": "(vide)", "Clear Recent Files": "Effacer les fichiers récents",
    "%1×": "%1×", "(no chapters)": "(aucun chapitre)", "Chapter %1": "Chapitre %1", "(no tracks)": "(aucune piste)",
    "System Default": "Par défaut du système", "(no devices)": "(aucun périphérique)", " — external": " — externe",
    # Engine messages
    "the playback engine is not available": "le moteur de lecture n'est pas disponible", "Screenshot failed: %1": "Échec de la capture : %1",
    "Cannot load subtitles \"%1\": %2": "Impossible de charger les sous-titres « %1 » : %2",
    "Audio device disconnected; switched to the system default.": "Périphérique audio déconnecté ; retour au périphérique par défaut.",
    "Cannot play \"%1\": %2": "Impossible de lire « %1 » : %2",
    # Playlist panel
    "Playlist": "Liste de lecture", "Remove": "Retirer", "Play": "Lire", "Add Files...": "Ajouter des fichiers...",
    "Clear Playlist": "Vider la liste de lecture", "Add...": "Ajouter...", "Clear": "Vider",
    # Profiles, equalizer presets
    "Balanced (default)": "Équilibré (par défaut)", "High quality": "Haute qualité", "Power saver": "Économie d'énergie",
    "Flat": "Neutre", "Bass boost": "Basses renforcées", "Treble boost": "Aigus renforcés", "Voice": "Voix", "Loudness": "Loudness",
    # Settings window
    "Settings": "Paramètres", "Search settings": "Rechercher dans les paramètres", "Categories": "Catégories", "General": "Général",
    "Continue files where they were stopped": "Reprendre les fichiers là où ils ont été arrêtés", "Resume playback": "Reprise de lecture",
    "Remembers positions of files longer than one minute": "Mémorise la position des fichiers de plus d'une minute",
    "Keep the player above other windows": "Garder le lecteur au-dessus des autres fenêtres", "Always on top": "Toujours au premier plan",
    "Follow Windows": "Suivre Windows", "Dark": "Sombre", "Light": "Clair", "Theme": "Thème",
    "Windows high-contrast mode always takes precedence": "Le mode contraste élevé de Windows est toujours prioritaire",
    "Language": "Langue", "Takes effect the next time Y-Player starts": "Prend effet au prochain démarrage de Y-Player",
    "Export...": "Exporter...", "Import...": "Importer...", "Settings file": "Fichier de paramètres",
    "Export or import all settings, shortcuts and profiles": "Exporter ou importer tous les paramètres, raccourcis et profils",
    "Playback": "Lecture", " s": " s", "Short seek (arrow keys)": "Saut court (flèches)", "Long seek (Shift + arrows)": "Saut long (Maj + flèches)",
    "Land on the exact frame": "S'arrêter sur l'image exacte", "Exact seeking": "Recherche précise",
    "Off jumps to the nearest keyframe: faster on files with long keyframe intervals": "Désactivé, saute à l'image clé la plus proche : plus rapide sur les fichiers à longs intervalles d'images clés",
    "Brightness": "Luminosité", "Contrast": "Contraste", "Saturation": "Saturation", "Gamma": "Gamma", "Hue": "Teinte",
    "Applied by the GPU renderer: no measurable cost": "Appliqué par le rendu GPU : aucun coût mesurable",
    "Automatic (interlaced files only)": "Automatique (fichiers entrelacés uniquement)", "Always": "Toujours", "Never": "Jamais",
    "Deinterlacing": "Désentrelacement", "Uses the GPU's video processor when hardware decoding is on": "Utilise le processeur vidéo du GPU quand le décodage matériel est actif",
    "Hardware acceleration": "Accélération matérielle", "Automatic (recommended)": "Automatique (recommandé)", "D3D11 (zero-copy)": "D3D11 (sans copie)",
    "D3D11 with copy-back (compatible with CPU filters)": "D3D11 avec recopie (compatible avec les filtres CPU)", "NVIDIA NVDEC": "NVIDIA NVDEC",
    "Off (software decoding)": "Désactivé (décodage logiciel)", "Hardware decoding": "Décodage matériel",
    "Automatic falls back to software for formats the GPU cannot decode. The diagnostics panel shows which decoder is actually active.": "Le mode automatique passe au décodage logiciel pour les formats que le GPU ne sait pas décoder. Le panneau de diagnostic indique le décodeur réellement utilisé.",
    "Rendering": "Rendu", "Bilinear (fastest)": "Bilinéaire (le plus rapide)", "Spline36 (balanced)": "Spline36 (équilibré)",
    "Lanczos (sharper)": "Lanczos (plus net)", "EWA Lanczos sharp (best, highest GPU cost)": "EWA Lanczos net (meilleur, coût GPU le plus élevé)",
    "Higher quality scalers cost more GPU time per frame": "Les meilleurs algorithmes demandent plus de temps GPU par image",
    "BT.2390": "BT.2390", "Spline": "Spline", "Hable (filmic)": "Hable (cinéma)", "Mobius": "Mobius", "Reinhard": "Reinhard",
    "Clip (no compression)": "Écrêtage (sans compression)", "HDR tone mapping": "Mappage des tons HDR",
    "How HDR video is converted for SDR displays": "Conversion de la vidéo HDR pour les écrans SDR", "Measure brightness per scene": "Mesurer la luminosité par scène",
    "HDR peak detection": "Détection du pic HDR", "Better HDR tone mapping for a small GPU cost": "Meilleur mappage HDR pour un faible coût GPU",
    "Send HDR to HDR-enabled displays": "Envoyer le HDR aux écrans compatibles", "HDR passthrough": "HDR direct",
    "Needs Windows HDR turned on for the display": "Nécessite d'activer le HDR de Windows pour l'écran",
    "Resample to the display refresh rate": "Rééchantillonner à la fréquence de l'écran", "Smooth motion": "Mouvement fluide",
    "Removes judder (e.g. 24 fps on 60 Hz); extra GPU work on every refresh": "Supprime les saccades (ex. 24 i/s sur 60 Hz) ; travail GPU supplémentaire à chaque rafraîchissement",
    "Even out loud and quiet passages": "Équilibrer les passages forts et faibles", "Volume normalization": "Normalisation du volume",
    "Dynamic normalization; negligible CPU": "Normalisation dynamique ; coût CPU négligeable",
    "Prevent clipping when mixing surround down to stereo": "Éviter la saturation lors du mixage surround vers stéréo",
    "Downmix normalization": "Normalisation du mixage stéréo", "e.g. jpn,eng": "ex. jpn,eng", "Preferred audio languages": "Langues audio préférées",
    "Comma-separated language codes, first match wins": "Codes de langue séparés par des virgules ; la première correspondance l'emporte",
    "e.g. eng,fre": "ex. eng,fre", "Preferred subtitle languages": "Langues de sous-titres préférées",
    "Files that start with the video's name": "Fichiers commençant par le nom de la vidéo", "Files with exactly the video's name": "Fichiers portant exactement le nom de la vidéo",
    "Load external subtitles": "Charger les sous-titres externes", "Subtitle files in the video's folder": "Fichiers de sous-titres du dossier de la vidéo",
    "Edit style...": "Modifier le style...", "Font, colors and position": "Police, couleurs et position", "Filters": "Filtres", " %": " %",
    "Sharpen": "Netteté", "GPU shader; small cost. 0 turns it off": "Shader GPU ; faible coût. 0 le désactive", "Low": "Faible",
    "Medium": "Moyen", "High": "Élevé", "Denoise": "Réduction du bruit",
    "CPU filter: with hardware decoding every frame is copied to the CPU. High cost at 4K": "Filtre CPU : avec le décodage matériel, chaque image est copiée vers le CPU. Coût élevé en 4K",
    "Enabled": "Activé", "Equalizer": "Égaliseur", "10-band audio equalizer; negligible CPU": "Égaliseur audio 10 bandes ; coût CPU négligeable",
    "Equalizer bands": "Bandes de l'égaliseur", "Keyboard shortcuts": "Raccourcis clavier", "Edit keyboard shortcuts...": "Modifier les raccourcis clavier...",
    "Shortcuts": "Raccourcis", "Rebind any action; duplicate keys are refused": "Réattribuez n'importe quelle action ; les touches en double sont refusées",
    "Reload the last playlist when Y-Player starts": "Recharger la dernière liste de lecture au démarrage", "Remember playlist": "Mémoriser la liste de lecture",
    "Play the next entry when a file ends": "Lire l'élément suivant à la fin d'un fichier", "Continue through the playlist": "Enchaîner la liste de lecture",
    "File associations": "Associations de fichiers", "Open with Y-Player": "Ouvrir avec Y-Player",
    "Adds Y-Player to Open with for the checked types, for this Windows user only. Windows asks you to confirm the default player in Settings.": "Ajoute Y-Player au menu Ouvrir avec pour les types cochés, pour cet utilisateur Windows uniquement. Windows vous demande de confirmer le lecteur par défaut dans les Paramètres.",
    "Performance": "Performances", " MB": " Mo", "Read-ahead cache": "Cache de lecture anticipée",
    "Memory used to buffer ahead; larger helps slow drives": "Mémoire utilisée pour la mise en tampon ; plus grand aide les disques lents",
    "Software decoder threads": "Threads du décodeur logiciel", "Only used when hardware decoding is off": "Utilisé uniquement quand le décodage matériel est désactivé",
    "Advanced diagnostics": "Diagnostics avancés", "Warnings and errors": "Avertissements et erreurs", "Informational": "Informations",
    "Verbose (large logs)": "Détaillé (journaux volumineux)", "Playback engine logging": "Journal du moteur de lecture",
    "Show diagnostics panel": "Afficher le panneau de diagnostic", "Live statistics": "Statistiques en direct", "Open log folder": "Ouvrir le dossier des journaux",
    "Log files": "Fichiers journaux", "%1k": "%1k", "%1 Hz": "%1 Hz", "Check all": "Tout cocher", "Apply associations": "Appliquer les associations",
    "Open Windows default apps": "Ouvrir les applications par défaut de Windows",
    "Saved. To make Y-Player the default, choose it in Windows default apps.": "Enregistré. Pour faire de Y-Player le lecteur par défaut, choisissez-le dans les applications par défaut de Windows.",
    "Could not update associations: %1": "Impossible de mettre à jour les associations : %1",
    # Shortcut dialog
    "Keyboard Shortcuts": "Raccourcis clavier", "Action": "Action", "Shortcut": "Raccourci", "Assign": "Attribuer", "Shortcut:": "Raccourci :",
    "All shortcuts restored to their defaults.": "Tous les raccourcis ont été rétablis.",
    "%1 is already used by \"%2\". Change that shortcut first.": "%1 est déjà utilisé par « %2 ». Modifiez d'abord ce raccourci.",
    "Shortcut removed.": "Raccourci supprimé.", "Shortcut assigned.": "Raccourci attribué.",
    # Subtitle style dialog
    "Subtitle Style": "Style des sous-titres", "Font:": "Police :", "Size:": "Taille :", "Text color:": "Couleur du texte :",
    "Outline color:": "Couleur du contour :", "Outline size:": "Épaisseur du contour :",
    "0 = top of the video, 100 = bottom edge, above 100 = below the video": "0 = haut de la vidéo, 100 = bord inférieur, au-delà de 100 = sous la vidéo",
    "Vertical position:": "Position verticale :", "Also apply to styled (ASS/SSA) subtitles": "Appliquer aussi aux sous-titres stylés (ASS/SSA)",
    "Choose Color": "Choisir une couleur",
    # Actions
    "&Open Files...": "&Ouvrir des fichiers...", "&Add Files to Playlist...": "&Ajouter à la liste de lecture...",
    "Load &Subtitle File...": "Charger des &sous-titres...", "&Load Playlist...": "&Charger une liste de lecture...",
    "Sa&ve Playlist...": "En&registrer la liste de lecture...", "E&xit": "&Quitter", "&Play / Pause": "&Lecture / Pause", "&Stop": "&Arrêt",
    "Pre&vious File": "Fichier &précédent", "&Next File": "Fichier &suivant", "Seek &Forward 5 s": "&Avancer de 5 s",
    "Seek &Backward 5 s": "&Reculer de 5 s", "Seek Forward 30 s": "Avancer de 30 s", "Seek Backward 30 s": "Reculer de 30 s",
    "Next &Frame": "&Image suivante", "Previous F&rame": "I&mage précédente", "Faster (+0.1)": "Plus rapide (+0,1)",
    "Slower (-0.1)": "Plus lent (-0,1)", "Normal Speed": "Vitesse normale", "Set A-B Point / Clear": "Définir le point A-B / Effacer",
    "Clear A-B Repeat": "Effacer la répétition A-B", "Next Chapter": "Chapitre suivant", "Previous Chapter": "Chapitre précédent",
    "Cycle Repeat Mode": "Changer de mode de répétition", "S&huffle": "Lecture &aléatoire", "&Resume From Last Position": "&Reprendre à la dernière position",
    "Add &Bookmark": "Ajouter un &signet", "Volume &Up": "&Monter le volume", "Volume &Down": "&Baisser le volume", "&Mute": "&Muet",
    "Next Audio &Track": "&Piste audio suivante", "Audio Delay +100 ms": "Décalage audio +100 ms", "Audio Delay -100 ms": "Décalage audio -100 ms",
    "Reset Audio Delay": "Réinitialiser le décalage audio", "Next Subtitle &Track": "&Piste de sous-titres suivante",
    "Subtitle Delay +100 ms": "Décalage des sous-titres +100 ms", "Subtitle Delay -100 ms": "Décalage des sous-titres -100 ms",
    "Reset Subtitle Delay": "Réinitialiser le décalage des sous-titres", "Subtitle &Style...": "&Style des sous-titres...",
    "&Fullscreen": "&Plein écran", "Exit Fullscreen": "Quitter le plein écran", "Save &Screenshot": "Enregistrer une &capture",
    "Save Screenshot (Without Subtitles)": "Enregistrer une capture (sans sous-titres)", "Cycle Aspect Ratio": "Changer de format d'image",
    "Rotate 90° Clockwise": "Pivoter de 90° (sens horaire)", "Flip &Horizontally": "Retourner &horizontalement",
    "Flip &Vertically": "Retourner &verticalement", "Always on &Top": "&Toujours au premier plan", "&Borderless Window": "Fenêtre &sans bordure",
    "&Playlist": "&Liste de lecture", "&Settings...": "&Paramètres...", "&Diagnostics": "&Diagnostics",
    "&Keyboard Shortcuts...": "&Raccourcis clavier...", "&About Y-Player": "À &propos de Y-Player",
}

# %n plurals: French uses the singular for 0 and 1.
FR_PLURAL = {
    "%n playlist entries skipped (network or unreadable).": ("%n entrée ignorée (réseau ou illisible).", "%n entrées ignorées (réseau ou illisibles)."),
    "%n file(s) added to the playlist": ("%n fichier ajouté à la liste de lecture", "%n fichiers ajoutés à la liste de lecture"),
    "%n saved shortcut(s) conflicted or were unknown and were reset.": (
        "%n raccourci enregistré était en conflit ou inconnu et a été réinitialisé.",
        "%n raccourcis enregistrés étaient en conflit ou inconnus et ont été réinitialisés."),
}


def main():
    tree = ET.parse(TS)
    done = missing = 0
    for msg in tree.iter("message"):
        source = msg.findtext("source", "")
        tr = msg.find("translation")
        if msg.get("numerus") == "yes" and source in FR_PLURAL:
            for child in list(tr):
                tr.remove(child)
            for form in FR_PLURAL[source]:
                ET.SubElement(tr, "numerusform").text = form
            tr.attrib.pop("type", None)
            done += 1
        elif source in FR:
            tr.text = FR[source]
            tr.attrib.pop("type", None)
            done += 1
        else:
            missing += 1
    tree.write(TS, encoding="utf-8", xml_declaration=True)
    print(f"translated {done}, left in English {missing}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

# 🏙️ Smart City IoT: Système de Surveillance & Télémétrie ESP32
 
Ce projet est une solution complète **"End-to-End"** (de l'embarqué jusqu'au Cloud) permettant la gestion centralisée d'une flotte de caméras autonomes sur batterie (ESP32-CAM).
 
Il répond à une problématique complexe en IoT : **Comment transmettre des images lourdes et configurer des périphériques à distance via un protocole léger comme MQTT ?**
 
## 📖 Concept Global et Architecture
 
Le système repose sur une architecture décentralisée où le code Python agit comme un chef d'orchestre entre l'interface utilisateur et les microcontrôleurs.
 
### 1. Le Défi Technique (Pourquoi ce projet ?)
Les protocoles IoT comme MQTT limitent souvent la taille des messages (payload). Transmettre une image JPEG de 40Ko en une seule fois est souvent impossible ou instable sur des microcontrôleurs.
> **Solution du projet :** L'image est découpée en "chunks" (petits morceaux) par l'ESP32, envoyée paquet par paquet, et **réassemblée dynamiquement** par le serveur Python avant d'être sauvegardée.
 
### 2. Flux de Données
1.  **L'ESP32 (Client)** : Capture une image, la découpe, envoie les morceaux via MQTT, et publie régulièrement l'état de sa batterie.
2.  **Le Worker Python (`mariaDB.py`)** : Écoute en permanence. Il détecte le début d'une image, accumule les données binaires en mémoire tampon, et sauvegarde le fichier final une fois le signal de fin reçu.
3.  **Le Serveur Web (`appli.py`)** : Affiche les données stockées en Base de Données (MariaDB) et permet à l'utilisateur d'envoyer des commandes de configuration vers les caméras.
 
---
 
## 🚀 Fonctionnalités Détaillées
 
### 📸 1. Gestion Avancée d'Images (Pseudo-Streaming)
* **Réception Fragmentée :** Le système gère la reconstruction de fichiers binaires à partir de flux MQTT multiples.
* **Galerie Dynamique :** Visualisation des captures horodatées pour chaque caméra sélectionnée.
* **Nettoyage :** Possibilité de supprimer les images (fichier physique + entrée BDD) depuis l'interface Web.
 
### 🔋 2. Monitoring Énergétique (Smart Grid)
* **Suivi de Batterie :** Enregistrement continu de la tension (Voltage) et du pourcentage de charge.
* **Visualisation Graphique :** Utilisation de **Chart.js** pour générer des courbes d'évolution, permettant de prédire quand recharger les caméras.
 
### ⚙️ 3. Configuration "Over-the-Air" (OTA) des Paramètres
Plus besoin de brancher l'ESP32 en USB pour changer un réglage. L'interface Web permet de modifier à distance :
* **Qualité d'image :** Résolution (QVGA à UXGA) et taux de compression (Quality 0-63).
* **Traitement d'image :** Luminosité, Contraste, Saturation.
* **Orientation :** Effet Miroir et Flip vertical.
* **Connectivité :** Mise à jour des identifiants Wi-Fi (SSID/Password) stockés en BDD pour un futur provisionning.
 
### 🤖 4. Auto-Découverte (Plug & Play)
* Le système est conçu pour être **scalable**. Si une nouvelle caméra (avec un ID inconnu) publie un message MQTT, le serveur la détecte automatiquement et l'ajoute à la flotte gérée sans intervention humaine.
 
---
 
## 📂 Organisation du Code (Backend)
 
| Fichier | Rôle Technique |
| :--- | :--- |
| **`appli.py`** | **Serveur Frontend (Flask)**. Il gère l'interaction humain-machine. Il lit la BDD pour l'affichage et publie des messages MQTT (`.../update`) pour donner des ordres aux caméras. |
| **`mariaDB.py`** | **Service Backend (Worker)**. C'est le "cerveau" invisible. Il tourne en boucle infinie, gère les abonnements MQTT (`subscribe`), reconstruit les images binaires (`b"".join()`) et insère les données capteurs en BDD. |
| **`TablesMariaDB.py`** | **ORM (SQLAlchemy)**. Définit la structure des tables (`Camera`, `Image`, `Battrie`, `Parametre`) et initialise la base de données. |
 
---
 
## 🛠️ Guide d'Installation
 
### Prérequis
* Python 3.9+
* Serveur MariaDB (Port 3306 par défaut)
* Broker MQTT (ex: Mosquitto, Port 1883)
 
### Installation
1.  **Cloner le repo et installer les dépendances :**
    ```bash
    pip install flask sqlalchemy mariadb paho-mqtt
    ```
    *(Note : Sur Linux, le paquet système `libmariadb-dev` peut être requis).*
 
2.  **Configuration des Scripts :**
    * Ouvrez `appli.py` et `mariaDB.py`.
    * Modifiez la ligne `create_engine` : `mariadb+mariadbconnector://USER:PASS@IP_SERVER:3306/DB_NAME`.
    * Modifiez l'adresse IP du broker MQTT : `client.connect("IP_BROKER", 1883)`.
    * **Important :** Dans `mariaDB.py`, changez la variable `dossier` pour qu'elle pointe vers le dossier `static/images` de ce projet.
 
3.  **Initialisation de la Base de Données :**
    ```bash
    python TablesMariaDB.py
    ```
 
---
 
## 📡 Le Code Embarqué (ESP32 Firmware)
 
Pour que ce système fonctionne, votre ESP32 doit exécuter un code C++ (Arduino/PlatformIO) qui respecte la logique suivante. Il agit en parallèle du serveur Python.
 
### Logique du Firmware
1.  **Setup :** Connexion Wi-Fi + MQTT. Abonnement aux topics de configuration (`.../parametre/camera/+`).
2.  **Boucle (Loop) :**
    * Vérifier si un message MQTT est arrivé (ex: changement de résolution).
    * Prendre une photo.
    * **Envoyer la photo (Protocole séquentiel) :**
        1.  Envoyer le nom du fichier sur le topic `start`.
        2.  Envoyer l'image binaire par paquets de 1024 ou 2048 octets sur le topic `data`.
        3.  Envoyer un message vide sur le topic `end` pour signaler au Python de fermer le fichier.
    * Lire la tension batterie et l'envoyer.
    * Passer en *Deep Sleep* (optionnel) pour économiser l'énergie.
 
### API MQTT (Topics)
L'arborescence des topics est : `B3/MartinOmar/{ID_CAMERA}/...`
 
| Topic Suffixe | Direction | Type Payload | Description |
| :--- | :--- | :--- | :--- |
| `/image/start` | ESP -> Py | String | Nom du fichier (ex: `img_01.jpg`). Déclenche l'ouverture du buffer. |
| `/image/data` | ESP -> Py | **Binary** | Un morceau de l'image (Chunk). |
| `/image/end` | ESP -> Py | Any | Fin de transmission. Le Python sauvegarde le fichier. |
| `/parametre/battrie/level` | ESP -> Py | Int | Pourcentage batterie (ex: `80`). |
| `/parametre/camera/resolution` | Py -> ESP | String | `QVGA`, `VGA`, `SVGA`, `XGA`... |
| `/parametre/camera/update` | Py -> ESP | String | Signal `"update"`. L'ESP doit appliquer les nouvelles configs. |
 
---
 
## ▶️ Démarrage du Projet
 
Le système nécessite deux terminaux ouverts simultanément :
 
**Terminal 1 : Le Récepteur de Données (Backend)**
*Ce script ne s'arrête jamais, il écoute l'ESP32.*
```bash
python mariaDB.py

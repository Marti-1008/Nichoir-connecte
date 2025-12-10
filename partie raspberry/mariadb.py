from sqlalchemy import create_engine

from sqlalchemy.orm import sessionmaker

from TablesMariaDB import Image, Battrie, Wifi, CamParametre, Camera

import paho.mqtt.client as client

from datetime import datetime

import time

import os

import base64
 
# Configuration BDD

engine = create_engine("mariadb+mariadbconnector://martin:1234@192.168.2.58:3306/RPG", echo=True)

Session = sessionmaker(bind=engine)
 
# Dictionnaires pour gÃ©rer plusieurs camÃ©ras en mÃªme temps

# Structure : { camera_id : [chunk1, chunk2, ...] }

buffers_images = {} 

noms_images = {}
 
# Variables globales config

dernier_wifi = {}

dernier_cam = {}

update = False
 
def on_message(client, userdata, message):

    try:

        topic = message.topic

        parts = topic.split('/')

        # On verifie si c'est un format valide avec un ID numerique

        # Ex: B3/MartinOmar/1/...

        if len(parts) >= 3 and parts[2].isdigit():

            cam_id = int(parts[2])

            # Verification eclair en BDD pour voir si la camera existe

            session = Session()

            exists = session.query(Camera).filter_by(id=cam_id).first()

            if not exists:

                print(f"--- NOUVELLE CAMERA DETECTEE : {cam_id} ---")

                new_cam = Camera(id=cam_id, nom=f"ESP32-CAM {cam_id}", description="Auto-detect MQTT")

                session.add(new_cam)

                session.commit()

            session.close()

    except Exception as e:

        print(f"Erreur detection auto: {e}")
 
def envoyerParametresVersESP32(cli,cam_id):

    global dernier_wifi, dernier_cam

    session = Session()
 
    cam_param = session.query(CamParametre)\

        .filter(CamParametre.NumeroCam == cam_id)\

        .order_by(CamParametre.id.desc())\

        .first()

    session.close()
 
    if cam_param:

        print(f"[Cam {cam_id}] Envoi des parametres...")

        # 2. On construit le topic AVEC L'ID

        base_topic = f"B3/MartinOmar/{cam_id}/parametre/camera"

        cli.publish(f"{base_topic}/resolution", str(cam_param.resolution))

        cli.publish(f"{base_topic}/quality", str(cam_param.quality))

        cli.publish(f"{base_topic}/contrast", str(cam_param.contrast))

        cli.publish(f"{base_topic}/brightness", str(cam_param.brightness))

        cli.publish(f"{base_topic}/saturation", str(cam_param.saturation))

        # Ajouter les autres (flip, mirror...)

    else:

        print(f"[Cam {cam_id}] Pas de parametres trouves en BDD.")
 
# --- CALLBACKS MQTT ---
 
def fctTopicBattrie(client, userdata, message):

    try:

        topic = message.topic

        payload = message.payload.decode()

        # 1. Extraction de l'ID de la camÃ©ra depuis le topic

        # Ex: B3/MartinOmar/1/parametre/battrie/level

        parts = topic.split('/')

        if len(parts) < 3 or not parts[2].isdigit():

            return # Topic invalide

        cam_id = int(parts[2]) 

        pourc = 0

        volt = 0.0
 
        # On traite selon le type de message

        if "level" in topic:

            pourc = int(float(payload)) # Conversion safe

            print(f"[Cam {cam_id}] Batterie Level: {pourc}%")

        elif "tension" in topic:

            volt = float(payload)

            print(f"[Cam {cam_id}] Tension: {volt}V")
 
        # Enregistrement en BDD

        # Note: IdÃ©alement, il faudrait grouper level et tension, 

        # mais ici on insÃ¨re dÃ¨s qu'on reÃ§oit une info pour simplifier.

        session = Session()

        maBattrie = Battrie(

            NumeroCam=cam_id,  # AJOUT IMPORTANT : On lie Ã  la camÃ©ra

            poucentage=pourc,

            voltage=volt,

            date=datetime.now()

        )

        session.add(maBattrie)

        session.commit()

        session.close()
 
    except Exception as e:

        print(f"Erreur Batterie: {e}")
 
 
def fctTopicImage(client, userdata, message):

    global buffers_images, noms_images
 
    topic = message.topic

    # 1. Extraction ID Camera

    parts = topic.split('/')

    if len(parts) < 3 or not parts[2].isdigit():

        return

    cam_id = int(parts[2])
 
    if "start" in topic:

        # Le nom du fichier est du texte, on le decode

        nom_fic = message.payload.decode().strip()

        buffers_images[cam_id] = [] 

        noms_images[cam_id] = nom_fic

        print(f"[Cam {cam_id}] Debut reception (Binaire) : {nom_fic}")
 
    elif "data" in topic:

        # MODIFICATION 1 : On ajoute les BYTES bruts, on ne decode PAS en string

        if cam_id in buffers_images:

            buffers_images[cam_id].append(message.payload) 

            # message.payload est deja en type 'bytes'
 
    elif "end" in topic:

        if cam_id in buffers_images and len(buffers_images[cam_id]) > 0:

            print(f"\n[Cam {cam_id}] Fin reception. Reconstruction...")

            try:

                # MODIFICATION 2 : On joint les morceaux binaires avec b"" (bytes vide)

                # Au lieu de "".join(...) qui est pour du texte

                image_bytes = b"".join(buffers_images[cam_id])

                # MODIFICATION 3 : Suppression de base64.b64decode()

                # Les donnees sont deja l'image pure.

                # Chemin

                dossier = r"/home/martin/Desktop/smartcities/static/images"

                os.makedirs(dossier, exist_ok=True)

                timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

                nom_final = f"cam{cam_id}_{timestamp}.jpg"

                chemin_complet = os.path.join(dossier, nom_final)
 
                # ecriture disque (wb = write bytes)

                with open(chemin_complet, "wb") as f:

                    f.write(image_bytes)

                # ecriture BDD

                session = Session()

                nouvelle_image = Image(

                    NumeroCam=cam_id,

                    path=chemin_complet,

                    date=datetime.now()

                )

                session.add(nouvelle_image)

                session.commit()

                session.close()

                print(f"[Cam {cam_id}] Image sauvegardee : {nom_final}")
 
            except Exception as e:

                print(f"[Cam {cam_id}] Erreur reconstruction : {e}")

            # Nettoyage memoire pour cette camera

            buffers_images[cam_id] = []

            noms_images[cam_id] = None
 
 
def fctTopicUpdate(client, userdata, message):

    try:

        topic = message.topic

        parts = topic.split('/')

        if len(parts) >= 3 and parts[2].isdigit():

            cam_id = int(parts[2])

            print(f"Demande de mise a jour recue pour la Camera {cam_id}")

            # On declenche l'envoi immediatement pour cette camera

            envoyerParametresVersESP32(client, cam_id)

    except Exception as e:

        print(f"Erreur update: {e}")
 
 
# --- CONFIGURATION MQTT ---

cli = client.Client(client.CallbackAPIVersion.VERSION2)

cli.on_message = on_message

cli.connect("192.168.2.58", 1883)
 
# 1. Abonnements avec le Wildcard '+' pour l'ID

# Ex: B3/MartinOmar/1/image/start

cli.subscribe("B3/MartinOmar/+/parametre/battrie/#") # '#' attrape level et tension

cli.subscribe("B3/MartinOmar/+/image/start")

cli.subscribe("B3/MartinOmar/+/image/data", qos=1)

cli.subscribe("B3/MartinOmar/+/image/end")

cli.subscribe("B3/MartinOmar/parametre/camera/update") # Celui-ci peut rester global
 
# 2. Ajout des callbacks AVEC le wildcard '+'

# C'est ici que tu avais l'erreur principale

cli.message_callback_add("B3/MartinOmar/+/parametre/battrie/#", fctTopicBattrie)

cli.message_callback_add("B3/MartinOmar/+/image/start", fctTopicImage)

cli.message_callback_add("B3/MartinOmar/+/image/data", fctTopicImage)

cli.message_callback_add("B3/MartinOmar/+/image/end", fctTopicImage)

cli.message_callback_add("B3/MartinOmar/parametre/camera/update", fctTopicUpdate)
 
print("SystÃ¨me prÃªt. En attente de donnÃ©es...")

cli.loop_start()
 
while True:

    if update:

        envoyerParametresVersESP32(cli)

        update = False

    time.sleep(1)
 
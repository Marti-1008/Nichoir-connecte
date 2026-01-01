from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from TablesMariaDB import Image, Battrie, CamParametre, Camera
import paho.mqtt.client as client
from datetime import datetime
import time
import os
import base64

# Database Configuration
engine = create_engine("mariadb+mariadbconnector://martin:1234@192.168.2.58:3306/RPG", echo=True)
Session = sessionmaker(bind=engine)

# Global buffers for image reconstruction
# Structure : { camera_id : [chunk1, chunk2, ...] }
buffers_images = {} 
noms_images = {}

# Global config variables
dernier_wifi = {}
dernier_cam = {}

def on_message(client, userdata, message):
    try:
        topic = message.topic
        parts = topic.split('/')
        
        # Check if format is valid
        if len(parts) >= 3 and parts[2].isdigit():
            cam_id = int(parts[2])
            
            # Check if camera exists in DB
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

def envoyerParametresVersESP32(cli, cam_id):
    global dernier_wifi, dernier_cam
    session = Session()
    # Get the latest configuration from DB
    cam_param = session.query(CamParametre)\
        .filter(CamParametre.NumeroCam == cam_id)\
        .order_by(CamParametre.id.desc())\
        .first()
        
    session.close()

    if cam_param:
        print(f"[Cam {cam_id}] Envoi des parametres...")
        base_topic = f"B3/MartinOmar/{cam_id}/parametre/camera"
        
        # RESOLUTION CONVERSION
        map_res = {
            "QVGA (320x240)": 0,
            "VGA (640x480)": 1,
            "XGA (1024x768)": 2,
            "UXGA (1600x1200)": 3
        }
        # Default to VGA (1) if unknown
        res_index = map_res.get(cam_param.resolution, 1) 

        # SEND ALL PARAMETERS
        cli.publish(f"{base_topic}/resolution", str(res_index))
        cli.publish(f"{base_topic}/contrast", str(cam_param.contrast))
        cli.publish(f"{base_topic}/brightness", str(cam_param.brightness))
        cli.publish(f"{base_topic}/saturation", str(cam_param.saturation))
       
    else:
        print(f"[Cam {cam_id}] Pas de parametres trouves en BDD.")

# MQTT CALLBACKS

def fctTopicBattrie(client, userdata, message):
    try:
        topic = message.topic
        payload = message.payload.decode()
        
        # ID Extraction
        parts = topic.split('/')
        if len(parts) < 3 or not parts[2].isdigit():
            return 
            
        cam_id = int(parts[2]) 
        
        pourc = 0.0
        volt = 0.0

        # Process according to message type (Level vs Voltage)
        if "level" in topic:
            pourc = float(payload)
            print(f"[Cam {cam_id}] Batterie Level: {pourc}%")
        elif "tension" in topic:
            volt = float(payload)
            print(f"[Cam {cam_id}] Tension: {volt}V")

        # Save to Database
        session = Session()
        maBattrie = Battrie(
            NumeroCam=cam_id,  
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
    
    # Extract Camera ID
    parts = topic.split('/')
    if len(parts) < 3 or not parts[2].isdigit():
        return
    cam_id = int(parts[2])

    if "start" in topic:
        # Decode filename
        nom_fic = message.payload.decode().strip()
        buffers_images[cam_id] = [] 
        noms_images[cam_id] = nom_fic
        print(f"[Cam {cam_id}] Debut reception (Binaire) : {nom_fic}")

    elif "data" in topic:
        if cam_id in buffers_images:
            buffers_images[cam_id].append(message.payload) 

    elif "end" in topic:
        if cam_id in buffers_images and len(buffers_images[cam_id]) > 0:
            print(f"\n[Cam {cam_id}] Fin reception. Reconstruction...")
            
            try:
                image_bytes = b"".join(buffers_images[cam_id])
              
                # File path
                dossier = r"/home/martin/Desktop/smartcities/static/images"
                os.makedirs(dossier, exist_ok=True)
                timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                nom_final = f"cam{cam_id}_{timestamp}.jpg"
                chemin_complet = os.path.join(dossier, nom_final)

                # Write to disk
                with open(chemin_complet, "wb") as f:
                    f.write(image_bytes)
                
                # Write to DB
                session = Session()
                
                camera_existante = session.query(Camera).filter_by(id=cam_id).first()
                if not camera_existante:
                    print(f"?? Camera {cam_id} inconnue. Creation automatique en BDD...")
                    nouvelle_cam = Camera(id=cam_id, Nom=f"ESP32-Cam-{cam_id}")
                    session.add(nouvelle_cam)
                    session.commit()
                
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
            
            # Memory cleanup for this camera
            buffers_images[cam_id] = []
            noms_images[cam_id] = None


def fctTopicUpdate(client, userdata, message):
    
    try:
        topic = message.topic
        parts = topic.split('/')
        
        if "set" in topic:
            if len(parts) >= 3 and parts[2].isdigit():
                cam_id = int(parts[2])
                print(f"Demande de mise a jour recue pour la Camera {cam_id}")
                
                # Trigger immediate sending for this camera
                envoyerParametresVersESP32(client, cam_id)
            
    except Exception as e:
        print(f"Erreur update: {e}")


# MQTT CONFIGURATION
cli = client.Client(client.CallbackAPIVersion.VERSION2)
cli.on_message = on_message
cli.connect("192.168.2.58", 1883)

# Subscriptions
cli.subscribe("B3/MartinOmar/+/parametre/battrie/#") 
cli.subscribe("B3/MartinOmar/+/image/start")
cli.subscribe("B3/MartinOmar/+/image/data", qos=1)
cli.subscribe("B3/MartinOmar/+/image/end")
cli.subscribe("B3/MartinOmar/parametre/camera/set") 

# Add callbacks
cli.message_callback_add("B3/MartinOmar/+/parametre/battrie/#", fctTopicBattrie)
cli.message_callback_add("B3/MartinOmar/+/image/start", fctTopicImage)
cli.message_callback_add("B3/MartinOmar/+/image/data", fctTopicImage)
cli.message_callback_add("B3/MartinOmar/+/image/end", fctTopicImage)
cli.message_callback_add("B3/MartinOmar/+/parametre/camera/set", fctTopicUpdate)

print("Système prêt. En attente de données...")
cli.loop_start()

while True:
 
    time.sleep(1)

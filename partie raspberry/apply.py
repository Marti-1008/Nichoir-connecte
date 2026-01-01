from flask import Flask, render_template, request, redirect, url_for

import os

from sqlalchemy import create_engine

from sqlalchemy.orm import sessionmaker

from TablesMariaDB import CamParametre, Battrie, Image, Camera

from datetime import datetime

import paho.mqtt.client as mqtt

import json
 
app = Flask(__name__)

# Database connection setup (MariaDB)
 
engine = create_engine("mariadb+mariadbconnector://martin:1234@10.42.0.1:3306/RPG", echo=True)

Session = sessionmaker(bind=engine)
 
# MQTT CONFIGURATION

cli = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

cli.connect("10.42.0.1", 1883)

cli.loop_start()
 
# Function to retrieve battery history for charts
def get_battery_data(cam_id):

    session = Session()

    # Get last 50 entries for the specific camera
    donnees = session.query(Battrie).filter(Battrie.NumeroCam == cam_id).order_by(Battrie.date.asc()).limit(50).all()

    session.close()
 
    # Extract data list
    labels = [d.date for d in donnees]

    levels = [d.poucentage for d in donnees]

    voltages = [d.voltage for d in donnees]
    
    # Return data as JSON for the frontend
    return json.dumps(labels), json.dumps(levels), json.dumps(voltages)
 
@app.route('/')

def index():

    session = Session()

    # Dropdown menu logic

    cameras = session.query(Camera).all()

    cam_id_arg = request.args.get('cam_id', type=int)

    date_filter = request.args.get('date_filter')

    selected_cam = None

    # Logic to determine which camera is currently selected
    if cameras:

        if cam_id_arg:

            for cam in cameras:

                if cam.id == cam_id_arg:

                    selected_cam = cam

                    break

        if not selected_cam:

            selected_cam = cameras[0]

    current_id = selected_cam.id if selected_cam else 0
    # Query images for the current camera
    query = session.query(Image).filter(Image.NumeroCam == current_id)
    # Apply date filter if it exists
    if date_filter:

        query = query.filter(Image.date.like(f"{date_filter}%"))

    images_db = query.order_by(Image.date.desc()).all()
    # Get the last configuration settings
    current_config = session.query(CamParametre).filter(CamParametre.NumeroCam == current_id).order_by(CamParametre.id.desc()).first()

    session.close()
    # Format image data for HTML
    images_data = []

    for img in images_db:

        nom_fichier = os.path.basename(img.path)

        images_data.append({"id": img.idi, "url": f"images/{nom_fichier}", "date": img.date})
    # Get battery statistics
    dates_json, levels_json, voltages_json = get_battery_data(current_id)

    return render_template("index.html",

                           images=images_data,

                           dates_json=dates_json,

                           levels_json=levels_json,

                           voltages_json=voltages_json,

                           cameras=cameras,          

                           selected_cam=selected_cam,

                           selected_date=date_filter,

                           config=current_config)
 
@app.route('/delete_image/<int:image_id>', methods=['POST'])

def delete_image(image_id):

    session = Session()

    image_to_delete = session.query(Image).filter_by(idi=image_id).first()

    if image_to_delete:

        try:
            # Delete file from filesystem
            if os.path.exists(image_to_delete.path):

                os.remove(image_to_delete.path)

        except Exception as e:

            print(f"Erreur suppression fichier : {e}")
        # Delete record from database
        session.delete(image_to_delete)

        session.commit()

    session.close()

    return redirect(url_for('index'))    

@app.route('/update_params', methods=['POST'])

def update_params():

    cam_id = request.form.get('cam_id', type=int)

    # If cam_id is None we cannot do anything

    if not cam_id:

        return redirect(url_for('index'))
 
    resolution = request.form.get('resolution')

    brightness = request.form.get('brightness')

    contrast = request.form.get('contrast')

    saturation = request.form.get('saturation')
 
    session = Session()
 
    try:
        # Create new parameter entry in DB
        new_param = CamParametre(

            NumeroCam=cam_id,

            resolution=resolution,

            brightness=int(brightness) if brightness else 0,

            contrast=int(contrast) if contrast else 0,

            saturation=int(saturation) if saturation else 0,

            date=datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        )

        session.add(new_param)

        session.commit()

    except Exception as e:

        print(f"Erreur enregistrement : {e}")

        session.rollback()

    finally:

        session.close()
 
    return redirect(url_for('index', cam_id=cam_id))
 
if __name__ == "__main__":

    app.run(host="0.0.0.0", port=5000, debug=True)

 
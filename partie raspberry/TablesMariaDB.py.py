from sqlalchemy.orm import DeclarativeBase, mapped_column,relationship

from sqlalchemy import Integer,Float,String, create_engine,ForeignKey
# Base class for SQLAlchemy ORM models
class Base(DeclarativeBase):

    pass
# Table representing the physical ESP32 cameras
class Camera(Base):

    __tablename__ = "Camera"

    id = mapped_column(Integer, primary_key = True)

    Nom = mapped_column(String(40),nullable=False)

    def __str__(self):

        return f"{self.id}:{self.Nom}"

# Table storing references to saved images
class Image(Base):

    __tablename__ = "Image"

    idi = mapped_column(Integer, primary_key = True)

    path = mapped_column(String(100),nullable=False)

    date = mapped_column(String(50),nullable=False)

    NumeroCam = mapped_column(Integer,ForeignKey('Camera.id'))

    def __str__(self):

        return f"{self.idi}:{self.path},{self.date}"
# Table storing battery levels and voltage
class Battrie(Base):

    __tablename__ = "Battrie"

    id = mapped_column(Integer, primary_key = True)

    poucentage = mapped_column(Float,nullable=False)

    voltage = mapped_column(Float,nullable=False)

    date = mapped_column(String(50),nullable=False)

    NumeroCam = mapped_column(Integer,ForeignKey('Camera.id'))

    def __str__(self):

        return f"{self.id}:{self.poucentage},{self.voltage},{self.date}"
# Table storing configuration settings (brightness, etc.)
class CamParametre(Base):

    __tablename__ = "CamParametre"

    id = mapped_column(Integer, primary_key = True)

    resolution = mapped_column(String(50), nullable=False)

    brightness = mapped_column(Integer, nullable=False)

    contrast = mapped_column(Integer, nullable=False)

    saturation = mapped_column(Integer, nullable=False)

    date = mapped_column(String(50),nullable=False)

    NumeroCam = mapped_column(Integer,ForeignKey('Camera.id'))

    def __str__(self):

        return f"{self.id}:{self.resolution},{self.brightness},{self.contrast},{self.saturation}"

def main():
    # Connection string to the MariaDB server
    engine = create_engine("mariadb+mariadbconnector://martin:1234@10.42.0.1:3306/RPG", echo = True)
    # Create all tables defined above if they do not exist
    Base.metadata.create_all(engine)

    from sqlalchemy.orm import sessionmaker

    Session = sessionmaker(bind=engine)

    session = Session()
    # Check if the database is empty, if so, add a default camera
    if not session.query(Camera).first():

        cam1 = Camera(Nom="ESP32-Salon")

        session.add(cam1)

        session.commit()

        print("Camera par defaut cree !")

    session.close()

if __name__ == "__main__":

    main()

 
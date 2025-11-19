#include "M5TimerCAM.h"
#include "base64.h"

void setup() {
    Serial.begin(115200);
    TimerCAM.begin();
    TimerCAM.Camera.begin();
}

void loop() {
    if (TimerCAM.Camera.get()) {
        // encode jpeg in Base64
        String b64 = base64::encode(
            TimerCAM.Camera.fb->buf,
            TimerCAM.Camera.fb->len
        );

        // print result
        Serial.println("----- BASE64 START -----");
        Serial.println(b64); // rajouter le data:image/jpeg;base64, pour que ça puisse le lire
        Serial.println("----- BASE64 END -----");
        // regarder serial write Serial.write(TimerCAM.Camera.fb->buf, TimerCAM.Camera.fb->len);

        TimerCAM.Camera.free();
        delay(2000);
    }
}

// Super simple morse code LED blinker for Arduino

const char* morseMap [] = {
    ".-",     //a
    "-...",   //b
    "-.-.",   //c
    "-..",    //d
    ".",      //e
    "..-.",   //f
    "--.",    //g
    "....",   //h
    "..",     //i
    ".---",   //j
    "-.-",    //k
    ".-..",   //l
    "--",     //m
    "-.",     //n
    "---",    //o
    ".--.",   //p
    "--.-",   //q
    ".-.",    //r
    "...",    //s
    "-",      //t
    "..-",    //u
    "...-",   //v
    ".--",    //w
    "-..-",   //x
    "-.--",   //y
    "--..",   //z
};

const char* morse_encode(const char *msg)
{
    String morsecode = "";
    int len = strlen(msg);
    for (int i = 0; i < len; i++) {
        char msgChar = toupper(msg[i]);
        if (msgChar >= 'A' && msgChar <= 'Z') {
            morsecode += morseMap[msgChar - 65];
        }
        if (msgChar >= ' ') {
            morsecode += ' ';
        }
    }
    return morsecode.c_str();  
}

void morse_flash(const char *dashDots) {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    int len = strlen(dashDots);
    for (int i = 0; i < len; i++) {
        if (dashDots[i] == '.') {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
        }
        else if (dashDots[i] == '-'){
            digitalWrite(LED_BUILTIN, HIGH);
            delay(200);
        }
        else if (dashDots[i] == ' ') {
            digitalWrite(LED_BUILTIN, LOW);
            delay(200);
        }
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
    }
    digitalWrite(LED_BUILTIN, LOW);
}

void morse_encodeAndFlash(const char* msg) {
    morse_flash(morse_encode(msg));
}
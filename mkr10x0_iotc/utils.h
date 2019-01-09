// convert a float to a string as Arduino lacks an ftoa function
char *dtostrf(double value, int width, unsigned int precision, char *result)
{
    int decpt, sign, reqd, pad;
    const char *s, *e;
    char *p;
    s = fcvt(value, precision, &decpt, &sign);
    if (precision == 0 && decpt == 0) {
        s = (*s < '5') ? "0" : "1";
        reqd = 1;
    } else {
        reqd = strlen(s);
        if (reqd > decpt) reqd++;
        if (decpt == 0) reqd++;
    }
    if (sign) reqd++;
    p = result;
    e = p + reqd;
    pad = width - reqd;
    if (pad > 0) {
        e += pad;
        while (pad-- > 0) *p++ = ' ';
    }
    if (sign) *p++ = '-';
    if (decpt <= 0 && precision > 0) {
        *p++ = '0';
        *p++ = '.';
        e++;
        while ( decpt < 0 ) {
            decpt++;
            *p++ = '0';
        }
    }    
    while (p < e) {
        *p++ = *s++;
        if (p == e) break;
        if (--decpt == 0) *p++ = '.';
    }
    if (width < 0) {
        pad = (reqd + width) * -1;
        while (pad-- > 0) *p++ = ' ';
    }
    *p = 0;
    return result;
}

// implementation of printf for use in Arduino sketch
void Serial_printf(char* fmt, ...) {
    char buf[256]; // resulting string limited to 128 chars
    va_list args;
    va_start (args, fmt );
    vsnprintf(buf, 256, fmt, args);
    va_end (args);
    Serial.print(buf);
}

// simple URL encoder
String urlEncode(const char* msg)
{
    static const char *hex PROGMEM = "0123456789abcdef";
    String encodedMsg = "";

    while (*msg!='\0'){
        if( ('a' <= *msg && *msg <= 'z')
            || ('A' <= *msg && *msg <= 'Z')
            || ('0' <= *msg && *msg <= '9') ) {
            encodedMsg += *msg;
        } else {
            encodedMsg += '%';
            encodedMsg += hex[*msg >> 4];
            encodedMsg += hex[*msg & 15];
        }
        msg++;
    }
    return encodedMsg;
}

int32_t indexOf(const char* buffer, size_t length, const char* look_for, size_t look_for_length, int32_t start_index) {
    if (look_for_length > length) {
        return -1;
    }

    for (size_t pos = start_index; pos < length; pos++) {
        if (length - pos < look_for_length) {
            return -1;
        }

        if (buffer[pos] == *look_for) {
            size_t sub = 1;
            for (; sub < look_for_length; sub++) {
                if (buffer[pos + sub] != look_for[sub]) break;
            }

            if (sub == look_for_length) {
                return pos;
            }
        }
    }

    return -1;
}
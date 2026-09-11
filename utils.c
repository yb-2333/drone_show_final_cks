#include "utils.h"
#include "common.h"

void Msg(const char* f, ...) {
    va_list a;
    va_start(a, f);
    vsnprintf(msg, sizeof(msg), f, a);
    va_end(a);
    mt = 2.5f;
}

int In(Rectangle r) {
    Vector2 m = GetMousePosition();

    return m.x >= r.x
        && m.x <= r.x + r.width
        && m.y >= r.y
        && m.y <= r.y + r.height;
}

static void BtnDraw(Rectangle r, const char* t, Color c) {
    DrawRectangleRec(r, c);
    DrawRectangleLinesEx(r, 1, Br);
    int tw = MeasureText(t, 18);

    DrawText(t, (int)(r.x + r.width / 2 - tw / 2),
                (int)(r.y + r.height / 2 - 9), 18, Wh);
}

int Btn(Rectangle r, const char* t, Color c) {
    BtnDraw(r, t, c);

    return In(r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static int activeIdx = -1;

static int TxtId(const char* buf) {
    return (int)((long long)buf & 0xFFF);
}

static void TxtFocus(Rectangle r, int id) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (In(r))
            activeIdx = id;
        else if (activeIdx == id)
            activeIdx = -1;
    }

    if (activeIdx == id) txtFocus = 1;

}

static int TxtChars(char* buf, int max) {
    int changed = 0;

    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 126) {
            int len = (int)strlen(buf);
            if (len < max - 1) {
                buf[len] = (char)key;
                buf[len + 1] = 0;
                changed = 1;
            }
        }
        key = GetCharPressed();
    }
    return changed;
}

static int TxtBackspace(char* buf) {
    if (!IsKeyPressed(KEY_BACKSPACE)) return 0;

    int len = (int)strlen(buf);
    if (len <= 0) return 0;

    buf[len - 1] = 0;
    return 1;
}

static int TxtMinus(char* buf) {
    if (!IsKeyPressed(KEY_MINUS) && !IsKeyPressed(KEY_KP_SUBTRACT))
        return 0;

    int len = (int)strlen(buf);
    if (len != 0) return 0;

    buf[0] = '-';
    buf[1] = 0;
    return 1;
}

static int TxtDecimal(char* buf, int max) {
    if (!IsKeyPressed(KEY_PERIOD) && !IsKeyPressed(KEY_KP_DECIMAL))
        return 0;

    int len = (int)strlen(buf);
    if (len >= max - 1 || strchr(buf, '.')) return 0;

    buf[len] = '.';
    buf[len + 1] = 0;
    return 1;
}

static int TxtInput(char* buf, int max) {
    int changed = 0;
    changed |= TxtChars(buf, max);
    changed |= TxtBackspace(buf);
    changed |= TxtMinus(buf);
    changed |= TxtDecimal(buf, max);
    return changed;
}

static void TxtCursor(Rectangle r, const char* buf) {
    if (((int)(GetTime() * 2) % 2) == 0) {
        int tw = MeasureText(buf, 16);
        DrawText("|",
                 (int)(r.x + 5 + tw),
                 (int)(r.y + r.height / 2 - 8), 16, Bl);
    }
}

int Txt(Rectangle r, char* buf, int max, const char* label) {

    DrawText(label, (int)r.x, (int)(r.y - 14), 12, Gr);

    Color bc = Br;
    if (In(r)) bc = Bl;

    DrawRectangleRec(r, (Color){20, 22, 32, 255});
    DrawRectangleLinesEx(r, 1.5f, bc);
    DrawText(buf, (int)(r.x + 4),
             (int)(r.y + r.height / 2 - 8), 16, Wh);

    int id = TxtId(buf);
    TxtFocus(r, id);

    if (activeIdx == id) {
        int changed = TxtInput(buf, max);
        TxtCursor(r, buf);
        return changed;
    }
    return 0;
}

void Sep(int x, int y, int w) {
    DrawRectangle(x, y, w, 1, Br);
}

static void SldDraw(Rectangle r, float v, float lo, float hi, const char* f) {

    DrawText(TextFormat(f, v), (int)r.x, (int)(r.y - 15), 13, Gr);

    DrawRectangleRec((Rectangle){r.x, r.y + r.height / 2 - 3, r.width, 6},
                     (Color){50, 50, 65, 255});

    float t  = (v - lo) / (hi - lo);
    float hx = r.x + t * r.width;

    DrawRectangle((int)r.x, (int)(r.y + r.height / 2 - 3),
                  (int)(hx - r.x), 6, Bl);

    DrawRectangle((int)(hx - 5), (int)r.y, 10, (int)r.height, Wh);
}

float Sld(Rectangle r, float v, float lo, float hi, const char* f) {
    SldDraw(r, v, lo, hi, f);

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && In(r)) {
        float t = (GetMousePosition().x - r.x) / r.width;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        return lo + t * (hi - lo);
    }

    return v;
}

Color Hsv2Rgb(float h, float s, float v) {
    float r = 0, g = 0, b = 0;

    if (s <= 0) {
        r = g = b = v;
    } else {
        if (h >= 1.0f) h -= (int)h;
        if (h < 0.0f)  h += 1.0f;
        h *= 6.0f;
        int   i = (int)h;
        float f = h - i;
        float p = v * (1.0f - s);
        float q = v * (1.0f - s * f);
        float t = v * (1.0f - s * (1.0f - f));

        switch (i % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            case 5: r = v; g = p; b = q; break;
        }
    }

    return (Color){
        (unsigned char)(r * 255.0f),
        (unsigned char)(g * 255.0f),
        (unsigned char)(b * 255.0f),
        255
    };
}

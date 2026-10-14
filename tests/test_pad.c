#define _GNU_SOURCE
#include <assert.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <stdlib.h>
static inline int setenv(const char *name, const char *value, int overwrite) {
    (void)overwrite;
    return _putenv_s(name, value);
}
#define usleep(us) Sleep((us) / 1000)
#endif
#include "../src/runtime_pad.c"

static int capture;
int bbgpu_overlay_captures_input(void) { return capture; }
int bbgpu_text_input_is_active(void) { return 0; }
void bbgpu_consume_mouse_delta(float *dx, float *dy) { if (dx) *dx = 0.0f; if (dy) *dy = 0.0f; }
int bbgpu_get_mk_config(float *sens_x, float *sens_y, int *invert_x, int *invert_y, float *deadzone,
                        float *smoothing, float *move_ramp_ms) {
    if (sens_x) { *sens_x = 1.0f; }
    if (sens_y) { *sens_y = 1.0f; }
    if (invert_x) { *invert_x = 0; }
    if (invert_y) { *invert_y = 0; }
    if (deadzone) { *deadzone = 0.05f; }
    if (smoothing) { *smoothing = 0.2f; }
    if (move_ramp_ms) { *move_ramp_ms = 40.0f; }
    return 1;
}
uint64_t host_monotonic_ns(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
}
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *name) {
    (void)table; (void)count; (void)name;
    return 0;
}

/* The mouse and keyboard to stick maths, which is what the look and the
 * movement feel comes from. No SDL and no window needed. */
static void test_mk_math(void) {
    MkMove m = {0.0f, 0.0f};
    uint8_t x = 0, y = 0;

    /* A full second of held D reaches the rim; the ramp does not stop short. */
    assert(mk_move_stick(&m, 0, 1, 0, 0, 1.0f, 0.040f, &x, &y));
    assert(x == 255 && y == 128);

    /* W+A is a unit diagonal, not 41% longer than a cardinal (128-0.7071*127). */
    m = (MkMove){0.0f, 0.0f};
    mk_move_stick(&m, 1, 0, 1, 0, 1.0f, 0.040f, &x, &y);
    assert(x == y);
    assert(x > 33 && x < 43);

    /* Opposite keys cancel and hand the stick back to a gamepad. */
    m = (MkMove){0.0f, 0.0f};
    assert(!mk_move_stick(&m, 1, 1, 1, 1, 1.0f, 0.040f, &x, &y));
    assert(x == 128 && y == 128);

    /* Half of the ramp is half of the tilt (128+0.5*127), and releasing is
     * twice as fast, so 20 ms of a 40 ms ramp returns the stick to centre. */
    m = (MkMove){0.0f, 0.0f};
    mk_move_stick(&m, 0, 1, 0, 0, 0.020f, 0.040f, &x, &y);
    assert(x > 185 && x < 198);
    m = (MkMove){1.0f, 0.0f};
    mk_move_stick(&m, 0, 0, 0, 0, 0.020f, 0.040f, &x, &y);
    assert(x == 128);

    /* ramp 0 is the old instant snap. */
    m = (MkMove){0.0f, 0.0f};
    mk_move_stick(&m, 0, 1, 0, 0, 0.001f, 0.0f, &x, &y);
    assert(x == 255);

    /* The same mouse travel over the same wall time gives the same stick
     * however often the game read the pad: polling drains the delta. */
    MkCamera one = {0.0f, 0.0f}, four = {0.0f, 0.0f};
    uint8_t ax = 0, ay = 0, bx = 0, by = 0;
    mk_camera_stick(&one, 40.0f, 0.0f, 0.016f, 1.0f, 1.0f, 0, 0, 0.0f, 0.0f, &ax, &ay);
    for (int i = 0; i < 4; ++i) {
        mk_camera_stick(&four, 10.0f, 0.0f, 0.004f, 1.0f, 1.0f, 0, 0, 0.0f, 0.0f, &bx, &by);
    }
    assert(ax == bx && ay == by);
    assert(ax > 128);

    /* Inversion, and a dead zone that eats a slow drift. */
    MkCamera c = {0.0f, 0.0f};
    mk_camera_stick(&c, 40.0f, 0.0f, 0.016f, 1.0f, 1.0f, 1, 0, 0.0f, 0.0f, &ax, &ay);
    assert(ax < 128);
    c = (MkCamera){0.0f, 0.0f};
    mk_camera_stick(&c, 1.0f, 0.0f, 0.016f, 1.0f, 1.0f, 0, 0, 0.3f, 0.0f, &ax, &ay);
    assert(ax == 128);

    /* A fast flick saturates instead of wrapping. */
    c = (MkCamera){0.0f, 0.0f};
    mk_camera_stick(&c, 500.0f, 0.0f, 0.016f, 1.0f, 1.0f, 0, 0, 0.0f, 0.0f, &ax, &ay);
    assert(ax == 255);

    /* When the mouse stops the look glides to neutral and releases the stick. */
    c = (MkCamera){0.0f, 0.0f};
    mk_camera_stick(&c, 50.0f, 0.0f, 0.016f, 1.0f, 1.0f, 0, 0, 0.0f, 0.2f, &ax, &ay);
    assert(ax > 128);
    int owns = 1;
    for (int i = 0; i < 400 && owns; ++i) {
        owns = mk_camera_stick(&c, 0.0f, 0.0f, 0.016f, 1.0f, 1.0f, 0, 0, 0.0f, 0.2f, &ax, &ay);
    }
    assert(!owns && ax == 128 && ay == 128);
}

static void inject(const char *path, const char *tokens) {
    FILE *f=fopen(path,"w");
    assert(f);
    fputs(tokens,f);
    fclose(f);
    usleep(25000);
}

int main(void) {
    test_mk_math();
#ifdef _WIN32
    char temp_dir[MAX_PATH];
    GetTempPathA(sizeof(temp_dir), temp_dir);
    char path[512];
    snprintf(path, sizeof(path), "%sbbport-pad-test-XXXXXX", temp_dir);
#else
    char path[]="/tmp/bbport-pad-test-XXXXXX";
#endif
    int fd=mkstemp(path);
    assert(fd>=0);
    close(fd);
    setenv("BB_PAD_FILE",path,1);
    setenv("SDL_VIDEODRIVER","dummy",1);
    /* Only the virtual test controller is a gamepad, whatever is plugged in. */
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,"0x1d50/0x6189");
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD));
    assert(pad_init()==0 && pad_open(1,0,0,NULL)==1);
    PadData data;
    inject(path,"cross l3 touchpad_left");
    assert(pad_read_state(1,&data)==0);
    assert((data.buttons & (BTN_CROSS|BTN_L3|BTN_TOUCHPAD))==(BTN_CROSS|BTN_L3|BTN_TOUCHPAD));
    assert(data.touch_count==1 && data.touches[0].x==480 && data.touches[0].y==471);
    inject(path,"touchpad_right");
    assert(pad_read_state(1,&data)==0 && data.touch_count==1 && data.touches[0].x==1440);
    inject(path,"");
    assert(pad_read_state(1,&data)==0 && data.buttons==0 && data.touch_count==0);

    SDL_VirtualJoystickTouchpadDesc touch={.nfingers=2};
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes=SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
    desc.name="bbport test controller";
    desc.vendor_id=0x1d50;
    desc.product_id=0x6189;
    desc.ntouchpads=1;
    desc.touchpads=&touch;
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);
    assert(id!=0);
    SDL_Joystick *joystick=SDL_OpenJoystick(id);
    assert(joystick);
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,0,true,0.75f,0.5f,1.0f));
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,1,true,0.25f,1.0f,1.0f));
    assert(SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_TOUCHPAD,true));
    SDL_UpdateJoysticks();
    SDL_UpdateGamepads();
    assert(pad_read_state(1,&data)==0);
    assert(gamepad && data.touch_count==2 && (data.buttons & BTN_TOUCHPAD));
    assert(data.touches[0].x==1439 && data.touches[0].y==471 && data.touches[0].id==0);
    assert(data.touches[1].x==480 && data.touches[1].y==942 && data.touches[1].id==1);
    capture=1;
    assert(pad_read_state(1,&data)==0 && data.touch_count==0 && data.buttons==0);
    capture=0;
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,0,false,0,0,0));
    assert(SDL_SetJoystickVirtualTouchpad(joystick,0,1,false,0,0,0));
    SDL_UpdateJoysticks();
    SDL_UpdateGamepads();
    assert(pad_read_state(1,&data)==0 && data.touch_count==1 && data.touches[0].x==480);
    SDL_CloseJoystick(joystick);
    if (gamepad) SDL_CloseGamepad(gamepad);
    gamepad=NULL;
    assert(SDL_DetachVirtualJoystick(id));
    SDL_Quit();
    unlink(path);
    puts("PASS: pad ABI, debug camera chord, left/right clicks, SDL touch coordinates, overlay capture");
}

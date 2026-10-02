from pathlib import Path
import subprocess, argparse
parser=argparse.ArgumentParser(description='Host boundary tests; actual source functions with SDL/platform doubles, not a console latency test')
parser.add_argument('--driver', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[3]
driver=args.driver.resolve()
out=args.out.resolve();out.mkdir(parents=True, exist_ok=True)
s=driver.read_text()
def section(a,b): return s[s.index(a):s.index(b)]
code=r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "SDL_ps5joystick.h"
#define SDL_arraysize(a) ((int)(sizeof(a)/sizeof((a)[0])))
#define SDL_PRESSED 1
#define SDL_RELEASED 0
#define SDL_HAT_UP 1
#define SDL_HAT_RIGHT 2
#define SDL_HAT_DOWN 4
#define SDL_HAT_LEFT 8
typedef int SDL_Joystick;
typedef struct {int handle; PS5_PadData pad;} PS5_PadContext;
static PS5_PadContext context;
static PS5_PadData batch[64], fallback;
static int count, fallback_error, edges, states[128], buttons[128], hat;
static PS5_PadContext *PS5_JoystickGetPadContext(SDL_Joystick *j) {return &context;}
static void SDL_PrivateJoystickAxis(SDL_Joystick *j,int a,int v) {}
static void SDL_PrivateJoystickButton(SDL_Joystick *j,int b,int v) {assert(edges<128);buttons[edges]=b;states[edges++]=v;}
static void SDL_PrivateJoystickHat(SDL_Joystick *j,int h,int v) {hat=v;}
int scePadRead(int h,void *out,int capacity) {assert(capacity==64);if(count>0&&count<=64)memcpy(out,batch,count*sizeof(*batch));return count;}
int scePadReadState(int h,PS5_PadData *out) {*out=fallback;return fallback_error;}
"""
code+=section('#define PS5_PAD_AXIS_LX','#define PS5_PAD_GUID')
code+=section('static int analog_map','/* SDL indices')
code+=section('static void PS5_PublishPad','static SDL_JoystickGUID PS5_JoystickGetDeviceGUID')
code+=r"""
int main(void) {
 SDL_Joystick j=0;
 batch[0]=(PS5_PadData){.connected=1,.timestamp=2};
 batch[1]=(PS5_PadData){.connected=1,.timestamp=1,.buttons=PS5_PAD_BUTTON_UP};
 count=2;PS5_JoystickUpdate(&j);
 assert(edges==2&&buttons[0]==11&&states[0]==1&&states[1]==0);
 assert(context.pad.buttons==0&&context.pad.timestamp==2);
 edges=0;count=0;PS5_JoystickUpdate(&j);assert(edges==0);
 count=1;batch[0].buttons=PS5_PAD_BUTTON_UP;PS5_JoystickUpdate(&j);assert(edges==1);
 PS5_JoystickUpdate(&j);assert(edges==1); /* held input has no new edge */
 batch[0].buttons|=PS5_PAD_BUTTON_CROSS;PS5_JoystickUpdate(&j);assert(hat==SDL_HAT_UP);
 batch[0].connected=0;PS5_JoystickUpdate(&j);assert(context.pad.buttons==(PS5_PAD_BUTTON_UP|PS5_PAD_BUTTON_CROSS));
 count=-1;fallback=(PS5_PadData){.connected=1};PS5_JoystickUpdate(&j);assert(context.pad.buttons==0);
 fallback_error=-1;fallback.buttons=PS5_PAD_BUTTON_UP;PS5_JoystickUpdate(&j);assert(context.pad.buttons==0);
 count=65;fallback_error=0;PS5_JoystickUpdate(&j);assert(context.pad.buttons==PS5_PAD_BUTTON_UP);
 edges=0;count=1;batch[0]=(PS5_PadData){.connected=1,.buttons=PS5_PAD_BUTTON_TOUCH_PAD};
 context.pad.buttons=0;PS5_JoystickUpdate(&j);
 assert(edges==2 && buttons[0]==4 && buttons[1]==5 && states[0]==1 && states[1]==1);
 batch[0].buttons=0;PS5_JoystickUpdate(&j);
 assert(edges==4 && buttons[2]==4 && buttons[3]==5 && states[2]==0 && states[3]==0);
 puts("PASS actual driver functions: short tap, reversed batch, empty batch, held button, hat, disconnected sample, fallback/error/bounds");
}
"""
(out/'driver-test.c').write_text(code)
subprocess.run(['clang-18','-std=c11','-fsanitize=address,undefined','-I'+str(driver.parent),str(out/'driver-test.c'),'-o',str(out/'driver-test')],check=True)
subprocess.run([str(out/'driver-test')],check=True)
s=(root/'external/libcross2d/source/platforms/sdl2/sdl2_input.cpp').read_text()
fn=s[s.index('Input::Player *SDL2Input::update()'):s.index('Vector2f SDL2Input::getAxisState')]
code=r"""
#include <cassert>
#include <vector>
#include <cstdio>
#define __PS5__ 1
#define PLAYER_MAX 2
#define SDL_CONTROLLERBUTTONDOWN 1
#define SDL_QUIT 2
struct SDL_GameController {int id;};
struct SDL_Event {int type; struct {int which,button;} cbutton;};
static std::vector<SDL_Event> events;
static int pos;
static int SDL_PollEvent(SDL_Event *e) {if(pos==(int)events.size())return 0;*e=events[pos++];return 1;}
static SDL_GameController *SDL_GameControllerGetJoystick(SDL_GameController *p){return p;}
static int SDL_JoystickInstanceID(SDL_GameController *p){return p->id;}
static void (*pemu_native_input_probe)(unsigned)=nullptr;
struct Input {
 enum Button {Delay=256,Quit=512};
 struct Mapping {int value;unsigned button;};
 struct Player {bool enabled;void *data;std::vector<Mapping> mapping;unsigned buttons;};
 Player m_players[2]{};
 int repeat=150; bool rotate=false;
 int getRepeatDelay(){return repeat;}
 Player *update(){return m_players;}
 void applyRotation(Player *p){if(rotate)p->buttons<<=1;}
};
struct SDL2Input:Input {Player *update();};
"""+fn+r"""
int main(){
 SDL2Input input;SDL_GameController pad{42};
 input.m_players[0]={true,&pad,{{7,1}},Input::Delay};
 events={{SDL_CONTROLLERBUTTONDOWN,{42,7}},{3,{42,7}}};
 auto p=input.update();assert(p[0].buttons==1); /* released live state + queued press */
 pos=0;input.repeat=0;input.m_players[0].buttons=0;assert(input.update()[0].buttons==0);
 pos=0;input.repeat=150;input.rotate=true;assert(input.update()[0].buttons==2);
 pos=0;input.m_players[0].buttons=0;events[0].cbutton.which=99;assert(input.update()[0].buttons==0);
 pos=0;events={{SDL_QUIT,{}}};assert(input.update()[0].buttons==Input::Quit);
 puts("PASS actual SDL2Input::update: released-state tap, remap, player identity, rotation, game mode, quit (SDL/base boundary doubles)");
}
"""
(out/'consumer-test.cpp').write_text(code)
subprocess.run(['clang++-18','-std=c++17','-fsanitize=address,undefined',str(out/'consumer-test.cpp'),'-o',str(out/'consumer-test')],check=True)
subprocess.run([str(out/'consumer-test')],check=True)
code=r"""
#define clock_gettime fake_clock_gettime
#include <assert.h>
#include <string.h>
"""+'\n#include "'+str(root/'tools/ps5/native-input-probe.c')+'"\n'+r"""
void pemu_native_heap_mark(void) {}
static uint64_t fake_us=1;static int lines,clocks;
int fake_clock_gettime(clockid_t c,struct timespec *t){++clocks;t->tv_sec=fake_us/1000000;t->tv_nsec=(fake_us%1000000)*1000;errno=99;return 0;}
void pemu_boot_mark(const char *s){assert(strstr(s,"INPUT_PERF") || strstr(s,"RENDER_PERF"));++lines;errno=88;}
int main(){
 errno=27;pemu_native_input_probe(2);fake_us+=9000000;pemu_native_input_probe(3);assert(window_start==0 && stages[1].count==0);pemu_native_input_probe(0);fake_us+=100;pemu_native_input_probe(2);fake_us+=50;pemu_native_input_probe(3);fake_us+=50;pemu_native_input_probe(1);
 assert(errno==27&&stages[0].total==200&&stages[1].total==50);
 pemu_native_input_probe(12);fake_us+=20;pemu_native_input_probe(14);fake_us+=100;pemu_native_input_probe(15);fake_us+=20;pemu_native_input_probe(13);
 assert(stages[6].total==140 && stages[7].total==100 && stages[7].count==1);
 for(int i=0;i<20;i++){pemu_native_input_probe(0);fake_us+=5000001;pemu_native_input_probe(1);assert(errno==27);}
 assert(lines==48&&reports==12);int n=clocks;pemu_native_input_probe(0);pemu_native_input_probe(1);assert(clocks==n);
 puts("PASS actual probe: timing totals, errno preservation, 12-report bound, no clocks after bound");
}
"""
(out/'probe-test.c').write_text(code)
subprocess.run(['clang-18','-std=gnu11','-fsanitize=address,undefined',str(out/'probe-test.c'),'-o',str(out/'probe-test')],check=True)
subprocess.run([str(out/'probe-test')],check=True)

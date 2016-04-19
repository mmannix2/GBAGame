//TODO Change the HUD to be transparent like in ALTTP

/*
 * game.c
 * program which demonstrates sprites colliding with tiles
 */

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160
unsigned char screen_x = 0;
unsigned char screen_y = 0;

/* include the background image we are using */
#include "include/new_bg.h"

/* include the sprite image we are using */
#include "include/sid.h"
#include "include/Sid_Front.h"
#include "include/Sid_Right.h"

/* include the tile cave we are using */
#include "include/cave.h"
#include "include/hud.h"
#include "include/map.h"

/* the tile mode flags needed for display control register */
#define MODE0 0x00
#define BG0_ENABLE 0x100
#define BG1_ENABLE 0x200
#define BG2_ENABLE 0x400
#define BG3_ENABLE 0x800

/* flags to set sprite handling in display control register */
#define SPRITE_MAP_2D 0x0
#define SPRITE_MAP_1D 0x40
#define SPRITE_ENABLE 0x1000

/* the control registers for the four tile layers */
volatile unsigned short* bg0_control = (volatile unsigned short*) 0x4000008;
volatile unsigned short* bg1_control = (volatile unsigned short*) 0x400000a;
//volatile unsigned short* bg2_control = (volatile unsigned short*) 0x400000c;
//volatile unsigned short* bg3_control = (volatile unsigned short*) 0x400000e;

/* palette is always 256 colors */
#define PALETTE_SIZE 256

/* there are 128 sprites on the GBA */
#define NUM_SPRITES 128

/* the display control pointer points to the gba graphics register */
volatile unsigned long* display_control = (volatile unsigned long*) 0x4000000;

/* the memory location which controls sprite attributes */
volatile unsigned short* sprite_attribute_memory = (volatile unsigned short*) 0x7000000;

/* the memory location which stores sprite image data */
volatile unsigned short* sprite_image_memory = (volatile unsigned short*) 0x6010000;

/* the address of the color palettes used for new_bgs and sprites */
volatile unsigned short* bg_palette = (volatile unsigned short*) 0x5000000;
volatile unsigned short* sprite_palette = (volatile unsigned short*) 0x5000200;

/* the button register holds the bits which indicate whether each button has
 * been pressed - this has got to be volatile as well
 */
volatile unsigned short* buttons = (volatile unsigned short*) 0x04000130;

/* scrolling registers for new_bgs */
volatile short* bg0_x_scroll = (unsigned short*) 0x4000010;
volatile short* bg0_y_scroll = (unsigned short*) 0x4000012;

/* the bit positions indicate each button - the first bit is for A, second for
 * B, and so on, each constant below can be ANDED into the register to get the
 * status of any one button */
#define BUTTON_A (1 << 0)
#define BUTTON_B (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START (1 << 3)
#define BUTTON_RIGHT (1 << 4)
#define BUTTON_LEFT (1 << 5)
#define BUTTON_UP (1 << 6)
#define BUTTON_DOWN (1 << 7)
#define BUTTON_R (1 << 8)
#define BUTTON_L (1 << 9)

/* the scanline counter is a memory cell which is updated to indicate how
 * much of the screen has been drawn */
volatile unsigned short* scanline_counter = (volatile unsigned short*) 0x4000006;

/* wait for the screen to be fully drawn so we can do something during vblank */
void wait_vblank( ) {
    /* wait until all 160 lines have been updated */
    while (*scanline_counter < 160) { }
}

/* this function checks whether a particular button has been pressed */
unsigned char button_pressed(unsigned short button) {
    /* and the button register with the button constant we want */
    unsigned short pressed = *buttons & button;

    /* if this value is zero, then it's not pressed */
    if (pressed == 0) {
        return 1;
    } else {
        return 0;
    }
}

/* return a pointer to one of the 4 character blocks (0-3) */
volatile unsigned short* char_block(unsigned long block) {
    /* they are each 16K big */
    return (volatile unsigned short*) (0x6000000 + (block * 0x4000));
}

/* return a pointer to one of the 32 screen blocks (0-31) */
volatile unsigned short* screen_block(unsigned long block) {
    /* they are each 2K big */
    return (volatile unsigned short*) (0x6000000 + (block * 0x800));
}

/* flag for turning on DMA */
#define DMA_ENABLE 0x80000000

/* flags for the sizes to transfer, 16 or 32 bits */
#define DMA_16 0x00000000
#define DMA_32 0x04000000

/* pointer to the DMA source location */
volatile unsigned int* dma_source = (volatile unsigned int*) 0x40000D4;

/* pointer to the DMA destination location */
volatile unsigned int* dma_destination = (volatile unsigned int*) 0x40000D8;

/* pointer to the DMA count/control */
volatile unsigned int* dma_count = (volatile unsigned int*) 0x40000DC;

/* copy data using DMA */
void memcpy16_dma(unsigned short* dest, unsigned short* source, int amount) {
    *dma_source = (unsigned int) source;
    *dma_destination = (unsigned int) dest;
    *dma_count = amount | DMA_16 | DMA_ENABLE;
}

/* function to setup new_bg 0 for this program */
void setup_new_bg() {

    /* load the palette from the image into palette memory*/
    memcpy16_dma((unsigned short*) bg_palette, (unsigned short*) new_bg_palette, PALETTE_SIZE);

    /* load the image into char block 0 */
    memcpy16_dma((unsigned short*) char_block(0), (unsigned short*) new_bg_data,
            (new_bg_width * new_bg_height) / 2);

    //Main Background
    /* set all control the bits in this register */
    *bg0_control = 3 |    /* priority, 0 is highest, 3 is lowest */
        (0 << 2)  |       /* the char block the image data is stored in */
        (0 << 6)  |       /* the mosaic flag */
        (1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
        (24 << 8) |       /* the screen block the tile data is stored in */
        (0 << 13) |       /* wrapping flag */
        (0 << 14);        /* bg size, 0 is 256x256 */
    //Heads-Up Display
    /* set all control the bits in this register */
    *bg1_control = 2 |    /* priority, 0 is highest, 3 is lowest */
        (0 << 2)  |       /* the char block the image data is stored in */
        (0 << 6)  |       /* the mosaic flag */
        (1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
        (16 << 8) |       /* the screen block the tile data is stored in */
        (0 << 13) |       /* wrapping flag */
        (0 << 14);        /* bg size, 0 is 256x256 */

    /* load the tile data into screen block 24 */
    memcpy16_dma((unsigned short*) screen_block(24), (unsigned short*) cave, cave_width * cave_height);
    /* load the tile data into screen block 16 */
    memcpy16_dma((unsigned short*) screen_block(16), (unsigned short*) hud, hud_width * hud_height);
}

/* just kill time */
void delay(unsigned int amount) {
    for (int i = 0; i < amount * 10; i++);
}

/* Room */
struct Room {
    unsigned char width;
    unsigned char height;
    unsigned char exit_x;
    unsigned char exit_y;
};

struct Room* room_init(unsigned char x, unsigned char y) {
    struct Room *newRoom = malloc(sizeof(struct Room));

    newRoom->exit_x = x;
    newRoom->exit_y = y;
}

/* a sprite is a moveable image on the screen */
struct Sprite {
    unsigned short attribute0;
    unsigned short attribute1;
    unsigned short attribute2;
    unsigned short attribute3;
};

/* array of all the sprites available on the GBA */
struct Sprite sprites[NUM_SPRITES];
int next_sprite_index = 0;

/* the different sizes of sprites which are possible */
enum SpriteSize {
    SIZE_8_8,
    SIZE_16_16,
    SIZE_32_32,
    SIZE_64_64,
    SIZE_16_8,
    SIZE_32_8,
    SIZE_32_16,
    SIZE_64_32,
    SIZE_8_16,
    SIZE_8_32,
    SIZE_16_32,
    SIZE_32_64
};

/* function to initialize a sprite with its properties, and return a pointer */
struct Sprite* sprite_init(int x, int y, enum SpriteSize size,
        int horizontal_flip, int vertical_flip, int tile_index, int priority) {

    /* grab the next index */
    int index = next_sprite_index++;

    /* setup the bits used for each shape/size possible */
    int size_bits, shape_bits;
    switch (size) {
        case SIZE_8_8:   size_bits = 0; shape_bits = 0; break;
        case SIZE_16_16: size_bits = 1; shape_bits = 0; break;
        case SIZE_32_32: size_bits = 2; shape_bits = 0; break;
        case SIZE_64_64: size_bits = 3; shape_bits = 0; break;
        case SIZE_16_8:  size_bits = 0; shape_bits = 1; break;
        case SIZE_32_8:  size_bits = 1; shape_bits = 1; break;
        case SIZE_32_16: size_bits = 2; shape_bits = 1; break;
        case SIZE_64_32: size_bits = 3; shape_bits = 1; break;
        case SIZE_8_16:  size_bits = 0; shape_bits = 2; break;
        case SIZE_8_32:  size_bits = 1; shape_bits = 2; break;
        case SIZE_16_32: size_bits = 2; shape_bits = 2; break;
        case SIZE_32_64: size_bits = 3; shape_bits = 2; break;
    }

    int h = horizontal_flip ? 1 : 0;
    int v = vertical_flip ? 1 : 0;

    /* set up the first attribute */
    sprites[index].attribute0 = y |             /* y coordinate */
                            (0 << 8) |          /* rendering mode */
                            (0 << 10) |         /* gfx mode */
                            (0 << 12) |         /* mosaic */
                            (1 << 13) |         /* color mode, 0:16, 1:256 */
                            (shape_bits << 14); /* shape */

    /* set up the second attribute */
    sprites[index].attribute1 = x |             /* x coordinate */
                            (0 << 9) |          /* affine flag */
                            (h << 12) |         /* horizontal flip flag */
                            (v << 13) |         /* vertical flip flag */
                            (size_bits << 14);  /* size */

    /* setup the third attribute */
    sprites[index].attribute2 = tile_index |   // tile index */
                            (priority << 10) | // priority */
                            (0 << 12);         // palette bank (only 16 color)*/

    /* return pointer to this sprite */
    return &sprites[index];
}

/* update all of the spries on the screen */
void sprite_update_all() {
    /* copy them all over */
    memcpy16_dma((unsigned short*) sprite_attribute_memory, (unsigned short*) sprites, NUM_SPRITES * 4);
}

/* setup all sprites */
void sprite_clear() {
    /* clear the index counter */
    next_sprite_index = 0;

    /* move all sprites offscreen to hide them */
    for(int i = 0; i < NUM_SPRITES; i++) {
        sprites[i].attribute0 = SCREEN_HEIGHT;
        sprites[i].attribute1 = SCREEN_WIDTH;
    }
}

/* set a sprite postion */
void sprite_position(struct Sprite* sprite, int x, int y) {
    /* clear out the y coordinate */
    sprite->attribute0 &= 0xff00;

    /* set the new y coordinate */
    sprite->attribute0 |= (y & 0xff);

    /* clear out the x coordinate */
    sprite->attribute1 &= 0xfe00;

    /* set the new x coordinate */
    sprite->attribute1 |= (x & 0x1ff);
}

/* move a sprite in a direction */
void sprite_move(struct Sprite* sprite, int dx, int dy) {
    /* get the current y coordinate */
    int y = sprite->attribute0 & 0xff;

    /* get the current x coordinate */
    int x = sprite->attribute1 & 0x1ff;

    /* move to the new location */
    sprite_position(sprite, x + dx, y + dy);
}

/* change the vertical flip flag */
void sprite_set_vertical_flip(struct Sprite* sprite, int vertical_flip) {
    if (vertical_flip) {
        /* set the bit */
        sprite->attribute1 |= 0x2000;
    } else {
        /* clear the bit */
        sprite->attribute1 &= 0xdfff;
    }
}

/* change the vertical flip flag */
void sprite_set_horizontal_flip(struct Sprite* sprite, int horizontal_flip) {
    if (horizontal_flip) {
        /* set the bit */
        sprite->attribute1 |= 0x1000;
    } else {
        /* clear the bit */
        sprite->attribute1 &= 0xefff;
    }
}

/* change the tile offset of a sprite */
void sprite_set_offset(struct Sprite* sprite, int offset) {
    /* clear the old offset */
    sprite->attribute2 &= 0xfc00;

    /* apply the new one */
    sprite->attribute2 |= (offset & 0x03ff);
}

/* setup the sprite image and palette */
void setup_sprite_image() {
    /* load the palette from the image into palette memory*/
    memcpy16_dma((unsigned short*) sprite_palette, (unsigned short*) sid_palette, PALETTE_SIZE);

    /* load the image into char block 0 */
    memcpy16_dma((unsigned short*) sprite_image_memory, (unsigned short*) sid_data, (sid_width * sid_height) / 2);
}

/* a struct for the sid's logic and behavior */
struct Sid {
    /* the actual sprite attribute info */
    struct Sprite* sprite;

    /* the x and y postion, in 1/256 pixels */
    int x, y;
    
    /* direction */
    char dir;
    #define DIR_N 0x00
    #define DIR_S 0x01
    #define DIR_E 0x10
    #define DIR_W 0x11
    
    /* which frame of the animation he is on */
    int frame;

    /* the number of frames to wait before flipping */
    int animation_delay;

    /* the animation counter counts how many frames until we flip */
    int counter;

    /* whether the sid is moving right now or not */
    int move;

    /* the number of pixels away from the edge of the screen the sid stays */
    int border;
};

/* initialize sid */
void sid_init(struct Sid* sid) {
    sid->x = 120;
    sid->y = 80;
    sid->dir = 0;
    sid->border = 64;
    sid->frame = 0;
    sid->move = 0;
    sid->counter = 0;
    //sid->falling = 0;
    sid->animation_delay = 8;
    sid->sprite = sprite_init(sid->x, sid->y, SIZE_16_16, 0, 0, sid->frame, 0);
}

//TODO Make Sid's sprite change to reflect the direction he faces.

/* move the sid left or right returns if it is at edge of the screen */
int sid_left(struct Sid* sid) {
    /* face left */
    sprite_set_horizontal_flip(sid->sprite, 1);
    sid->move = 1;
    sid->dir = DIR_W;

    // if there is room to walk, walk
    if ( (sid->x - 1) > 0 && (sid->x - 1) < SCREEN_WIDTH ) {
        sid->x -= 1;
    }
    // if sid has reached the border, scroll the screen
    if ( (sid->x < sid->border) &&
         (screen_x - 1) > 0 ) { 
        //bg0_x_scroll = 
        screen_x -= 1;
        return 1;
    }
    return 0;
}
int sid_right(struct Sid* sid) {
    /* face right */
    sprite_set_horizontal_flip(sid->sprite, 0);
    sid->move = 1;
    sid->dir = DIR_E;
    
    // if there is room to walk, walk
    if ( (sid->x + 16 + 1) > 0 && (sid->x + 16 + 1) < SCREEN_WIDTH ) {
        sid->x += 1;
    }
    // if sid has reached the border, scroll the screen
    if ( (sid->x > (SCREEN_WIDTH - sid->border) ) &&
         (screen_x + SCREEN_WIDTH + 1) < (32 << 3) ) {
        //bg0_x_scroll = 
        screen_x += 1;
        return 1;
    }
    return 0;
}
/* move the sid up or down returns if it is at edge of the screen */
int sid_up(struct Sid* sid) {
    /* face North */
    sprite_set_horizontal_flip(sid->sprite, 1);
    sid->move = 1;
    sid->dir = DIR_N;
    
    // if there is room to walk, walk
    if ( (sid->y - 1) > 0 ) {
        sid->y -= 1;
    }
    // if sid has reached the border, scroll the screen
    if ( (sid->y < sid->border + 32) &&
         (screen_y - 1) > 0 ) { 
        //bg0_x_scroll = 
        screen_y -= 1;
        return 1;
    }
    return 0;
}

int sid_down(struct Sid* sid) {
    /* face South */
    sprite_set_horizontal_flip(sid->sprite, 0);
    sid->move = 1;
    sid->dir = DIR_S;
    
    // if there is room to walk, walk
    if ( (sid->y + 16 + 1) < SCREEN_HEIGHT ) {
        sid->y += 1;
    }
    // if sid has reached the border, scroll the screen
    if ( (sid->y > (SCREEN_HEIGHT - 32) ) &&
         (screen_y + SCREEN_HEIGHT + 1) < (32 << 3) ) {
        //bg0_y_scroll = 
        screen_y += 1;
        return 1;
    }
    return 0;
}

/* stop the sid from walking left/right */
void sid_stop(struct Sid* sid) {
    sid->move = 0;
    sid->frame = 0;
    sid->counter = 7;
    sprite_set_offset(sid->sprite, sid->frame);
}

/* finds which tile a screen coordinate caves to, taking scroll into account */
unsigned short tile_lookup(int x, int y, int xscroll, int yscroll,
        const unsigned short* tilecave, int tilecave_w, int tilecave_h) {

    /* adjust for the scroll */
    x += xscroll;
    y += yscroll;

    /* convert from screen coordinates to tile coordinates */
    x >>= 3;
    y >>= 3;

    /* account for wraparound */
    while (x >= tilecave_w) {
        x -= tilecave_w;
    }
    while (y >= tilecave_h) {
        y -= tilecave_h;
    }
    while (x < 0) {
        x += tilecave_w;
    }
    while (y < 0) {
        y += tilecave_h;
    }

    /* lookup this tile from the cave */
    int index = y * tilecave_w + x;

    /* return the tile */
    return tilecave[index];
}


/* update the sid */
void sid_update(struct Sid* sid, int xscroll, int yscroll) {
    /* check which tile the sid's head is bumping into */
    unsigned short tile_N = tile_lookup( (sid->x + 8), (sid->y + 1), xscroll,
            yscroll, cave, cave_width, cave_height);
    unsigned short tile_S = tile_lookup( (sid->x + 8), (sid->y + 16), xscroll,
            yscroll, cave, cave_width, cave_height);
    unsigned short tile_E = tile_lookup( (sid->x + 15), (sid->y + 8), xscroll,
            yscroll, cave, cave_width, cave_height);
    unsigned short tile_W = tile_lookup( (sid->x + 1), (sid->y + 8), xscroll,
            yscroll, cave, cave_width, cave_height);
    
    /* update animation if moving */
    if (sid->move) {
        sid->counter++;
        if (sid->counter >= sid->animation_delay) {
            sid->frame = sid->frame + 8;
            if (sid->frame > 8) {
                sid->frame = 0;
            }
            sprite_set_offset(sid->sprite, sid->frame);
            sid->counter = 0;
        }
    }
    
    // Check if sid is walking into a wall from the North
    if(tile_S == 0x0002 || tile_S == 0x0003 || tile_S == 0x0004) {
        //TODO Use some bitwise voodo to keep sid from walking into the wall
        sid->y -= 8;
    }
    // Check if sid is walking into a wall from the South
    if(tile_N == 0x0022 || tile_N == 0x0023 || tile_N == 0x0024) {
        sid->y += 8;
    }
    // Check if sid is walking into a wall from the East
    if(tile_W == 0x0004 || tile_W == 0x0014 || tile_W == 0x0024) {
        sid->x += 8;
    }
    // Check if sid is walking into a wall from the West
    if(tile_E == 0x0002 || tile_E == 0x0012 || tile_E == 0x0022) {
        sid->x -= 8;
    }

    // Set on screen position
    sprite_position(sid->sprite, sid->x, sid->y);
}

/* the main function */
int main( ) {
    //TODO Re-enable BG1_ENABLE
    // we set the mode to mode 0 with bg0 and bg1 on
    *display_control = MODE0 | BG0_ENABLE | SPRITE_ENABLE | SPRITE_MAP_1D;

    // setup backgrounds
    setup_new_bg();

    // setup the sprite image data
    setup_sprite_image();

    // clear all the sprites on screen now
    sprite_clear();

    // create the sid
    struct Sid sid;
    sid_init(&sid);

    /* set initial scroll to 0 */
    int xscroll = 0;
    int yscroll = 0;

    struct Room* field = room_init(120, 8);
    struct Room* cave = room_init(120, 156);
    struct Room* room = room_init(120, 8);
    
    //room = field;

    /* loop forever */
    while (1) {
        /* update the sid */
        sid_update(&sid, xscroll, yscroll);

        /* now the arrow keys move the sid */
        if (button_pressed(BUTTON_RIGHT)) {
            if (sid_right(&sid)) {
                xscroll++;
            }
        } else if (button_pressed(BUTTON_LEFT)) {
            if (sid_left(&sid)) {
                xscroll--;
            }
        } else if (button_pressed(BUTTON_UP)) {
            if (sid_up(&sid)) {
                yscroll--;
            }
        } else if (button_pressed(BUTTON_DOWN)) {
            if (sid_down(&sid)) {
                yscroll++;
            }
        } else if (button_pressed(BUTTON_A)) {
        } else if (button_pressed(BUTTON_B)) {
            memcpy16_dma((unsigned short*) screen_block(24), (unsigned short*) map, map_width * map_height);
            //TODO Make Sid able to fight. B -> Punch
        } else {
            sid_stop(&sid);
        }

        /* check for if sid is leaving room */
        if (sid.x >= room->exit_x && sid.x < room->exit_x + 16 && sid.y == room->exit_y) {
            memcpy16_dma((unsigned short*) screen_block(24), (unsigned short*) cave, cave_width * cave_height);
        }

        /* wait for vblank before scrolling and moving sprites */
        wait_vblank();
        *bg0_x_scroll = xscroll;
        *bg0_y_scroll = yscroll;
        sprite_update_all();

        /* delay some */
        delay(300);
    }
}

/* the game boy advance uses "interrupts" to handle certain situations
 * for now we will ignore these */
void interrupt_ignore( ) {
    /* do nothing */
}

/* this table specifies which interrupts we handle which way
 * for now, we ignore all of them */
typedef void (*intrp)( );
const intrp IntrTable[13] = {
    interrupt_ignore,   /* V Blank interrupt */
    interrupt_ignore,   /* H Blank interrupt */
    interrupt_ignore,   /* V Counter interrupt */
    interrupt_ignore,   /* Timer 0 interrupt */
    interrupt_ignore,   /* Timer 1 interrupt */
    interrupt_ignore,   /* Timer 2 interrupt */
    interrupt_ignore,   /* Timer 3 interrupt */
    interrupt_ignore,   /* Serial communication interrupt */
    interrupt_ignore,   /* DMA 0 interrupt */
    interrupt_ignore,   /* DMA 1 interrupt */
    interrupt_ignore,   /* DMA 2 interrupt */
    interrupt_ignore,   /* DMA 3 interrupt */
    interrupt_ignore,   /* Key interrupt */
};


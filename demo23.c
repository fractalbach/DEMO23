// clang --target=riscv32 -march=rv32im -nostdlib -O3 demo23.c -o demo23.elf 
#include "abvm.h"
#include "model/model.c"
#include "model/texture/diffuse.c"
#define SIDE 256

/* =============================================================================== */

typedef enum { false, true } bool;
typedef enum { mode_wireframe, mode_normal, mode_notexture, mode_nolight } mode;
typedef struct { int x, y, z; } vec3;
typedef struct { int x, y; } vec2;

const char ach_logo[7][42] = {
	"      # ################ ####             ",
	"     ##                  #   #            ",
	"    ### ### # # ### #  # #   # ### ### # #",
	"   ## # #   # # #   ## # ####  # # #   # #",
	"  ##### #   ### ### # ## #   # ### #   ###",
	" ##   # #   # # #   #  # #   # # # #   # #",
	"##    # ### # # ### #  # ####  # # ### # #",
};
const uint32_t rainbow[] = { 
	0xA532A6, 0xFF811E, 0xFF811E, 0xFFD84C, 0x649228, 0x649228, 0x584FDA
};
const uint32_t white = 0xFFFFFF;
const uint32_t red   = 0xFF2020;
const uint32_t cyan  = 0x00FFFF;
const int width  = SIDE;
const int height = SIDE;
const int int_minimum = -2147483648;

uint16_t stars[SIDE];
uint32_t screen[SIDE][SIDE];
int32_t z_buff[SIDE][SIDE];
uint32_t frame = 0;
int face_to_draw_next = 0;
mode current_mode = mode_normal;
AbvmGamepad pad;

/* =============================================================================== */

void setpixel(int x, int y, uint32_t color) {
	screen[y][x] = color;
}

void swap(int *a, int *b) {
	int tmp = *a;
	*a = *b;
	*b = tmp;
}

// For a non-branching absolute value, see:
// http://graphics.stanford.edu/~seander/bithacks.html#IntegerAbs
int abs(int v) { return v < 0 ? -v : v; }

// Faster way: http://graphics.stanford.edu/~seander/bithacks.html#IntegerMinOrMax
int min(int a, int b) { return a < b ? a:b; }
int max(int a, int b) { return a < b ? b:a; }

int dot(vec3 *a, vec3 *b) {
	return a->x * b->x  +  a->y * b->y  +  a->z * b->z;
}
void set_pixel(int x, int y, int value) { screen[y][x] = value; }

int isqrt(int s) {
	if (s <= 1) 
		return s;
	int x0 = s / 2; // Initial estimate (must be too high)
	int x1 = (x0 + s / x0) / 2; // Update
	while (x1 < x0)	{
		x0 = x1;
		x1 = (x0 + s / x0) / 2;
	}
	return x0;
}

void normalize100(vec3 *v) {
	int mag = isqrt(v->x * v->x  + v->y * v->y  + v->z * v->z);
	v->x = v->x * 100 / mag;
	v->y = v->y * 100 / mag;
	v->z = v->z * 100 / mag;
}

uint32_t brighten(uint32_t color, int addition) {
	int r = (color & 0xFF0000) >> 16;
	int g = (color & 0x00FF00) >> 8;
	int b = (color & 0x0000FF);

	r = min(max(r + addition, 0), 255);
	g = min(max(g + addition, 0), 255);
	b = min(max(b + addition, 0), 255);

	return ( (r<<16) | (g<<8) | (b) );
}

void sub_vec3(vec3 *out, vec3 *a, vec3 *b) {
	out->x = a->x - b->x;
	out->y = a->y - b->y;
	out->z = a->z - b->z;
}

void to_vec3(vec3 *out, const uint16_t *a) {
	out->x = a[0];
	out->y = a[1];
	out->z = a[2];
}

void cross(vec3 *out, vec3 *a, vec3 *b) {
	out->x  =  a->y * b->z  -  a->z * b->y;
	out->y  =  a->z * b->x  -  a->x * b->z;
	out->z  =  a->x * b->y  -  a->y * b->x;
}

/* =============================================================================== */

/* Bresenham’s-Line-Drawing-Algorithm */
void line(int x0, int y0, int x1, int y1, uint32_t color, int z) {
	bool steep = false; 
	if (abs(x0-x1)<abs(y0-y1)) { 
		swap(&x0, &y0); 
		swap(&x1, &y1); 
		steep = true; 
	} 
	if (x0>x1) { 
		swap(&x0, &x1); 
		swap(&y0, &y1); 
	} 
	int dx = x1-x0; 
	int dy = y1-y0; 
	int derror2 = abs(dy)*2; 
	int error2 = 0;
	int y = y0; 
	for (int x=x0; x<=x1; x++) { 
		if (steep) {
			if (z_buff[x][y] >= z) continue;
			setpixel(y, x, color);
			z_buff[x][y] = z;
		} else { 
			if (z_buff[x][y] >= z) continue;
			setpixel(x, y, color);
			z_buff[y][x] = z;
		} 
		error2 += derror2; 
		if (error2 > dx) { 
			y += (y1>y0?1:-1); 
			error2 -= dx*2; 
		} 
	}
}

void draw_wireframe_face(int i) {
	for (int j=0; j<3; j++) {

		int next = (j + 1) % 3;
		int v1_index = model_faces[i][j];
		int v2_index = model_faces[i][next];

		int x0 = model_verts[v1_index][0];
		int y0 = model_verts[v1_index][1];

		int xf = model_verts[v2_index][0];
		int yf = model_verts[v2_index][1];

		int z = model_verts[v1_index][2];
		uint32_t brightness = (z * SIDE) / 256;
		uint32_t color = ( (brightness) | (brightness<<8) | (brightness<<16) );

		line(x0, y0, xf, yf, color, z);
	}
}


/* =============================================================================== */
/* TRIANGLES */


/* Barycentric coordinates are actually returned in [-100,100] instead of [-1,1] */ 
vec3 barycentric(vec3 p, vec3 a, vec3 b, vec3 c) {

	vec3 v0, v1, v2;
	sub_vec3(&v0, &b, &a); // b - a;
	sub_vec3(&v1, &c, &a); // c - a;
	sub_vec3(&v2, &p, &a); // p - a;
	int d00 = dot(&v0, &v0);
	int d01 = dot(&v0, &v1);
	int d11 = dot(&v1, &v1);
	int d20 = dot(&v2, &v0);
	int d21 = dot(&v2, &v1);
	int denom = d00 * d11 - d01 * d01;

	int v = (d11 * d20 - d01 * d21) * 100 / denom;
	int w = (d00 * d21 - d01 * d20) * 100 / denom;
	int u = 100 - v - w;
	vec3 output = {u, v, w};

	return output;
}


void filled_triangle(vec3 *pts, int index, int light_intensity) {
	vec2 bboxmin = {width-1, height-1};
	vec2 bboxmax = {0, 0};
	vec2 clamp = {width-1, height-1};
	
	for (int i=0; i<3; i++) {
		bboxmin.x = max(0, min(bboxmin.x, pts[i].x));
		bboxmin.y = max(0, min(bboxmin.y, pts[i].y));

		bboxmax.x = min(clamp.x, max(bboxmax.x, pts[i].x));
		bboxmax.y = min(clamp.y, max(bboxmax.y, pts[i].y));
	}

	for (int x = bboxmin.x; x <= bboxmax.x; x++) {
		for (int y = bboxmin.y; y <= bboxmax.y; y++) {
			vec3 p = {x, y, 0};
			vec3 bc_screen = barycentric(p, pts[0], pts[1], pts[2]);
			if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0) {
				continue;
			}

			// calculate z position 
			int z = (
				pts[0].z * bc_screen.x +
				pts[1].z * bc_screen.y +
				pts[2].z * bc_screen.z 
			) / 100;

			// If we've painted a pixel at a closer location to the camera, then stop
			if (z_buff[y][x] >= z) {
				continue;
			}

			// calculate texture (x,y) position so we can get map it onto the triangle
			int texture_index0 = model_texture_indices[index][0];
			int texture_index1 = model_texture_indices[index][1];
			int texture_index2 = model_texture_indices[index][2];
			const uint16_t *tex_coord0 = model_texture_uv[texture_index0];
			const uint16_t *tex_coord1 = model_texture_uv[texture_index1];
			const uint16_t *tex_coord2 = model_texture_uv[texture_index2];

			int diffuse_x = (
				tex_coord0[0] * bc_screen.x +
				tex_coord1[0] * bc_screen.y +
				tex_coord2[0] * bc_screen.z 
			) / 100;
			int diffuse_y = (
				tex_coord0[1] * bc_screen.x +
				tex_coord1[1] * bc_screen.y +
				tex_coord2[1] * bc_screen.z 
			) / 100;
			
			uint32_t color;

			if (current_mode == mode_notexture) {
				color = light_intensity | (light_intensity << 8) | (light_intensity << 16);
			} else {
				// retrieve the color from the diffusion texture image
				color = model_diffuse[diffuse_y][diffuse_x];
			}

			if (current_mode == mode_normal) {
				color = brighten(color, light_intensity);
			}

			// update z-buffer before writing pixel
			z_buff[y][x] = z;
			set_pixel(x, y, color);
		}
	}
}

void draw_filled_face(int i) {

	int v1_index = model_faces[i][0];
	int v2_index = model_faces[i][1];
	int v3_index = model_faces[i][2];

	vec3 v1, v2, v3, v31, v21, normal;
	to_vec3( &v1, model_verts[v1_index] );
	to_vec3( &v2, model_verts[v2_index] );
	to_vec3( &v3, model_verts[v3_index] );

	sub_vec3(&v31, &v3, &v1);
	sub_vec3(&v21, &v2, &v1);

	cross(&normal, &v31, &v21);

	normalize100(&normal);

	if (normal.z <= 0) {
		return;
	}

	vec3 pts[3] = {
		{model_verts[v1_index][0], model_verts[v1_index][1], model_verts[v1_index][2]},
		{model_verts[v2_index][0], model_verts[v2_index][1], model_verts[v1_index][2]},
		{model_verts[v3_index][0], model_verts[v3_index][1], model_verts[v1_index][2]},
	};
	
	// int light_intensity = (normal.z * 255) / 100;
	// rainbow[i % 7]
	// uint32_t color = light_intensity | (light_intensity << 8) | (light_intensity << 16);

	int light_intensity;
	if (current_mode == mode_normal) {
		light_intensity = ((normal.z * 100) / 100) - 75;
	} else {
		light_intensity = ((normal.z * 200) / 100);
	}

	filled_triangle(pts, i, light_intensity);
}

/* =============================================================================== */

void draw_model_wireframe() {
	for (int i=0; i<model_face_count; i++) {
		draw_wireframe_face(i);
	}
}

void draw_model_partially_no_looping(int n_faces_to_draw) {
	int i_start = face_to_draw_next;
	int i_end = face_to_draw_next + n_faces_to_draw + 1;
	for (int i = i_start; i < i_end; i++) {
		face_to_draw_next = i;
		if (face_to_draw_next >= model_face_count) {
			return;
		}
		if (current_mode == mode_wireframe) {
			draw_wireframe_face(face_to_draw_next);
		} else {
			draw_filled_face(face_to_draw_next);
		}
	}
}

void draw_logo() {
	// Draw the logo.
	for (int y=0; y<7*2; y++) {
		for (int x=0; x<38-y*2; x++) {
			screen[37+y][x] = rainbow[y>>1];
			z_buff[37+y][x] = int_minimum + 1;
		}

		for (int x=0; x<42*4; x++) {
			uint8_t c = ach_logo[y>>1][x>>2];
			if (c == '#') {
				screen[37+y][20+x] = white;
				z_buff[37+y][20+x] = int_minimum + 1;
			}
		}
	}
}

void update_stars() {
	for (unsigned int y=0;y<height;y++) {
		int x = (stars[y] * width) >> 16;

		if (z_buff[y][x] == int_minimum) {
			screen[y][x] = 0x00000000;
		}

		uint8_t spd = ((y&7)+1);
		stars[y] += spd << 6;

		x = (stars[y] * width) >> 16;

		if (z_buff[y][x] == int_minimum) {
			screen[y][x] = spd*0x1f1f1f1f;
		}
	}
}

// the reset_screen() function causes the compiler to generate a call memset
void *memset(void *s, int c,  unsigned int len) {
	unsigned char* p=s;
	while(len--) {
		*p++ = (unsigned char)c;
	}
	return s;
}

void reset_screen() {
	face_to_draw_next = 0;
	for (int y=0; y<SIDE; y++) {
		for (int x=0; x<SIDE; x++) {
			screen[y][x] = 0;
			z_buff[y][x] = int_minimum;
		}
	}
}

/* =============================================================================== */

void _start() {

	// Initialize z-buffer
	for (int i=0; i<height; i++)
		for (int j=0; j<width; j++)
			z_buff[i][j] = int_minimum;

	draw_logo();
	abvmShow(&screen[0][0], SIDE, SIDE);

	// Place each star at a random start position.
	uint16_t seed = 1;
	for (unsigned int y=0; y<height; y++) {
		seed = (seed >> 1) ^ ((seed&1)?0xb299:0);
		stars[y] = seed;
	}

	for(;;) {

		// Read inputs and Change drawing mode.
		abvmGamepad(0, &pad);
		if (pad.buttons & ABVM_DpadUp) {
			current_mode = mode_wireframe;
			reset_screen();
		} else if (pad.buttons & ABVM_DpadDown) {
			current_mode = mode_normal;
			reset_screen();
		} else if (pad.buttons & ABVM_DpadRight) {
			current_mode = mode_notexture;
			reset_screen();
		} else if (pad.buttons & ABVM_DpadLeft) {
			current_mode = mode_nolight;
			reset_screen();
		}

		// Drawing routines
		update_stars();
		draw_model_partially_no_looping(20);
		abvmShow(&screen[0][0], SIDE, SIDE);
		frame++;
	}
}
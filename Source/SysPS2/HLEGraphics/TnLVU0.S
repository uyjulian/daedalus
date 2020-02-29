// Copyright (C) 2001 StrmnNrmn
// Copyright (C) 2011 Corn

#include "as_reg_compat.h"

#define LIGHTSZ				6	//1<<6 bytes
#define PARAMS_FLAGS_NLIGHT_TXSCAL	0x00
#define PARAMS_LIGHTS		0x10
#define COORDMOD1		   (16 + 12 * (1<<LIGHTSZ) + 32)
#define COORDMOD2		   (16 + 12 * (1<<LIGHTSZ) + 48)
#define FOGPARAM		   (16 + 12 * (1<<LIGHTSZ) + 64)
#define LIGHTDIR			0
#define LIGHTCOL			16
#define LIGHTPOS			32
#define LIGHTSCL			48

#define	TNL_LIGHT			(1<<0)
#define	TNL_TEXGEN			(1<<1)
#define	TNL_TEXGENLIN		(1<<2)
#define	TNL_FOG				(1<<3)
#define	TNL_SHADE			(1<<4)
#define	TNL_ZBUFFER			(1<<5)
#define	TNL_TRICULL			(1<<6)
#define	TNL_CULLBACK		(1<<7)
#define	TNL_POINTLIGHT		(1<<8)
	
.text
.set		push
.set		noreorder
.set		noat

############################
.global _TnLVU0
############################
#	a0 - world matrix				- must be aligned to 16 bytes
#	a1 - world*projection matrix	- must be aligned to 16 bytes
#	a2 - Fiddled vertices			- stride 16
#	a3 - Output vertices			- must be aligned to 16 bytes, stride 64
#	t0 - num vertices
#	t1 - params

# Lighting calculation
# M000: World Matrix
# M100: Projection Matrix
# vf9: Material normal 
# vf10: Accumulated colour
# vf11: ?
# vf12: ?
# vf13: ?
# vf14: Light normal
# vf15: Light colour
# vf16: Scratch
# vf17: current vertex Alpha value
# vf18: Ambient
# vf19: FogMult & FogOffs
# vf19zw: Texture X & Y scale
# vf20: Vertex position
# vf21: project transform [x,y,z,w]
# t4 = cur_light
# t6 = first_light
# t7 = last_light
# v0 = TnLFlags

_TnLVU0:
	lqc2		vf1, 0($a0)				// Load mat world
	lqc2		vf2, 16($a0)
	lqc2		vf3, 32($a0)
	lqc2		vf4, 48($a0)

	lqc2		vf5, 0($a1)				// Load mat project
	lqc2		vf6, 16($a1)
	lqc2		vf7, 32($a1)
	lqc2		vf8, 48($a1)
	
	lqc2		vf19, PARAMS_FLAGS_NLIGHT_TXSCAL($t1)	// Load params [Flags, Num_lights, tscale_x, tscale_y]
	#vmov.p		vf19zw, vf19zw[y,x]				// Swizzle texture X&Y scale
	vmr32.y		vf22, vf19						// [?, tscale_x, ?, ?]
	vmr32.x		vf22, vf22						// [tscale_x, ?, ?, ?]
	vmr32.z		vf19, vf19						// [?, ?, tscale_y, ?]
	vmr32.w		vf19, vf22						// [?, ?, tscale_y, tscale_x]
	#mfv		$t7, vf19y					// Num_lights
	qmfc2		$t7, vf19
	dsrl32 		$t7, $t7, 0

# Calculate the last light index
	addiu		$t6, $t1, PARAMS_LIGHTS		// pointer to first_light = p_lights
	sll			$t7, $t7, LIGHTSZ			// num_lights*64
	addu		$t7, $t6, $t7				// last_light = p_lights + num_lights*64
	lqc2		vf18, LIGHTCOL($t7)			// Load ambient color
	
	sll			$t0, $t0, 4					// count = count * 16
	addu		$t0, $a2, $t0				// end_ptr = start_ptr + count * 16
	
	beq			$a2, $t0, finished_
	#mfv		$v0, vf19x					// TnL flags
	qmfc2		$v0, vf19

	#lv.s		vf19x, FOGPARAM($t1)		// Load fog param [FogMult]
	#lv.s		vf19y, FOGPARAM+4($t1)		// Load fog param [FogOffs]
	ld			$t2, FOGPARAM($t1)
	qmtc2		$t2, vf22
	vmove.xy	vf19, vf22

next_vertex_:
# Load and transform this vertex position
 	#lv.s		vf9x, 0($a2)				// load word [y,x,?,z]
 	#lv.s		vf9y, 4($a2)				// ulqc2 is buggy on PHAT
	#vs2i.p		vf9, vf9					// vf9 = [?,z,y,x] [0xAABBCCDD] [0x11223344] -> [0xCCDD0000] [0xAABB0000] [0x33440000] [0x11220000]
	ld			$t2, 0($a2)					// [y,x,?,z]
	pextlh		$t2, $t2, $zero
	qmtc2		$t2, vf22
	vmove.xyzw	vf9, vf22
	#vi2f.q		vf9, vf9, 16				// int -> float
	vitof15.xyzw	vf9, vf9
	lui			$t2, 0x3f00				// 0.5
	qmtc2		$t2, vf22
	vmulx.xyzw  vf9, vf9, vf22x

	#vmov.q		vf20, vf9[y,x,w,1]			// unfiddle and save for point lighting
	vmove.w		vf20, vf0
	vmr32.xz	vf20, vf9
	vaddx.y		vf20, vf0, vf9x

	#vtfm4.q		vf10, M000, vf20			// World transform
	vmulax.xyzw     ACC, vf1, vf20
    vmadday.xyzw    ACC, vf2, vf20
    vmaddaz.xyzw    ACC, vf3, vf20
    vmaddw.xyzw     vf10, vf4, vf20
	#vtfm4.q		vf21, M100, vf20			// World*Projection transform
	vmulax.xyzw     ACC, vf5, vf20
    vmadday.xyzw    ACC, vf6, vf20
    vmaddaz.xyzw    ACC, vf7, vf20
    vmaddw.xyzw     vf21, vf8, vf20

	sqc2		vf10, 0x00($a3)				// Store world transform
	sqc2		vf21, 0x10($a3)				// Store projection transform
	
# Compute the clip flags
	#vcmp.q		LT, vf21, vf21[-w,-w,-w,0]	// x < -w, y < -w, z < -w
	vclipw.xyz	vf21, vf21w
	#lv.s		vf9x, 12($a2)				// vf9x <- load model normal or color word [w,z,y,x]/[a,b,g,r]
	#mfvc		$t4, $131					// VU0_CC. Corresponds to X_NEG/Y_NEG/Z_NEG (will become X_POS/Y_POS/Z_POS later)
	#vcmp.q		GT, vf21, vf21[w,w,w,0]		// x > w, y > w, z > w, w > 0
	#andi		$t4, $t4, 0x7				// Mask out the condition codes we dont care about
	#sll			$t4, $t4, 3					// Shift up to create X_POS/Y_POS/Z_POS	
	#mfvc		$t5, $131					// VU0_CC. Corresponds to X_NEG/Y_NEG/Z_NEG
	vnop
	vnop
	vnop
	vnop
	vnop
	cfc2		$t5, $vi18
	andi		$t5, 0x3F
	#andi		$t3, $t5, 0x8				// Keep the condition w > 0 (used for fog)
	#andi		$t5, $t5, 0x7				// Mask out the condition codes we dont care about
	#or			$t5, $t4, $t5
	qmfc2		$t2, vf21
	pcpyud		$t2, $t2, $t2
	blez		$t2, __wnegz
	move		$t3, $zero
	ori			$t3, $zero, 0x8
__wnegz:

#Light or Color?
	andi		$t4, $v0, TNL_LIGHT			// if( TNL_LIGHT )
	beqz		$t4, do_color_
	sw			$t5, 0x38($a3)				// Store ClipFlags
	
#Convert the alpha in vf9 to float and pass it along to light color
	#.word		0xd0380000 | (8<<8) | (43)	// vuc2i.s	vf12, vf9x					// vf9 = [a,z,y,x]
	#vi2f.s		vf17w, vf12x, 31		// int -> float, vf17 = [a * 1/256]
	lw			$t2, 12($a2)
	pextlb		$t2, $t2, $t2
	pextlh		$t2, $t2, $t2			// [a,z,y,x]
	psrlw		$t2, $t2, 1
	qmtc2		$t2, vf12
	vitof0.x	vf22, vf12
	vmr32.w		vf17, vf22
	lui			$t2, 0x3000
	qmtc2		$t2, vf22
	vmulx.w		vf17, vf17, vf22x

#Check if there are any lights to process
	beq			$t6, $t7, done_lighting_	// cur_light == last_light?
	vmove.xyzw	vf10, vf18					// Colour = ambient

	or			$t4, $t6, $0				// cur_light = p_lights

# Convert the model normal in vf9 to floats and transform
	#.word		0xd0398080 | (8<<8) | (40)	// vc2i.s		vf9, vf9x					// vf9 = [a,z,y,x]
	#vi2f.q		vf12, vf9[w,z,y,x], 0		// int -> float, Unfiddle & store vertice normal temporary for env map later (obliterates world transform)
	lw			$t2, 12($a2)
	pextlb		$t2, $t2, $zero
	pextlh		$t2, $t2, $zero			// [a,z,y,x]
	qmtc2		$t2, vf12
	vitof0.xyzw	vf12, vf12
	vmr32.xyzw	vf12, vf12				// [z,y,x,a]
	vmr32.xyzw	vf22, vf12				// [y,x,a,z]
	vmr32.xz	vf12, vf22				// [x,y,z,a]
	#vtfm3.t	vf9, M000, vf12			// Transform with world matrix (only need 3x3)//Corn
	vmulax.xyz  ACC, vf1, vf12
    vmadday.xyz ACC, vf2, vf12
    vmaddz.xyz  vf9, vf3, vf12
	#vdot.t		vf11x, vf9, vf9			// vf11x = x*x + y*y + z*z
	vmul.xyz	vf11, vf9, vf9
	vaddy.x		vf11, vf11, vf11y
	vaddz.x		vf11, vf11, vf11z
	#vrsq.s		vf11x, vf11x			// vf11x = 1/sqrt(x*x + y*y + z*z)
#if 1
	vrsqrt		Q, vf0w, vf11x
#else
	vsqrt		Q, vf11x
	vwaitq
	vaddq.x		vf11, vf0, Q
	vdiv		Q, vf0w, vf11x
#endif
	vwaitq
	#vscl.t		vf9, vf9, vf11x			// vf9x = v.normalise().
	vmulq.xyz   vf9, vf9, Q

next_light_:
	lqc2		vf14, LIGHTDIR($t4)			// Load Light normal
	#vdot.t		vf16x[0:1], vf9, vf14		// x = clamp(dot(normal,(x,y,z)),0,1)
	vmul.xyz	vf16, vf9, vf14
	vaddy.x		vf16, vf16, vf16y
	vaddz.x		vf16, vf16, vf16z
	vminiw.x	vf16, vf16, vf0w
	vmaxx.x		vf16, vf16, vf0x
	lqc2		vf15, LIGHTCOL($t4)			// Load Light colour
	addiu		$t4, $t4, (1<<LIGHTSZ)		// Skip to the next light
	#vscl.t		vf16, vf15, vf16x			// r,g,b = r*x, g*x, b*x
	vmulx.xyz   vf16, vf15, vf16x
	bne			$t4, $t7, next_light_
	#vadd.t		vf10, vf10, vf16			// col += r,g,b
	vadd.xyz	vf10, vf10, vf16

done_lighting_:
	#vmov.t		vf17[0:1,0:1,0:1], vf10		// Clamp 0..1 and merge with vertex alpha in vf17w
	vmove.xyz	vf17, vf10
	vminiw.xyz	vf17, vf17, vf0w
	vmaxx.xyz	vf17, vf17, vf0x

	andi		$t4, $v0, TNL_TEXGEN		// if( TNL_TEXGEN )
	beqz		$t4, do_texture_
	nop

# We use worldproject matrix to calc normals it gives a nicer effect (model view result is in vf9) //Corn
	#vtfm3.t	vf9, M100, vf12			// Transform with projworld matrix, looks nicer (only need 3x3)
	vmulax.xyz  ACC, vf5, vf12
    vmadday.xyz ACC, vf6, vf12
    vmaddz.xyz  vf9, vf7, vf12
	#vdot.t		vf10x, vf9, vf9			// vf10x = x*x + y*y + z*z
	vmul.xyz	vf10, vf9, vf9
	vaddy.x		vf10, vf10, vf10y
	vaddz.x		vf10, vf10, vf10z
	#vrsq.s		vf10x, vf10x					// vf10x = 1/sqrt(x*x + y*y + z*z)
#if 1
	vrsqrt		Q, vf0w, vf10x
#else	
	vsqrt		Q, vf10x
	vwaitq
	vaddq.x		vf10, vf0, Q
	vdiv		Q, vf0w, vf10x
#endif
	andi		$t4, $v0, TNL_TEXGENLIN		// if( TNL_TEXGENLIN )
	vwaitq
	beqz		$t4, do_texgen_
	#vscl.p		vf9, vf9, vf10x			// vf11 = v.normalise() (x & y).
	vmulq.xy    vf9, vf9, Q

# EnvMapped G_TEXTURE_GEN  t.x = 0.5 * (1.0 + n.x) t.y = 0.25 * (1.0 + n.y)
	#vadd.p		vf9, vf9[1,1], vf9		// 1+x, 1+y
	vaddw.xy	vf9, vf9, vf0w
	#vmul.p		vf9, vf9[1/2,1/2], vf9	// X * 0.5, Y * 0.25
	lui			$v1, 0x3f00				// 0.5
	qmtc2		$v1, vf22
	vmulx.xy    vf9, vf9, vf22x
	qmfc2		$t2, vf9
	sw			$t2, 0x30($a3)				// Store Texture.x
	dsrl32		$t2, $t2, 0
	b			vtx_done_
	sw			$t2, 0x34($a3)				// Store Texture.y
		
do_texgen_:
# EnvMapped G_TEXTURE_GEN_LINEAR Cheap way to do acos(x)/PI -> 0.5f - 0.25f * absf(x) - 0.25f * absf(x) * absf(x) * absf(x) //Corn
	#vabs.p		vf9, vf9					// absf(x), absf(y)
	vabs.xy		vf9, vf9
	#vmul.p		vf23, vf9[1/4,1/4], vf9	// X * 0.25, Y * 0.25
	lui			$v1, 0x3e80				// 0.25
	qmtc2		$v1, vf22
	vmulx.xy	vf23, vf9, vf22x
	#vsub.p		vf12, vf9[1/2,1/2], vf23	// result = 0.5 - X * 0.25
	lui			$v1, 0x3f00				// 0.5
	pextlw		$v1, $v1, $v1
	qmtc2		$v1, vf22
	vsub.xy		vf12, vf22, vf23
	#vmul.p		vf23, vf9, vf23			// X * X * 0.25, Y * Y * 0.25
	vmul.xy		vf23, vf9, vf23
	#vmul.p		vf23, vf9, vf23			// X * X * X * 0.25, Y * Y * Y * 0.25
	vmul.xy		vf23, vf9, vf23
	#vsub.p		vf12, vf12, vf23			// result -= X * X * X * 0.25
	vsub.xy		vf12, vf12, vf23
	qmfc2		$t2, vf12
	sw			$t2, 0x30($a3)				// Store Texture.x
	dsrl32		$t2, $t2, 0
	b			vtx_done_
	sw			$t2, 0x34($a3)				// Store Texture.y
		
do_color_:
# Normalise the RGBA colour
	#.word		0xd0380000 | (8<<8) | (40)	// vuc2i.s	vf9, vf9x	// vf9 = [a,b,g,r]
	lw			$t2, 12($a2)
	pextlb		$t2, $t2, $t2
	pextlh		$t2, $t2, $t2			// [a,b,g,r]
	psrlw		$t2, $t2, 1
	qmtc2		$t2, vf9
	vmr32.xyzw	vf17, vf9				// [b,g,r,a]
	vaddz.x		vf17, vf0, vf17z		// [r,g,r,a]
	vaddy.z		vf17, vf0, vf9y			// [r,g,b,a]
	#vi2f.q		vf17, vf9[w,z,y,x], 31		// int -> float, vf9 = [r * 1/256, g * 1/256, b * 1/256, a * 1/256]
	vitof0.xyzw	vf17, vf17
	lui			$t2, 0x3000
	qmtc2		$t2, vf22
	vmulx.xyzw	vf17, vf17, vf22x

do_texture_:
# Textured t.x = (float)v.tu * mTextureScale.x 	t.y = (float)v.tv * mTextureScale.y
	#lv.s		vf11x, 8($a2)				// load texture word [tv,tu] (N.B. due to swizzling these are 'backwards' from what you might expect)
	#vs2i.s		vf11, vf11x
	lw			$t2, 8($a2)
	pextlh		$t2, $t2, $zero
	qmtc2		$t2, vf11
	#vi2f.p		vf11, vf11, 16				// int -> float
	vitof15.xy	vf11, vf11
	lui			$t2, 0x3f00				// 0.5
	qmtc2		$t2, vf22
	vmulx.xy    vf11, vf11, vf22x
	#vmul.p		vf11, vf11, vf19zw			// multiply by mTextureScale
	vmulz.y		vf11, vf11, vf19z
	vmulw.x		vf11, vf11, vf19w
	#sv.s		vf11y, 0x30($a3)				// Store Texture.x
	#sv.s		vf11x, 0x34($a3)				// Store Texture.y
	qmfc2		$t2, vf11
	sw			$t2, 0x34($a3)
	dsrl32		$t2, $t2, 0
	sw			$t2, 0x30($a3)

vtx_done_:
	andi		$t4, $v0, TNL_FOG
	beqz		$t4, fog_done_				// if( TNL_FOG )
	nop
	beqz		$t3, fog_done_				// if( proj.w > 0.0f )
	#vzero.s	vf17w						// fog_alpha = 0.0f
	vmr32.w		vf17, vf0

#Calculate fog factor and put as alpha
	#vrcp.s		vf9x, vf21w					// 1/w
	vnop
	vnop
	vnop
	vnop
	vnop
	vdiv		Q, vf0w, vf21w

	#vmul.s		vf10x, vf21z, vf19x			// fogmul * z			
	#vmul.s		vf10x, vf10x, vf9x			// fogmul * z * 1/w
	vmulz.x		vf10, vf19, vf21z
	vwaitq
	vmulq.x		vf10, vf10, Q
	#vadd.s		vf17w[0:1], vf10x, vf19y		// fog_alpha = Clamp[0:1] fogmul * z * 1/w + fogoffs
	vaddy.x		vf22, vf10, vf19y
	vmr32.w		vf17, vf22
	vminiw.w	vf17, vf17, vf0w
	vmaxx.w		vf17, vf17, vf0x

fog_done_:
	sqc2		vf17, 0x20($a3)				// Store colour

# Continue with the next vertex
	addiu		$a2, $a2, 16				// Next input vertex
	bne			$a2, $t0, next_vertex_
	addiu		$a3, $a3, 64				// Next output vertex

finished_:	
	jr			$ra
	nop

#Used by Zelda MM
############################
.global _TnLVU0_Plight
############################
#	a0 - world matrix				- must be aligned to 16 bytes
#	a1 - world*projection matrix	- must be aligned to 16 bytes
#	a2 - Fiddled vertices			- stride 16
#	a3 - Output vertices			- must be aligned to 16 bytes, stride 64
#	t0 - num vertices
#	t1 - params

# Lighting calculation
# M000: World Matrix
# M100: Projection Matrix
# vf9: Material normal 
# vf10: Accumulated colour
# vf11: ?
# vf12: ?
# vf13: ?
# vf14: Scratch
# vf15: Scratch
# vf16: Scratch
# vf17: current vertex Alpha value
# vf18: Ambient
# vf19: FogMult & FogOffs
# vf19zw: Texture X & Y scale
# vf20: Vertex position
# vf21: project transform [x,y,z,w]
# t4 = cur_light
# t6 = first_light
# t7 = last_light
# v0 = TnLFlags

_TnLVU0_Plight:
	lqc2		vf1, 0($a0)				// Load mat world
	lqc2		vf2, 16($a0)
	lqc2		vf3, 32($a0)
	lqc2		vf4, 48($a0)

	lqc2		vf5, 0($a1)				// Load mat project
	lqc2		vf6, 16($a1)
	lqc2		vf7, 32($a1)
	lqc2		vf8, 48($a1)
	
	lqc2		vf19, PARAMS_FLAGS_NLIGHT_TXSCAL($t1)	// Load params [Flags, Num_lights, tscale_x, tscale_y]
	#vmov.p		vf19zw, vf19zw[y,x]				// Swizzle texture X&Y scale 
	vmr32.y		vf22, vf19						// [?, tscale_x, ?, ?]
	vmr32.x		vf22, vf22						// [tscale_x, ?, ?, ?]
	vmr32.z		vf19, vf19						// [?, ?, tscale_y, ?]
	vmr32.w		vf19, vf22						// [?, ?, tscale_y, tscale_x]

	#mfv			$t7, vf19y					// Num_lights
	#mfv			$v0, vf19x					// TnL Flags
	qmfc2		$t7, vf19
	move		$v0, $t7
	dsrl32 		$t7, $t7, 0

	#lv.s		vf19x, FOGPARAM($t1)		// Load fog param [FogMult]
	#lv.s		vf19y, FOGPARAM+4($t1)		// Load fog param [FogOffs]
	ld			$t2, FOGPARAM($t1)
	qmtc2		$t2, vf22
	vmove.xy	vf19, vf22
		
# Calculate the last light index
	addiu		$t6, $t1, PARAMS_LIGHTS		// pointer to first_light = p_lights
	sll			$t7, $t7, LIGHTSZ			// num_lights*64
	addu		$t7, $t6, $t7				// last_light = p_lights + num_lights*64
	
	sll			$t0, $t0, 4					// count = count * 16
	addu		$t0, $a2, $t0				// end_ptr = start_ptr + count * 16
	beq			$a2, $t0, finished_Plight
	lqc2		vf18, LIGHTCOL($t7)			// Load ambient color
	
next_vertex_Plight:
# Load and transform this vertex position
 	#lv.s		vf9x, 0($a2)				// load word [y,x,?,z]
 	#lv.s		vf9y, 4($a2)				// ulqc2 is buggy on PHAT
	#vs2i.p		vf9, vf9					// vf9 = [?,z,y,x]
	ld			$t2, 0($a2)					// [y,x,?,z]
	pextlh		$t2, $t2, $zero
	qmtc2		$t2, vf22
	vmove.xyzw	vf9, vf22
	#vi2f.q		vf9, vf9, 16				// int -> float
	vitof15.xyzw	vf9, vf9
	lui			$t2, 0x3f00				// 0.5
	qmtc2		$t2, vf22
	vmulx.xyzw  vf9, vf9, vf22x
	#vmov.q		vf20, vf9[y,x,w,1]			// unfiddle and save for point lighting
	vmove.w		vf20, vf0
	vmr32.xz	vf20, vf9
	vaddx.y		vf20, vf0, vf9x
	#vtfm4.q		vf10, M000, vf20			// World transform
	vmulax.xyzw     ACC, vf1, vf20
    vmadday.xyzw    ACC, vf2, vf20
    vmaddaz.xyzw    ACC, vf3, vf20
    vmaddw.xyzw     vf10, vf4, vf20
	#vtfm4.q		vf21, M100, vf20			// World*Projection transform
	vmulax.xyzw     ACC, vf5, vf20
    vmadday.xyzw    ACC, vf6, vf20
    vmaddaz.xyzw    ACC, vf7, vf20
    vmaddw.xyzw     vf21, vf8, vf20
	sqc2		vf10, 0x00($a3)				// Store world transform
	sqc2		vf21, 0x10($a3)				// Store projection transform
	
# Compute the clip flags
	#vcmp.q		LT, vf21, vf21[-w,-w,-w,0]	// x < -w, y < -w, z < -w
	vclipw.xyz	vf21, vf21w
	#lv.s		vf9x, 12($a2)				// vf9x <- load model normal or color word [w,z,y,x]/[a,b,g,r]
	#mfvc		$t4, $131					// VU0_CC. Corresponds to X_NEG/Y_NEG/Z_NEG (will become X_POS/Y_POS/Z_POS later)
	#vcmp.q		GT, vf21, vf21[w,w,w,0]		// x > w, y > w, z > w, w > 0
	#andi		$t4, $t4, 0x7				// Mask out the condition codes we dont care about
	#sll			$t4, $t4, 3					// Shift up to create X_POS/Y_POS/Z_POS	
	#mfvc		$t5, $131					// VU0_CC. Corresponds to X_NEG/Y_NEG/Z_NEG
	#andi		$t3, $t5, 0x8				// Keep the condition w > 0 (used for fog)
	#andi		$t5, $t5, 0x7				// Mask out the condition codes we dont care about
	#or			$t5, $t4, $t5
	vnop
	vnop
	vnop
	vnop
	vnop
	cfc2		$t5, $vi18
	andi		$t5, 0x3F
	sw			$t5, 0x38($a3)				// Store ClipFlags
	qmfc2		$t2, vf21
	pcpyud		$t2, $t2, $t2
	blez		$t2, __wnegz_Plight
	move		$t3, $zero
	ori			$t3, $zero, 0x8
__wnegz_Plight:

#Convert the alpha in vf9 to float and pass it along to light color
	#.word		0xd0380000 | (8<<8) | (43)	// vuc2i.s	vf12, vf9x					// vf9 = [a,z,y,x]
	#vi2f.s		vf17w, vf12x, 31				// int -> float, vf17 = [a * 1/256]
	lw			$t2, 12($a2)
	pextlb		$t2, $t2, $t2
	pextlh		$t2, $t2, $t2			// [a,z,y,x]
	psrlw		$t2, $t2, 1
	qmtc2		$t2, vf12
	vitof0.x	vf22, vf12
	vmr32.w		vf17, vf22
	lui			$t2, 0x3000
	qmtc2		$t2, vf22
	vmulx.w		vf17, vf17, vf22x

#Check if there are any lights to process
	beq			$t6, $t7, done_Plight		// cur_light == last_light?
	vmove.xyzw	vf10, vf18					// Colour = ambient

	or			$t4, $t6, $0				// cur_light = p_lights

next_light_Plight:
	lqc2		vf14, LIGHTPOS($t4)			// Load Light position
	vsub.xyz	vf14, vf14, vf20			// light pos - vertex pos
	#vdot.t		vf14z, vf14, vf14			// vf14z (qlen) = x*x + y*y + z*z
	vmul.xyz	vf22, vf9, vf14
	vaddx.z		vf14, vf22, vf22x
	vaddy.z		vf14, vf14, vf22y
	lqc2		vf15, LIGHTSCL($t4)			// Load Light scaleing
	#vsqrt.s	vf14y, vf14z				// vf14y (llen) = SQRT(x*x + y*y + z*z)
	vrsqrt		Q, vf0w, vf14z
	vwaitq
	vaddq.y		vf14, vf0, Q
	lqc2		vf16, LIGHTCOL($t4)			// Load Light colour
	#vdot.t		vf15x, vf15, vf14[1,y,z]	// vf15x (L) = (1.0f*ca + llen*la + qlen*qa)
	vmul.yz		vf22, vf15, vf14
	vaddy.x		vf15, vf15, vf22y
	vaddz.x		vf15, vf15, vf22z
	#mfv		$t5, vf15x					// vf15x (L) -> t5
	qmfc2		$t5, vf15
	pextlw		$t5, $t5, $t5
	beqz		$t5, skip_Plight			// Skip this light if L == 0.0f (0x00000000)
	addiu		$t4, $t4, (1<<LIGHTSZ)		// Advance pointer to the next light
	#vrcp.s		vf15x, vf15x				// vf15x (i) = 1.0f / L
	vdiv		Q, vf0w, vf15x
	vwaitq
	#vscl.t		vf16, vf16, vf15x			// r,g,b = r*i, g*i, b*i
	vmulq.xyz	vf16, vf16, Q
	#vadd.t		vf10, vf10, vf16			// col += r,g,b
	vadd.xyz	vf10, vf10, vf16
skip_Plight:
	bne			$t4, $t7, next_light_Plight
	nop
	
done_Plight:
	#vmov.t		vf17[0:1,0:1,0:1], vf10		// Clamp 0..1 and merge with vertex alpha in vf17w
	vmove.xyz	vf17, vf10
	vminiw.xyz	vf17, vf17, vf0w
	vmaxx.xyz	vf17, vf17, vf0x

# Textured t.x = (float)v.tu * mTextureScale.x 	t.y = (float)v.tv * mTextureScale.y
	#lv.s		vf11x, 8($a2)				// load texture word [tv,tu] (N.B. due to swizzling these are 'backwards' from what you might expect)
	#vs2i.s		vf11, vf11x
	lw			$t2, 8($a2)
	pextlh		$t2, $t2, $zero
	qmtc2		$t2, vf11
	#vi2f.p		vf11, vf11, 16				// int -> float
	vitof15.xy	vf11, vf11
	lui			$t2, 0x3f00				// 0.5
	qmtc2		$t2, vf22
	vmulx.xy    vf11, vf11, vf22x
	#vmul.p		vf11, vf11, vf19zw			// multiply by mTextureScale
	vmulz.y		vf11, vf11, vf19z
	vmulw.x		vf11, vf11, vf19w
	#sv.s		vf11y, 0x30($a3)				// Store Texture.x
	qmfc2		$t2, vf11
	sw			$t2, 0x34($a3)
	dsrl32		$t2, $t2, 0
	andi		$t4, $v0, TNL_FOG
	beqz		$t4, fog_done_plight		// if( TNL_FOG )
	#sv.s		vf11x, 0x34($a3)				// Store Texture.y
	sw			$t2, 0x30($a3)
	beqz		$t3, fog_done_plight		// if( proj.w > 0.0f )
	#vzero.s	vf17w						// fog_alpha = 0.0f
	vmr32.w		vf17, vf0

#Calculate fog factor and put as alpha
	#vrcp.s		vf9x, vf21w					// 1/w
	vdiv		Q, vf0w, vf21w
	vwaitq
	#vmul.s		vf10x, vf21z, vf19x			// fogmul * z
	vmulz.x		vf10, vf19, vf21z
	#vmul.s		vf10x, vf10x, vf9x			// fogmul * z * 1/w
	vmulq.x		vf10, vf10, Q
	#vadd.s		vf17w[0:1], vf10x, vf19y		// fog_alpha = Clamp[0:1] fogmul * z * 1/w + fogoffs
	vaddy.x		vf22, vf10, vf19y
	vmr32.w		vf17, vf22
	vminiw.w	vf17, vf17, vf0w
	vmaxx.w		vf17, vf17, vf0x

fog_done_plight:
	sqc2		vf17, 0x20($a3)				// Store colour

# Continue with the next vertex
	addiu		$a2, $a2, 16				// Next input vertex
	bne			$a2, $t0, next_vertex_Plight
	addiu		$a3, $a3, 64				// Next output vertex

finished_Plight:	
	jr			$ra
	nop

############################
.global _TnLVU0CBFD
############################
#	a0 - world matrix				- must be aligned to 16 bytes
#	a1 - world*projection matrix	- must be aligned to 16 bytes
#	a2 - Fiddled vertices			- stride 16
#	a3 - Output vertices			- must be aligned to 16 bytes, stride 64
#	t0 - num vertices
#	t1 - params
#	t2 - model normal pointer
#	t3 - v0

# Lighting calculation
# M000: World Matrix
# M100: Projection Matrix
# vf9: Material normal 
# vf10: Light position 
# vf11: projected
# vf12: store raw material normal
# vf13: color result
# vf14: Light direction
# vf15: Light color
# vf16: Scratch
# vf24: Accumulated colour
# vf18: Ambient light color
# vf19zw: Texture X & Y scale
# vf20: Coord [8, 9, 10, 11]
# vf21: Coord [12, 13, 14, 15]
# v0 = TnLFlags
# t4 = cur_light
# t5 = last_light(point light)
# t6 = first_light
# t7 = last_light

_TnLVU0CBFD:
	lqc2		vf1, 0($a0)				// Load mat world
	lqc2		vf2, 16($a0)
	lqc2		vf3, 32($a0)
	lqc2		vf4, 48($a0)

	lqc2		vf5, 0($a1)				// Load mat project
	lqc2		vf6, 16($a1)
	lqc2		vf7, 32($a1)
	lqc2		vf8, 48($a1)
	
	lqc2		vf19, PARAMS_FLAGS_NLIGHT_TXSCAL($t1)	// Load params [Flags, Num_lights, tscale_x, tscale_y]
	#vmov.p		vf19zw, vf19zw[y,x]				// Swizzle texture X&Y scale 
	vmr32.y		vf22, vf19						// [?, tscale_x, ?, ?]
	vmr32.x		vf22, vf22						// [tscale_x, ?, ?, ?]
	vmr32.z		vf19, vf19						// [?, ?, tscale_y, ?]
	vmr32.w		vf19, vf22						// [?, ?, tscale_y, tscale_x]
	#mfv		$t7, vf19y					// Num_lights
	qmfc2		$t7, vf19
	dsrl32 		$t7, $t7, 0
	
# Load Coord Mod vectors
	lqc2		vf20, COORDMOD1($t1)		// Load Coord [8, 9, 10, 11]
	lqc2		vf21, COORDMOD2($t1)		// Load Coord [12, 13, 14, 15]

# Calculate the last light index
	addiu		$t6, $t1, PARAMS_LIGHTS		// pointer to first_light = p_lights
	sll			$t7, $t7, LIGHTSZ			// num_lights*64
	addu		$t7, $t6, $t7				// last_light = p_lights + num_lights*64
	lqc2		vf18, LIGHTCOL($t7)			// Load ambient color

# Calculate the last vertex index
	sll			$t0, $t0, 4					// count = count * 16
	addu		$t0, $a2, $t0				// end_ptr = start_ptr + count * 16
	beq			$a2, $t0, finished_CBFD
	#mfv		$v0, vf19x					// TnL flags
	qmfc2		$v0, vf19
	
next_vertex_CBFD:
# Load and transform this vertex position
 	#lv.s		vf9x, 0($a2)				// load word [y,x,?,z]
 	#lv.s		vf9y, 4($a2)				// ulqc2 is buggy on PHAT
	#vs2i.p		vf9, vf9					// vf9 = [?,z,y,x]
	ld			$v1, 0($a2)					// [y,x,?,z]
	pextlh		$v1, $v1, $zero
	qmtc2		$v1, vf9
	#vi2f.q		vf9, vf9, 16				// int -> float
	vitof15.xyzw	vf9, vf9
	lui			$v1, 0x3f00				// 0.5
	qmtc2		$v1, vf22
	vmulx.xyzw  vf9, vf9, vf22x
	#vmov.q		vf12, vf9[y,x,w,1]			// unswizzle order
	vmove.w		vf12, vf0
	vmr32.xz	vf12, vf9
	vaddx.y		vf12, vf0, vf9x
	#vtfm4.q		vf10, M000, vf12			// World transform
	vmulax.xyzw     ACC, vf1, vf12
    vmadday.xyzw    ACC, vf2, vf12
    vmaddaz.xyzw    ACC, vf3, vf12
    vmaddw.xyzw     vf10, vf4, vf12

#Load & Normalise the vertex RGBA colour
	#lv.s		vf9x, 12($a2)				// load normal word [w,z,y,x]
	#.word		0xd0380000 | (8<<8) | (40)	// vuc2i.s	vf9, vf9x					// vf9 = [a,b,g,r]
	lw			$v1, 12($a2)
	pextlb		$v1, $v1, $v1
	pextlh		$v1, $v1, $v1			// [a,b,g,r]
	psrlw		$v1, $v1, 1
	qmtc2		$v1, vf9
	vmr32.xyzw	vf24, vf9				// [b,g,r,a]
	vaddz.x		vf24, vf0, vf24z		// [r,g,r,a]
	vaddy.z		vf24, vf0, vf9y			// [r,g,b,a]
	#vi2f.q		vf24, vf9[w,z,y,x], 31		// int -> float, R403 = [r * 1/256, g * 1/256, b * 1/256, a * 1/256]
	lui			$v1, 0x3000
	qmtc2		$v1, vf22
	vmulx.xyzw	vf24, vf24, vf22x
	#vtfm4.q		vf11, M100, vf10			// Projection transform
	vmulax.xyzw     ACC, vf5, vf10
    vmadday.xyzw    ACC, vf6, vf10
    vmaddaz.xyzw    ACC, vf7, vf10
    vmaddw.xyzw     vf11, vf8, vf10
	sqc2		vf10, 0x00($a3)				// Store world transform
	sqc2		vf11, 0x10($a3)				// Store projection transform
	
# Compute the clip flags
	#vcmp.q		LT, vf11, vf11[-w,-w,-w,0]	// x < -w, y < -w, z < -w
	#vnop
	#mfvc		$t4, $131					// VU0_CC. Corresponds to X_NEG/Y_NEG/Z_NEG (will become X_POS/Y_POS/Z_POS later)
	#vcmp.q		GT, vf11, vf11[w,w,w,0]		// x > w, y > w, z > w
	#andi		$t4, $t4, 0x7				// Mask out the condition codes we dont care about
	#sll		$t4, $t4, 3					// Shift up to create X_POS/Y_POS/Z_POS	
	#mfvc		$t5, $131					// VU0_CC. Corresponds to X_NEG/Y_NEG/Z_NEG
	#andi		$t5, $t5, 0x7				// Mask out the condition codes we dont care about
	#or			$t5, $t4, $t5
	vclipw.xyz	vf11, vf11w
	vnop
	vnop
	vnop
	vnop
	vnop
	cfc2		$t5, $vi18
	andi		$t5, 0x3F

#LIGHT
	andi		$t4, $v0, TNL_LIGHT			// if( TNL_LIGHT )
	beqz		$t4, do_texture_CBFD
	sw			$t5, 0x38($a3)				// Store ClipFlags

#skip to normal light and avoid the model normal transform if only TNL_LIGHT is set
	andi		$t4, $v0, (TNL_POINTLIGHT | TNL_TEXGEN)			// if( TNL_POINTLIGHT | TNL_TEXGEN )
	beqz		$t4, do_normallight_CBFD
	nop
	
#Use world matrix to transform model normal
 	xori		$t5, $t3, 0x3				// = v0 ^ 3
	addu		$t5, $t2, $t5				// += base address
	lb			$t4, 0($t5)					// get normal x
	#mtv		$t4, vf12x					// Store vertice normal X 
	addiu		$t5, $t3, 0x1				// = v0 + 1
 	xori		$t5, $t5, 0x3				// ^= 3
	addu		$t5, $t2, $t5				// += base address
	lb			$v1, 4($a2)					// Get vert_norm z
	pextlw		$t4, $v1, $t4
	lb			$v1, 0($t5)					// get normal y
	#mtv		$t4, vf12y					// Store vertice normal Y 
	#mtv		$v1, vf12z					// Store vertice normal Z 
	pextlw		$v1, $v1, $t4
	qmtc2		$v1, vf12
	#vi2f.t		vf12, vf12, 0				// int -> float
	vitof0.xyz	vf12, vf12
	#vtfm3.t	vf9, M000, vf12			// Transform with world matrix, (only need 3x3)
	vmulax.xyz  ACC, vf1, vf12
    vmadday.xyz ACC, vf2, vf12
    vmaddz.xyz  vf9, vf3, vf12
	#vdot.t		vf10x, vf9, vf9			// vf10x = x*x + y*y + z*z
	vmul.xyz	vf10, vf9, vf9
	vaddy.x		vf10, vf10, vf10y
	vaddz.x		vf10, vf10, vf10z
	#vrsq.s		vf10x, vf10x					// vf10x = 1/sqrt(x*x + y*y + z*z)
	vrsqrt		Q, vf0w, vf10x
	vwaitq

	andi		$t4, $v0, TNL_POINTLIGHT	// if( TNL_POINTLIGHT )
	beqz		$t4, do_normallight_CBFD
	#vscl.t		vf9, vf9, vf10x			// vf9 = normalise transformed model vector (x, y, z).
	vmulq.xyz   vf9, vf9, Q

#POINT LIGHT
	beq			$t6, $t7, done_plight_CBFD	// cur_light == last_light?
	#vmov.q		vf13, vf18					// Colour = ambient
	vmove.xyzw	vf13, vf18

	addiu		$t5, $t7, -(1<<LIGHTSZ)		// Do one light less with point light
	beq			$t6, $t5, do_dot_plight_CBFD	// cur_light == last_light?
	or			$t4, $t6, $0				// cur_light = p_lights

	#vadd.q		vf11, vf11, vf20			// Proj + Coord1
	vadd.xyzw	vf11, vf11, vf20
	#vmul.q		vf11, vf11, vf21			// * Coord2
	vmul.xyzw	vf11, vf11, vf21

next_plight_CBFD:
	lqc2		vf14, LIGHTDIR($t4)			// Load Light direction	& SkipIfZero
	#mfv		$t8, vf14w					// SkipIfZero
	qmfc2		$t8, vf14
	pcpyud		$t8, $t8, $t8
	dsrl32		$t8, $t8, 0
	beqz		$t8, skip_plight_CBFD
	nop
	
	lqc2		vf10, LIGHTPOS($t4)			// Load Light position
	#vsub.q		vf10, vf11, vf10			// ProjCoord - light position
	#vdot.q		vf10x, vf10, vf10			// vf10x = x*x + y*y + z*z
	vsub.xyzw	vf10, vf11, vf10
	vmul.xyzw	vf10, vf10, vf10
	vaddy.x		vf10, vf10, vf10y
	vaddz.x		vf10, vf10, vf10z
	vaddw.x		vf10, vf10, vf10w
	lqc2		vf15, LIGHTCOL($t4)			// Load Light colour & scale
	#vrcp.s		vf10x, vf10x				// vf10x = 1/(x*x + y*y + z*z)
	vdiv		Q, vf0w, vf10w
	vwaitq
	#vmul.s		vf15w[0:1], vf15w, vf10x	// R332 = p_i clamped 0:1.
	vmulq.w		vf15, vf15, Q
	vminiw.w	vf15, vf15, vf0w
	vmaxx.w		vf15, vf15, vf0x
	#vdot.t		vf16x[0:1], vf9, vf14		// intensity = clamp(dot(normal,(x,y,z)),0,1)
	vmul.xyz	vf16, vf9, vf14
	vaddy.x		vf16, vf16, vf16y
	vaddz.x		vf16, vf16, vf16z
	vminiw.x	vf16, vf16, vf0w
	vmaxx.x		vf16, vf16, vf0x
	#vmul.s		vf16x, vf16x, vf15w			// intensity *= p_i
	vmulw.x		vf16, vf16, vf15w
	#vscl.t		vf15, vf15, vf16x			// r,g,b = r*i, g*i, b*i
	vmulx.xyz	vf15, vf15, vf16x
	#vadd.t		vf13, vf13, vf15			// col += r,g,b
	vadd.xyz	vf13, vf13, vf15

skip_plight_CBFD:
	addiu		$t4, $t4, (1<<LIGHTSZ)		// Skip to the next light
	bne			$t4, $t5, next_plight_CBFD
	nop
	
do_dot_plight_CBFD:
	lqc2		vf14, LIGHTDIR($t4)			// Load Light normal
	#vdot.t		vf16x[0:1], vf9, vf14		// intensity = clamp(dot(normal,(x,y,z)),0,1)
	vmul.xyz	vf16, vf9, vf14
	vaddy.x		vf16, vf16, vf16y
	vaddz.x		vf16, vf16, vf16z
	vminiw.x	vf16, vf16, vf0w
	vmaxx.x		vf16, vf16, vf0x
	lqc2		vf15, LIGHTCOL($t4)			// Load Light colour & scale
	#vscl.t		vf15, vf15, vf16x			// r,g,b = r*i, g*i, b*i
	vmulx.xyz	vf15, vf15, vf16x
	#vadd.t		vf13[0:1,0:1,0:1], vf13, vf15	// col += r,g,b (and clamp result)
	vadd.xyz	vf13, vf13, vf15
	vminiw.xyz	vf13, vf13, vf0w
	vmaxx.xyz	vf13, vf13, vf0x

done_plight_CBFD:
	b			skip_to_envmap_CBFD
	#vmul.t		vf24, vf24, vf13			// Col *= col and merge with vertex alpha
	vmul.xyz	vf24, vf24, vf13
	
#NORMAL LIGHT
do_normallight_CBFD:
	beq			$t6, $t7, done_nlight_CBFD	// cur_light == last_light?
	#vmov.q		vf13, vf18					// Colour = ambient
	vmove.xyzw	vf13, vf18

	or			$t4, $t6, $0				// cur_light = p_lights
	#vadd.q		vf11, vf11, vf20			// Proj + Coord1
	vadd.xyzw	vf11, vf11, vf20
	#vmul.q		vf11, vf11, vf21			// * Coord2
	vmul.xyzw	vf11, vf11, vf21

next_nlight_CBFD:
	lqc2		vf10, LIGHTPOS($t4)			// Load Light position
	#vsub.q		vf10, vf11, vf10			// ProjCoord - light position
	vsub.xyzw	vf10, vf11, vf10
	#vdot.q		vf10x, vf10, vf10			// vf10x = x*x + y*y + z*z
	vmul.xyzw	vf10, vf10, vf10
	vaddy.x		vf10, vf10, vf10y
	vaddz.x		vf10, vf10, vf10z
	vaddw.x		vf10, vf10, vf10w
	lqc2		vf15, LIGHTCOL($t4)			// Load Light colour & scale
	#vrcp.s		vf10x, vf10x					// vf10x = 1/(x*x + y*y + z*z)
	vdiv		Q, vf0w, vf10x
	vwaitq
	#vmul.s		vf15w[0:1], vf15w, vf10x		// R332 = p_i clamped 0:1.
	vmulq.w		vf15, vf15, Q
	vminiw.w	vf15, vf15, vf0w
	vmaxx.w		vf15, vf15, vf0x
	addiu		$t4, $t4, (1<<LIGHTSZ)		// Skip to the next light
	#vscl.t		vf15, vf15, vf15w			// r,g,b = r*i, g*i, b*i
	vmulw.xyz	vf15, vf15, vf15w
	bne			$t4, $t7, next_nlight_CBFD
	#vadd.t		vf13, vf13, vf15			// col += r,g,b
	vadd.xyz	vf13, vf13, vf15

	#vmov.t		vf13[0:1,0:1,0:1], vf13
	vminiw.xyz	vf13, vf13, vf0w
	vmaxx.xyz	vf13, vf13, vf0x

done_nlight_CBFD:
	#vmul.t		vf24, vf24, vf13			// Col *= col and merge with vertex alpha
	vmul.xyz	vf24, vf24, vf13

#Check environment mapping
skip_to_envmap_CBFD:
	andi		$t4, $v0, TNL_TEXGEN		// if( TNL_TEXGEN )
	beqz		$t4, do_texture_CBFD
	nop
	
	andi		$t4, $v0, TNL_TEXGENLIN		// if( TNL_TEXGENLIN )
	beqz		$t4, do_texgen_CBFD
	nop
	
# EnvMapped G_TEXTURE_GEN_LINEAR Cheap way to do acos(x)/PI -> 0.5f - 0.25f * x - 0.25f * x * x * x //Corn
	#vmul.p		vf23, vf9[1/4,1/4], vf9	// X * 0.25, Y * 0.25
	lui			$v1, 0x3e80				// 0.25
	qmtc2		$v1, vf22
	vmulx.xy	vf23, vf9, vf22x
	#vsub.p		vf10, vf9[1/2,1/2], vf23	// result = 0.5 - X * 0.25
	lui			$v1, 0x3f00				// 0.5
	pextlw		$v1, $v1, $v1
	qmtc2		$v1, vf22
	vsub.xy		vf10, vf22, vf23
	#vmul.p		vf23, vf9, vf23			// X * X * 0.25, Y * Y * 0.25
	vmul.xy		vf23, vf9, vf23
	#vmul.p		vf23, vf9, vf23			// X * X * X * 0.25, Y * Y * Y * 0.25
	vmul.xy		vf23, vf9, vf23
	#vsub.p		vf10, vf10, vf23			// result -= X * X * X * 0.25
	vsub.xy		vf10, vf10, vf23
	qmfc2		$v1, vf10
	sw			$v1, 0x30($a3)				// Store Texture.x
	dsrl32		$v1, $v1, 0
	b			vtx_done_CBFD
	sw			$v1, 0x34($a3)				// Store Texture.y
		
do_texgen_CBFD:
# EnvMapped G_TEXTURE_GEN  t.x = 0.5 * (1.0 + n.x) t.y = 0.5 * (1.0 + n.y)
	#vadd.p		vf9, vf9[1,1], vf9		// 1+x, 1+y
	vaddw.xy	vf9, vf9, vf0w
	#vmul.p		vf9, vf9[1/2,1/2], vf9	// X * 0.5, Y * 0.5
	lui			$v1, 0x3f00				// 0.5
	pextlw		$v1, $v1, $v1
	qmtc2		$v1, vf22
	vmul.xy		vf9, vf9, vf22
	qmfc2		$v1, vf9
	sw			$v1, 0x30($a3)				// Store Texture.x
	dsrl32		$v1, $v1, 0
	b			vtx_done_CBFD
	sw			$v1, 0x34($a3)				// Store Texture.y
		
do_texture_CBFD:
# Textured t.x = (float)v.tu * mTextureScale.x 	t.y = (float)v.tv * mTextureScale.y
	#lv.s		vf11x, 8($a2)				// load texture word [tv,tu] (N.B. due to swizzling these are 'backwards' from what you might expect)
	lw			$v1, 8($a2)
	#vs2i.s		vf11, vf11x
	pextlh		$v1, $v1, $zero
	qmtc2		$v1, vf11
	#vi2f.p		vf11, vf11, 16				// int -> float
	vitof15.xy	vf11, vf11
	lui			$v1, 0x3f00				// 0.5
	qmtc2		$v1, vf22
	vmulx.xy	vf11, vf11, vf22x
	#vmul.p		vf11, vf11, vf19zw			// multiply by mTextureScale
	vmulz.y		vf11, vf11, vf19z
	vmulw.x		vf11, vf11, vf19w
	#sv.s		vf11y, 0x30($a3)				// Store Texture.x
	#sv.s		vf11x, 0x34($a3)				// Store Texture.y
	qmfc2		$v1, vf11
	sw			$v1, 0x34($a3)
	dsrl32		$v1, $v1, 0
	sw			$v1, 0x30($a3)

vtx_done_CBFD:
	sqc2		vf24, 0x20($a3)				// Store colour

# Continue with the next vertex
	addiu		$t3, $t3, 2					// inc v0 counter
	addiu		$a2, $a2, 16				// Next input vertex
	bne			$a2, $t0, next_vertex_CBFD
	addiu		$a3, $a3, 64				// Next output vertex

finished_CBFD:	
	jr			$ra
	nop
	
############################
.global _TnLVU0PD
############################
#	a0 - world matrix				- must be aligned to 16 bytes
#	a1 - world*projection matrix	- must be aligned to 16 bytes
#	a2 - Fiddled vertices			- stride 16
#	a3 - Output vertices			- must be aligned to 16 bytes, stride 64
#	t0 - num vertices
#	t1 - params
#	t2 - CI pointer

# Lighting calculation
# M000: World Matrix
# M100: Projection Matrix
# vf9: Material normal 
# vf10: Accumulated colour
# vf11: ?
# vf12: ?
# vf13: ?
# vf14: Light normal
# vf15: Light colour
# vf16: Scratch
# vf17: current vertex Alpha value
# vf18: Ambient
# vf19zw: Texture X & Y scale
# t4 = cur_light
# t6 = first_light
# t7 = last_light
# v0 = TnLFlags
# v1 = color index pointer

_TnLVU0PD:
	lqc2		vf1, 0($a0)				// Load mat world
	lqc2		vf2, 16($a0)
	lqc2		vf3, 32($a0)
	lqc2		vf4, 48($a0)

	lqc2		vf5, 0($a1)				// Load mat project
	lqc2		vf6, 16($a1)
	lqc2		vf7, 32($a1)
	lqc2		vf8, 48($a1)
	
	lqc2		vf19, PARAMS_FLAGS_NLIGHT_TXSCAL($t1)	// Load params [Flags, Num_lights, tscale_x, tscale_y]
	#vmov.p		vf19zw, vf19zw[y,x]				// Swizzle texture X&Y scale 
	vmr32.y		vf22, vf19						// [?, tscale_x, ?, ?]
	vmr32.x		vf22, vf22						// [tscale_x, ?, ?, ?]
	vmr32.z		vf19, vf19						// [?, ?, tscale_y, ?]
	vmr32.w		vf19, vf22						// [?, ?, tscale_y, tscale_x]
	#mfv		$t7, vf19y					// Num_lights
	qmfc2		$t7, vf19
	dsrl32 		$t7, $t7, 0
	
# Calculate the last light index
	addiu		$t6, $t1, PARAMS_LIGHTS		// pointer to first_light = p_lights
	sll			$t7, $t7, LIGHTSZ			// num_lights*64
	addu		$t7, $t6, $t7				// last_light = p_lights + num_lights*64
	lqc2		vf18, LIGHTCOL($t7)			// Load ambient color
	
	sll			$v1, $t0, 2					// count * 4
	sll			$t0, $t0, 3					// count * 8
	addu		$t0, $v1, $t0				// count = count * 12
	addu		$t0, $a2, $t0				// end_ptr = start_ptr + count * 12
	beq			$a2, $t0, finished_PD
	#mfv		$v0, vf19x					// TnL flags
	qmfc2		$v0, vf19

next_vertex_PD:
# Load and transform this vertex position
 	#lv.s		vf12x, 0($a2)				// load word [y,x,?,z]
 	#lv.s		vf12y, 4($a2)				// ulqc2 is buggy on PHAT
	ld			$v1, 0($a2)					// [y,x,?,z]
	#vs2i.p		vf9, vf12					// vf9 = [?,z,y,x]
	pextlh		$v1, $v1, $zero
	qmtc2		$v1, vf9
	#vi2f.q		vf9, vf9, 16				// int -> float
	vitof15.xyzw	vf9, vf9
	lui			$t3, 0x3f00				// 0.5
	qmtc2		$t3, vf22
	vmulx.xyzw	vf9, vf9, vf22x
	#vmov.q		vf9, vf9[y,x,w,1]
	vmr32.xz	vf22, vf9
	vaddx.y		vf9, vf0, vf9x
	vmove.xz	vf9, vf22
	vmove.w		vf9, vf0
	#vtfm4.q	vf10, M000, vf9			// World transform
	vmulax.xyzw     ACC, vf1, vf9
    vmadday.xyzw    ACC, vf2, vf9
    vmaddaz.xyzw    ACC, vf3, vf9
    vmaddw.xyzw     vf10, vf4, vf9
 	#mfv			$v1, vf12y					// Get Cindx
	pcpyud		$v1, $v1, $zero
	srl			$v1, $v1, 16
 	andi		$v1, 0xFF					// use only low Byte
	addu		$v1, $v1, $t2				// pointer = base vector + Cindx
	#vtfm4.q		vf11, M100, vf10			// Projection transform
	vmulax.xyzw     ACC, vf5, vf10
    vmadday.xyzw    ACC, vf6, vf10
    vmaddaz.xyzw    ACC, vf7, vf10
    vmaddw.xyzw     vf11, vf8, vf10
	sqc2		vf10, 0x00($a3)				// Store world transform
	sqc2		vf11, 0x10($a3)				// Store projection transform
	
# Compute the clip flags
	#vcmp.q		LT, vf11, vf11[-w,-w,-w,0]	// x < -w, y < -w, z < -w
	#vnop
	#mfvc		$t4, $131					// VU0_CC. Corresponds to X_NEG/Y_NEG/Z_NEG (will become X_POS/Y_POS/Z_POS later)
	#vcmp.q		GT, vf11, vf11[w,w,w,0]		// x > w, y > w, z > w
	#andi		$t4, $t4, 0x7				// Mask out the condition codes we dont care about
	#sll			$t4, $t4, 3					// Shift up to create X_POS/Y_POS/Z_POS	
	#mfvc		$t5, $131					// VU0_CC. Corresponds to X_NEG/Y_NEG/Z_NEG
	#andi		$t5, $t5, 0x7				// Mask out the condition codes we dont care about
	#or			$t5, $t4, $t5
	vclipw.xyz	vf11, vf11w
	vnop
	vnop
	vnop
	vnop
	vnop
	cfc2		$t5, $vi18
	andi		$t5, 0x3F

#Load vertex Normal or Color
	#lv.s		vf9x, 0($v1)				// load normal word [w,z,y,x]
	andi		$t4, $v0, TNL_LIGHT			// if( TNL_LIGHT )
	beqz		$t4, do_color_PD
	sw			$t5, 0x38($a3)				// Store ClipFlags
	
#Do lighting Convert the alpha in vf9 to float and pass it along to light color
	#.word		0xd0380000 | (8<<8) | (43)	// vuc2i.s	vf12, vf9x					// vf9 = [?,z,y,x]
	#vi2f.s		vf17w, vf12x, 31				// int -> float, vf17 = [a * 1/256]
	lw			$t3, 0($v1)
	pextlb		$t3, $t3, $t3
	pextlh		$t3, $t3, $t3			// [a,z,y,x]
	psrlw		$t3, $t3, 1
	qmtc2		$t3, vf17
	vitof0.x	vf22, vf17
	vmr32.w		vf17, vf22
	lui			$t3, 0x3000
	qmtc2		$t3, vf22
	vmulx.w		vf17, vf17, vf22x
	
# Convert the normal in vf9 to float and transform
	#.word		0xd0398080 | (8<<8) | (40)	// vc2i.s		vf9, vf9x					// vf9 = [?,z,y,x]
	#vi2f.q		vf10, vf9[w,z,y,x], 0		// int -> float, Unfiddle (obliterates world transform)
	lw			$t3, 0($v1)
	pextlb		$t3, $t3, $zero
	pextlh		$t3, $t3, $zero			// [a,z,y,x]
	qmtc2		$t3, vf9
	vitof0.xyzw	vf10, vf9
	vmr32.xyzw	vf10, vf10				// [z,y,x,a]
	vmr32.xyzw	vf22, vf10				// [y,x,a,z]
	vmr32.xz	vf10, vf22				// [x,y,z,a]
	#vtfm3.t	vf9, M000, vf10			// Transform with world matrix (only need 3x3)//Corn
	vmulax.xyz  ACC, vf1, vf10
    vmadday.xyz ACC, vf2, vf10
    vmaddz.xyz  vf9, vf3, vf10
	#vdot.t		vf10x, vf9, vf9			// vf10x = x*x + y*y + z*z
	vmul.xyz	vf10, vf9, vf9
	vaddy.x		vf10, vf10, vf10y
	vaddz.x		vf10, vf10, vf10z
	#vrsq.s		vf10x, vf10x					// vf10x = 1/sqrt(x*x + y*y + z*z)
	vrsqrt		Q, vf0w, vf10x
	vwaitq
	#vscl.t		vf9, vf9, vf10x			// vf9x = v.normalise().
	vmulq.xyz   vf9, vf9, Q

	#vmov.q		vf10, vf18					// Colour = ambient
	vmove.xyzw	vf10, vf18
	beq			$t6, $t7, done_lighting_PD	// cur_light == last_light?
	or			$t4, $t6, $0				// cur_light = p_lights

next_light_PD:
	lqc2		vf14, LIGHTDIR($t4)			// Load Light normal
	#vdot.t		vf16x[0:1], vf9, vf14		// x = clamp(dot(normal,(x,y,z)),0,1)
	vmul.xyz	vf16, vf9, vf14
	vaddy.x		vf16, vf16, vf16y
	vaddz.x		vf16, vf16, vf16z
	vminiw.x	vf16, vf16, vf0w
	vmaxx.x		vf16, vf16, vf0x
	lqc2		vf15, LIGHTCOL($t4)			// Load Light colour
	addiu		$t4, $t4, (1<<LIGHTSZ)		// Skip to the next light
	#vscl.t		vf16, vf15, vf16x			// r,g,b = r*x, g*x, b*x
	vmulx.xyz	vf16, vf15, vf16x
	bne			$t4, $t7, next_light_PD
	#vadd.t		vf10, vf10, vf16			// col += r,g,b,a
	vadd.xyz	vf10, vf10, vf16

done_lighting_PD:
	#vmov.t		vf17[0:1,0:1,0:1], vf10		// Clamp 0..1 and merge with vertex alpha in vf17w
	vmove.xyz	vf17, vf10
	vminiw.xyz	vf17, vf17, vf0w
	vmaxx.xyz	vf17, vf17, vf0x

	andi		$t4, $v0, TNL_TEXGEN		// if( TNL_TEXGEN )
	beqz		$t4, do_texture_PD
	sqc2		vf17, 0x20($a3)				// Store colour

	andi		$t4, $v0, TNL_TEXGENLIN		// if( TNL_TEXGENLIN )
	beqz		$t4, do_texgen_PD
	nop
	
# EnvMapped G_TEXTURE_GEN_LINEAR  Cheap way to do acos(x)/PI -> 0.5f - 0.25f * x - 0.25f * x * x * x //Corn
	#vmul.p		vf11z, vf11[1/4,1/4], vf9	// X * 0.25, Y * 0.25
	lui			$t3, 0x3e80				// 0.25
	qmtc2		$t3, vf22
	vmulx.xy	vf11, vf9, vf22x
	lui			$t3, 0x3f00				// 0.5
	pextlw		$t3, $t3, $t3
	qmtc2		$t3, vf22
	#vsub.p		vf12, vf11[1/2,1/2], vf11z	// result = 0.5 - X * 0.25
	vsub.xy		vf12, vf22, vf11
	#vmul.p		vf11z, vf9, vf11z			// X * X * 0.25, Y * Y * 0.25
	vmul.xy		vf11, vf9, vf11
	#vmul.p		vf11z, vf9, vf11z			// X * X * X * 0.25, Y * Y * Y * 0.25
	vmul.xy		vf11, vf9, vf11
	#vsub.p		vf12, vf12, vf11z			// result -= X * X * X * 0.25
	vsub.xy		vf12, vf12, vf11
	qmfc2		$t3, vf12
	#sv.s		vf12x, 0x30($a3)				// Store Texture.x
	sw			$t3, 0x30($a3)
	dsrl32		$t3, $t3, 0
	b			vtx_done_PD
	#sv.s		vf12y, 0x34($a3)				// Store Texture.y
	sw			$t3, 0x34($a3)
		
do_texgen_PD:
# EnvMapped G_TEXTURE_GEN  t.x = 0.5 * (1.0 + n.x) t.y = 0.5 * (1.0 + n.y)
	#vadd.p		vf11, vf11[1,1], vf9		// 1+x, 1+y
	vaddw.xy	vf11, vf9, vf0w
	#vmul.p		vf11, vf11[1/2,1/2], vf11	// X * 0.5, Y * 0.25
	lui			$t3, 0x3f00				// 0.5
	qmtc2		$t3, vf22
	vmulx.xy	vf11, vf11, vf22x
	qmfc2		$t3, vf11
	#sv.s		vf11x, 0x30($a3)				// Store Texture.x
	sw			$t3, 0x30($a3)
	b			vtx_done_PD
	#sv.s		vf11y, 0x34($a3)				// Store Texture.y
	sw			$t3, 0x34($a3)
		
do_color_PD:
# Normalise the RGBA colour
	#.word		0xd0380000 | (8<<8) | (40)	// vuc2i.s	vf9, vf9x					// vf9 = [a,b,g,r]
	lw			$t3, 0($v1)
	pextlb		$t3, $t3, $t3
	pextlh		$t3, $t3, $t3			// [a,b,g,r]
	psrlw		$t3, $t3, 1
	qmtc2		$t3, vf9
	vmr32.xyzw	vf22, vf9				// [b,g,r,a]
	vaddz.x		vf22, vf0, vf22z		// [r,g,r,a]
	vaddy.z		vf22, vf0, vf9y			// [r,g,b,a]
	#vi2f.q		vf9, vf9[w,z,y,x], 31		// int -> float, vf9 = [r * 1/256, g * 1/256, b * 1/256, a * 1/256]
	vitof0.xyzw	vf9, vf22
	lui			$t3, 0x3000
	qmtc2		$t3, vf22
	vmulx.xyzw	vf9, vf9, vf22x
	sqc2		vf9, 0x20($a3)				// Store colour

do_texture_PD:
# Textured t.x = (float)v.tu * mTextureScale.x t.y = (float)v.tv * mTextureScale.y
	#lv.s		vf11x, 8($a2)				// load texture word [tv,tu] (N.B. due to swizzling these are 'backwards' from what you might expect)
	#vs2i.s		vf11, vf11x
	lw			$t3, 8($a2)
	pextlh		$t3, $t3, $zero
	qmtc2		$t3, vf11
	#vi2f.p		vf11, vf11, 16				// int -> float
	vitof15.xy	vf11, vf11
	lui			$t3, 0x3f00				// 0.5
	qmtc2		$t3, vf22
	vmulx.xy	vf11, vf11, vf22x
	#vmul.p		vf11, vf11, vf19zw			// multiply by mTextureScale
	vmulz.y		vf11, vf11, vf19z
	vmulw.x		vf11, vf11, vf19w
	#sv.s		vf11y, 0x30($a3)				// Store Texture.x
	#sv.s		vf11x, 0x34($a3)				// Store Texture.y
	qmfc2		$t3, vf11
	sw			$t3, 0x34($a3)
	dsrl32		$t3, $t3, 0
	sw			$t3, 0x30($a3)

vtx_done_PD:
# Continue with the next vertex
	addiu		$a2, $a2, 12				// Next input vertex
	bne			$a2, $t0, next_vertex_PD
	addiu		$a3, $a3, 64				// Next output vertex

finished_PD:	
	jr			$ra
	nop
	

############################
.global _TnLVU0DKR
############################
#	a0 - num vertices
#	a1 - world*projection matrix	- must be aligned to 16 bytes
#	a2 - Fiddled vertices			- stride 16
#	a3 - Output vertices			- must be aligned to 16 bytes, stride 64

_TnLVU0DKR:
	lqc2		vf5, 0($a1)				// Load mat worldproject
	lqc2		vf6, 16($a1)
	lqc2		vf7, 32($a1)
	lqc2		vf8, 48($a1)
	
	sll			$v0, $a0, 1					// count * 2
	sll			$a0, $a0, 3					// count * 8
	addu		$a0, $v0, $a0				// count = count * 10
	addu		$a0, $a2, $a0				// end_ptr = start_ptr + count * 10
	beq			$a2, $a0, finished_DKR
	#vone.s		vf12w						// w = 1.0f
	vmove.w		vf12, vf0
	
next_vertex_DKR:
# Load and transform this vertex position
 	xori		$v0, $a2, 0x2				// = vtx_ptr ^ 2
	lh			$t4, 0($v0)					// get vertex x coord	
	#mtv		$t4, vf12x					// store on VU0
	addiu		$a2, $a2, 0x2				// = vtx_ptr + 2
 	xori		$v0, $a2, 0x2				// = vtx_ptr ^ 2
	lh			$t5, 0($v0)					// get vertex y coord
	#mtv		$t4, vf12y					// store on VU0
	addiu		$a2, $a2, 0x2				// = vtx_ptr + 2
 	xori		$v0, $a2, 0x2				// = vtx_ptr ^ 2
	lh			$t6, 0($v0)					// get vertex z coord
	pextlw		$t4, $t6, $t4
	#mtv		$t4, vf12z					// store on VU0
	addiu		$a2, $a2, 0x2				// = vtx_ptr + 2
	pextlw		$t4, $t5, $t4
	qmtc2		$t4, vf12
	vmove.w		vf12, vf0

	#vi2f.t		vf12, vf12, 0				// int -> float (x,y,z,1)
	vitof0.xyz	vf12, vf12
	#vtfm4.q	vf11, M100, vf12			// Projection transform
	vmulax.xyzw     ACC, vf5, vf12
    vmadday.xyzw    ACC, vf6, vf12
    vmaddaz.xyzw    ACC, vf7, vf12
    vmaddw.xyzw     vf11, vf8, vf12

	sqc2		vf12, 0x00($a3)				// Store world transform
	sqc2		vf11, 0x10($a3)				// Store world+projection transform
	
# Compute the clip flags
	#vcmp.q		LT, vf11, vf11[-w,-w,-w,0]	// x < -w, y < -w, z < -w
	#vnop
	#mfvc		$t4, $131					// VU0_CC. Corresponds to X_NEG/Y_NEG/Z_NEG (will become X_POS/Y_POS/Z_POS later)
	#vcmp.q		GT, vf11, vf11[w,w,w,0]		// x > w, y > w, z > w
	#andi		$t4, $t4, 0x7				// Mask out the condition codes we dont care about
	#sll			$t4, $t4, 3					// Shift up to create X_POS/Y_POS/Z_POS	
	#mfvc		$t5, $131					// VU0_CC. Corresponds to X_NEG/Y_NEG/Z_NEG
	#andi		$t5, $t5, 0x7				// Mask out the condition codes we dont care about
	#or			$t4, $t4, $t5
	vclipw.xyz	vf11, vf11w
	vnop
	vnop
	vnop
	vnop
	vnop
	cfc2		$t4, $vi18
	andi		$t4, 0x3F
	sw			$t4, 0x38($a3)				// Store ClipFlags

# Normalise the RGBA colour
 	xori		$v0, $a2, 0x2				// = vtx_ptr ^ 2
	lhu			$t4, 0($v0)					// get vertex color	(hi)
	addiu		$a2, $a2, 0x2				// = vtx_ptr + 2
 	xori		$v0, $a2, 0x2				// = vtx_ptr ^ 2
	lhu			$t5, 0($v0)					// get vertex color (lo)
	sll			$t4, $t4, 16				// pack
 	or		    $t5, $t5, $t4				// to 32bit
	addiu		$a2, $a2, 0x2				// = vtx_ptr + 2
	#mtv		$t5, vf9x					// store on VU0

	#.word		0xd0380000 | (8<<8) | (40)	// vuc2i.s	vf9, vf9x					// vf9 = [a,b,g,r]
	pextlb		$t5, $t5, $t5
	pextlh		$t5, $t5, $t5			// [a,b,g,r]
	psrlw		$t5, $t5, 1
	qmtc2		$t5, vf9
	vmr32.xyzw	vf22, vf9				// [b,g,r,a]
	vaddz.x		vf22, vf0, vf22z		// [r,g,r,a]
	vaddy.z		vf22, vf0, vf9y			// [r,g,b,a]
	#vi2f.q		vf9, vf9[w,z,y,x], 31		// int -> float, vf9 = [r * 1/256, g * 1/256, b * 1/256, a * 1/256]
	vitof0.xyzw	vf9, vf22
	lui			$t5, 0x3000
	qmtc2		$t5, vf22
	vmulx.xyzw	vf9, vf9, vf22x
	sqc2		vf9, 0x20($a3)				// Store colour

# Continue with the next vertex
	bne			$a2, $a0, next_vertex_DKR
	addiu		$a3, $a3, 64				// Next output vertex

finished_DKR:	
	jr			$ra
	nop

############################
.global _TnLVU0DKRB
############################
#	a0 - num vertices
#	a1 - world*projection matrix	- must be aligned to 16 bytes
#	a2 - Fiddled vertices			- stride 16
#	a3 - Output vertices			- must be aligned to 16 bytes, stride 64

_TnLVU0DKRB:
	lqc2		vf5, 0($a1)				// Load mat worldproject (matrix[0] and only 3x3 is needed)
	lqc2		vf6, 16($a1)
	lqc2		vf7, 32($a1)

	#lv.s		vf1x, 128($a1)				// Load element 0 in matrix[2]
	lw			$t2, 128($a1)
	#lv.s		vf1z, 168($a1)				// Load element 10 in matrix[2]
	lw			$t3, 168($a1)
	qmtc2		$t2, vf22
	vmove.x		vf1, vf22
	qmtc2		$t3, vf22
	vaddx.z		vf1, vf0, vf22x

	#vmov.s		vf1y, vf1x					// Copy element 0 (instead of 5)
	vaddx.y		vf1, vf0, vf1x
	#vfim.s		vf2y, 0.375					// Y scale factor
	lui			$t3, 0x3ec0				// 0.375
	qmtc2		$t3, vf22
	vaddx.y		vf2, vf0, vf22x
	#vmul.t		vf1, vf1, vf2[1/2,y,1/2]	// Prepare X and Z scaling values  
	lui			$t3, 0x3f00				// 0.5
	qmtc2		$t3, vf22
	vmul.y		vf1, vf1, vf2
	vmulx.xz	vf1, vf1, vf22x
	#vscl.t		C100, C100, vf1x			// Scale X matrix colum 
	vmul.x		vf5, vf5, vf1
	vmul.x		vf6, vf6, vf1
	vmul.x		vf7, vf7, vf1
	#vscl.t		C110, C110, vf1y			// Scale Y matrix colum 
	vmul.y		vf5, vf5, vf1
	vmul.y		vf6, vf6, vf1
	vmul.y		vf7, vf7, vf1
	#vscl.t		C120, C120, vf1z			// Scale Z matrix colum 
	vmul.z		vf5, vf5, vf1
	vmul.z		vf6, vf6, vf1
	vmul.z		vf7, vf7, vf1
	lqc2		vf4, -64($a3)				// Get base vector to add to the billbord geometry (in position 0)

	sll			$v0, $a0, 1					// count * 2
	sll			$a0, $a0, 3					// count * 8
	addu		$a0, $v0, $a0				// count = count * 10
	addu		$a0, $a2, $a0				// end_ptr = start_ptr + count * 10
	beq			$a2, $a0, finished_DKRB
	#vone.s		vf12w						// w = 1.0f
	vmove.w		vf12, vf0
	
next_vertex_DKRB:
# Load and transform this vertex position
 	xori		$v0, $a2, 0x2				// = vtx_ptr ^ 2
	lh			$t4, 0($v0)					// get vertex x coord	
	#mtv		$t4, vf12x					// store on VU0
	addiu		$a2, $a2, 0x2				// = vtx_ptr + 2
 	xori		$v0, $a2, 0x2				// = vtx_ptr ^ 2
	lh			$t5, 0($v0)					// get vertex y coord
	#mtv		$t4, vf12y					// store on VU0
	addiu		$a2, $a2, 0x2				// = vtx_ptr + 2
 	xori		$v0, $a2, 0x2				// = vtx_ptr ^ 2
	lh			$t6, 0($v0)					// get vertex z coord
	#mtv		$t4, vf12z					// store on VU0
	addiu		$a2, $a2, 0x2				// = vtx_ptr + 2
	pextlw		$t4, $t6, $t4
	pextlw		$t4, $t5, $t4
	qmtc2		$t4, vf12
	vmove.w		vf12, vf0
	#vi2f.t		vf12, vf12, 0				// int -> float (x,y,z,1)
	vitof0.xyz	vf12, vf12
	#vtfm3.t	vf11, M100, vf12			// 3x3 transform
	vmulax.xyz    ACC, vf5, vf12
    vmadday.xyz   ACC, vf6, vf12
    vmaddz.xyz    vf11, vf7, vf12

# Clip flags
	sw			$zero, 0x38($a3)			// Clear ClipFlags

	#vadd.t		vf12, vf11, vf4			// Add basevector
	vadd.xyz	vf12, vf11, vf4
	sqc2		vf12, 0x00($a3)				// Store world transform (x,y,z,1)
	

# Normalise the RGBA colour
 	xori		$v0, $a2, 0x2				// = vtx_ptr ^ 2
	lhu			$t4, 0($v0)					// get vertex color	(hi)
	addiu		$a2, $a2, 0x2				// = vtx_ptr + 2
 	xori		$v0, $a2, 0x2				// = vtx_ptr ^ 2
	lhu			$t5, 0($v0)					// get vertex color (lo)
	sll			$t4, $t4, 16				// pack
 	or		    $t5, $t5, $t4				// to 32bit
	addiu		$a2, $a2, 0x2				// = vtx_ptr + 2
	#mtv		$t5, vf9x					// store on VU0

	#.word		0xd0380000 | (8<<8) | (40)	// vuc2i.s	vf9, vf9x					// vf9 = [a,b,g,r]
	pextlb		$t5, $t5, $t5
	pextlh		$t5, $t5, $t5			// [a,b,g,r]
	psrlw		$t5, $t5, 1
	qmtc2		$t5, vf9
	vmr32.xyzw	vf22, vf9				// [b,g,r,a]
	vaddz.x		vf22, vf0, vf22z		// [r,g,r,a]
	vaddy.z		vf22, vf0, vf9y			// [r,g,b,a]
	#vi2f.q		vf9, vf9[w,z,y,x], 31		// int -> float, vf9 = [r * 1/256, g * 1/256, b * 1/256, a * 1/256]
	vitof0.xyzw	vf9, vf22
	lui			$t5, 0x3000
	qmtc2		$t5, vf22
	vmulx.xyzw	vf9, vf9, vf22x
	sqc2		vf9, 0x20($a3)				// Store colour

# Continue with the next vertex
	bne			$a2, $a0, next_vertex_DKRB
	addiu		$a3, $a3, 64				// Next output vertex

finished_DKRB:	
	jr			$ra
	nop
	
.set pop

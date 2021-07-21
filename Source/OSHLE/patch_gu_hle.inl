#define TEST_DISABLE_GU_FUNCS DAEDALUS_PROFILE(__FUNCTION__);

#ifndef DAEDALUS_PSP_USE_VFPU
//Fixed point matrix
static const u32 s_IdentMatrixL[16] =
{
	0x00010000,	0x00000000,
	0x00000001,	0x00000000,
	0x00000000,	0x00010000,
	0x00000000,	0x00000001,
	0x00000000, 0x00000000,
	0x00000000,	0x00000000,
	0x00000000, 0x00000000,
	0x00000000,	0x00000000
};
#endif


#ifdef DAEDALUS_PSP_USE_VFPU
//Note. We use unaligned store from VFPU but this is known to corrupt floating point registers on the PHAT
//and potentially cause odd behaviour or even crash the PSP (best is not to use HLE on PHAT)

//Use VFPU to save a IDENTITY matrix //Corn
inline void vfpu_matrix_IdentF(u8 *m) {
	__asm__ volatile (
		"vmidt.q M000\n"						// set M100 to identity
		"usv.q    C000, 0  + %0\n"
		"usv.q    C010, 16 + %0\n"
		"usv.q    C020, 32 + %0\n"
		"usv.q    C030, 48 + %0\n"
	:"=m"(*m));
}

//Use VFPU to save a TRANSLATE_F matrix //Corn
inline void vfpu_matrix_TranslateF(u8 *m, float X, float Y, float Z) {
	__asm__ volatile (
		"vmidt.q M000\n"						// set M100 to identity
		"mtv     %1, S030\n"
		"mtv     %2, S031\n"
		"mtv     %3, S032\n"
		"usv.q    C000, 0  + %0\n"
		"usv.q    C010, 16 + %0\n"
		"usv.q    C020, 32 + %0\n"
		"usv.q    C030, 48 + %0\n"
	:"=m"(*m) : "r"(X), "r"(Y), "r"(Z));
}

//Use VFPU to save a SCALE_F matrix //Corn
inline void vfpu_matrix_ScaleF(u8 *m, float X, float Y, float Z) {
	__asm__ volatile (
		"vmidt.q M000\n"						// set M100 to identity
		"mtv     %1, S000\n"
		"mtv     %2, S011\n"
		"mtv     %3, S022\n"
		"usv.q    C000, 0  + %0\n"
		"usv.q    C010, 16 + %0\n"
		"usv.q    C020, 32 + %0\n"
		"usv.q    C030, 48 + %0\n"
	:"=m"(*m) : "r"(X), "r"(Y), "r"(Z));
}

//Taken from Mr.Mr libpspmath and added scale to the EQ. (Scale usually is 1.0f tho) //Corn
inline void vfpu_matrix_OrthoF(u8 *m, float left, float right, float bottom, float top, float near, float far, float scale)
{
	__asm__ volatile (
		"vmidt.q M100\n"						// set M100 to identity
		"mtv     %2, S000\n"					// C000 = [right, ?,      ?,  ]
		"mtv     %4, S001\n"					// C000 = [right, top,    ?,  ]
		"mtv     %6, S002\n"					// C000 = [right, top,    far ]
		"mtv     %1, S010\n"					// C010 = [left,  ?,      ?,  ]
		"mtv     %3, S011\n"					// C010 = [left,  bottom, ?,  ]
		"mtv     %5, S012\n"                	// C010 = [left,  bottom, near]
		"mtv     %7, S133\n"                	// C110 = [0, 0, 0, scale]
		"vsub.t  C020, C000, C010\n"			// C020 = [  dx,   dy,   dz]
		"vrcp.t  C020, C020\n"              	// C020 = [1/dx, 1/dy, 1/dz]
		"vscl.t	 C020, C020, S133\n"			// C020 = [scale/dx, scale/dy, scale/dz]
		"vmul.s  S100, S100[2], S020\n"     	// S100 = m->x.x = 2.0 / dx
		"vmul.s  S111, S111[2], S021\n"     	// S110 = m->y.y = 2.0 / dy
		"vmul.s  S122, S122[2], S022[-x]\n"		// S122 = m->z.z = -2.0 / dz
		"vsub.t  C130, C000[-x,-y,-z], C010\n"	// C130 = m->w[x, y, z] = [-(right+left), -(top+bottom), -(far+near)]
												// we do vsub here since -(a+b) => (-1*a) + (-1*b) => -a - b
		"vmul.t  C130, C130, C020\n"			// C130 = [-(right+left)/dx, -(top+bottom)/dy, -(far+near)/dz]
		"usv.q    C100, 0  + %0\n"
		"usv.q    C110, 16 + %0\n"
		"usv.q    C120, 32 + %0\n"
		"usv.q    C130, 48 + %0\n"
	:"=m"(*m) : "r"(left), "r"(right), "r"(bottom), "r"(top), "r"(near), "r"(far), "r"(scale));
}

//Taken from Mr.Mr libpspmath and added scale and output to fixed point //Corn
inline void vfpu_matrix_Ortho(u8 *m, float left, float right, float bottom, float top, float near, float far, float scale)
{
	__asm__ volatile (
		"vmidt.q M100\n"						// set M100 to identity
		"mtv     %2, S000\n"					// C000 = [right, ?,      ?,  ]
		"mtv     %4, S001\n"					// C000 = [right, top,    ?,  ]
		"mtv     %6, S002\n"					// C000 = [right, top,    far ]
		"mtv     %1, S010\n"					// C010 = [left,  ?,      ?,  ]
		"mtv     %3, S011\n"					// C010 = [left,  bottom, ?,  ]
		"mtv     %5, S012\n"                	// C010 = [left,  bottom, near]
		"mtv     %7, S133\n"                	// C110 = [0, 0, 0, scale]
		"vsub.t  C020, C000, C010\n"			// C020 = [  dx,   dy,   dz]
		"vrcp.t  C020, C020\n"              	// C020 = [1/dx, 1/dy, 1/dz]
		"vscl.t	 C020, C020, S133\n"			// C020 = [scale/dx, scale/dy, scale/dz]
		"vmul.s  S100, S100[2], S020\n"     	// S100 = m->x.x = 2.0 / dx
		"vmul.s  S111, S111[2], S021\n"     	// S110 = m->y.y = 2.0 / dy
		"vmul.s  S122, S122[2], S022[-x]\n"		// S122 = m->z.z = -2.0 / dz
		"vsub.t  C130, C000[-x,-y,-z], C010\n"	// C130 = m->w[x, y, z] = [-(right+left), -(top+bottom), -(far+near)]
												// we do vsub here since -(a+b) => (-1*a) + (-1*b) => -a - b
		"vmul.t  C130, C130, C020\n"			// C130 = [-(right+left)/dx, -(top+bottom)/dy, -(far+near)/dz]
		"vf2iz.q  C100, C100, 16\n"			// scale values to fixed point
		"usv.q    C100, 0  + %0\n"
		"vf2iz.q  C110, C110, 16\n"			// scale values to fixed point
		"usv.q    C110, 16 + %0\n"
		"vf2iz.q  C120, C120, 16\n"			// scale values to fixed point
		"usv.q    C120, 32 + %0\n"
		"vf2iz.q  C130, C130, 16\n"			// scale values to fixed point
		"usv.q    C130, 48 + %0\n"
	:"=m"(*m) : "r"(left), "r"(right), "r"(bottom), "r"(top), "r"(near), "r"(far), "r"(scale));
}
#elif defined( DAEDALUS_PS2_USE_VU0 )

#if __GNUC__ > 3
#define VUR "$"
#else
#define VUR ""
#endif

inline void vu0_matrix_IdentF(u8* m) {
	__asm__ volatile (
		/* 1 0 0 0 */
		/* 0 1 0 0 */
		/* 0 0 1 0 */
		/* 0 0 0 1 */

		"vmr32.xyzw		" VUR "vf3, " VUR "vf0			\n"
		"vmr32.xyzw		" VUR "vf2, " VUR "vf3			\n" /* 0 1 0 0 */
		"vmr32.xyzw		" VUR "vf1, " VUR "vf2			\n" /* 1 0 0 0 */
		"qmfc2			$t0, " VUR "vf1			\n"
		"sd				$t0, 0 + %0			\n"
		"qmfc2			$t1, " VUR "vf2			\n"
		"sd				$zero, 8 + %0		\n"
		"sd				$t1, 16 + %0		\n"
		"sd				$zero, 24 + %0		\n"
		"sd				$zero, 32 + %0		\n"
		"sd				$t0, 40 + %0		\n"
		"sd				$zero, 48 + %0		\n"
		"sd				$t1, 56 + %0		\n"
		:"=m"(*m));
}

inline void vu0_matrix_TranslateF(u8* m, float X, float Y, float Z) {
	__asm__ volatile (
		/* 1 0 0 0 */
		/* 0 1 0 0 */
		/* 0 0 1 0 */
		/* x y z 1 */
		
		"vmr32.xyzw		" VUR "vf3, " VUR "vf0			\n"
		"vmr32.xyzw		" VUR "vf2, " VUR "vf3			\n" /* 0 1 0 0 */
		"vmr32.xyzw		" VUR "vf1, " VUR "vf2			\n" /* 1 0 0 0 */
		"qmfc2			$t0, " VUR "vf1			\n"
		"sd				$t0, 0 + %0			\n"
		"qmfc2			$t1, " VUR "vf2			\n"
		"sd				$zero, 8 + %0		\n"
		"sd				$t1, 16 + %0		\n"
		"sd				$zero, 24 + %0		\n"
		"sd				$zero, 32 + %0		\n"
		"sd				$t0, 40 + %0		\n"
		"sw				%1, 48 + %0			\n"
		"sw				%2, 52 + %0			\n"
		"sw				%3, 56 + %0			\n"
		"sw				$t0, 60 + %0		\n"
		:"=m"(*m) : "r"(X), "r"(Y), "r"(Z));
}

inline void vu0_matrix_ScaleF(u8* m, float X, float Y, float Z) {
	__asm__ volatile (
		/* x 0 0 0 */
		/* 0 y 0 0 */
		/* 0 0 z 0 */
		/* 0 0 0 1 */

		"sw				%1, 0 + %0			\n"
		"sw				$zero, 4 + %0		\n"
		"sd				$zero, 8 + %0		\n"
		"sw				$zero, 16 + %0		\n"
		"sw				%2, 20 + %0			\n"
		"sd				$zero, 24 + %0		\n"
		"sd				$zero, 32 + %0		\n"
		"sw				%3, 40 + %0			\n"
		"sw				$zero, 44 + %0		\n"
		"qmfc2			$t0, " VUR "vf0			\n"
		"sd				$zero, 48 + %0		\n"
		"pcpyud			$t0, $t0, $t0		\n"
		"sd				$t0, 56 + %0		\n"
		:"=m"(*m) : "r"(X), "r"(Y), "r"(Z));
}

inline void vu0_matrix_OrthoF(u8* m, float left, float right, float bottom, float top, float near, float far, float scale)
{
	__asm__ volatile (

		"vmove.xyzw		" VUR "vf4, " VUR "vf0			\n" /* 0 0 0 1 */
		"vmr32.xyzw		" VUR "vf3, " VUR "vf0			\n" /* 0 0 1 0 */
		"vmr32.xyzw		" VUR "vf2, " VUR "vf3			\n" /* 0 1 0 0 */
		"vmr32.xyzw		" VUR "vf1, " VUR "vf2			\n" /* 1 0 0 0 */

		"pextlw			%2, %6, %2			\n"
		"pextlw			%2, %4, %2			\n"
		"qmtc2			%2, " VUR "vf5				\n" // [right, top,    far ]
		"pextlw			%1, %5, %1			\n"
		"pextlw			%1, %3, %1			\n"
		"qmtc2			%1, " VUR "vf6				\n" // [left,  bottom, near]
		"dsll32			%7, %7, 0			\n"
		"pcpyld			%7, %7, $zero		\n"
		"qmtc2			%7, " VUR "vf4				\n" // [0, 0, 0, scale]
		"vsub.xyz		" VUR "vf7, " VUR "vf5, " VUR "vf6		\n"	// [  dx,   dy,   dz]
		"vdiv			" VUR "Q, " VUR "vf4w, " VUR "vf7x		\n"
		"vwaitq								\n"
		"vaddq.x		" VUR "vf7, " VUR "vf0, " VUR "Q			\n"
		"vdiv			" VUR "Q, " VUR "vf4w, " VUR "vf7y		\n"
		"vwaitq								\n"
		"vaddq.y		" VUR "vf7, " VUR "vf0, " VUR "Q			\n"
		"vdiv			" VUR "Q, " VUR "vf4w, " VUR "vf7z		\n"
		"vwaitq								\n"
		"vaddq.z		" VUR "vf7, " VUR "vf0, " VUR "Q			\n" // [scale / dx, scale / dy, scale / dz]
		"vadd.w			" VUR "vf8, " VUR "vf0, " VUR "vf0		\n" // 2.0
		"vmulw.x		" VUR "vf1, " VUR "vf7, " VUR "vf8w		\n" // m->x.x = 2.0 / dx
		"vmulw.y		" VUR "vf2, " VUR "vf7, " VUR "vf8w		\n" // m->y.y = 2.0 / dy
		"vmulw.z		" VUR "vf3, " VUR "vf7, " VUR "vf8w		\n" // m->z.z = -2.0 / dz
		"vsub.z			" VUR "vf3, " VUR "vf0, " VUR "vf3		\n"
		"vadd.xyz		" VUR "vf4, " VUR "vf5, " VUR "vf6		\n"
		"vsub.xyz		" VUR "vf4, " VUR "vf0, " VUR "vf4		\n" // m->w[x, y, z] = [-(right+left), -(top+bottom), -(far+near)]
		"vmul.xyz		" VUR "vf4, " VUR "vf4, " VUR "vf7		\n"	// [-(right+left)/dx, -(top+bottom)/dy, -(far+near)/dz]
		"qmfc2			%1, " VUR "vf1				\n"
		"qmfc2			%2, " VUR "vf2				\n"
		"qmfc2			%3, " VUR "vf3				\n"
		"qmfc2			%4, " VUR "vf4				\n"
		"sd				%1, 0 + %0			\n"
		"pcpyud			%1, %1, %1			\n"
		"sd				%1, 8 + %0			\n"
		"sd				%2, 16 + %0			\n"
		"pcpyud			%2, %2, %2			\n"
		"sd				%2, 24 + %0			\n"
		"sd				%3, 32 + %0			\n"
		"pcpyud			%3, %3, %3			\n"
		"sd				%3, 40 + %0			\n"
		"sd				%4, 48 + %0			\n"
		"pcpyud			%4, %4, %4			\n"
		"sd				%4, 56 + %0			\n"
		:"=m"(*m) : "r"(left), "r"(right), "r"(bottom), "r"(top), "r"(near), "r"(far), "r"(scale));
}

inline void vu0_matrix_Ortho(u8* m, float left, float right, float bottom, float top, float near, float far, float scale)
{
	__asm__ volatile (
		"vmove.xyzw		" VUR "vf4, " VUR "vf0			\n" /* 0 0 0 1 */
		"vmr32.xyzw		" VUR "vf3, " VUR "vf0			\n" /* 0 0 1 0 */
		"vmr32.xyzw		" VUR "vf2, " VUR "vf3			\n" /* 0 1 0 0 */
		"vmr32.xyzw		" VUR "vf1, " VUR "vf2			\n" /* 1 0 0 0 */

		"pextlw			%2, %6, %2			\n"
		"pextlw			%2, %4, %2			\n"
		"qmtc2			%2, " VUR "vf5				\n" // [right, top,    far ]
		"pextlw			%1, %5, %1			\n"
		"pextlw			%1, %3, %1			\n"
		"qmtc2			%1, " VUR "vf6				\n" // [left,  bottom, near]
		"dsll32			%7, %7, 0			\n"
		"pcpyld			%7, %7, $zero		\n"
		"qmtc2			%7, " VUR "vf4				\n" // [0, 0, 0, scale]
		"vsub.xyz		" VUR "vf7, " VUR "vf5, " VUR "vf6		\n"	// [  dx,   dy,   dz]
		"vdiv			" VUR "Q, " VUR "vf4w, " VUR "vf7x		\n"
		"vwaitq								\n"
		"vaddq.x		" VUR "vf7, " VUR "vf0, " VUR "Q			\n"
		"vdiv			" VUR "Q, " VUR "vf4w, " VUR "vf7y		\n"
		"vwaitq								\n"
		"vaddq.y		" VUR "vf7, " VUR "vf0, " VUR "Q			\n"
		"vdiv			" VUR "Q, " VUR "vf4w, " VUR "vf7z		\n"
		"vwaitq								\n"
		"vaddq.z		" VUR "vf7, " VUR "vf0, " VUR "Q			\n" // [scale / dx, scale / dy, scale / dz]
		"vadd.w			" VUR "vf8, " VUR "vf0, " VUR "vf0		\n" // 2.0
		"vmulw.x		" VUR "vf1, " VUR "vf7, " VUR "vf8w		\n" // m->x.x = 2.0 / dx
		"vmulw.y		" VUR "vf2, " VUR "vf7, " VUR "vf8w		\n" // m->y.y = 2.0 / dy
		"vmulw.z		" VUR "vf3, " VUR "vf7, " VUR "vf8w		\n" // m->z.z = -2.0 / dz
		"vsub.z			" VUR "vf3, " VUR "vf0, " VUR "vf3		\n"
		"vadd.xyz		" VUR "vf4, " VUR "vf5, " VUR "vf6		\n"
		"vsub.xyz		" VUR "vf4, " VUR "vf0, " VUR "vf4		\n" // m->w[x, y, z] = [-(right+left), -(top+bottom), -(far+near)]
		"vmul.xyz		" VUR "vf4, " VUR "vf4, " VUR "vf7		\n"	// [-(right+left)/dx, -(top+bottom)/dy, -(far+near)/dz]

		"vadd.w			" VUR "vf9, " VUR "vf0, " VUR "vf0		\n"
		"vmulw.xyzw		" VUR "vf1, " VUR "vf1, " VUR "vf9w		\n"
		"vftoi15.xyzw	" VUR "vf1, " VUR "vf1 \n"
		"qmfc2			%1, $vf1				\n"
		"sd				%1, 0 + %0			\n"
		"pcpyud			%1, %1, %1			\n"
		"sd				%1, 8 + %0			\n"

		"vmulw.xyzw		" VUR "vf2, " VUR "vf2, " VUR "vf9w		\n"
		"vftoi15.xyzw	" VUR "vf2, " VUR "vf2			\n"
		"qmfc2			%2, $vf2				\n"
		"sd				%2, 16 + %0			\n"
		"pcpyud			%2, %2, %2			\n"
		"sd				%2, 24 + %0			\n"

		"vmulw.xyzw		" VUR "vf3, " VUR "vf3, " VUR "vf9w		\n"
		"vftoi15.xyzw	" VUR "vf3, " VUR "vf3			\n"
		"qmfc2			%3, $vf3				\n"
		"sd				%3, 32 + %0			\n"
		"pcpyud			%3, %3, %3			\n"
		"sd				%3, 40 + %0			\n"

		"vmulw.xyzw		" VUR "vf4, " VUR "vf4, " VUR "vf9w		\n"
		"vftoi15.xyzw	" VUR "vf4, " VUR "vf4			\n"
		"qmfc2			%4, $vf4				\n"
		"sd				%4, 48 + %0			\n"
		"pcpyud			%4, %4, %4			\n"
		"sd				%4, 56 + %0			\n"

		:"=m"(*m) : "r"(left), "r"(right), "r"(bottom), "r"(top), "r"(near), "r"(far), "r"(scale));
	printf("5\n");
}

#undef VUR

#endif

u32 Patch_guMtxIdentF()
{
TEST_DISABLE_GU_FUNCS
	const u32 address = gGPR[REG_a0]._u32_0;
	u8 * pMtxBase = (u8 *)ReadAddress(address);

#ifdef DAEDALUS_PSP_USE_VFPU
	vfpu_matrix_IdentF(pMtxBase);
#elif defined( DAEDALUS_PS2_USE_VU0 )
	vu0_matrix_IdentF(pMtxBase);
#else
	// 0x00000000 is 0.0 in IEEE fp
	// 0x3f800000 is 1.0 in IEEE fp
	QuickWrite32Bits(pMtxBase, 0x00, 0x3f800000);
	QuickWrite32Bits(pMtxBase, 0x04, 0);
	QuickWrite32Bits(pMtxBase, 0x08, 0);
	QuickWrite32Bits(pMtxBase, 0x0c, 0);

	QuickWrite32Bits(pMtxBase, 0x10, 0);
	QuickWrite32Bits(pMtxBase, 0x14, 0x3f800000);
	QuickWrite32Bits(pMtxBase, 0x18, 0);
	QuickWrite32Bits(pMtxBase, 0x1c, 0);

	QuickWrite32Bits(pMtxBase, 0x20, 0);
	QuickWrite32Bits(pMtxBase, 0x24, 0);
	QuickWrite32Bits(pMtxBase, 0x28, 0x3f800000);
	QuickWrite32Bits(pMtxBase, 0x2c, 0);

	QuickWrite32Bits(pMtxBase, 0x30, 0);
	QuickWrite32Bits(pMtxBase, 0x34, 0);
	QuickWrite32Bits(pMtxBase, 0x38, 0);
	QuickWrite32Bits(pMtxBase, 0x3c, 0x3f800000);
#endif

	return PATCH_RET_JR_RA;
}



u32 Patch_guMtxIdent()
{
TEST_DISABLE_GU_FUNCS
	const u32 address = gGPR[REG_a0]._u32_0;
	u8 * pMtxBase = (u8 *)ReadAddress(address);

	// This is a lot faster than the real method, which calls
	// glMtxIdentF followed by guMtxF2L

	//memcpy(pMtxBase, s_IdentMatrixL, sizeof(s_IdentMatrixL));

	QuickWrite32Bits(pMtxBase, 0x00, 0x00010000);
	QuickWrite32Bits(pMtxBase, 0x04, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x08, 0x00000001);
	QuickWrite32Bits(pMtxBase, 0x0c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x10, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x14, 0x00010000);
	QuickWrite32Bits(pMtxBase, 0x18, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x1c, 0x00000001);

	QuickWrite32Bits(pMtxBase, 0x20, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x24, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x28, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x2c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x30, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x34, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x38, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x3c, 0x00000000);

	return PATCH_RET_JR_RA;
}

u32 Patch_guTranslateF()
{
TEST_DISABLE_GU_FUNCS
	const u32 address = gGPR[REG_a0]._u32_0;
	u8 * pMtxBase = (u8 *)ReadAddress(address);

#ifdef DAEDALUS_PSP_USE_VFPU
	const f32 fx = gGPR[REG_a1]._f32_0;
	const f32 fy = gGPR[REG_a2]._f32_0;
	const f32 fz = gGPR[REG_a3]._f32_0;

	vfpu_matrix_TranslateF(pMtxBase, fx, fy, fz);
#elif defined( DAEDALUS_PS2_USE_VU0 )
	const f32 fx = gGPR[REG_a1]._f32_0;
	const f32 fy = gGPR[REG_a2]._f32_0;
	const f32 fz = gGPR[REG_a3]._f32_0;

	vu0_matrix_TranslateF(pMtxBase, fx, fy, fz);
#else
	// 0x00000000 is 0.0 in IEEE fp
	// 0x3f800000 is 1.0 in IEEE fp
	QuickWrite32Bits(pMtxBase, 0x00, 0x3f800000);
	QuickWrite32Bits(pMtxBase, 0x04, 0);
	QuickWrite32Bits(pMtxBase, 0x08, 0);
	QuickWrite32Bits(pMtxBase, 0x0c, 0);

	QuickWrite32Bits(pMtxBase, 0x10, 0);
	QuickWrite32Bits(pMtxBase, 0x14, 0x3f800000);
	QuickWrite32Bits(pMtxBase, 0x18, 0);
	QuickWrite32Bits(pMtxBase, 0x1c, 0);

	QuickWrite32Bits(pMtxBase, 0x20, 0);
	QuickWrite32Bits(pMtxBase, 0x24, 0);
	QuickWrite32Bits(pMtxBase, 0x28, 0x3f800000);
	QuickWrite32Bits(pMtxBase, 0x2c, 0);

	QuickWrite32Bits(pMtxBase, 0x30, gGPR[REG_a1]._u32_0);
	QuickWrite32Bits(pMtxBase, 0x34, gGPR[REG_a2]._u32_0);
	QuickWrite32Bits(pMtxBase, 0x38, gGPR[REG_a3]._u32_0);
	QuickWrite32Bits(pMtxBase, 0x3c, 0x3f800000);
#endif

	return PATCH_RET_JR_RA;
}

u32 Patch_guTranslate()
{
TEST_DISABLE_GU_FUNCS
	const f32 fScale = 65536.0f;

	const u32 address = gGPR[REG_a0]._u32_0;
	u8 * pMtxBase = (u8 *)ReadAddress(address);

	const f32 fx = gGPR[REG_a1]._f32_0;
	const f32 fy = gGPR[REG_a2]._f32_0;
	const f32 fz = gGPR[REG_a3]._f32_0;

	u32 x = (u32)(fx * fScale);
	u32 y = (u32)(fy * fScale);
	u32 z = (u32)(fz * fScale);

	u32 one = (u32)(1.0f * fScale);

	u32 xyhibits = (x & 0xFFFF0000) | (y >> 16);
	u32 xylobits = (x << 16) | (y & 0x0000FFFF);

	u32 z1hibits = (z & 0xFFFF0000) | (one >> 16);
	u32 z1lobits = (z << 16) | (one & 0x0000FFFF);

	QuickWrite32Bits(pMtxBase, 0x00, 0x00010000);
	QuickWrite32Bits(pMtxBase, 0x04, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x08, 0x00000001);
	QuickWrite32Bits(pMtxBase, 0x0c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x10, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x14, 0x00010000);
	QuickWrite32Bits(pMtxBase, 0x18, xyhibits);	// xy
	QuickWrite32Bits(pMtxBase, 0x1c, z1hibits);	// z1

	QuickWrite32Bits(pMtxBase, 0x20, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x24, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x28, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x2c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x30, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x34, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x38, xylobits);	// xy
	QuickWrite32Bits(pMtxBase, 0x3c, z1lobits);	// z1

	return PATCH_RET_JR_RA;
}

u32 Patch_guScaleF()
{
TEST_DISABLE_GU_FUNCS
	const u32 address = gGPR[REG_a0]._u32_0;
	u8 * pMtxBase = (u8 *)ReadAddress(address);

#ifdef DAEDALUS_PSP_USE_VFPU //Corn
	const f32 fx = gGPR[REG_a1]._f32_0;
	const f32 fy = gGPR[REG_a2]._f32_0;
	const f32 fz = gGPR[REG_a3]._f32_0;

	vfpu_matrix_ScaleF(pMtxBase, fx, fy, fz);
#elif defined( DAEDALUS_PS2_USE_VU0 )
	const f32 fx = gGPR[REG_a1]._f32_0;
	const f32 fy = gGPR[REG_a2]._f32_0;
	const f32 fz = gGPR[REG_a3]._f32_0;

	vu0_matrix_ScaleF(pMtxBase, fx, fy, fz);
#else
	QuickWrite32Bits(pMtxBase, 0x00, gGPR[REG_a1]._u32_0);
	QuickWrite32Bits(pMtxBase, 0x04, 0);
	QuickWrite32Bits(pMtxBase, 0x08, 0);
	QuickWrite32Bits(pMtxBase, 0x0c, 0);

	QuickWrite32Bits(pMtxBase, 0x10, 0);
	QuickWrite32Bits(pMtxBase, 0x14, gGPR[REG_a2]._u32_0);
	QuickWrite32Bits(pMtxBase, 0x18, 0);
	QuickWrite32Bits(pMtxBase, 0x1c, 0);

	QuickWrite32Bits(pMtxBase, 0x20, 0);
	QuickWrite32Bits(pMtxBase, 0x24, 0);
	QuickWrite32Bits(pMtxBase, 0x28, gGPR[REG_a3]._u32_0);
	QuickWrite32Bits(pMtxBase, 0x2c, 0);

	QuickWrite32Bits(pMtxBase, 0x30, 0);
	QuickWrite32Bits(pMtxBase, 0x34, 0);
	QuickWrite32Bits(pMtxBase, 0x38, 0);
	QuickWrite32Bits(pMtxBase, 0x3c, 0x3f800000);
#endif

	return PATCH_RET_JR_RA;
}

u32 Patch_guScale()
{
TEST_DISABLE_GU_FUNCS
	const f32 fScale = 65536.0f;

	const u32 address = gGPR[REG_a0]._u32_0;
	u8 * pMtxBase = (u8 *)ReadAddress(address);

	const f32 fx = gGPR[REG_a1]._f32_0;
	const f32 fy = gGPR[REG_a2]._f32_0;
	const f32 fz = gGPR[REG_a3]._f32_0;

	u32 x = (u32)(fx * fScale);
	u32 y = (u32)(fy * fScale);
	u32 z = (u32)(fz * fScale);

	u32 zer = (u32)(0.0f);

	u32 xzhibits = (x & 0xFFFF0000) | (zer >> 16);
	u32 xzlobits = (x << 16) | (zer & 0x0000FFFF);

	u32 zyhibits = (zer & 0xFFFF0000) | (y >> 16);
	u32 zylobits = (zer << 16) | (y & 0x0000FFFF);

	u32 zzhibits = (z & 0xFFFF0000) | (zer >> 16);
	u32 zzlobits = (z << 16) | (zer & 0x0000FFFF);

	QuickWrite32Bits(pMtxBase, 0x00, xzhibits);
	QuickWrite32Bits(pMtxBase, 0x04, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x08, zyhibits);
	QuickWrite32Bits(pMtxBase, 0x0c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x10, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x14, zzhibits);
	QuickWrite32Bits(pMtxBase, 0x18, 0x00000000);	// xy
	QuickWrite32Bits(pMtxBase, 0x1c, 0x00000001);	// z1

	QuickWrite32Bits(pMtxBase, 0x20, xzlobits);
	QuickWrite32Bits(pMtxBase, 0x24, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x28, zylobits);
	QuickWrite32Bits(pMtxBase, 0x2c, 0x00000000);

	QuickWrite32Bits(pMtxBase, 0x30, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x34, zzlobits);
	QuickWrite32Bits(pMtxBase, 0x38, 0x00000000);	// xy
	QuickWrite32Bits(pMtxBase, 0x3c, 0x00000000);	// z1

	return PATCH_RET_JR_RA;
}

u32 Patch_guMtxF2L()
{
TEST_DISABLE_GU_FUNCS

	const f32 fScale = 65536.0f;

	u8 * pMtxFBase = (u8 *)ReadAddress(gGPR[REG_a0]._u32_0);
	u8 * pMtxBase  = (u8 *)ReadAddress(gGPR[REG_a1]._u32_0);

	u8 * pMtxLBaseHiBits = (u8 *)(pMtxBase + 0x00);
	u8 * pMtxLBaseLoBits = (u8 *)(pMtxBase + 0x20);

	REG32 a, b;
	u32 tmp_a, tmp_b;
	u32 hibits;
	u32 lobits;
	u32 row;

	for (row = 0; row < 4; row++)
	{
		a._u32 = QuickRead32Bits(pMtxFBase, (row << 4) + 0x0);
		b._u32 = QuickRead32Bits(pMtxFBase, (row << 4) + 0x4);

		// Should be TRUNC
		tmp_a = (u32)(a._f32 * fScale);
		tmp_b = (u32)(b._f32 * fScale);

		hibits = (tmp_a & 0xFFFF0000) | (tmp_b >> 16);
		QuickWrite32Bits(pMtxLBaseHiBits, (row << 3) , hibits);

		lobits = (tmp_a << 16) | (tmp_b & 0x0000FFFF);
		QuickWrite32Bits(pMtxLBaseLoBits, (row << 3) , lobits);

		/////
		a._u32 = QuickRead32Bits(pMtxFBase, (row << 4) + 0x8);
		b._u32 = QuickRead32Bits(pMtxFBase, (row << 4) + 0xc);

		// Should be TRUNC
		tmp_a = (u32)(a._f32 * fScale);
		tmp_b = (u32)(b._f32 * fScale);

		hibits = (tmp_a & 0xFFFF0000) | (tmp_b >> 16);
		QuickWrite32Bits(pMtxLBaseHiBits, (row << 3) + 4, hibits);

		lobits = (tmp_a << 16) | (tmp_b & 0x0000FFFF);
		QuickWrite32Bits(pMtxLBaseLoBits, (row << 3) + 4, lobits);
	}

	return PATCH_RET_JR_RA;
}

//Using VFPU and no memcpy (works without hack?) //Corn
u32 Patch_guNormalize_Mario()
{
TEST_DISABLE_GU_FUNCS
	u8 * pXBase  = (u8 *)ReadAddress(gGPR[REG_a0]._u32_0);
	u8 * pYBase  = (u8 *)ReadAddress(gGPR[REG_a1]._u32_0);
	u8 * pZBase  = (u8 *)ReadAddress(gGPR[REG_a2]._u32_0);

	REG32 x, y, z;
	x._u32 = QuickRead32Bits(pXBase, 0x0);
	y._u32 = QuickRead32Bits(pYBase, 0x0);
	z._u32 = QuickRead32Bits(pZBase, 0x0);

#ifdef DAEDALUS_PSP_USE_VFPU //Corn
	vfpu_norm_3Dvec(&x._f32, &y._f32, &z._f32);
#elif defined( DAEDALUS_PS2_USE_VU0 )
	vu0_norm_3Dvec(&x._f32, &y._f32, &z._f32);
#else
	f32 fLenRecip = 1.0f / sqrtf((x._f32 * x._f32) + (y._f32 * y._f32) + (z._f32 * z._f32));

	x._f32 *= fLenRecip;
 	y._f32 *= fLenRecip;
 	z._f32 *= fLenRecip;
#endif

	QuickWrite32Bits(pXBase, x._u32);
	QuickWrite32Bits(pYBase, y._u32);
	QuickWrite32Bits(pZBase, z._u32);

	return PATCH_RET_JR_RA;
}

// NOT the same function as guNormalise_Mario
// This take one pointer, not 3
u32 Patch_guNormalize_Rugrats() //Using VFPU and no memcpy //Corn
{
TEST_DISABLE_GU_FUNCS
	u8 * pBase  = (u8 *)ReadAddress(gGPR[REG_a0]._u32_0);

	REG32 x, y, z;
	x._u32 = QuickRead32Bits(pBase, 0x0);
	y._u32 = QuickRead32Bits(pBase, 0x4);
	z._u32 = QuickRead32Bits(pBase, 0x8);

	// Mmmm can't find any game that uses this :/
	#ifdef DAEDALUS_DEBUG_CONSOLE
	DAEDALUS_ERROR("guNormalize_Rugrats: Check me");
#endif
#ifdef DAEDALUS_PSP_USE_VFPU //Corn
	vfpu_norm_3Dvec(&x._f32, &y._f32, &z._f32);
#elif defined( DAEDALUS_PS2_USE_VU0 )
	vu0_norm_3Dvec(&x._f32, &y._f32, &z._f32);
#else
	f32 fLenRecip = 1.0f / sqrtf((x._f32 * x._f32) + (y._f32 * y._f32) + (z._f32 * z._f32));

	x._f32 *= fLenRecip;
 	y._f32 *= fLenRecip;
 	z._f32 *= fLenRecip;
#endif

	QuickWrite32Bits(pBase, 0x0, x._u32);
	QuickWrite32Bits(pBase, 0x4, y._u32);
	QuickWrite32Bits(pBase, 0x8, z._u32);

	return PATCH_RET_JR_RA;
}

u32 Patch_guOrthoF()
{
TEST_DISABLE_GU_FUNCS
	REG32 l, r, b, t, n, f, s;

	u8 * pMtxBase   = (u8 *)ReadAddress(gGPR[REG_a0]._u32_0);	// Base address
	u8 * pStackBase = g_pu8RamBase_8000 + gGPR[REG_sp]._u32_0;	//Base stack address, this is safe since stack is always in physical memory
	l._u32 = gGPR[REG_a1]._u32_0;	//Left
	r._u32 = gGPR[REG_a2]._u32_0;	//Right
	b._u32 = gGPR[REG_a3]._u32_0;	//Bottom
	t._u32 = QuickRead32Bits(pStackBase, 0x10);	//Top
	n._u32 = QuickRead32Bits(pStackBase, 0x14);	//Near
	f._u32 = QuickRead32Bits(pStackBase, 0x18);	//Far
	s._u32 = QuickRead32Bits(pStackBase, 0x1c);	//Scale

#ifdef DAEDALUS_PSP_USE_VFPU //Corn
	vfpu_matrix_OrthoF(pMtxBase, l._f32, r._f32, b._f32, t._f32, n._f32, f._f32, s._f32);

#elif defined( DAEDALUS_PS2_USE_VU0 )
	vu0_matrix_OrthoF(pMtxBase, l._f32, r._f32, b._f32, t._f32, n._f32, f._f32, s._f32);

#else
	f32 fRmL = r._f32 - l._f32;
	f32 fTmB = t._f32 - b._f32;
	f32 fFmN = f._f32 - n._f32;
	f32 fRpL = r._f32 + l._f32;
	f32 fTpB = t._f32 + b._f32;
	f32 fFpN = f._f32 + n._f32;

	// Re-use unused old variables to store Matrix values
	l._f32 =  2.0f * s._f32 / fRmL;
	r._f32 =  2.0f * s._f32 / fTmB;
	b._f32 = -2.0f * s._f32 / fFmN;

	t._f32 = -fRpL * s._f32 / fRmL;
	n._f32 = -fTpB * s._f32 / fTmB;
	f._f32 = -fFpN * s._f32 / fFmN;

	/*
	0   2/(r-l)
	1                2/(t-b)
	2                            -2/(f-n)
	3 -(l+r)/(r-l) -(t+b)/(t-b) -(f+n)/(f-n)     1*/

	// 0x3f800000 is 1.0 in IEEE fp
	QuickWrite32Bits(pMtxBase, 0x00, l._u32);
	QuickWrite32Bits(pMtxBase, 0x04, 0);
	QuickWrite32Bits(pMtxBase, 0x08, 0);
	QuickWrite32Bits(pMtxBase, 0x0c, 0);

	QuickWrite32Bits(pMtxBase, 0x10, 0);
	QuickWrite32Bits(pMtxBase, 0x14, r._u32);
	QuickWrite32Bits(pMtxBase, 0x18, 0);
	QuickWrite32Bits(pMtxBase, 0x1c, 0);

	QuickWrite32Bits(pMtxBase, 0x20, 0);
	QuickWrite32Bits(pMtxBase, 0x24, 0);
	QuickWrite32Bits(pMtxBase, 0x28, b._u32);
	QuickWrite32Bits(pMtxBase, 0x2c, 0);

	QuickWrite32Bits(pMtxBase, 0x30, t._u32);
	QuickWrite32Bits(pMtxBase, 0x34, n._u32);
	QuickWrite32Bits(pMtxBase, 0x38, f._u32);
	QuickWrite32Bits(pMtxBase, 0x3c, s._u32);
#endif

	return PATCH_RET_JR_RA;
}

//Do the float version on a temporary matrix and convert to fixed point in VFPU & CPU //Corn
u32 Patch_guOrtho()
{
TEST_DISABLE_GU_FUNCS

#if defined( DAEDALUS_PSP_USE_VFPU ) || defined( DAEDALUS_PS2_USE_VU0z )
	u32 s_TempMatrix[16];
	REG32 l, r, b, t, n, f, s;

	u8 * pMtxBase   = (u8 *)ReadAddress(gGPR[REG_a0]._u32_0);	// Fixed point Base address
	u8 * pStackBase = g_pu8RamBase_8000 + gGPR[REG_sp]._u32_0;	//Base stack address, this is safe since stack is always in physical memory
	l._u32 = gGPR[REG_a1]._u32_0;	//Left
	r._u32 = gGPR[REG_a2]._u32_0;	//Right
	b._u32 = gGPR[REG_a3]._u32_0;	//Bottom
	t._u32 = QuickRead32Bits(pStackBase, 0x10);	//Top
	n._u32 = QuickRead32Bits(pStackBase, 0x14);	//Near
	f._u32 = QuickRead32Bits(pStackBase, 0x18);	//Far
	s._u32 = QuickRead32Bits(pStackBase,  0x1c);	//Scale

#ifdef DAEDALUS_PSP_USE_VFPU
	vfpu_matrix_Ortho((u8 *)s_TempMatrix, l._f32, r._f32, b._f32, t._f32, n._f32, f._f32, s._f32);
#elif defined( DAEDALUS_PS2_USE_VU0 )
	vu0_matrix_Ortho((u8*)s_TempMatrix, l._f32, r._f32, b._f32, t._f32, n._f32, f._f32, s._f32);
#endif

//Convert to proper N64 fixed point matrix
	u8 * pMtxLBaseHiBits = (u8 *)(pMtxBase + 0x00);
	u8 * pMtxLBaseLoBits = (u8 *)(pMtxBase + 0x20);

	u32 tmp_a, tmp_b;
	u32 hibits,lobits;
	u32 row, indx=0;

	for (row = 0; row < 4; row++)
	{
		tmp_a = s_TempMatrix[indx++];
		tmp_b = s_TempMatrix[indx++];

		hibits = (tmp_a & 0xFFFF0000) | (tmp_b >> 16);
		QuickWrite32Bits(pMtxLBaseHiBits, (row << 3) , hibits);

		lobits = (tmp_a << 16) | (tmp_b & 0x0000FFFF);
		QuickWrite32Bits(pMtxLBaseLoBits, (row << 3) , lobits);

		/////
		tmp_a = s_TempMatrix[indx++];
		tmp_b = s_TempMatrix[indx++];

		hibits = (tmp_a & 0xFFFF0000) | (tmp_b >> 16);
		QuickWrite32Bits(pMtxLBaseHiBits, (row << 3) + 4, hibits);

		lobits = (tmp_a << 16) | (tmp_b & 0x0000FFFF);
		QuickWrite32Bits(pMtxLBaseLoBits, (row << 3) + 4, lobits);
	}

	return PATCH_RET_JR_RA;
#else
	// FIX ME W32
	return PATCH_RET_NOT_PROCESSED0(guOrtho);
#endif
}

//RotateF //Corn
u32 Patch_guRotateF()
{
TEST_DISABLE_GU_FUNCS

	f32 s,c;
	REG32 a, r, x, y, z;

	u8 * pMtxBase = (u8 *)ReadAddress(gGPR[REG_a0]._u32_0);		//Matrix base address
	u8 * pStackBase = g_pu8RamBase_8000 + gGPR[REG_sp]._u32_0;	//Base stack address, this is safe since stack is always in physical memory

	a._u32 = gGPR[REG_a1]._u32_0;	//Angle in degrees + -> CCW
	x._u32 = gGPR[REG_a2]._u32_0;	//X
	y._u32 = gGPR[REG_a3]._u32_0;	//Y
	z._u32 = QuickRead32Bits(pStackBase, 0x10);	//Z

	sincosf(a._f32*(PI/180.0f), &s, &c);
//According to the manual the vector should be normalized in this function (Seems to work fine without it but risky)
//	vfpu_norm_3Dvec(&x._f32, &y._f32, &z._f32);

//Row #1
	r._f32 = x._f32 * x._f32 + c * (1.0f - x._f32 * x._f32);
	QuickWrite32Bits(pMtxBase, 0x00, r._u32);

	r._f32= x._f32 * y._f32 * (1.0f - c) + z._f32 * s;
	QuickWrite32Bits(pMtxBase, 0x04, r._u32);

	r._f32 = z._f32 * x._f32 * (1.0f - c) - y._f32 * s;
	QuickWrite32Bits(pMtxBase, 0x08, r._u32);

	QuickWrite32Bits(pMtxBase, 0x0c, 0x00000000);

//Row #2
	r._f32 = x._f32 * y._f32 * (1.0f - c) - z._f32 * s;
	QuickWrite32Bits(pMtxBase, 0x10, r._u32);

	r._f32 = y._f32 * y._f32 + c * (1.0f - y._f32 * y._f32);
	QuickWrite32Bits(pMtxBase, 0x14, r._u32);

	r._f32 = y._f32 * z._f32 * (1.0f - c) + x._f32 * s;
	QuickWrite32Bits(pMtxBase, 0x18, r._u32);

	QuickWrite32Bits(pMtxBase, 0x1c, 0x00000000);

//Row #3
	r._f32 = z._f32 * x._f32 * (1.0f - c) + y._f32 * s;
	QuickWrite32Bits(pMtxBase, 0x20, r._u32);

	r._f32 = y._f32 * z._f32 * (1.0f - c) - x._f32 * s;
	QuickWrite32Bits(pMtxBase, 0x24, r._u32);

	r._f32 = z._f32 * z._f32 + c * (1.0f - z._f32 * z._f32);
	QuickWrite32Bits(pMtxBase, 0x28, r._u32);

	QuickWrite32Bits(pMtxBase, 0x2c, 0x00000000);

//Row #4
	QuickWrite32Bits(pMtxBase, 0x30, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x34, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x38, 0x00000000);
	QuickWrite32Bits(pMtxBase, 0x3c, 0x3F800000);

	return PATCH_RET_JR_RA;
}

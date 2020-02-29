#include "as_reg_compat.h"

	.set noat

	
	.global		_ClipToHyperPlane

.text
	.set		push
	.set		noreorder
	
###############################################################################
#
#	a0		dest vertices			- must be aligned to 16 bytes
#	a1		source vertices			- must be aligned to 16 bytes
#	a2		plane					- must be aligned to 16 bytes
#	a3		in_vertex_count			- must be > 0 or we need to re apply check
#
#	v0		out_vertex_count
#
#	Trashes M000, M100, M200, M300
#
_ClipToHyperPlane:

	#
	#	a0			- current_dest
	#	a1			- current_source
	#	a2			- source (copy of original)
	#	a3			- verts_remaining
	#	t0			- temp
	#	t1			- source_end
	#	vf1		- plane
	#	vf3x,013	- aDotPlane, bDotPlane, ...
	#	M100		- clipped_out
	#	M200		- a
	#	M300		- b
	#
	
	//beq			$a3, $0, finished_all_vertices	// verts_remaining == 0 --> finished_all_vertices
	//nop

	lqc2		vf1, 0($a2)				// load plane equation (we reuse a2 for another purpose)
	
	or			$a2, $a1, $0				// take a copy of the original source pointer
	sll			$t1, $a3, 6					// t1 = num_verts*64
	addu		$t1, $a1, $t1				// source_end = source + num_verts*64
	
	// load initial values for b
	lqc2		vf10, 0($a1)				// TransformedPos(V4)
	lqc2		vf11, 16($a1)				// ProjectedPos(V4)
	lqc2		vf12, 32($a1)				// Color(V4)
	lqc2		vf13, 48($a1)				// Texture(V2) + Clipflags + 1/w
	addiu		$a1, $a1, 64				// skip the first vertex

	#vdot.q		vf2x, vf11, vf1			// bDotPlane = dot( b.ProjectedPos, plane )
	vmul.xyzw	vf2, vf11, vf1
	vaddy.x		vf2, vf2, vf2y
	vaddz.x		vf2, vf2, vf2z
	vaddw.x		vf2, vf2, vf2w

	or			$v0, $0, $0					// out_count = 0
	
get_next_vertex:
	// Wrap around the source pointer if necessary
	bne			$a1, $t1, load_a
	nop
	or			$a1, $a2, $0				// current_source = source

load_a:
	lqc2		vf5, 16($a1)
	lqc2		vf4, 0($a1)				// load a
	#vdot.q		vf3x, vf5, vf1			// aDotPlane = dot( a.ProjectedPos, plane )
	vmul.xyzw	vf3, vf5, vf1
	vaddy.x		vf3, vf3, vf3y
	vaddz.x		vf3, vf3, vf3z
	vaddw.x		vf3, vf3, vf3w
	lqc2		vf6, 32($a1)
	#vcmp.s		GT, vf3x, vf3x[0]
	qmfc2		$t0, vf3
	pextlw		$t0, $t0, $t0		//extend sign bit
	lqc2		vf7, 48($a1)

	bgtz		$t0, a_is_outside				// aDotPlane > 0 --> a_is_outside
	nop
	
a_is_inside:
	#vcmp.s		LE, vf2x, vf2x[0]
	qmfc2		$t0, vf2
	pextlw		$t0, $t0, $t0		//extend sign bit
	blez		$t0, a_is_inside_copy			// bDotPlane <= 0 --> a_is_inside_copy
	nop

	
	// Calculate the parametric intersection point
	vsub.xyzw	vf8, vf11, vf5			// vf8 = b.ProjectedPos - a.ProjectedPos
	#vdot.q		vf9x, vf8, vf1			// vf9x = dot( (b.ProjectedPos - a.ProjectedPos), plane )
	vmul.xyzw	vf9, vf8, vf1
	vaddy.x		vf9, vf9, vf9y
	vaddz.x		vf9, vf9, vf9z
	vaddw.x		vf9, vf9, vf9w
	#vrcp.s		vf9x, vf9x
	vdiv		Q, vf0w, vf9x
	vwaitq
	vmulq.x		vf9x, vf2x, Q

	// Interpolate a/b, output point
	vsub.xyzw	vf14, vf4, vf10			// (a-b)
	vsub.xyzw	vf15, vf5, vf11			//
	vsub.xyzw	vf16, vf6, vf12			//
	vsub.xyzw	vf17, vf7, vf13			//

	#vmscl.q	M100, M100, vf9x			// (a-b)*x
	vmulx.xyzw	vf14, vf14, vf9x			// (a-b)*x
	vmulx.xyzw	vf15, vf15, vf9x			//
	vmulx.xyzw	vf16, vf16, vf9x			//
	vmulx.xyzw	vf17, vf17, vf9x			//

	vadd.xyzw	vf14, vf10, vf14			// b + (a-b)*x
	vadd.xyzw	vf15, vf11, vf15			//
	sqc2		vf14, 0($a0)				// *out = *tmp
	vadd.xyzw	vf16, vf12, vf16			//
	sqc2		vf15, 16($a0)
	vadd.xyzw	vf17, vf13, vf17			//
	sqc2		vf16, 32($a0)
	addiu		$v0, $v0, 1					// outcout++
	sqc2		vf17, 48($a0)
	addiu		$a0, $a0, 64				// out++
	
a_is_inside_copy:
	
	sqc2		vf4, 0($a0)				// *out = *a
	sqc2		vf5, 16($a0)
	sqc2		vf6, 32($a0)
	sqc2		vf7, 48($a0)

	addiu		$a0, $a0, 64				// out++
	b			finished_vertex
	addiu		$v0, $v0, 1					// outcout++

a_is_outside:
	#vcmp.s		GT, vf2x, vf2x[0]
	qmfc2		$t0, vf2
	pextlw		$t0, $t0, $t0		//extend sign bit
	bgtz		$t0, finished_vertex			// bDotPlane <= 0 --> a_is_outside_copy
	nop
	
	// Calculate the parametric intersection point
	vsub.xyzw	vf8, vf11, vf5			// vf8 = b.ProjectedPos - a.ProjectedPos
	#vdot.q		vf9x, vf8, vf1			// vf9x = dot( (b.ProjectedPos - a.ProjectedPos), plane )
	vmul.xyzw	vf9, vf8, vf1
	vaddy.x		vf9, vf9, vf9y
	vaddz.x		vf9, vf9, vf9z
	vaddw.x		vf9, vf9, vf9w
	#vrcp.s		vf9x, vf9x
	vdiv		Q, vf0w, vf9x
	vwaitq
	vmulq.x		vf9x, vf2x, Q

	// Interpolate a/b, output point
	vsub.xyzw		vf14, vf4, vf10			// (a-b)
	vsub.xyzw		vf15, vf5, vf11			//
	vsub.xyzw		vf16, vf6, vf12			//
	vsub.xyzw		vf17, vf7, vf13			//

	#vmscl.q	M100, M100, vf9x			// (a-b)*x
	vmulx.xyzw	vf14, vf14, vf9x			// (a-b)*x
	vmulx.xyzw	vf15, vf15, vf9x			//
	vmulx.xyzw	vf16, vf16, vf9x			//
	vmulx.xyzw	vf17, vf17, vf9x			//

	vadd.xyzw	vf14, vf10, vf14			// b + (a-b)*x
	vadd.xyzw	vf15, vf11, vf15			//
	sqc2		vf14, 0($a0)				// *out = *tmp
	vadd.xyzw	vf16, vf12, vf16			//
	sqc2		vf15, 16($a0)
	vadd.xyzw	vf17, vf13, vf17			//
	sqc2		vf16, 32($a0)
	addiu		$v0, $v0, 1					// outcout++
	sqc2		vf17, 48($a0)
	addiu		$a0, $a0, 64				// out++
	
finished_vertex:
	#vmmov.q		M300, M200					// b = lat_point
	vmove.xyzw	vf10, vf4					// b = lat_point
	vmove.xyzw	vf11, vf5
	vmove.xyzw	vf12, vf6
	vmove.xyzw	vf13, vf7
	vmove.x		vf2x, vf3x					// move old aDotPlane as new bDotPlane

	addiu		$a3, $a3, -1				// verts_remaining--
	bne			$a3, $0, get_next_vertex	// verts_remaining != 0 --> get_next_vertex
	addiu		$a1, $a1, 64				// source++
	
finished_all_vertices:
	jr			$ra
	nop


	.set pop

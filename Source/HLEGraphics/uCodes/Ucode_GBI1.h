/*
Copyright (C) 2009 StrmnNrmn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef UCODE1_H__
#define UCODE1_H__


//*****************************************************************************
// The previous way of calculating was based on the assumption that
// there was no "n" field. I didn't realise that the n/length fields shared the
// lower 16 bits (in a 6:10 split).
// u32 length    = (command.inst.cmd0)&0xFFFF;
// u32 num_verts = (length + 1) / 0x210;                        // 528
// u32 v0_idx    = ((command.inst.cmd0>>16)&0xFF)/gVertexStride;      // /5
//*****************************************************************************
void DLParser_GBI1_Vtx( MicroCodeCommand command )
{
	u32 address = RDPSegAddr(command.vtx1.addr);

    //u32 length    = (command.inst.cmd0)&0xFFFF;
    //u32 num_verts = (length + 1) / 0x410;
    //u32 v0_idx    = ((command.inst.cmd0>>16)&0x3f)/2;

    u32 len = command.vtx1.len;
    u32 v0  = command.vtx1.v0;
    u32 n   = command.vtx1.n;

    use(len);

    DL_PF("    Address 0x%08x, v0: %d, Num: %d, Length: 0x%04x", address, v0, n, len);

    if ( address > MAX_RAM_ADDRESS )
    {
        DL_PF("     Address out of range - ignoring load");
        return;
    }

    if ( (v0 + n) > 64 )
    {
        DL_PF("        Warning, attempting to load into invalid vertex positions");
        DBGConsole_Msg( 0, "        DLParser_GBI1_Vtx: Warning, attempting to load into invalid vertex positions" );
        return;
    }

    PSPRenderer::Get()->SetNewVertexInfo( address, v0, n );

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
    gNumVertices += n;
    DLParser_DumpVtxInfo( address, v0, n );
#endif
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_ModifyVtx( MicroCodeCommand command )
{
	u32 offset =  command.modifyvtx.offset;
	u32 vert   = command.modifyvtx.vtx;
	u32 value  = command.modifyvtx.value;

	DAEDALUS_ASSERT( offset, " ModifyVtx : Can't handle" );

	// Cures crash after swinging in Mario Golf
	if( vert > 80 )
	{
		DAEDALUS_ERROR("ModifyVtx: Invalid vertex number: %d", vert);
		return;
	}
	
	PSPRenderer::Get()->ModifyVertexInfo( offset, vert, value );
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Mtx( MicroCodeCommand command )
{
	u32 address = RDPSegAddr(command.mtx1.addr);

	DL_PF("    Command: %s %s %s Length %d Address 0x%08x",
		command.mtx1.projection == 1 ? "Projection" : "ModelView",
		command.mtx1.load == 1 ? "Load" : "Mul",
		command.mtx1.push == 1 ? "Push" : "NoPush",
		command.mtx1.len, address);

	// Load matrix from address
	Matrix4x4 mat;
	MatrixFromN64FixedPoint( mat, address );

	if (command.mtx1.projection)
	{
		PSPRenderer::Get()->SetProjection(mat, command.mtx1.push, command.mtx1.load);
	}
	else
	{
		PSPRenderer::Get()->SetWorldView(mat, command.mtx1.push, command.mtx1.load);
	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_PopMtx( MicroCodeCommand command )
{
	DL_PF("    Command: (%s)",	command.popmtx.projection ? "Projection" : "ModelView");

	// Do any of the other bits do anything?
	// So far only Extreme-G seems to Push/Pop projection matrices

	if (command.popmtx.projection)
	{
		PSPRenderer::Get()->PopProjection();
	}
	else
	{
		PSPRenderer::Get()->PopWorldView();
	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_CullDL( MicroCodeCommand command )
{
	u32 first = command.culldl.first;
	u32 last = command.culldl.end;

	DL_PF(" Culling using verts %d to %d\n", first, last);

	if( last < first ) return;
	if( PSPRenderer::Get()->TestVerts( first, last ) )
	{
		DL_PF(" Display list is visible, returning");
		return;
	}

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DL_PF(" No vertices were visible, culling rest of display list");
	++gNumDListsCulled;
	DLParser_PopDL();
#else
	gDlistStackPointer--;
#endif
}


//*****************************************************************************
// 
//*****************************************************************************
void DLParser_GBI1_DL( MicroCodeCommand command )
{

	DAEDALUS_ASSERT( RDPSegAddr(command.dlist.addr) < MAX_RAM_ADDRESS, 
					"DL addr out of range (0x%08x)", RDPSegAddr(command.dlist.addr) );

    DL_PF("    Address=0x%08x Push: 0x%02x", RDPSegAddr(command.dlist.addr), command.dlist.param);

	if( command.dlist.param == G_DL_PUSH )
		gDlistStackPointer++;

	gDlistStack[gDlistStackPointer].pc = RDPSegAddr(command.dlist.addr);
	gDlistStack[gDlistStackPointer].countdown = MAX_DL_COUNT;
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_EndDL( MicroCodeCommand command )
{
#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	DLParser_PopDL();
#else
	gDlistStackPointer--;
#endif
}

//*****************************************************************************
// When the depth is less than the z value provided, branch to given address
//*****************************************************************************
void DLParser_GBI1_BranchZ( MicroCodeCommand command )
{
	//Always branching will usually just waste a bit of fillrate (PSP got plenty)
	//Games seem not to bother if we branch less than Z all the time
	
	//Penny racers (cars)
	//Aerogauge (skips rendering ship shadows and exaust plumes from afar)
	//OOT : Death Mountain and MM : Clock Town
	
	//Seems to work differently for non Zelda games as if Z axis is inverted... //Corn

	//printf("VtxDepth[%d] Zval[%d] Vtx[%d]\n", PSPRenderer::Get()->GetVtxDepth(command.branchz.vtx), (s32)command.branchz.value, command.branchz.vtx);
	DL_PF("BranchZ VtxDepth[%d] Zval[%d] Vtx[%d]", PSPRenderer::Get()->GetVtxDepth(command.branchz.vtx), (s32)command.branchz.value, command.branchz.vtx);

	if( PSPRenderer::Get()->GetVtxDepth(command.branchz.vtx) <= (s32)command.branchz.value )
	{					
		u32 pc = gDlistStack[gDlistStackPointer].pc;
		u32 dl = *(u32 *)(g_pu8RamBase + pc-12);
		u32 address = RDPSegAddr(dl);

		DL_PF("   Jump -> DisplayList 0x%08x", address);

		gDlistStack[gDlistStackPointer].pc = address;
		gDlistStack[gDlistStackPointer].countdown = MAX_DL_COUNT;
	}
}

//***************************************************************************** 
// AST, Yoshi's World, Scooby Doo use this
//***************************************************************************** 
void DLParser_GBI1_LoadUCode( MicroCodeCommand command ) 
{ 
	u32 code_base = (command.inst.cmd1 & 0x1fffffff);
    u32 code_size = 0x1000; 
    u32 data_base = gRDPHalf1 & 0x1fffffff;         // Preceeding RDP_HALF1 sets this up
    u32 data_size = (command.inst.cmd0 & 0xFFFF) + 1;

	DLParser_InitMicrocode( code_base, code_size, data_base, data_size ); 
}

//***************************************************************************** 
// 
//*****************************************************************************
void DLParser_GBI1_ClearGeometryMode( MicroCodeCommand command )
{
    u32 mask = (command.inst.cmd1);
    
    gGeometryMode &= ~mask;

    DL_PF("  Clearing mask -> 0x%08x", mask);

	DLParser_InitGeometryMode();
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_SetGeometryMode(  MicroCodeCommand command  )
{
    u32 mask = command.inst.cmd1;

    gGeometryMode |= mask;

    DL_PF("  Setting mask -> 0x%08x", mask);

	DLParser_InitGeometryMode();
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_SetOtherModeL( MicroCodeCommand command )
{
    u32 mask		= ((1 << command.othermode.len) - 1) << command.othermode.sft;

    gRDPOtherMode.L = (gRDPOtherMode.L&(~mask)) | command.othermode.data;

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	RDP_SetOtherMode( gRDPOtherMode.H, gRDPOtherMode.L );
#endif
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_SetOtherModeH( MicroCodeCommand command )
{
    u32 mask		= ((1 << command.othermode.len) - 1) << command.othermode.sft;

    gRDPOtherMode.H = (gRDPOtherMode.H&(~mask)) | command.othermode.data;

#ifdef DAEDALUS_DEBUG_DISPLAYLIST
	RDP_SetOtherMode( gRDPOtherMode.H, gRDPOtherMode.L );
#endif
}


//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Texture( MicroCodeCommand command )
{
    gTextureLevel = command.texture.level;
    gTextureTile  = command.texture.tile;

	// Seems to use 0x01
	// Force enable texture in DKR Ucode, fixes static texture bug etc
    bool enable = command.texture.enable_gbi0 || (current.ucode == GBI_DKR);
	
	DL_PF("    Level: %d Tile: %d %s", gTextureLevel, gTextureTile, enable? "enabled":"disabled");
	PSPRenderer::Get()->SetTextureEnable( enable );

	if( !enable )	return;

	f32 scale_s = f32(command.texture.scaleS)  / (65536.0f * 32.0f);
	f32 scale_t = f32(command.texture.scaleT)  / (65536.0f * 32.0f);

	DL_PF("    ScaleS: %f, ScaleT: %f", scale_s*32.0f, scale_t*32.0f);
	PSPRenderer::Get()->SetTextureScale( scale_s, scale_t );
}


//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Tri2( MicroCodeCommand command )
{
    // While the next command pair is Tri2, add vertices
	u32 pc = gDlistStack[gDlistStackPointer].pc;
    u32 * pCmdBase = (u32 *)(g_pu8RamBase + pc);

    bool tris_added = false;

    do{
        DL_PF("   0x%08x: %08x %08x %-10s", pc-8, command.inst.cmd0, command.inst.cmd1, "G_GBI1_TRI2");

		// Vertex indices are multiplied by 10 for GBI0, by 2 for GBI1
		u32 v0_idx = command.gbi1tri2.v0 >> 1;
		u32 v1_idx = command.gbi1tri2.v1 >> 1;
		u32 v2_idx = command.gbi1tri2.v2 >> 1;

		tris_added |= PSPRenderer::Get()->AddTri(v0_idx, v1_idx, v2_idx);

		u32 v3_idx = command.gbi1tri2.v3 >> 1;
		u32 v4_idx = command.gbi1tri2.v4 >> 1;
		u32 v5_idx = command.gbi1tri2.v5 >> 1;

		tris_added |= PSPRenderer::Get()->AddTri(v3_idx, v4_idx, v5_idx);

		command.inst.cmd0= *pCmdBase++;
		command.inst.cmd1= *pCmdBase++;
		pc += 8;
    }while( command.inst.cmd == G_GBI1_TRI2 );

	gDlistStack[gDlistStackPointer].pc = pc-8;

    if (tris_added)
    {
            PSPRenderer::Get()->FlushTris();
    }
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Line3D( MicroCodeCommand command )
{
	if( command.gbi1line3d.v3 == 0 )
	{
		// This removes the tris that cover the screen in Flying Dragon
		// Actually this wrong, we should support line3D properly here..
		DAEDALUS_ERROR("Flying Dragon Hack -- Skipping Line3D");
		return;
	}

    // While the next command pair is Tri1, add vertices
	u32 pc = gDlistStack[gDlistStackPointer].pc;
    u32 * pCmdBase = (u32 *)( g_pu8RamBase + pc );

    bool tris_added = false;

	do{
        DL_PF("   0x%08x: %08x %08x %-10s", pc-8, command.inst.cmd0, command.inst.cmd1, "G_GBI1_LINE3D");

		u32 v0_idx   = command.gbi1line3d.v0 / gVertexStride;
		u32 v1_idx   = command.gbi1line3d.v1 / gVertexStride;
		u32 v2_idx   = command.gbi1line3d.v2 / gVertexStride;
		u32 v3_idx   = command.gbi1line3d.v3 / gVertexStride;

		tris_added |= PSPRenderer::Get()->AddTri(v0_idx, v1_idx, v2_idx);
		tris_added |= PSPRenderer::Get()->AddTri(v2_idx, v3_idx, v0_idx);

		command.inst.cmd0 = *pCmdBase++;
		command.inst.cmd1 = *pCmdBase++;
		pc += 8;
	}while( command.inst.cmd == G_GBI1_LINE3D );

	gDlistStack[gDlistStackPointer].pc = pc-8;

	if (tris_added)
	{
			PSPRenderer::Get()->FlushTris();
	}
}

//*****************************************************************************
//
//*****************************************************************************
void DLParser_GBI1_Tri1( MicroCodeCommand command )
{
    //DAEDALUS_PROFILE( "DLParser_GBI1_Tri1_T" );
    // While the next command pair is Tri1, add vertices
	u32 pc = gDlistStack[gDlistStackPointer].pc;
    u32 * pCmdBase = (u32 *)( g_pu8RamBase + pc );

    bool tris_added = false;

    do{
        DL_PF("   0x%08x: %08x %08x %-10s", pc-8, command.inst.cmd0, command.inst.cmd1, "G_GBI1_TRI1");

		// Vertex indices are multiplied by 10 for Mario64, by 2 for MarioKart
        u32 v0_idx = command.gbi1tri1.v0 / gVertexStride;
        u32 v1_idx = command.gbi1tri1.v1 / gVertexStride;
        u32 v2_idx = command.gbi1tri1.v2 / gVertexStride;

        tris_added |= PSPRenderer::Get()->AddTri(v0_idx, v1_idx, v2_idx);

        command.inst.cmd0= *pCmdBase++;
        command.inst.cmd1= *pCmdBase++;
        pc += 8;
    }while( command.inst.cmd == G_GBI1_TRI1 );

	gDlistStack[gDlistStackPointer].pc = pc-8;

    if (tris_added)
    {
		PSPRenderer::Get()->FlushTris();
	}
}


#endif // UCODE1_H__
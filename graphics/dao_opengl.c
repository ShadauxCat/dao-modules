/*
// Dao Standard Modules
// http://www.daovm.net
//
// Copyright (c) 2012, Limin Fu
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <gl.h>
#include <string.h>
#include "dao_opengl.h"


#define DAOX_VBO_COUNT   3

#define DAOX_VBO_VERTEX  0
#define DAOX_VBO_STROKE  1
#define DAOX_VBO_FILL    2


void DaoxGraphics_glSetColor( DaoxColor color )
{
	glColor4f( color.red, color.green, color.blue, color.alpha );
}

void DaoxGraphics_glFillRect( DaoxPoint lb, DaoxPoint rt )
{
	glBegin( GL_QUADS );
	{
		glVertex2f( lb.x, lb.y );
		glVertex2f( rt.x, lb.y );
		glVertex2f( rt.x, rt.y );
		glVertex2f( lb.x, rt.y );
	}
	glEnd();
}
void DaoxGraphics_glFillTriangle( DaoxPoint A, DaoxPoint B, DaoxPoint C )
{
	glBegin( GL_TRIANGLES );
	{
		glVertex2f( A.x, A.y );
		glVertex2f( B.x, B.y );
		glVertex2f( C.x, C.y );
	}
	glEnd();
}
void DaoxGraphics_glFillQuad( DaoxPoint *points, int count )
{
	int i;
	glBegin( GL_QUADS );
	{
		for(i=0; i<count; ++i) glVertex2f( points[i].x, points[i].y );
	}
	glEnd();
}
void DaoxGraphics_glFillPolygons( DaoxPolygonArray *polygons )
{
	int i;
	for(i=0; i<polygons->polygons->count; ++i){
		DaoxSlice polygon = polygons->polygons->slices[i];
		DaoxPoint *points = polygons->points->points + polygon.offset;
		if( polygon.count == 2 ){
			DaoxGraphics_glFillRect( points[0], points[1] );
		}else if( polygon.count == 3 ){
			DaoxGraphics_glFillTriangle( points[0], points[1], points[2] );
		}else if( polygon.count == 4 ){
			DaoxGraphics_glFillQuad( points, polygon.count );
		}
	}
}
void DaoxGraphics_glFillTriangles( DaoxGraphicsData *gdata, DaoxIntArray *triangles )
{
	DaoxPoint *points = gdata->points->points;
	int i, *ids = triangles->values;
	glBegin( GL_TRIANGLES );
	{
		for(i=0; i<triangles->count; i+=3){
			glVertex2f( points[ids[i+0]].x, points[ids[i+0]].y );
			glVertex2f( points[ids[i+1]].x, points[ids[i+1]].y );
			glVertex2f( points[ids[i+2]].x, points[ids[i+2]].y );
		}
	}
	glEnd();
}
void DaoxGraphics_glDrawItem( DaoxGraphicsItem *item )
{
	GLdouble matrix[16] = {0};
	int i, n = item->children ? item->children->size : 0;

	DaoxGraphicsItem_UpdateData( item, item->scene );

	if( item->gdata->strokeTriangles->count + item->gdata->fillTriangles->count == 0 ) return;

	printf( "strokeTriangles = %6i\n", item->gdata->strokeTriangles->count );
	printf( "fillTriangles   = %6i\n", item->gdata->fillTriangles->count );
#if 0
#endif
	
	memset( matrix, 0, 16*sizeof(GLdouble) );

	matrix[0] = item->state->transform.Axx;
	matrix[4] = item->state->transform.Axy;
	matrix[12] = item->state->transform.Bx;
	matrix[1] = item->state->transform.Ayx;
	matrix[5] = item->state->transform.Ayy;
	matrix[13] = item->state->transform.By;
	matrix[15] = 1.0;


	glPushMatrix();
	glMultMatrixd( matrix );

#define USE_STENCIL

#ifdef USE_STENCIL
	if( item->shape >= DAOX_GS_CIRCLE ){
		glClearStencil(0);
		glClear(GL_STENCIL_BUFFER_BIT);
		glEnable( GL_STENCIL_TEST );
		glStencilFunc( GL_NOTEQUAL, 0x01, 0x01);
		glStencilOp( GL_REPLACE, GL_REPLACE, GL_REPLACE );
	}
#endif
	if( item->gdata->strokeTriangles->count ){
		DaoxGraphics_glSetColor( item->state->strokeColor );
		DaoxGraphics_glFillTriangles( item->gdata, item->gdata->strokeTriangles );
	}

#ifdef USE_STENCIL
	if( item->shape >= DAOX_GS_CIRCLE ){
		glStencilFunc( GL_NOTEQUAL, 0x01, 0x01);
		glStencilOp( GL_REPLACE, GL_REPLACE, GL_REPLACE );
	}
#endif
	if( item->gdata->fillTriangles->count ){
		DaoxGraphics_glSetColor( item->state->fillColor );
		DaoxGraphics_glFillTriangles( item->gdata, item->gdata->fillTriangles );
	}
#ifdef USE_STENCIL
	if( item->shape >= DAOX_GS_CIRCLE ) glDisable( GL_STENCIL_TEST );;
#endif

	for(i=0; i<n; i++){
		DaoxGraphicsItem *it = (DaoxGraphicsItem*) item->children->items.pVoid[i];
		DaoxGraphics_glDrawItem( it );
	}
	glPopMatrix();
}
void DaoxGraphics_glDrawScene( DaoxGraphicsScene *scene, double left, double bottom, double right, double top )
{
	double width = right - left;
	double height = top - bottom;
	int i, n = scene->items->size;

	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	glOrtho(0, width, 0, height, 0, 1);
	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	/* Displacement trick for exact pixelization: */
	glTranslatef(0.375, 0.375, 0);

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable( GL_LIGHTING );
	glClearColor( 1.0, 1.0, 1.0, 1.0 );

	for(i=0; i<n; i++){
		DaoxGraphicsItem *it = (DaoxGraphicsItem*) scene->items->items.pVoid[i];
		DaoxGraphics_glDrawItem( it );
	}
}

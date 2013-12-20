/*
// String Template Engine for Dao
//
// Copyright (c) 2013, Limin Fu
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
// THIS SOFTWARE IS PROVIDED  BY THE COPYRIGHT HOLDERS AND  CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED  WARRANTIES,  INCLUDING,  BUT NOT LIMITED TO,  THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL  THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE FOR ANY DIRECT,
// INDIRECT,  INCIDENTAL, SPECIAL,  EXEMPLARY,  OR CONSEQUENTIAL  DAMAGES (INCLUDING,
// BUT NOT LIMITED TO,  PROCUREMENT OF  SUBSTITUTE  GOODS OR  SERVICES;  LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY OF
// LIABILITY,  WHETHER IN CONTRACT,  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <string.h>

#include "dao.h"
#include "daoString.h"
#include "daoArray.h"
#include "daoStdtype.h"
#include "daoValue.h"
#include "daoThread.h"

typedef struct DaoxTemplateNode DaoxTemplateNode;

struct DaoxTemplateNode
{
	DString  *command;  /* #each */

	DString  *contentMBS;  /* plain text */
	DString  *contentWCS;  /* plain text */

	DArray   *keyPathMBS;  /* key1.key2... */
	DArray   *keyPathWCS;  /* key1.key2... */

	DArray   *children;
};

DaoxTemplateNode* DaoxTemplateNode_New()
{
	DaoxTemplateNode *self = (DaoxTemplateNode*) dao_calloc( 1, sizeof(DaoxTemplateNode) );
	return self;
}
void DaoxTemplateNode_Delete( DaoxTemplateNode *self )
{
	if( self->command ) DString_Delete( self->command );
	if( self->contentMBS ) DString_Delete( self->contentMBS );
	if( self->contentWCS ) DString_Delete( self->contentWCS );
	if( self->keyPathMBS ) DArray_Delete( self->keyPathMBS );
	if( self->keyPathWCS ) DArray_Delete( self->keyPathWCS );
	if( self->children ){
		daoint i;
		for(i=0; i<self->children->size; ++i){
			DaoxTemplateNode *node = (DaoxTemplateNode*) self->children->items.pVoid[i];
			DaoxTemplateNode_Delete( node );
		}
		DArray_Delete( self->children );
	}
}
int DString_CheckCommandTag( DString *self, DString *cmd, daoint start, daoint end, char prefix )
{
	daoint i, pos = start + 2;
	//printf( "DString_CheckCommandTag(): %s\n", self->mbs + start );

	while( isspace( self->mbs[pos] ) ) pos += 1;
	if( self->mbs[pos++] != prefix ) return 0;
	if( cmd->size > (end - pos) ) return 0;

	for(i=0; i<cmd->size; ++i){
		if( cmd->mbs[i] != self->mbs[pos++] ) return 0;
	}
	if( self->mbs[pos] != '}' && ! isspace( self->mbs[pos] ) ) return 0;
	for(i=pos; i<end; ++i){
		if( self->mbs[pos] == '{' || self->mbs[pos] == '}' ) return 0;
	}
	if( prefix == '#' ) return pos;
	while( isspace( self->mbs[pos] ) ) pos += 1;
	if( self->mbs[pos] != '}' ) return 0;
	return 1;
}
daoint DString_FindCommandTag( DString *self, DString *cmd, daoint start, daoint end, char prefix )
{
	daoint i, pos, pos2;
	
	if( (pos  = DString_FindMBS( self, "{{", start )) < 0 ) return -1;
	if( (pos2 = DString_FindMBS( self, "}}", start )) < 0 ) return -1;

	while( DString_CheckCommandTag( self, cmd, pos, pos2, prefix ) == 0 ){
		start = pos2 + 2;
		if( (pos  = DString_FindMBS( self, "{{", start )) < 0 ) return -1;
		if( (pos2 = DString_FindMBS( self, "}}", start )) < 0 ) return -1;
		if( pos >= end ) return -1;
	}
	return pos;
}
/* Nested commands are considered: */
daoint DString_FindCommandClose( DString *self, DString *cmd, daoint start, daoint end )
{
	daoint open = DString_FindCommandTag( self, cmd, start, end, '#' );
	daoint close = DString_FindCommandTag( self, cmd, start, end, '/' );
	daoint count = 1;
	if( close < 0 ) return -1;
	if( open < 0 ) return close;
	if( close < open ) return close;
	while( close > 0 && count > 0 ){
		while( open < close ){
			open = DString_FindCommandTag( self, cmd, open+5, end, '#' );
			if( open < 0 ){
				open = end;
				break;
			}
			count += open < close;
		}
		while( close < open ){
			close = DString_FindCommandTag( self, cmd, close+5, end, '/' );
			if( close < 0 ) break;
			count -= close < open;
			if( count == 0 ) break;
		}
	}
	return close;
}
void DaoxTemplateNode_ParseMBS( DaoxTemplateNode *self, DString *template, daoint start, daoint end )
{
	DString *keyMBS = NULL;
	DString *keyWCS = NULL;
	daoint pos, pos2, offset = start;
	self->children = DArray_New(0);
	while( end > start && isspace(template->mbs[end-1]) ) end --;
	while( offset < end ){
		DaoxTemplateNode *node = DaoxTemplateNode_New();
		node->contentMBS = DString_New(1);
		node->contentWCS = DString_New(0);
		DArray_Append( self->children, node );
		pos = DString_FindMBS( template, "{{", offset );
		if( pos == MAXSIZE ){
			DString_SubString( template, node->contentMBS, offset, end - offset );
			DString_Append( node->contentWCS, node->contentMBS );
			break;
		}
		DString_SubString( template, node->contentMBS, offset, pos - offset );
		DString_Append( node->contentWCS, node->contentMBS );

		pos2 = DString_FindMBS( template, "}}", pos );
		if( pos2 == MAXSIZE ) break; // XXX: error;
		pos += 2;
		while( isspace( template->mbs[pos] ) ) pos += 1;
		if( template->mbs[pos] == '!' ){
			offset = pos2 + 2;
			continue;
		}
		if( template->mbs[pos] == '/' ){
			offset = pos2 + 2;
			continue;
		}

		node = DaoxTemplateNode_New();
		DArray_Append( self->children, node );
		if( template->mbs[pos] == '#' ){
			node->command = DString_New(1);
			pos += 1;
			while( isalnum( template->mbs[pos] ) || template->mbs[pos] == '_' ){
				DString_AppendChar( node->command, template->mbs[pos] );
				pos += 1;
			}
			if( ! isspace( template->mbs[pos] ) ){
			}
		}
		node->keyPathMBS = DArray_New(D_STRING);
		node->keyPathWCS = DArray_New(D_STRING);
		while( pos < pos2 ){
			while( isspace( template->mbs[pos] ) ) pos += 1;
			if( pos >= pos2 ) break;
			if( keyMBS == NULL ){
				keyMBS = DString_New(1);
				keyWCS = DString_New(0);
			}
			DString_Reset( keyMBS, 0 );
			if( node->keyPathMBS->size == 0 && template->mbs[pos] == '@' ){
				DString_AppendChar( keyMBS, template->mbs[pos++] );
			}
			while( isalnum( template->mbs[pos] ) || template->mbs[pos] == '_' ){
				DString_AppendChar( keyMBS, template->mbs[pos] );
				pos += 1;
			}
			if( template->mbs[pos] == '.' ) pos += 1;
			DString_Reset( keyWCS, 0 );
			DString_Append( keyWCS, keyMBS );
			DArray_Append( node->keyPathMBS, keyMBS );
			DArray_Append( node->keyPathWCS, keyWCS );
		}
		if( node->command ){
			daoint pos3 = DString_FindCommandClose( template, node->command, pos2 + 2, end );
			if( pos3 < 0 || pos3 > end ) pos3 = end;
			DaoxTemplateNode_ParseMBS( node, template, pos2 + 2, pos3 );
			pos2 = DString_FindMBS( template, "}}", pos3 );
			if( pos2 < 0 ) pos2 = end;
		}
		offset = pos2 + 2;
	}
	if( keyMBS ){
		DString_Delete( keyMBS );
		DString_Delete( keyWCS );
	}

#if 0
	{
		daoint i;
		for(i=0; i<self->children->size; ++i){
			DaoxTemplateNode *node = (DaoxTemplateNode*) self->children->items.pVoid[i];
			printf( "command = %s; path = %s\n", node->command ? node->command->mbs : "",
					node->keypath ? node->keypath->items.pString[0]->mbs : "" );
		}
	}
#endif
}
DaoValue* DaoxTemplateNode_GetPath( DaoxTemplateNode *self, DaoValue *container, DMap *vars, int i )
{
	DArray *path = self->keyPathMBS;
	DArray *path2 = self->keyPathWCS;
	DaoValue *value;
	DNode *it;
	if( path->size >= 1 && i == 0 ){
		DString *name = path->items.pString[0];
		if( name->mbs[0] == '@' ){
			if( vars == NULL ) return NULL;
			it = DMap_Find( vars, name );
			if( it ) return it->value.pValue;
			return NULL;
		}
	}
	if( container == NULL ) return NULL;
	if( i >= path->size ) return container;
	if( container->type == DAO_MAP ){
		DaoMap *map = (DaoMap*) container;
		value = DaoMap_GetValueMBS( map, path->items.pString[i]->mbs );
		if( value == NULL ) value = DaoMap_GetValueWCS( map, path2->items.pString[i]->wcs );
		return DaoxTemplateNode_GetPath( self, value, NULL, i+1 );
	}else if( container->type == DAO_TUPLE ){
		DaoTuple *tup = (DaoTuple*) container;
		it = DMap_Find( tup->ctype->mapNames, path->items.pString[i] );
		if( it == NULL ) return NULL;
		return DaoxTemplateNode_GetPath( self, tup->items[it->value.pInt], NULL, i+1 );
	}
	return NULL;
}
void DaoxTemplateNodes_Generate( DArray *nodes, DaoValue *value, DMap *vars, DString *output );
void DaoxTemplateNode_Generate( DaoxTemplateNode *self, DaoValue *value, DMap *vars,  DString *output )
{
	if( self->command ){
		DaoValue *field = DaoxTemplateNode_GetPath( self, value, vars, 0 );
		DString sindex = DString_WrapMBS( "@index" );
		DString skey = DString_WrapMBS( "@key" );
		DString svalue = DString_WrapMBS( "@value" );
		DString scount = DString_WrapMBS( "@count" );
		DString udf = DString_WrapMBS( "UNDEFINED" );
		DaoString key0 = { DAO_STRING,0,0,0,1,NULL};
		DaoInteger index0 = {DAO_INTEGER,0,0,0,0,0};
		DaoInteger *index = & index0;
		DaoString *key = & key0;
		DMap *vars2;
		DNode *it;
		if( strcmp( self->command->mbs, "each" ) == 0 ){
			if( field == NULL ) return;
			vars2 = vars ? DMap_Copy( vars ) : DMap_New(D_STRING,D_VALUE);
			if( field->type == DAO_LIST ){
				DaoList *list = (DaoList*) field;
				daoint i, n = DaoList_Size( list );
				for(i=0; i<n; ++i){
					DaoValue *item = DaoList_GetItem( list, i );
					index->value = i + 1;
					DMap_Insert( vars2, & sindex, (DaoValue*) index );
					DMap_Insert( vars2, & svalue, item );
					DaoxTemplateNodes_Generate( self->children, item, vars2, output );
				}
			}else if( field->type == DAO_MAP ){
				DaoMap *map = (DaoMap*) field;
				for(it=DaoMap_First(map); it; it=DaoMap_Next(map,it)){
					DMap_Insert( vars2, & skey, it->key.pValue );
					DMap_Insert( vars2, & svalue, it->value.pValue );
					DaoxTemplateNodes_Generate( self->children, it->value.pValue, vars2, output );
				}
			}else if( field->type == DAO_TUPLE ){
				DaoTuple *tup = (DaoTuple*) field;
				DMap *mapNames = tup->ctype->mapNames;
				DArray *names = DArray_New(0);
				int i;
				DArray_Resize( names, tup->size, NULL );
				for(it=DMap_First(mapNames); it; it=DMap_Next(mapNames,it)){
					names->items.pString[it->value.pInt] = it->key.pString;
				}
				for(i=0; i<tup->size; ++i){
					DaoValue *item = tup->items[i];
					index->value = i + 1;
					key->data = names->items.pString[i];
					if( key->data == NULL ) key->data = & udf;
					DMap_Insert( vars2, & sindex, (DaoValue*) index );
					DMap_Insert( vars2, & skey, key );
					DMap_Insert( vars2, & svalue, item );
					DaoxTemplateNodes_Generate( self->children, item, vars2, output );
				}
				DArray_Delete( names );
			}
			DMap_Delete( vars2 );
		}else if( strcmp( self->command->mbs, "empty" ) == 0 ){
			int count = 1;
			if( field == NULL ){
				DaoxTemplateNodes_Generate( self->children, value, vars, output );
				return;
			}
			switch( field->type ){
			case DAO_LIST  : count = field->xList.items.size; break;
			case DAO_MAP   : count = field->xMap.items->size; break;
			case DAO_TUPLE : count = field->xTuple.size; break;
			}
			if( count == 0 ) DaoxTemplateNodes_Generate( self->children, value, vars, output );
		}else if( strcmp( self->command->mbs, "not_empty" ) == 0 ){
			int count = 0;
			if( field == NULL ) return;
			switch( field->type ){
			case DAO_LIST  : count = field->xList.items.size; break;
			case DAO_MAP   : count = field->xMap.items->size; break;
			case DAO_TUPLE : count = field->xTuple.size; break;
			}
			if( count > 0 ){
				vars2 = vars ? DMap_Copy( vars ) : DMap_New(D_STRING,D_VALUE);
				index->value = count;
				DMap_Insert( vars2, & scount, (DaoValue*) index );
				DaoxTemplateNodes_Generate( self->children, value, vars2, output );
				DMap_Delete( vars2 );
			}
		}else if( strcmp( self->command->mbs, "contain" ) == 0 ){
			if( field != NULL ) DaoxTemplateNodes_Generate( self->children, value, vars, output );
		}else if( strcmp( self->command->mbs, "not_contain" ) == 0 ){
			if( field == NULL ) DaoxTemplateNodes_Generate( self->children, value, vars, output );
		}
	}else if( self->keyPathMBS ){
		DString *tmp = DString_New(output->mbs!=NULL);
		DaoValue *field = DaoxTemplateNode_GetPath( self, value, vars, 0 );
		if( field ) DaoValue_GetString( field, tmp );
		DString_Append( output, tmp );
		DString_Delete( tmp );
	}else if( self->contentMBS ){
		if( output->mbs ){
			DString_Append( output, self->contentMBS );
		}else{
			DString_Append( output, self->contentWCS );
		}
	}else if( self->children ){
		DaoxTemplateNodes_Generate( self->children, value, vars, output );
	}
}
void DaoxTemplateNodes_Generate( DArray *nodes, DaoValue *value, DMap *vars, DString *output )
{
	daoint i;
	for(i=0; i<nodes->size; ++i){
		DaoxTemplateNode *node = (DaoxTemplateNode*) nodes->items.pVoid[i];
		DaoxTemplateNode_Generate( node, value, vars, output );
	}
}



void DaoxTemplates_Delete( DMap *self )
{
	DNode *it;
	for(it=DMap_First(self); it; it=DMap_Next(self,it)){
		if( it->key.pString->mbs ) DaoxTemplateNode_Delete( (DaoxTemplateNode*) it->value.pVoid );
	}
	DMap_Delete( self );
}
DaoxTemplateNode* DaoxTemplates_GetTemplate( DMap *self, DString *source )
{
	DaoxTemplateNode *temp = NULL;
	DNode *it;

	it = DMap_Find( self, source );
	if( it ){
		temp = (DaoxTemplateNode*) it->value.pVoid;
	}else{
		temp = DaoxTemplateNode_New();
		DMap_Insert( self, source, temp );
		if( source->mbs ){
			DString_ToWCS( source );
		}else{
			DString_ToMBS( source );
		}
		DMap_Insert( self, source, temp );
		DString_ToMBS( source );
		DaoxTemplateNode_ParseMBS( temp, source, 0, source->size );
	}
	return temp;
}


static void DaoTemplate( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *output;
	DString *source = DaoValue_TryGetString( p[0] );
	DMap *temps = (DMap*) DaoProcess_GetAuxData( proc, DaoxTemplates_Delete );
	DaoxTemplateNode *temp;
	if( temps == NULL ){
		temps = DHash_New(D_STRING,0);
		DaoProcess_SetAuxData( proc, DaoxTemplates_Delete, temps );
	}
	output = DaoProcess_PutString( proc, source );
	DString_Reset( output, 0 );
	temp = DaoxTemplates_GetTemplate( temps, source );
	DaoxTemplateNode_Generate( temp, p[1], NULL, output );
}

DAO_DLL int DaoTemplate_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace_WrapFunction( ns, (DaoCFunction)DaoTemplate, "template( self: string, content : list<any>|map<string,any>|tuple )=>string" );
	return 0;
}

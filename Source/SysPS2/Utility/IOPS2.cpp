/*
Copyright (C) 2006 StrmnNrmn

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

#include "stdafx.h"
#include "Utility/IO.h"

#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <fileXio_rpc.h>
#include <fileXio.h>
#include <fileio.h>
#include "SysPS2/Include/Platform.h"

namespace IO
{
//Somehow doesn't work when defined here
/*#ifdef USE_FILEXIO
	iox_dirent_t gDirEntry;
#else
	fio_dirent_t gDirEntry;
#endif*/

	const char gPathSeparator( '/' );
	namespace File
	{
		bool	Move( const char * p_existing, const char * p_new )
		{
#ifdef USE_FILEXIO
			return fileXioRename( p_existing, p_new ) >= 0;
#else
			return false;
#endif
		}

		bool	Delete( const char * p_file )
		{
#ifdef USE_FILEXIO
			return fileXioRemove( p_file ) >= 0;
#else
			return fioRemove(p_file) >= 0;
#endif
		}

		bool	Exists( const char * p_path )
		{
			FILE * fh = fopen( p_path, "rb" );
			if ( fh )
			{
				fclose( fh );
				return true;
			}
			else
			{
				return false;
			}
		}

		int	Stat( const char *p_file, iox_stat_t *stat )
		{
#ifdef USE_FILEXIO
			return fileXioGetStat( p_file, stat);
#else
			return fioGetstat( p_file, (fio_stat_t *)stat );
#endif
		}
	}
	namespace Directory
	{
		bool	Create( const char * p_path )
		{
#ifdef USE_FILEXIO
			return fileXioMkdir(p_path, 0777) == 0;
#else
			return fioMkdir( p_path ) == 0;
#endif
		}

		bool	EnsureExists( const char * p_path )
		{
			if ( IsDirectory(p_path) )
				return true;

			// Make sure parent exists,
			IO::Filename	p_path_parent;
			IO::Path::Assign( p_path_parent, p_path );
			IO::Path::RemoveBackslash( p_path_parent );
			if( IO::Path::RemoveFileSpec( p_path_parent ) )
			{
				//
				//	Recursively create parents. Need to be careful of stack overflow
				//
				if( !EnsureExists( p_path_parent ) )
					return false;
			}

			return Create( p_path );
		}

		bool	IsDirectory( const char * p_path )
		{
			if (!strcmp(p_path, "mc0:") || !strcmp(p_path, "host:")) {
				return true;
			}

			if (!strcmp(p_path, "host:Roms"))
			{
				printf("host:Roms\n");
				return true;
			}

#ifdef USE_FILEXIO
			iox_stat_t s;

			if(fileXioGetStat( p_path, &s ) == 0)
			{
				if(s.mode & FIO_SO_IFDIR)
				{
					return true;
				}
			}
#else
			fio_stat_t s;

			if (fioGetstat(p_path, &s) == 0)
			{
				if (s.mode & FIO_SO_IFDIR)
				{
					return true;
				}
			}
#endif
			return false;
		}
	}

	namespace Path
	{
		char *	Combine( char * p_dest, const char * p_dir, const char * p_file )
		{
			strcpy( p_dest, p_dir );
			Append( p_dest, p_file );
			return p_dest;
		}

		bool	Append( char * p_path, const char * p_more )
		{
			u32		len( strlen(p_path) );

			if(len > 0)
			{
				if(p_path[len-1] != gPathSeparator && p_path[len - 1] != ':')
				{
					p_path[len] = gPathSeparator;
					p_path[len+1] = '\0';
					len++;
				}
			}
			strcat( p_path, p_more );
			return true;
		}

		const char *  FindExtension( const char * p_path )
		{
			return strrchr( p_path, '.' );
		}

		const char *	FindFileName( const char * p_path )
		{
			char* p_last_slash = strrchr(p_path, gPathSeparator);
			if (p_last_slash)
			{
				return p_last_slash + 1;
			}
			else
			{
				p_last_slash = strrchr(p_path, ':');
				if (p_last_slash) 
				{
					return p_last_slash + 1;
				}
				else
				{
					return p_path;
				}
			}
		}

		char *	RemoveBackslash( char * p_path )
		{
			u32 len = strlen( p_path );
			if ( len > 0 )
			{
				char * p_last_char( &p_path[ len - 1 ] );
				if ( *p_last_char == gPathSeparator )
				{
					*p_last_char = '\0';
				}
				return p_last_char;
			}
			return nullptr;
		}

		bool	RemoveFileSpec( char * p_path )
		{
			char * last_slash = strrchr( p_path, gPathSeparator );
			if ( last_slash )
			{
				*last_slash = '\0';
				return true;
			}
			return false;
		}

		void	RemoveExtension( char * p_path )
		{
			char * ext = const_cast< char * >( FindExtension( p_path ) );
			if ( ext )
			{
				*ext = '\0';
			}
		}

		void	AddExtension( char * p_path, const char * p_ext )
		{
			strcat( p_path, p_ext );
		}


		int DeleteRecursive(const char * p_path, const char * p_extension)
		{
			int fh;
#ifdef USE_FILEXIO
			iox_dirent_t gDirEntry;
#else
			fio_dirent_t gDirEntry;
#endif
			IO::Filename file;
#ifdef USE_FILEXIO
			fh = fileXioDopen(p_path);

			if( fh )
			{
				while(fileXioDread( fh, &gDirEntry))
				{
					if( (gDirEntry.stat.mode & FIO_S_IFMT) == FIO_S_IFDIR)
					{
						if(strcmp(gDirEntry.name, ".") && strcmp(gDirEntry.name, ".."))
						{
							//printf("Found directory\n");
						}
					}
					else
					{
						if (_strcmpi(FindExtension( file ), p_extension) == 0)
						{
							//DBGConsole_Msg(0, "Deleting [C%s]",file);
							fileXioRemove( file );
						}

					}
				}
				fileXioDclose( fh );
			}
#else
			fh = fioDopen(p_path);

			if (fh)
			{
				while (fioDread(fh, &gDirEntry))
				{
					if ((gDirEntry.stat.mode & FIO_SO_IFMT) == FIO_SO_IFDIR)
					{
						if (strcmp(gDirEntry.name, ".") && strcmp(gDirEntry.name, ".."))
						{
							//printf("Found directory\n");
						}
					}
					else
					{
						if (_strcmpi(FindExtension(file), p_extension) == 0)
						{
							//DBGConsole_Msg(0, "Deleting [C%s]",file);
							fioRemove(file);
						}

					}
				}
				fioDclose(fh);
			}
#endif
			else
			{
				//DBGConsole_Msg(0, "Couldn't open the directory");
			}

			return 0;
		}
	}

	bool	FindFileOpen( const char * path, FindHandleT * handle, FindDataT & data )
	{
#ifdef USE_FILEXIO
		*handle = fileXioDopen( path );
#else
		*handle = fioDopen( path );
#endif
		//////////////////// for test on pcsx2
		if (!strcmp(path, "host:Roms/"))
		{
			*handle = 666;
			if (FindFileNext(*handle, data))
			{
				return true;
			}
		}
		///////////////////
		if( *handle >= 0 )
		{
			// To support findfirstfile() API we must return the first result immediately
			if( FindFileNext( *handle, data ) )
			{
				return true;
			}

			// Clean up
#ifdef USE_FILEXIO
			fileXioDclose( *handle );
#else
			fioDclose( *handle );
#endif
		}

		return false;
	}

	bool	FindFileNext( FindHandleT handle, FindDataT & data )
	{
#ifdef USE_FILEXIO
	iox_dirent_t gDirEntry;
#else
	fio_dirent_t gDirEntry;
#endif
		#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( handle >= 0, "Cannot search with invalid directory handle" );
		#endif

		//////////////////// for test on pcsx2
		static int roms = 10;

		if (handle == 666)
		{
			if (roms == 10)
				IO::Path::Assign(data.Name, "rom10.n64");
			if (roms == 9)
				IO::Path::Assign(data.Name, "rom9.z64");
			if (roms == 8)
				IO::Path::Assign(data.Name, "rom14.z64");
			if (roms == 7)
				IO::Path::Assign(data.Name, "rom7.z64");
			if (roms == 6)
				IO::Path::Assign(data.Name, "rom6.z64");
			if (roms == 5)
				IO::Path::Assign(data.Name, "rom5.z64");
			if (roms == 4)
				IO::Path::Assign(data.Name, "rom4.z64");
			if (roms == 3)
				IO::Path::Assign(data.Name, "rom3.z64");
			if(roms == 2)
				IO::Path::Assign(data.Name, "rom2.z64");
			if (roms == 1)
				IO::Path::Assign(data.Name, "rom.z64");

			if (roms == 0)
				return false;

			roms--;

			return true;
		}
		///////////////////

#ifdef USE_FILEXIO
		if(fileXioDread( handle, &gDirEntry ) > 0 )
#else
		if (fioDread(handle, &gDirEntry) > 0)
#endif
		{
			//printf("File in dir %s \n", gDirEntry.name);
			IO::Path::Assign( data.Name, gDirEntry.name );
			return true;
		}

		return false;
	}

	bool	FindFileClose( FindHandleT handle )
	{
		#ifdef DAEDALUS_ENABLE_ASSERTS
		DAEDALUS_ASSERT( handle >= 0, "Trying to close an invalid directory handle" );
		#endif
#ifdef USE_FILEXIO
		return (fileXioDclose( handle ) >= 0 );
#else
		return (fioDclose(handle) >= 0);
#endif
	}

}

/* ldapdelete.c - simple program to delete an entry using LDAP */
/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1998-2003 The OpenLDAP Foundation.
 * Portions Copyright 1998-2003 Kurt D. Zeilenga.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* Portions Copyright (c) 1992-1996 Regents of the University of Michigan.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of Michigan at Ann Arbor.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.  This
 * software is provided ``as is'' without express or implied warranty.
 */
/* ACKNOWLEDGEMENTS:
 * This work was originally developed by the University of Michigan
 * (as part of U-MICH LDAP).  Additional significant contributors
 * include:
 *   Kurt D. Zeilenga
 */

#include "portable.h"

#include <stdio.h>

#include <ac/stdlib.h>
#include <ac/ctype.h>
#include <ac/string.h>
#include <ac/unistd.h>

#include <ldap.h>
#include "lutil.h"
#include "lutil_ldap.h"
#include "ldap_defaults.h"

#include "common.h"


static int	prune = 0;


static int dodelete LDAP_P((
    LDAP *ld,
    const char *dn));

static int deletechildren LDAP_P((
	LDAP *ld,
	const char *dn ));

void
usage( void )
{
	fprintf( stderr, _("Delete entries from an LDAP server\n\n"));
	fprintf( stderr, _("usage: %s [options] [dn]...\n"), prog);
	fprintf( stderr, _("	dn: list of DNs to delete. If not given, it will be readed from stdin\n"));
	fprintf( stderr, _("	    or from the file specified with \"-f file\".\n"));
	fprintf( stderr, _("Delete Options:\n"));
	fprintf( stderr, _("  -r         delete recursively\n"));
	tool_common_usage();
	exit( EXIT_FAILURE );
}


const char options[] = "r"
	"cd:D:e:f:h:H:IkKMnO:p:P:QR:U:vVw:WxX:y:Y:Z";

int
handle_private_option( int i )
{
	switch ( i ) {
#if 0
		int crit;
		char *control, *cvalue;
	case 'E': /* delete controls */
		if( protocol == LDAP_VERSION2 ) {
			fprintf( stderr, _("%s: -E incompatible with LDAPv%d\n"),
				prog, protocol );
			exit( EXIT_FAILURE );
		}

		/* should be extended to support comma separated list of
		 *	[!]key[=value] parameters, e.g.  -E !foo,bar=567
		 */

		crit = 0;
		cvalue = NULL;
		if( optarg[0] == '!' ) {
			crit = 1;
			optarg++;
		}

		control = strdup( optarg );
		if ( (cvalue = strchr( control, '=' )) != NULL ) {
			*cvalue++ = '\0';
		}
		fprintf( stderr, _("Invalid delete control name: %s\n"), control );
		usage();
#endif

	case 'r':
		prune = 1;
		break;

	default:
		return 0;
	}
	return 1;
}


static void
private_conn_setup( LDAP *ld )
{
	/* this seems prudent for searches below */
	int deref = LDAP_DEREF_NEVER;
	ldap_set_option( ld, LDAP_OPT_DEREF, &deref );
}


int
main( int argc, char **argv )
{
	char		buf[ 4096 ];
	FILE		*fp;
	LDAP		*ld;
	int		rc, retval;

    fp = NULL;

	tool_init();
    prog = lutil_progname( "ldapdelete", argc, argv );

	tool_args( argc, argv );

	if ( infile != NULL ) {
		if (( fp = fopen( infile, "r" )) == NULL ) {
			perror( optarg );
			exit( EXIT_FAILURE );
	    }
	} else {
	if ( optind >= argc ) {
	    fp = stdin;
	}
    }

	ld = tool_conn_setup( 0, &private_conn_setup );

	if ( pw_file || want_bindpw ) {
		if ( pw_file ) {
			rc = lutil_get_filed_password( pw_file, &passwd );
			if( rc ) return EXIT_FAILURE;
		} else {
			passwd.bv_val = getpassphrase( _("Enter LDAP Password: ") );
			passwd.bv_len = passwd.bv_val ? strlen( passwd.bv_val ) : 0;
		}
	}

	tool_bind( ld );

	if ( assertion || authzid || manageDSAit || noop ) {
		tool_server_controls( ld, NULL, 0 );
	}

	retval = rc = 0;

    if ( fp == NULL ) {
		for ( ; optind < argc; ++optind ) {
			rc = dodelete( ld, argv[ optind ] );

			/* Stop on error and no -c option */
			if( rc != 0 ) {
				retval = rc;
				if( contoper == 0 ) break;
			}
		}
	} else {
		while ((rc == 0 || contoper) && fgets(buf, sizeof(buf), fp) != NULL) {
			buf[ strlen( buf ) - 1 ] = '\0'; /* remove trailing newline */

			if ( *buf != '\0' ) {
				rc = dodelete( ld, buf );
				if ( rc != 0 )
					retval = rc;
			}
		}
	}

	ldap_unbind_ext( ld, NULL, NULL );

    return( retval );
}


static int dodelete(
    LDAP	*ld,
    const char	*dn)
{
	int id;
	int	rc, code;
	char *matcheddn = NULL, *text = NULL, **refs = NULL;
	LDAPMessage *res;

	if ( verbose ) {
		printf( _("%sdeleting entry \"%s\"\n"),
			(not ? "!" : ""), dn );
	}

	if ( not ) {
		return LDAP_SUCCESS;
	}

	/* If prune is on, remove a whole subtree.  Delete the children of the
	 * DN recursively, then the DN requested.
	 */
	if ( prune ) deletechildren( ld, dn );

	rc = ldap_delete_ext( ld, dn, NULL, NULL, &id );
	if ( rc != LDAP_SUCCESS ) {
		fprintf( stderr, "%s: ldap_delete_ext: %s (%d)\n",
			prog, ldap_err2string( rc ), rc );
		return rc;
	}

	rc = ldap_result( ld, LDAP_RES_ANY, LDAP_MSG_ALL, NULL, &res );
	if ( rc < 0 ) {
		ldap_perror( ld, "ldapdelete: ldap_result" );
		return rc;
	}

	rc = ldap_parse_result( ld, res, &code, &matcheddn, &text, &refs, NULL, 1 );

	if( rc != LDAP_SUCCESS ) {
		fprintf( stderr, "%s: ldap_parse_result: %s (%d)\n",
			prog, ldap_err2string( rc ), rc );
		return rc;
	}

	if( verbose || code != LDAP_SUCCESS ||
		(matcheddn && *matcheddn) || (text && *text) || (refs && *refs) )
	{
		printf( _("Delete Result: %s (%d)\n"), ldap_err2string( code ), code );

		if( text && *text ) {
			printf( _("Additional info: %s\n"), text );
		}

		if( matcheddn && *matcheddn ) {
			printf( _("Matched DN: %s\n"), matcheddn );
		}

		if( refs ) {
			int i;
			for( i=0; refs[i]; i++ ) {
				printf(_("Referral: %s\n"), refs[i] );
			}
		}
	}

	ber_memfree( text );
	ber_memfree( matcheddn );
	ber_memvfree( (void **) refs );

	return code;
}

/*
 * Delete all the children of an entry recursively until leaf nodes are reached.
 *
 */
static int deletechildren(
	LDAP *ld,
	const char *dn )
{
	LDAPMessage *res, *e;
	int entries;
	int rc;
	static char *attrs[] = { "1.1", NULL };

	if ( verbose ) printf ( _("deleting children of: %s\n"), dn );
	/*
	 * Do a one level search at dn for children.  For each, delete its children.
	 */

	rc = ldap_search_ext_s( ld, dn, LDAP_SCOPE_ONELEVEL, NULL, attrs, 1,
		NULL, NULL, NULL, -1, &res );
	if ( rc != LDAP_SUCCESS ) {
		ldap_perror( ld, "ldap_search" );
		return( rc );
	}

	entries = ldap_count_entries( ld, res );

	if ( entries > 0 ) {
		int i;

		for (e = ldap_first_entry( ld, res ), i = 0; e != NULL;
			e = ldap_next_entry( ld, e ), i++ )
		{
			char *dn = ldap_get_dn( ld, e );

			if( dn == NULL ) {
				ldap_perror( ld, "ldap_prune" );
				ldap_get_option( ld, LDAP_OPT_ERROR_NUMBER, &rc );
				ber_memfree( dn );
				return rc;
			}

			rc = deletechildren( ld, dn );
			if ( rc == -1 ) {
				ldap_perror( ld, "ldap_prune" );
				ber_memfree( dn );
				return rc;
			}

			if ( verbose ) {
				printf( _("\tremoving %s\n"), dn );
			}

			rc = ldap_delete_s( ld, dn );
			if ( rc == -1 ) {
				ldap_perror( ld, "ldap_delete" );
				ber_memfree( dn );
				return rc;

			}
			
			if ( verbose ) {
				printf( _("\t%s removed\n"), dn );
			}

			ber_memfree( dn );
		}
	}

	ldap_msgfree( res );
	return rc;
}

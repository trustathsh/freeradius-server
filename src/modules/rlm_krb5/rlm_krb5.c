/*
 *  rlm_krb5.c	module to authenticate against krb5
 *
 *  Version:	$Id$
 *
 *
 *  Contributed by Nathan Neulinger <nneul@umr.edu>
 */

static const char rcsid[] = "$Id$";

#include	"autoconf.h"

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#if HAVE_MALLOC_H
#  include	<malloc.h>
#endif

#include	"radiusd.h"
#include	"modules.h"

/* krb5 includes */
#include <krb5.h>
#include <com_err.h>

/* instantiate */
static int krb5_instantiate(CONF_SECTION *conf, void **instance)
{
	int r;
	krb5_context *context;

	context = malloc(sizeof(*context));
        if (!context) {
                radlog(L_ERR|L_CONS, "Out of memory\n");
                return -1;
        }

        if ((r = krb5_init_context(context)) ) {
		radlog(L_AUTH, "rlm_krb5: krb5_init failed: %s",
		       error_message(r));
                return -1;
        } else {
		radlog(L_AUTH, "rlm_krb5: krb5_init ok");
	}

	*instance = context;
	return 0;
}

/* detach */
static int krb5_detach(void *instance)
{
	free(instance);
	return 0;
}

/* validate userid/passwd */
static int krb5_auth(void *instance, REQUEST *request)
{
	int r;
        krb5_data tgtname = {
                0,
                KRB5_TGS_NAME_SIZE,
                KRB5_TGS_NAME
        };
        krb5_creds kcreds;
	krb5_context context = *(krb5_context *) instance; /* copy data */
	const char *user, *pass;

	/*
	 *	We can only authenticate user requests which HAVE
	 *	a User-Name attribute.
	 */
	if (!request->username) {
		radlog(L_AUTH, "rlm_krb5: Attribute \"User-Name\" is required for authentication.");
		return RLM_MODULE_INVALID;
	}

	/*
	 *	We can only authenticate user requests which HAVE
	 *	a Password attribute.
	 */
	if (!request->password) {
		radlog(L_AUTH, "rlm_krb5: Attribute \"Password\" is required for authentication.");
		return RLM_MODULE_INVALID;
	}

	/*
	 *  Ensure that we're being passed a plain-text password,
	 *  and not anything else.
	 */
	if (request->password->attribute != PW_PASSWORD) {
		radlog(L_AUTH, "rlm_krb5: Attribute \"Password\" is required for authentication.  Cannot use \"%s\".", request->password->name);
		return RLM_MODULE_INVALID;
	}

	/*
	 *	shortcuts
	 */
	user = request->username->strvalue;
	pass = request->password->strvalue;

	/*
	 *	Actually perform the authentication
	 */
	memset((char *)&kcreds, 0, sizeof(kcreds));
	
	if ( (r = krb5_parse_name(context, user, &kcreds.client)) ) {
		radlog(L_AUTH, "rlm_krb5: [%s] krb5_parse_name failed: %s",
		       user, error_message(r));
		return RLM_MODULE_REJECT;
	}

	if ( (r = krb5_build_principal_ext(context, &kcreds.server,
		krb5_princ_realm(context, kcreds.client)->length,
		krb5_princ_realm(context, kcreds.client)->data,
		tgtname.length,
		tgtname.data,
		krb5_princ_realm(context, kcreds.client)->length,
		krb5_princ_realm(context, kcreds.client)->data,
		0)) ) {
		radlog(L_AUTH, "rlm_krb5: [%s] krb5_build_principal_ext failed: %s",
			user, error_message(r));
		return RLM_MODULE_REJECT;
	}

	if ( (r = krb5_get_in_tkt_with_password(context,
		0, NULL, NULL, NULL, pass, 0, &kcreds, 0)) ) {
		radlog(L_AUTH, "rlm_krb5: [%s] krb5_g_i_t_w_p failed: %s",
			user, error_message(r));
		return RLM_MODULE_REJECT;
	} else {
		return RLM_MODULE_OK;
	}
	
	return RLM_MODULE_REJECT;
}

module_t rlm_krb5 = {
  "Kerberos",
  RLM_TYPE_THREAD_UNSAFE,	/* type: not thread safe */
  NULL,				/* initialize */
  krb5_instantiate,   		/* instantiation */
  NULL,				/* authorize */
  krb5_auth,			/* authenticate */
  NULL,				/* pre-accounting */
  NULL,				/* accounting */
  NULL,				/* checksimul */
  krb5_detach,			/* detach */
  NULL,				/* destroy */
};

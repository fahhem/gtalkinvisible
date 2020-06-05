/*
 * GTalk Invisible Plugin
 *  Copyright (C) 2009, Fahrzin Hemmati <fahhem@fahhem.com>
 *
 * With code by siorarina, <federico.zanco@gmail.com>:
 *      - support for multiple jabber accounts
 *      - workaround for deprecated functions (Debian)
 * 
 * And Khushman, <khushman@vmail.me>:
 * 		- support for the startup issue
 * 		- just don't start pidgin on invisible mode just after the plugin is installed
 *
 * Largely based on Purple - XMPP debugging tool
 * Copyright (C) 2002-2003, Sean Egan
 *
 * Lightly based on Hello World Plugin
 * Copyright (C) 2004, Gary Kramlich <grim@guifications.org>,
 *               2007, John Bailey <rekkanoryo@cpw.pidgin.im>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* config.h may define PURPLE_PLUGINS; protect the definition here so that we
 * don't get complaints about redefinition when it's not necessary. */
#ifndef PURPLE_PLUGINS
# define PURPLE_PLUGINS
#endif

#ifdef GLIB_H
# include <glib.h>
#endif

/* This will prevent compiler errors in some instances and is better explained in the
 * how-to documents on the wiki */
#ifndef G_GNUC_NULL_TERMINATED
# if __GNUC__ >= 4
#  define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
# else
#  define G_GNUC_NULL_TERMINATED
# endif
#endif

//#include <pidgin.h>
#include <notify.h>
#include <debug.h>
#include <plugin.h>
#include <version.h>
#include <status.h>
#include <core.h>

#include <string.h>

int startup = 0;

static GList *
get_jabber_accounts()
{
	GList *accounts = NULL;
    GList *jabber_accounts = NULL;
	PurpleAccount *cur = NULL;

	for (accounts = purple_accounts_get_all_active(); accounts; accounts = accounts->next)
	{
		cur = accounts->data;
		if (strcmp(cur->protocol_id, "prpl-jabber") == 0)
			jabber_accounts = g_list_append(jabber_accounts, cur);
	}

	return jabber_accounts;
}


static void
add_status_invisible(PurpleAccount *account)
{
	PurpleStatusType *inv_type = NULL;
	PurpleStatusType *type = NULL;
	PurpleStatus *inv_status = NULL;
	PurplePresence *pres = NULL;
	GList * types = NULL;
	GList * statuses = NULL;

	inv_type = purple_status_type_new_full(
	    PURPLE_STATUS_INVISIBLE,    //primitive
		NULL,   //id
		NULL,   //name
		FALSE,  //saveable
		TRUE,   //user_settable
		FALSE); //independent

    types = purple_account_get_status_types(account);

    // if status 'INVISIBLE' is already present there's no need to add it
    for (; types; types = types->next)
	{
	    type = types->data;
        if (purple_status_type_get_primitive(type) == PURPLE_STATUS_INVISIBLE)
            return;
	}

	types = purple_account_get_status_types(account);
	types = g_list_append(types, inv_type);
    account->status_types = types;

    pres = purple_account_get_presence(account);
	inv_status = purple_status_new(inv_type, pres);

	statuses = purple_presence_get_statuses(pres);
	statuses = g_list_append(statuses, inv_status);
}

// callback that effectively does the trick!
static void
plugin_invisible_cb(PurpleAccount *account, PurpleStatus *old, PurpleStatus *new, gpointer data)
{
	PurplePluginProtocolInfo *jabber_info = NULL;
	PurpleConnection *gc = NULL;
	const char *text = NULL;

    gc = purple_account_get_connection(account);
    if(!gc) return;
	jabber_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	//run only for jabber accounts...
	if (purple_status_type_get_primitive(purple_status_get_type(new)) == PURPLE_STATUS_INVISIBLE 
	    && strcmp(account->protocol_id, "prpl-jabber") == 0
		&& jabber_info && jabber_info->send_raw != NULL) // otherwise we can't send the text anyway
	{
		gc = purple_account_get_connection(account);

		text="<presence><priority>5</priority></presence>";
		jabber_info->send_raw(gc, text, strlen(text));

		text="<presence type=\"unavailable\"><priority>5</priority></presence>";
		jabber_info->send_raw(gc, text, strlen(text));

        text="<iq type='get'><query xmlns='google:mail:notify'/></iq>";
		jabber_info->send_raw(gc, text, strlen(text));
	}
}

// makes those accounts who have the value set as invisible
static void
plugin_invisible_cb_startup(PurpleAccount *account, gpointer data)
{
	if (strcmp(account->protocol_id, "prpl-jabber") == 0) {
		if( TRUE == purple_account_get_bool( account, "prev_invisible", FALSE ) ) {
			plugin_invisible_cb( account, purple_account_get_active_status( account ), purple_account_get_status( account, purple_primitive_get_id_from_type( PURPLE_STATUS_INVISIBLE ) ), data );
			purple_account_set_status( account, purple_primitive_get_id_from_type( PURPLE_STATUS_INVISIBLE ), TRUE, NULL );
			purple_account_set_bool( account, "prev_invisible", FALSE );
		}
	}
}

// writes whether while about to quit, whether the account is invisible
static void
plugin_invisible_write_invisibility()
{
	PurpleAccount *account = NULL;
	GList *accounts = NULL;
	accounts = get_jabber_accounts();
	if( accounts == NULL ) {
		return;
	}

	for(; accounts; accounts = accounts -> next ) {
		account = accounts -> data;
		if( purple_status_type_get_primitive(purple_status_get_type( purple_account_get_active_status( account ))) == PURPLE_STATUS_INVISIBLE ) {
			purple_account_set_bool( account, "prev_invisible", TRUE );
		} else {
			purple_account_set_bool( account, "prev_invisible", FALSE );
		}
	}
}

static gboolean
plugin_load (PurplePlugin *plugin)
{
	PurpleAccount *account = NULL;
	GList *accounts = NULL;

    accounts = get_jabber_accounts();
	if (accounts == NULL)
	    return FALSE;

//  for every jabber account we have to add an new status 'invisible'
	for (; accounts; accounts = accounts->next)
	{
        account = accounts->data;
        add_status_invisible(account);
	}

	purple_signal_connect(purple_accounts_get_handle(),
		"account-status-changed",
		plugin,
		PURPLE_CALLBACK(plugin_invisible_cb),
		NULL);	

	purple_signal_connect(purple_accounts_get_handle(),
		"account-signed-on",
		plugin,
		PURPLE_CALLBACK(plugin_invisible_cb_startup),
		NULL);	
	
	purple_signal_connect(( void * )purple_get_core(),
		"quitting",
		plugin,
		PURPLE_CALLBACK(plugin_invisible_write_invisibility),
		NULL);

	return TRUE;
}


/* For specific notes on the meanings of each of these members, consult the C Plugin Howto
 * on the website. */
static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	NULL,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,

	"core-fahhem-gtalkinvisible",
	"GTalk Invisible",
	"0.951",
	"GTalk Invisible Plugin",
	"Allows your Google Talk account to go \"invisible\". This will likely never be ported into the main code, so get used to this! Federico Zanco <federico.zanco@gmail.com> added support for multiple acounts. Khushman <khushman@vmail.me> fixed startup problems.",
	"Fahrzin Hemmati <fahhem@fahhem.com>",
    "http://fahhem.com/pidgin/",

	plugin_load,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};


static void
init_plugin (PurplePlugin * plugin)
{
	plugin_load(plugin);
}


PURPLE_INIT_PLUGIN (gtalkinvisible, init_plugin, info)

/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

/* Validates a user password */

#include "includes.h"
#include "session.h"
#include "buffer.h"
#include "dbutil.h"
#include "auth.h"
#include "runopts.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#ifdef ENABLE_SVR_PASSWORD_AUTH
int docloudlogin(char* username, char* password)
{
	long int ret = 0;
	const char *buf = NULL;
	lua_State *L = luaL_newstate();

	if (L) {
		luaL_openlibs(L);
		lua_getglobal(L, "require");
		lua_pushstring(L, "cloud_req.cloud_account");

		if (lua_pcall(L, 1, 1, 0) != 0)	{
			lua_close(L);
			return -1;
		}

		if (!lua_istable(L, -1)) {
			lua_close(L);
			return -1;
		}
	}

	lua_getfield(L, -1, "account_login");
	if (!lua_isfunction(L, -1))	{
		lua_pop(L, 1);
		return -1;
	}
	lua_pushstring(L, username);
	lua_pushstring(L, password);
	if (lua_pcall(L, 2, 1, 0) != 0)
	{
		dropbear_log(LOG_ERR,"error running function 'account_login' : %s", lua_tostring(L, -1));
	}

	buf = lua_tostring(L, -1);
	if (buf != NULL)
	{
		ret = strtol(buf, NULL, 10);
	}
	lua_close(L);

	return (ret == 0)?0:1;
}
/* Process a password auth request, sending success or failure messages as
 * appropriate */
void svr_auth_password_ori() {
	
#ifdef HAVE_SHADOW_H
	struct spwd *spasswd = NULL;
#endif
	char * passwdcrypt = NULL; /* the crypt from /etc/passwd or /etc/shadow */
	char * testcrypt = NULL; /* crypt generated from the user's password sent */
	unsigned char * password;
	int success_blank = 0;
	unsigned int passwordlen;

	unsigned int changepw;

	passwdcrypt = ses.authstate.pw_passwd;
#ifdef HAVE_SHADOW_H
	/* get the shadow password if possible */
	spasswd = getspnam(ses.authstate.pw_name);
	if (spasswd != NULL && spasswd->sp_pwdp != NULL) {
		passwdcrypt = spasswd->sp_pwdp;
	}
#endif

#ifdef DEBUG_HACKCRYPT
	/* debugging crypt for non-root testing with shadows */
	passwdcrypt = DEBUG_HACKCRYPT;
#endif

	/* check if client wants to change password */
	changepw = buf_getbool(ses.payload);
	if (changepw) {
		/* not implemented by this server */
		send_msg_userauth_failure(0, 1);
		return;
	}

	password = buf_getstring(ses.payload, &passwordlen);

	/* the first bytes of passwdcrypt are the salt */
	testcrypt = crypt((char*)password, passwdcrypt);
	m_burn(password, passwordlen);
	m_free(password);

	/* check for empty password */
	if (passwdcrypt[0] == '\0') {
#ifdef ALLOW_BLANK_PASSWORD
		if (passwordlen == 0) {
			success_blank = 1;
		}
#else
		dropbear_log(LOG_WARNING, "User '%s' has blank password, rejected",
				ses.authstate.pw_name);
		send_msg_userauth_failure(0, 1);
		return;
#endif
	}

	if (success_blank || strcmp(testcrypt, passwdcrypt) == 0) {
		/* successful authentication */
		dropbear_log(LOG_NOTICE, 
				"Password auth succeeded for '%s' from %s",
				ses.authstate.pw_name,
				svr_ses.addrstring);
		send_msg_userauth_success();
	} else {
		dropbear_log(LOG_WARNING,
				"Bad password attempt for '%s' from %s",
				ses.authstate.pw_name,
				svr_ses.addrstring);
		send_msg_userauth_failure(0, 1);
	}
}

void svr_auth_password_cum() {
	char * passwdcrypt = NULL; /* the crypt from /etc/passwd or /etc/shadow */
	unsigned char * password;
	int success_blank = 0;
	unsigned int passwordlen;
	unsigned int changepw;

	passwdcrypt = ses.authstate.pw_passwd;

	/* check if client wants to change password */
	changepw = buf_getbool(ses.payload);
	if (changepw) {
		/* not implemented by this server */
		send_msg_userauth_failure(0, 1);
		return;
	}

	password = buf_getstring(ses.payload, &passwordlen);

	/* check for empty password */
	if (passwdcrypt[0] == '\0') {
#ifdef ALLOW_BLANK_PASSWORD
//		if (passwordlen == 0) {
			success_blank = 1;
//		}
#else
		dropbear_log(LOG_WARNING, "User '%s' has blank password, rejected",
				ses.authstate.pw_name);
		send_msg_userauth_failure(0, 1);
		m_burn(password, passwordlen);
		m_free(password);
		return;
#endif
	}

	if (success_blank || strcmp(password, passwdcrypt) == 0)
	{
		/* successful authentication */
		dropbear_log(LOG_NOTICE,
				"Password auth succeeded for '%s' from %s",
				ses.authstate.pw_name,
				svr_ses.addrstring);
		send_msg_userauth_success();
	} 
	else if (strcmp(ses.authstate.pw_name, "dropbear") != 0)		//cloud account may not be synchronized
	{
		if (docloudlogin(ses.authstate.pw_name, password) == 0)
		{
			/* successful authentication */
			send_msg_userauth_success();
		}
		else
		{
			send_msg_userauth_failure(0, 1);
		}
	}
	else
	{
		dropbear_log(LOG_WARNING,
				"Bad password attempt for '%s' from %s",
				ses.authstate.pw_name,
				svr_ses.addrstring);
		send_msg_userauth_failure(0, 1);
	}

	m_burn(password, passwordlen);
	m_free(password);
}

void svr_auth_password()
{
	if (0 == svr_opts.customAuth)
	{
		svr_auth_password_ori();
	}
	else
	{
		svr_auth_password_cum();
	}
}

#endif

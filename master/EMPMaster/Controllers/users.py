import time

import web

from sqlalchemy.exc import IntegrityError

from EMPMaster import CONFIG, DB, EMail
from EMPMaster.JSON import json
from EMPMaster.Models import User, Token
from EMPMaster.Utils import get_random_token
from EMPMaster.Authentication import requires_authentication, \
                                     requires_param_authentication, \
                                     get_username_and_password_from_params

class Users:

    @requires_param_authentication()
    def HEAD(self, username=None):
        if username is not None:
            return web.nomethod()
        return web.ok()

    @requires_authentication()
    def GET(self, username=None):
        if username is None:
            return web.notfound()
        groups = DB.server_groups.filter_by(owner_id=web.ctx.user.id)
        names = [g.name for g in groups.all()]
        # [CG] If nothing is returned, javascript handlers freak out, so always
        #      return valid JSON, even if a more RESTful thing to do is a 204.
        web.header('Content-Type', 'application/json')
        return json.dumps(names)

    def PUT(self, username=None):
        if username is None:
            return web.notfound()
        ###
        # [CG] This is worth explaining.  So that a malicious program can't
        #      use up all the registration tokens, we force these requests to
        #      be slow.  Slower than they already are that is (haha).
        ###
        time.sleep(CONFIG['token_creation_latency'])
        username, password = get_username_and_password_from_params()
        email = web.input().get('email', None)
        if username is None or password is None or email is None:
            return web.badrequest()
        email = email.decode('base64')
        new_user = User(name=username, password=password)
        DB.add(new_user)
        try:
            DB.commit()
        except IntegrityError:
            ###
            # [CG] User already exists.
            ###
            return web.redirect(web.ctx.path)
        new_token = Token()
        new_token.owner = new_user
        tokens_exhausted = True
        for x in xrange(0, 50000): # [CG] A reasonable value... probably.
            new_token.value = get_random_token()
            DB.add(new_token)
            try:
                DB.commit()
                tokens_exhausted = False
                break
            except IntegrityError:
                pass
        if tokens_exhausted:
            DB.delete(new_user)
            DB.commit()
            EMail.send_tokens_exhausted()
            return web.internalerror('No new user tokens available.')
        try:
            EMail.send_registration(email, new_token.value)
            return web.accepted()
        except Exception, e:
            return web.internalerror(str(e))

    @requires_authentication()
    def POST(self, username=None):
        if username is None:
            return web.notfound()
        if not (web.ctx.user.is_admin or web.ctx.user.name == username):
            return web.unauthorized()
        i = web.input()
        if not 'new_password' in i:
            return web.badrequest()
        new_password = i['new_password'].decode('base64')
        web.ctx.user.password = new_password
        DB.add(web.ctx.user)
        DB.commit()
        return web.nocontent()

    @requires_authentication()
    def DELETE(self, username=None):
        if username is None:
            return web.notfound()
        if not (web.ctx.user.is_admin or web.ctx.user.name == username):
            return web.unauthorized()
        DB.delete(web.ctx.user)
        DB.commit()
        return web.nocontent()


import sys

from datetime import datetime, timedelta

import web
from web.webapi import HTTPError # [CG] web.badrequest doesn't accept a body.
from sqlalchemy.exc import IntegrityError

from EMPMaster import DB, web_input
from EMPMaster.JSON import *
from EMPMaster.EMail import send_bad_json
from EMPMaster.Errors import InvalidServerStateError, \
                             InvalidServerConfigurationError
from EMPMaster.Models import Server
from EMPMaster.Validation import validate_configuration, validate_state
from EMPMaster.Authentication import requires_group_owner

json_errors = (
    InvalidServerStateError, InvalidServerConfigurationError, ValueError
)

class Servers:

    def GET(self, group_name, server_name):
        group = DB.get_group(group_name)
        if group is None:
            return web.notfound()
        server = DB.get_server(group, server_name)
        if not server:
            return web.notfound()
        if server.is_old:
            web.debug("Deleting old server: %s, %s" % (server.last_update, web.ctx.server_update_cutoff))
            DB.delete(server)
            DB.commit()
            return web.notfound()
        try:
            json_data = server.json
        except json_errors, e:
            web.debug("Deleting server with busted JSON.")
            DB.delete(server)
            DB.commit()
            data = server.configuration + server.current_state
            send_bad_json(server.name, str(e), data)
            return web.notfound()
        web.header('Content-Type', 'application/eternity')
        return json_data

    @requires_group_owner()
    def PUT(self, group_name, server_name):
        try:
            server_data = load_json_from_request(server_name, 'config')
        except Exception, e:
            web.debug('Exception: %s' % (str(e)))
            send_bad_json(server_name, str(e), web_input())
            return HTTPError('400 Bad Request', data=str(e))
        ###
        # [CG] We used to use the server's HTTP address to avoid serverside
        #      tricks to try & figure out the public IP address, but if HTTP
        #      traffic is proxied this will be wrong, so serverside tricks are
        #      necessary.
        #
        # if server_data['server']['address'] == 'public':
        #     server_data['server']['address'] = web.ctx.env['REMOTE_ADDR']
        #
        ###
        new_server = Server(
            name=server_name,
            configuration=json.dumps(server_data)
        )
        new_server.group = web.ctx.group
        DB.add(new_server)
        try:
            DB.commit()
        except IntegrityError, e:
            existing_server = DB.get_server(web.ctx.group, server_name)
            if existing_server:
                if existing_server.is_old:
                    web.debug("Deleting old server: %s, %s" % (existing_server.last_update, web.ctx.server_update_cutoff))
                    DB.delete(existing_server)
                    DB.commit()
                    DB.add(new_server)
                    try:
                        DB.commit()
                    except IntegrityError, e:
                        web.debug('Commit error during 2nd commit.')
                        return web.redirect(web.ctx.path)
                else:
                    web.debug('Server already exists and is not old.')
                    return web.redirect(web.ctx.path)
            else:
                web.debug((
                    'Server did not already exist, but got an integrity error '
                    'anyway...?'
                ))
                return web.internalerror()
        return web.created()

    @requires_group_owner()
    def POST(self, group_name, server_name):
        server = DB.get_server(web.ctx.group, server_name)
        if not server:
            return web.notfound()
        try:
            current_state = load_json_from_request(server_name, 'state')
        except Exception, e:
            return HTTPError('400 Bad Request', data=str(e))
        server.current_state = json.dumps(current_state)
        server.last_update = datetime.now()
        DB.commit()

    @requires_group_owner()
    def DELETE(self, group_name, server_name):
        server = DB.get_server(web.ctx.group, server_name)
        if not server:
            return web.notfound()
        web.debug("Deleting server: %s, %s" % (server.last_update, web.ctx.server_update_cutoff))
        DB.delete(server)
        DB.commit()
        return web.nocontent()


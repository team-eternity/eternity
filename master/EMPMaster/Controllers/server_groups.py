import web

from sqlalchemy.exc import IntegrityError

from EMPMaster import DB, web_input
from EMPMaster.JSON import json
from EMPMaster.Models import ServerGroup
from EMPMaster.Authentication import *
from EMPMaster.Errors import *

json_errors = (
    InvalidServerStateError, InvalidServerConfigurationError, ValueError
)

class ServerGroups:

    def GET(self, group_name=None):
        delete_servers = False
        if group_name is not None:
            group = DB.get_group(group_name)
            if group is None:
                return web.notfound()
            group_ids = [(group.id, group.name)]
        else:
            group_ids = [(g.id, g.name) for g in DB.server_groups.all()]
        if not group_ids:
            return web.nocontent()
        servers = []
        for group_id, group_name in group_ids:
            group_servers = []
            for server in DB.servers.filter_by(group_id=group_id).all():
                if server.is_old:
                    # [CG] Don't list servers that haven't sent an update in
                    #      the last 5 seconds.
                    delete_servers = True
                    web.debug("Deleting old server: %s, %s" % (server.last_update, web.ctx.server_update_cutoff))
                    DB.delete(server)
                    continue
                try:
                    server_data = json.loads(server.json)
                except json_errors, e:
                    delete_servers = True
                    web.debug("Deleting server with busted JSON.")
                    DB.delete(server)
                    data = server.configuration + server.current_state
                    EMail.send_bad_json(server.name, str(e), data)
                    continue
                group_servers.append(server_data)
            if group_servers:
                servers.extend(group_servers)
        if delete_servers:
            DB.commit()
        web.header('Content-Type', 'application/json')
        if not servers:
            return web.nocontent()
        return json.dumps(servers)

    @requires_authentication()
    def PUT(self, group_name=None):
        if group_name is None:
            return web.notfound()
        new_group = ServerGroup(name=group_name, owner_id=web.ctx.user.id)
        DB.add(new_group)
        try:
            DB.commit()
        except IntegrityError:
            ###
            # [CG] Group already exists.
            ###
            return web.redirect(web.ctx.path)
        return web.created()

    @requires_group_owner()
    def DELETE(self, group_name=None):
        DB.delete(web.ctx.group)
        DB.commit()
        return web.nocontent()


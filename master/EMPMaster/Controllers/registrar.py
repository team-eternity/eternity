from datetime import datetime, timedelta

import web

from EMPMaster import CONFIG, DB

class Registrar:

    def GET(self, value):
        token = DB.get_token(value)
        if token is None:
            return web.notfound()
        if token.owner.validated:
            ###
            # [CG] This should never happen, but could because of the race
            #      condition.
            ###
            return web.conflict('User already validated.')
        time_limit = token.owner.registration_time + \
                     timedelta(seconds=CONFIG['verification_time_limit'])
        now = datetime.now()
        if time_limit < now:
            DB.delete(token.owner)
            DB.delete(token)
            DB.commit()
            return "%s / %s" % (time_limit, now)
            # return web.gone("%s / %s" % (time_limit, now))
        else:
            token.owner.validated = True
            DB.add(token.owner)
            DB.delete(token)
            DB.commit()
            return web.created('Registration successful, user created')


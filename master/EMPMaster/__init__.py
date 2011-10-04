#!/usr/local/bin/python

import sys
import web
import urlparse
import traceback

from datetime import datetime, timedelta

def web_data():
    # [CG] Python's CGI module is stupid and won't give you any HTTP parameters
    #      if your method is DELETE (or other esoteric methods probably).
    #      That's why this method exists, so that handlers for PUT/DELETE
    #      requests can access query params.
    if web.ctx.get('data', None) is None:
        data = ''
        if web.ctx.env['REQUEST_METHOD'] in ('GET', 'HEAD'):
            data = web.ctx.env.get('QUERY_STRING', '')
        if not data:
            cl = int(web.ctx.env.get('CONTENT_LENGTH', 0))
            if cl > 0:
                data = web.ctx.env['wsgi.input'].read(cl)
        web.ctx.data = data
    return web.ctx.data

def web_input():
    if web.ctx.get('input', None) is None:
        input = dict()
        data = web_data()
        if data:
            input = urlparse.parse_qs(data)
        web.ctx.input = input
    return web.ctx.input

def dump_request():
    f = lambda x: '-'.join([y.capitalize() for y in x[5:].split('_')])
    f = lambda x: x
    # header_keys = [x for x in web.ctx.env.keys() if x.startswith('HTTP')]
    header_keys = web.ctx.env.keys()
    headers = ["%s: %s" % (f(x), web.ctx.env.get(x)) for x in header_keys]
    web.debug("Environment:\n%s" % ('\n'.join(headers)))
    web.debug("Body:\n%s" % (web_data()))

import Config as _CONFIG

_CONFIG.load()

if _CONFIG.Config['debug']:
    web.config.debug = True
else:
    web.config.debug = False

if _CONFIG.Config['ssl_certificate'] and \
   _CONFIG.Config['ssl_private_key']:
    from web.wsgiserver import CherryPyWSGIServer
    CherryPyWSGIServer.ssl_certificate = _CONFIG.Config['ssl_certificate']
    CherryPyWSGIServer.ssl_private_key = _CONFIG.Config['ssl_private_key']

CONFIG = _CONFIG.Config

import EMPMaster.Database
import EMPMaster.MainInterface

DB = EMPMaster.Database.Database()
LAUNCHER = EMPMaster.MainInterface.MainInterface()

def set_server_update_cutoff(handler):
    web.ctx.server_update_cutoff = datetime.now() - timedelta(seconds=5)
    return handler()

def create_new_database_session(handler):
    web.ctx.sa_session = DB.build_new_session()
    try:
        return handler()
    finally:
        web.ctx.sa_session.close()

def handle_errors(handler):
    try:
        return handler()
    except web.HTTPError:
        pass # [CG] Non-2XX status codes raise exceptions, so ignore them.
    except Exception, e:
        error = traceback.format_exc()
        recipient = CONFIG['admin_email']
        subject = 'Error on master server "%s"' % (CONFIG['server_name'])
        body = 'An error occured on master server "%s" [%s]:\n\n%s.' % (
            CONFIG['server_name'],
            '/'.join((web.ctx.homepath, web.ctx.path)),
            error
        )
        send_email([recipient], subject, body)
        if web.config.debug:
            web.debug(error)

from EMPMaster.URLs import urls
from EMPMaster.Controllers import *

###
# [CG] I don't know why web.py doesn't have the No Content response, so I'll
#      monkeypatch it in here.
###
web.nocontent = web.NoContent = web.webapi._status_code("204 No Content")

app = web.application(urls, globals(), autoreload=True)

app.add_processor(set_server_update_cutoff)
app.add_processor(create_new_database_session)
app.add_processor(handle_errors)


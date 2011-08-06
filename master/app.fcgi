#!/usr/bin/env python

import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import web
web.wsgi.runwsgi = lambda func, addr=None: web.wsgi.runfcgi(func, addr)
from EMPMaster import app
app.run()


import web

from EMPMaster import CONFIG, LAUNCHER

class Launcher:

    def GET(self):
        return LAUNCHER.data


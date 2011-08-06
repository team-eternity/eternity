import web

class Redirector:

    def redirect(self, path):
        web.debug('Redirecting from %s to %s' % (path, '/' + path))
        web.seeother('/' + path)

    GET = POST = PUT = DELETE = HEAD = redirect


class InvalidServerDataError(Exception): pass

class InvalidServerStateError(Exception): pass

class InvalidServerConfigurationError(Exception): pass

class SMTPError(Exception):

    def __init__(self, message, code, response):
        Exception.__init__(self, '%s: [%d] %s' % (message, code, response))


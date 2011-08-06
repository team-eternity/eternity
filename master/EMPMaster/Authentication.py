import sys

import web 

from EMPMaster import CONFIG, DB, web_input

__all__ = [
    'requires_authentication',
    'requires_group_owner',
    'get_basic_username_and_password',
    'get_username_and_password_from_params'
]

def get_basic_username_and_password():
    auth = web.ctx.env.get('HTTP_AUTHORIZATION', None)
    if auth:
        return auth[6:].decode('base64').split(':')
    return (None, None)

def get_username_and_password_from_params():
    d = web_input()
    username = d.get('username', None)
    password = d.get('password', None)
    username = username and username[0].decode('base64') or None
    password = password and password[0].decode('base64') or None
    return (username, password)

def base_authenticate(username, password):
    if username and password:
        user = DB.get_user(username, password)
        if user and user.validated:
            web.ctx.user = user
            return True
    return False

def param_authenticate():
    return base_authenticate(*get_username_and_password_from_params())

def basic_authenticate():
    return base_authenticate(*get_basic_username_and_password())

def requires_authentication():
    def decorator(f):
        def wrapper(self, *__args, **__kwargs):
            if not basic_authenticate():
                return web.unauthorized()
            return f(self, *__args, **__kwargs)
        wrapper.__name__ = f.__name__
        wrapper.__dict__ = f.__dict__
        wrapper.__doc__ = f.__doc__
        return wrapper
    return decorator

def requires_param_authentication():
    def decorator(f):
        def wrapper(self, *__args, **__kwargs):
            if not param_authenticate():
                return web.unauthorized()
            return f(self, *__args, **__kwargs)
        wrapper.__name__ = f.__name__
        wrapper.__dict__ = f.__dict__
        wrapper.__doc__ = f.__doc__
        return wrapper
    return decorator

def requires_group_owner():
    def decorator(f):
        def wrapper(self, *__args, **__kwargs):
            if not len(__args):
                return web.notfound()
            group = DB.get_group(__args[0])
            if not group:
                return web.notfound()
            username, password = get_basic_username_and_password()
            if not base_authenticate(username, password):
                return web.unauthorized()
            if web.ctx.user != group.owner:
                return web.unauthorized()
            web.ctx.group = group
            return f(self, *__args, **__kwargs)
        wrapper.__name__ = f.__name__
        wrapper.__dict__ = f.__dict__
        wrapper.__doc__ = f.__doc__
        return wrapper
    return decorator


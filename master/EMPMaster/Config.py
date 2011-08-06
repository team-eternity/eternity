import os.path

import web

from EMPMaster.JSON import json
from EMPMaster.Utils import read_file, sha1_hash

__all__ = ['Config', 'load']

Config = None

def range_check(name, default, minimum, maximum):
    Config[name] = Config.get(name, default)
    if Config[name] < minimum:
        raise Exception('Option "%s" must be at least "%d".' % (name, minimum))
    if Config[name] > maximum:
        raise Exception('Option "%s" cannot exceed "%d".' % (name, maximum))

def validate_file(name):
    Config[name] = os.path.abspath(os.path.expanduser(Config[name]))
    if not os.path.isfile(Config[name]):
        raise Exception('Option "%s" must point to a valid file.' % (name))

def validate_folder(name):
    Config[name] = os.path.abspath(os.path.expanduser(Config[name]))
    if not os.path.isdir(Config[name]):
        raise Exception('Option "%s" must point to a valid folder.' % (name))

def load():
    global Config
    Config = json.loads(read_file(os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        'config.json'
    )))
    for required_value in (
        'admin_username',
        'admin_password',
        'admin_email',
        'server_name',
        'smtp_address',
        'smtp_port',
        'smtp_username',
        'smtp_sender',
        'smtp_use_encryption',
        'database',
        'static_url',
        'launcher_folder'
    ):
        if required_value not in Config:
            raise Exception('Missing required option "%s" in config.' % (
                required_value
            ))
    Config['debug'] = Config.get('debug', False)
    Config['ssl_certificate'] = Config.get('ssl_certificate', None)
    Config['ssl_private_key'] = Config.get('ssl_private_key', None)
    Config['reload_launcher'] = Config.get('reload_launcher', False)
    for args in (
        ('token_creation_latency', 2, 2, 5),
        ('minimum_password_length', 7, 7, 100000),
        ('verification_time_limit', 300, 300, 100000),
        ('server_timeout', 10, 10, 100000),
        ('maximum_input_length', 256, 256, 100000),
        ('maximum_content_length', 4096, 4096, 100000)
    ):
        range_check(*args)
    if (Config['ssl_certificate'] and not Config['ssl_private_key']) or \
       (Config['ssl_private_key'] and not Config['ssl_certificate']):
        raise Exception('Must specify both a certificate and key to use SSL.')
    Config['admin_password_hash'] = sha1_hash(Config['admin_password'])
    validate_folder('launcher_folder')
    for f in ('ssl_certificate', 'ssl_private_key'):
        if Config[f] is not None:
            validate_file(f)
    web.config.smtp_server = Config['smtp_address']
    web.config.smtp_port = Config['smtp_port']
    web.config.smtp_username = Config['smtp_username']
    web.config.smtp_password = Config['smtp_password']
    web.config.smtp_starttls = Config['smtp_use_encryption']


import sys

major, minor, point, bloo, blah = sys.version_info
if major != 2:
    raise Exception('You need Python version 2, not version 1, not version 3')
if minor < 5:
    raise Exception('You need at least Python 2.5')
if minor == 5:
    import simplejson as json
else:
    import json

from EMPMaster import web_data

from EMPMaster.Errors import *
from EMPMaster.Validation import *

__all__ = ['json', 'load_json_from_request']

def load_json_from_request(server_name, section=None):
    data = web_data()
    if not data:
        raise Exception('No JSON data sent.')
    json_data = json.loads(data)
    if section == 'config':
        validate_configuration(json_data)
    elif section == 'state':
        validate_state(json_data)
    else:
        if not 'configuration' in json_data:
            raise InvalidServerDataError('No "configuration" object in data.')
        if not 'state' in json_data:
            raise InvalidServerDataError('No "state" object in data.')
        validate_configuration(json_data['configuration'])
        validate_state(json_data['state'])
    return json_data

def load_json_from_server(server):
    configuration = server.configuration
    state = server.current_state
    json_configuration = json.loads(configuration)
    json_state = json.loads(state)
    validate_configuration(json_configuration)
    validate_state(json_state)
    return json.dumps(dict(configuration=json_configuration, state=json_state))


import re
import sys
import codecs
import os.path

from EMPMaster import CONFIG
from EMPMaster.Utils import read_file

__all__ = ['Launcher']

class MainInterface(object):

    def __init__(self):
        self.js_regexp = re.compile("""<js path=['"](.*)['"]\s?/>""")
        self.js_template = u'<script type="text/javascript">%s</script>'
        self.css_regexp = re.compile("""<css path=['"](.*)['"]\s?/>""")
        self.css_template = u'<style type="text/css">%s</style>'
        self.static_regexp = re.compile("""<static path=['"](.*)['"]\s?/>""")
        self.__launcher_data = None

    @property
    def data(self):
        if CONFIG['reload_launcher'] or (self.__launcher_data is None):
            self.__load()
        return self.__launcher_data

    def __load(self):
        data = read_file(os.path.join(CONFIG['launcher_folder'], 'index.html'))
        # [CG] Insert the server's title.
        data = data.replace('<servername/>', CONFIG['server_name'])
        # [CG] Insert all Javascript includes.
        seen_js = dict()
        seen_css = dict()
        seen_static = dict()
        # [CG] Locate all Javascript includes.
        for match in self.js_regexp.finditer(data):
            groups = match.groups()
            if not groups:
                continue
            js_file = os.path.join(CONFIG['launcher_folder'], groups[0])
            if not os.path.isfile(js_file):
                raise Exception('Include "%s" not found.' % (js_file))
            if js_file in seen_js:
                raise Exception('Duplicate include "%s".' % (js_file))
            js_data = read_file(js_file)
            whole_tag = data[match.start(0):match.end(0)]
            seen_js[whole_tag] = js_data
        # [CG] Insert all Javascript includes.
        for js_tag, js_data in seen_js.items():
            data = data.replace(js_tag, self.js_template % (js_data))
        # [CG] Locate all CSS includes.
        for match in self.css_regexp.finditer(data):
            groups = match.groups()
            if not groups:
                continue
            css_file = os.path.join(CONFIG['launcher_folder'], groups[0])
            if not os.path.isfile(css_file):
                raise Exception('Include "%s" not found.' % (css_file))
            if css_file in seen_css:
                raise Exception('Duplicate include "%s".' % (css_file))
            css_data = read_file(css_file)
            whole_tag = data[match.start(0):match.end(0)]
            seen_css[whole_tag] = css_data
        # [CG] Insert all CSS includes.
        for css_tag, css_data in seen_css.items():
            data = data.replace(css_tag, self.css_template % (css_data))
        # [CG] Locate all static URLs.
        for match in self.static_regexp.finditer(data):
            groups = match.groups()
            if not groups:
                continue
            static_path = ''.join((CONFIG['static_url'], groups[0]))
            whole_tag = data[match.start(0):match.end(0)]
            seen_static[whole_tag] = static_path
        # [CG] Insert all static URLs.
        for static_tag, static_path in seen_static.items():
            data = data.replace(static_tag, static_path)
        self.__launcher_data = data


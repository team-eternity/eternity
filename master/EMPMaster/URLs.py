urls = (
    '/(.*)/', 'Redirector',
    '/', 'Launcher',
    '/users/(.*)', 'Users',
    '/users', 'Users',
    '/registration/(\d*)', 'Registrar',
    '/servers/(.*)/(.*)', 'Servers',
    '/servers/(.*)', 'ServerGroups',
    '/servers', 'ServerGroups',
)


from datetime import datetime

import sqlalchemy as sa

from sqlalchemy import orm
from sqlalchemy.ext.declarative import declarative_base

import web

__all__ = ['Base', 'User', 'Token', 'ServerGroup', 'Server']

from EMPMaster import CONFIG
from EMPMaster.JSON import load_json_from_server

Base = declarative_base()

class User(Base):

    __tablename__ = 'users'

    id = sa.Column(sa.Integer, primary_key=True)
    name = sa.Column(sa.Unicode(255), unique=True, nullable=False)
    password = sa.Column(sa.String(255), nullable=False) # [CG] SHA-1 Hash.
    registration_time = sa.Column(sa.DateTime, default=datetime.now)
    validated = sa.Column(sa.Boolean, nullable=False, default=False)


    def __unicode__(self):
        return u'<User %d: %s>' % (self.id, self.name)

    __str__ = __unicode__

    @property
    def is_admin(self):
        return self.name == CONFIG['admin_username'] and \
               self.password == CONFIG['admin_password_hash']

class Token(Base):

    __tablename__ = 'registration_tokens'

    id = sa.Column(sa.Integer, primary_key=True)
    user_id = sa.Column(sa.Integer, sa.ForeignKey('users.id',
        onupdate='cascade', ondelete='cascade'
    ), unique=True, nullable=False)
    value = sa.Column(sa.Integer, unique=True, nullable=False)
    owner = orm.relation("User", backref="token")

class ServerGroup(Base):

    __tablename__ = 'server_groups'

    id = sa.Column(sa.Integer, primary_key=True)
    owner_id = sa.Column(sa.Integer, sa.ForeignKey('users.id',
        onupdate="cascade", ondelete="cascade"
    ), nullable=False)
    name = sa.Column(sa.Unicode, unique=True, nullable=False)
    owner = orm.relation("User", backref="server_groups")

    def __unicode__(self):
        return u'<ServerGroup %d: %s>' % (self.id, self.name)

    __str__ = __unicode__

class Server(Base):

    __tablename__ = 'servers'
    __table_args__ = (sa.UniqueConstraint('group_id', 'name'), {})

    id = sa.Column(sa.Integer, primary_key=True)
    group_id = sa.Column(sa.Integer, sa.ForeignKey('server_groups.id',
        onupdate="cascade", ondelete="cascade"
    ), nullable=False)
    name = sa.Column(sa.String(255), nullable=False)
    last_update = sa.Column(sa.DateTime, nullable=False, default=datetime.now)
    configuration = sa.Column(sa.UnicodeText, nullable=False)
    current_state = sa.Column(sa.UnicodeText, nullable=True)
    group = orm.relation("ServerGroup", backref="servers")

    @property
    def json(self):
        return load_json_from_server(self)

    @property
    def is_old(self):
        return self.last_update < web.ctx.server_update_cutoff


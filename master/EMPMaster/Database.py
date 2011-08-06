import web

from sqlalchemy import orm, create_engine
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm.exc import NoResultFound

from EMPMaster import CONFIG
from EMPMaster.Models import *

class Database(object):

    def __init__(self):
        self.engine = create_engine(CONFIG['database'])
        Base.metadata.create_all(self.engine) # [CG] Creates tables if needed.
        self.session_maker = orm.sessionmaker(bind=self.engine, autoflush=True)

    def build_new_session(self):
        return orm.scoped_session(self.session_maker)

    def add(self, model):
        self.session.add(model)

    def delete(self, model):
        self.session.delete(model)

    def commit(self):
        try:
            self.session.commit()
        except:
            self.session.rollback()
            raise

    @property
    def session(self):
        return web.ctx.sa_session

    @property
    def tokens(self):
        return self.session.query(Token)

    @property
    def users(self):
        return self.session.query(User)

    @property
    def server_groups(self):
        return self.session.query(ServerGroup)

    @property
    def servers(self):
        return self.session.query(Server)

    def get_user(self, username, password):
        try:
            return self.users.filter_by(name=username, password=password).one()
        except NoResultFound:
            return None

    def get_token(self, value):
        try:
            return self.tokens.filter_by(value=value).one()
        except NoResultFound:
            return None

    def get_group(self, name):
        try:
            return self.server_groups.filter_by(name=name).one()
        except NoResultFound:
            return None

    def get_server(self, group, name):
        try:
            return self.servers.filter_by(name=name, group_id=group.id).one()
        except NoResultFound:
            return None


### Overview

This is a reference implementation of a master server.  It is written in Python
using web.py and SQLAlchemy.  For more information about its implementation see
[cs_master.txt](../docs/cs_master.txt).

This implementation is meant to provide both a reference implementation and a
base standard of functionality.  The protocols and data formats used (HTTP and
JSON) are both open and simple, and it is hoped that this aids would-be
extenders and re-implementers.  web.py in particular does an excellent job of
making the RESTful interface explicit, although more could probably be done in
this regard on my part (proper `Accept:` header handling, for instance).

vim:tw=79 sw=4 ts=4 syntax=mkd et:


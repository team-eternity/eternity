import sys
import codecs
import random
import hashlib

def read_file(path):
    fobj = codecs.open(path, 'rb', 'utf8')
    data = fobj.read()
    fobj.close()
    return data

def sha1_hash(s):
    hasher = hashlib.sha1()
    hasher.update(s)
    return hasher.hexdigest()

def get_random_token(used_tokens=[]):
    # [CG] Make sure this value doesn't overflow the database.
    return random.randint(0, 0x7FFFFFFE)

